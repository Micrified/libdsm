#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/mman.h>

#include "dsm.h"
#include "dsm_msg.h"
#include "dsm_inet.h"
#include "dsm_util.h"
#include "dsm_signal.h"
#include "dsm_sync.h"
#include "dsm_holes.h"
#include "dsm_msg_io.h"

/*
 *******************************************************************************
 *                             Symbolic Constants                              *
 *******************************************************************************
*/


// The maximum number of connection attempts allowed.
#define DSM_MAX_SOCK_POLL			15

// The timeout (in microseconds) between connection attempts.
#define DSM_SOCK_POLL_RATE			250000


/*
 *******************************************************************************
 *                              Global Variables                               *
 *******************************************************************************
*/


// Pointer to the shared (and protected) memory map.
void *g_shared_map;

// Size of the shared map.
off_t g_map_size;

// Pointer to shared memory holes list.
dsm_hole *g_shm_holes;

// Number of local processes.
static unsigned int g_lproc;

// Rank of local process.
static unsigned int g_lrank;

// The global identifier of the calling process.
static int g_gid = -1;

// Communication socket.
int g_sock_io = -1;

/* Saved signal-handlers (to be restored after).
 * 0 - SIGSEGV
 * 1 - SIGILL
 * 2 - SIGTSTP
*/
struct sigaction g_old_actions[3];


/*
 *******************************************************************************
 *                              Message Functions                              *
 *******************************************************************************
*/


// Sends DSM_MSG_ADD_PID to the arbiter.
static void send_add_pid (void) {
    dsm_msg msg = {.type = DSM_MSG_ADD_PID};
    msg.proc.pid = getpid();
	dsm_send_msg(g_sock_io, &msg);
}

// Sends DSM_MSG_HIT_BAR to the arbiter.
static void send_hit_bar (void) {
    dsm_msg msg = {.type = DSM_MSG_HIT_BAR};
    msg.proc.pid = getpid();
    dsm_send_msg(g_sock_io, &msg);
}

// Sends payload for DSM_MSG_POST_SEM and DSM_MSG_WAIT_SEM to arbiter.
static void send_sem_msg (dsm_msg_t type, const char *sem_name) {
    dsm_msg msg = {.type = type};
    msg.sem.pid = getpid();

    // Semaphore name is truncated if too long.
    snprintf(msg.sem.sem_name, DSM_MSG_STR_SIZE, "%s", sem_name);

    dsm_send_msg(g_sock_io, &msg);
}

// Sends all data for a newly filled hole.
static void send_hole_data (dsm_hole *hole) {
	dsm_msg msg;

	// Request write-authorization.
	msg.type = DSM_MSG_REQ_WRT;
	msg.proc.pid = getpid();
	dsm_send_msg(g_sock_io, &msg);

	// Receive write-authorization.
	dsm_recv_msg(g_sock_io, &msg);
	ASSERT_COND(msg.type == DSM_MSG_WRT_NOW && msg.proc.pid == getpid());

	// Send data.
	msg.type = DSM_MSG_WRT_DATA;
	msg.data.offset = hole->offset;
	msg.data.size = hole->size;
	msg.data.buf = (void *)((intptr_t)g_shared_map + (intptr_t)hole->offset);
	dsm_send_msg(g_sock_io, &msg);
	
	// Signal end of data stream.
	msg.type = DSM_MSG_WRT_END;
	dsm_send_msg(g_sock_io, &msg);
}

// Sends exit message to arbiter.
static void send_exit (void) {
    dsm_msg msg = {.type = DSM_MSG_EXIT};
    dsm_send_msg(g_sock_io, &msg);
}

// Receives DSM_POST_SEM from arbiter.
static void recv_post_sem (const char *sem_name) {
    dsm_msg msg;
    UNUSED(sem_name);

    // Receive message.
    dsm_recv_msg(g_sock_io, &msg);

    // Verify.
    ASSERT_COND(msg.type == DSM_MSG_POST_SEM && msg.sem.pid == getpid());
}

// Receives DSM_MSG_SET_GID from arbiter. Sets the process global identifier.
static int recv_set_gid (void) {
    dsm_msg msg;

    // Receive message.
    dsm_recv_msg(g_sock_io, &msg);

    // Verify.
    ASSERT_COND(msg.type == DSM_MSG_SET_GID && msg.proc.pid == getpid());

    // Return global identifier.
    return msg.proc.gid;
}


/*
 *******************************************************************************
 *                              Utility Functions                              *
 *******************************************************************************
*/


// Launches the arbiter and its cleanup daemon.
static void fork_arbiter (dsm_cfg *cfg) {
	int pid;
	char proc_buf[6] = {0}, size_buf[11] = {0};
	snprintf(proc_buf, 6, "%u", cfg->tproc);
	snprintf(size_buf, 11, "%zu", cfg->map_size);

	// Fork once and exit to orphan arbiter to init.
	if ((pid = dsm_fork()) == 0) {

		// Set as new session group leader to remove terminal.
		if (dsm_fork() == 0) {
			setsid();
			execlp("dsm_arbiter", "dsm_arbiter", proc_buf, cfg->sid_name,
				cfg->d_addr, cfg->d_port, size_buf, NULL);
			dsm_panic("Bad execlp for dsm_arbiter. Can it be found in PATH?");
		}

		// Exit the fork.
		exit(EXIT_SUCCESS);
	}

	waitpid(pid, NULL, 0);
}


/*
 *******************************************************************************
 *                            Function Definitions                             *
 *******************************************************************************
*/


/*
 * Forks local processes; initializes DSM. Returns shared memory pointer.
 * - cfg: Configuration structure. See dsm_arbiter.h.
*/
void *dsm_init2 (dsm_cfg *cfg) {
    int fd, first = 0, attempts = 0;

    // Verify: Initializer not already called.
    ASSERT_STATE(g_sock_io == -1 || g_shared_map == NULL);

	// Fork and exec arbiter.
	fork_arbiter(cfg);

	// Set local process count.
	g_lproc = cfg->lproc;

	// Perform local forks.
	for (unsigned int rank = 1; rank < g_lproc; rank++) {
		if (dsm_fork() == 0) {
			g_lrank = rank;
			break;
		}
	}

	// Try connecting to the arbiter.
	do {
		usleep(DSM_SOCK_POLL_RATE);
		g_sock_io = dsm_getConnectedSocket(DSM_LOOPBACK_ADDR, DSM_ARB_PORT);
		attempts++;
	} while (g_sock_io == -1 && attempts < DSM_MAX_SOCK_POLL);

	// Error out if couldn't connect.
	if (g_sock_io == -1) {
		dsm_panicf("Couldn't reach arbiter. Ensure port %s is free!", 
			DSM_ARB_PORT);
	}

	// Open the shared file.
	fd = dsm_getSharedFile(DSM_SHM_FILE_NAME, &first);

	// Assert process did not create it (only arbiter should).
	ASSERT_COND(first == 0);

	// Get the file size.
	g_map_size = dsm_getSharedFileSize(fd);

	// Map shared file to memory.
	g_shared_map = dsm_mapSharedFile(fd, g_map_size, PROT_READ|PROT_WRITE);

    // Send check-in message.
    send_add_pid();

    // Initialize decoder.
    dsm_sync_init();

    // Install signal handlers (save old ones).
    dsm_sigaction(SIGSEGV, dsm_sync_sigsegv, g_old_actions);
    dsm_sigaction(SIGILL, dsm_sync_sigill, g_old_actions + 1);

	// Restore default behavior for SIGTSTP during session.
	dsm_sigdefault(SIGTSTP, g_old_actions + 2);

    // Protect shared page.
    dsm_mprotect(g_shared_map, g_map_size, PROT_READ);

    // Block until start signal (set_gid) is received.
    g_gid = recv_set_gid();

    // Return shared map pointer.
    return g_shared_map;
}

/*
 * Forks local processes; initializes DSM. Returns shared memory pointer.
 * - sid:      Session identifier.
 * - lproc:    Number of local processes to fork.
 * - tproc:    Total expected process count.
 * - map_size: Desired map size. Rounded to page size multiple (minimum 1 page).
*/
void *dsm_init (const char *sid, unsigned int lproc, unsigned int tproc,
	size_t map_size) {

	// Default configuration.
	dsm_cfg cfg = {
		.lproc = lproc,
		.tproc = tproc,
		.sid_name = sid,
		.d_addr = "127.0.0.1",
		.d_port = "4200",
		.map_size = map_size
	};

	return dsm_init2(&cfg);
}

// Returns the global process identifier (GID) to the caller.
int dsm_get_gid (void) {
    return g_gid;
}

// Blocks process until all other processes are synchronized at the same point.
void dsm_barrier (void) {
    send_hit_bar();
    if (kill(getpid(), SIGTSTP) != 0) {
		dsm_panic("Couldn't block on barrier!");
	}
}

/*
 * Performs a post (up) on named semaphore. Target created if nonexistant.
 * - sem_name: Named semaphore identifier.
*/
void dsm_post_sem (const char *sem_name) {
    send_sem_msg(DSM_MSG_POST_SEM, sem_name);
}

/*
 * Performs a wait (down) on named semaphore. Target created if nonexistent.
 * - sem_name: Named semaphore identifier.
*/
void dsm_wait_sem (const char *sem_name) {
    send_sem_msg(DSM_MSG_WAIT_SEM, sem_name);
    recv_post_sem(sem_name);
}


/*
 * Creates a hole in the shared memory space. Returns a positive hole ID
 * on success, and faults on error. Memory within this space will not be
 * synchronized until the hole is filled. Any access that overlaps with a 
 * active (synchronized) memory space will result in the entire access being
 * synchronized.
 * - addr: The starting address of the hole (must be in shared memory map).
 * - size: The size (in bytes) of the hole. Must be > 0.
*/
int dsm_dig_hole (void *addr, size_t size) {
	intptr_t offset = (intptr_t)addr - (intptr_t)g_shared_map;
	intptr_t ubound = (intptr_t)g_map_size;
	intptr_t length = (intptr_t)size;
	int id;

	// Ensure size is reasonable.
	if (size == 0 || size > (size_t)g_map_size) {
		dsm_panicf("Bad hole size: (%zu == 0 || %zu > (%zu <- max))!",
			size, size, g_map_size);
	}

	// Ensure hole range in shared memory space.
	if (offset < 0 || (offset + length) > ubound) {
		dsm_panicf("Bad hole range: [%p->%p) not in [%p->%p)!",
			g_shared_map, (intptr_t)g_shared_map + (intptr_t)g_map_size, 
			addr, (intptr_t)addr + (intptr_t)size);
	}

	// Ensure hole doesn't overlap with another hole.
	if (dsm_overlaps_hole(offset, size, g_shm_holes) != 0) {
		dsm_panicf("Hole with range: [%p->%p) overlaps existing hole!",
			addr, (intptr_t)addr + (intptr_t)size);
	}

	// Otherwise dig a hole.
	if ((id = dsm_new_hole(offset, size, &g_shm_holes)) == -1) {
		dsm_panicf("(%s:%d) Couldn't create hole!\n", __FILE__, __LINE__);
	}

	// Return hole ID.
	return id;
}

/*
 * Fills a hole in the shared memory space. Panics on error.
 * Filling a hole results in the hole memory space being
 * synchronized. The hole then no longer exists.
 * - id: The hole ID returned from dsm_dig_hole.
*/
void dsm_fill_hole (int id) {
	dsm_hole *hole;

	// Ensure hole exists.
	if ((hole = dsm_get_hole(id, g_shm_holes)) == NULL) {
		dsm_panicf("Can't fill hole! No hole exists with ID %d!", id);
	}

	// Synchronize data across hole range.
	send_hole_data(hole);

	// Ensure hole was successfully removed.
	if (dsm_del_hole(id, &g_shm_holes) != 0) {
		dsm_panicf("Can't fill hole! Bad deletion of ID %d!", id);
	}
}

// Disconnects from DSM. Unmaps shared memory. Collects local process forks.
void dsm_exit (void) {

	// Restore original signal handlers.
    dsm_sigaction_restore(SIGSEGV, g_old_actions);
    dsm_sigaction_restore(SIGILL, g_old_actions + 1);
	dsm_sigaction_restore(SIGTSTP, g_old_actions + 2);

    // Verify: Initializer has been called.
    ASSERT_STATE(g_sock_io != -1 && g_shared_map != NULL);

    // Exit synchronization.
    dsm_barrier();

    // Send exit message.
    send_exit();

    // Close socket.
    close(g_sock_io);

    // Reset socket.
    g_sock_io = -1;

    // Unmap shared file.
    if (munmap(g_shared_map, g_map_size) == -1) {
        dsm_panic("Couldn't unmap shared file!");
    }

	// Free the shared memory holes.
	dsm_free_holes(g_shm_holes);

    // Reset shared map pointer.
    g_shared_map = NULL;

	// Collect zombies.
	if (g_lrank == 0) {
		while (--g_lproc) {
			waitpid(-1, NULL, 0);
		}
	} else {
		exit(EXIT_SUCCESS);
	}

}