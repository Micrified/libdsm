#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "dsm.h"
#include "dsm_msg.h"
#include "dsm_inet.h"
#include "dsm_util.h"
#include "dsm_signal.h"
#include "dsm_sync.h"


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

// The global identifier of the calling process.
static int g_gid = -1;

// Communication socket.
int g_sock_io;


/*
 *******************************************************************************
 *                          Message Wrapper Functions                          *
 *******************************************************************************
*/


// Sends a message to target file-descriptor. Performs packing task.
static void send_msg (int fd, dsm_msg *mp) {
    unsigned char buf[DSM_MSG_SIZE];

    // Pack message.
    dsm_pack_msg(mp, buf);

    // Send message.
    dsm_sendall(fd, buf, DSM_MSG_SIZE);
}

// Receives a message and configures the target message pointer.
static void recv_msg (int fd, dsm_msg *mp) {
    unsigned char buf[DSM_MSG_SIZE];

    // Receive message.
    if (dsm_recvall(fd, buf, DSM_MSG_SIZE) != 0) {
        dsm_cpanic("recv_msg", "Lost connection to host!");
    }

    // Unpack message.
    dsm_unpack_msg(mp, buf);
}


/*
 *******************************************************************************
 *                              Message Functions                              *
 *******************************************************************************
*/


// Sends DSM_MSG_ADD_PID to the arbiter.
static void send_add_pid (void) {
    dsm_msg msg = {.type = DSM_MSG_ADD_PID};
    msg.proc.pid = getpid();
    send_msg(g_sock_io, &msg);
}

// Sends DSM_MSG_HIT_BAR to the arbiter.
static void send_hit_bar (void) {
    dsm_msg msg = {.type = DSM_MSG_HIT_BAR};
    msg.proc.pid = getpid();
    send_msg(g_sock_io, &msg);
}

// Sends payload for DSM_MSG_POST_SEM and DSM_MSG_WAIT_SEM to arbiter.
static void send_sem_msg (dsm_msg_t type, const char *sem_name) {
    dsm_msg msg = {.type = type};
    msg.sem.pid = getpid();

    // Semaphore name is truncated if too long.
    snprintf(msg.sem.sem_name, DSM_MSG_STR_SIZE, "%s", sem_name);

    send_msg(g_sock_io, &msg);
}

// Sends exit message to arbiter.
static void send_exit (void) {
    dsm_msg msg = {.type = DSM_MSG_EXIT};
    send_msg(g_sock_io, &msg);
}

// Receives DSM_POST_SEM from arbiter.
static void recv_post_sem (const char *sem_name) {
    dsm_msg msg;
    UNUSED(sem_name);

    // Receive message.
    recv_msg(g_sock_io, &msg);

    // Verify.
    ASSERT_COND(msg.type == DSM_MSG_POST_SEM && msg.sem.pid == getpid());
}

// Receives DSM_MSG_SET_GID from arbiter. Sets the process global identifier.
static int recv_set_gid (void) {
    dsm_msg msg;

    // Receive message.
    recv_msg(g_sock_io, &msg);

    // Verify.
    ASSERT_COND(msg.type == DSM_MSG_SET_GID && msg.proc.pid == getpid());

    // Return global identifier.
    return msg.proc.gid;
}


/*
 *******************************************************************************
 *                            Function Definitions                             *
 *******************************************************************************
*/

// Sets new session ID, and forks the cleanup daemon and arbiter. Call as fork.
static void fork_services (dsm_cfg *cfg) {

	// Fork the daemon and arbiter.
	if (dsm_fork() == 0) {
		dsm_arbiter(cfg);
	}

	// Die.
	exit(EXIT_SUCCESS);
}


// Initializes the shared memory system. dsm_cfg is defined in dsm_arbiter.h.
// Returns a pointer to the shared map.
void *dsm_init (dsm_cfg *cfg) {
    int fd, pid, first, attempts = 0;

    // Verify: Initializer not already called.
    ASSERT_STATE(g_sock_io != -1 || g_shared_map != NULL);

    // Create or open shared file.
    fd = getSharedFile(DSM_SHM_FILE_NAME, &first);

    // Set or get file size. 
    if (first) {
        g_map_size = setSharedFileSize(fd, MAX(DSM_SHM_FILE_SIZE, 
            cfg->map_size));
    } else {
        g_map_size = getSharedFileSize(fd);
    }

    // Map shared file to memory.
    g_shared_map = mapSharedFile(fd, g_map_size, PROT_READ|PROT_WRITE);

	// If first: Fork cleanup daemon + arbiter as child.
	if (first) {
		if ((pid = dsm_fork()) == 0) {
			fork_services(cfg);
		}
		waitpid(pid, NULL, 0);
	}

	// Fork cleanup daemon + arbiter as child.
	// If I do this here, then a failing arbiter can't unmap while the 
	// Proper one tries to map. Some mutual exclusion mechanism needed.

	// Try connecting to the arbiter.
	do {
		usleep(DSM_SOCK_POLL_RATE);
		g_sock_io = dsm_getConnectedSocket(DSM_LOOPBACK_ADDR, DSM_ARB_PORT);
		attempts++;
	} while (g_sock_io == -1 && attempts < DSM_MAX_SOCK_POLL);

	// Error out if couldn't connect.
	if (g_sock_io == -1) {
		dsm_panic("Couldn't reach arbiter. Delete /dev/shm/dsm_file");
	} 

    // Send check-in message.
    send_add_pid();

    // Initialize decoder.
    dsm_sync_init();

    // Install signal handlers.
    dsm_sigaction(SIGSEGV, dsm_sync_sigsegv);
    dsm_sigaction(SIGILL, dsm_sync_sigill);

    // Protect shared page.
    dsm_mprotect(g_shared_map, g_map_size, PROT_READ);

    // Block until start signal (set_gid) is received.
    g_gid = recv_set_gid();
    //printf("[%d] I got gid: %d\n", getpid(), dsm_getgid());

    // Return shared map pointer.
    //printf("[%d] Starting...!\n", getpid());
    return g_shared_map;
}

// Initializes the shared memory system with default configuration.
// Returns a pointer to the shared map.
void *dsm_init2 (const char *sid, unsigned int nproc, size_t map_size) {

    // Default configuration.
    dsm_cfg cfg = {
        .nproc = nproc,
        .sid_name = sid,
        .d_addr = "127.0.0.1",
        .d_port = "4200",
        .map_size = map_size
    };

    return dsm_init(&cfg);
}

// Returns the global process identifier (GID) to the caller.
int dsm_getgid (void) {
    return g_gid;
}

// Blocks process until all other processes are synchronized at the same point.
void dsm_barrier (void) {
    send_hit_bar();
    kill(getpid(), SIGTSTP);
}

// Posts (up's) on the named semaphore. Semaphore is created if needed.
void dsm_post_sem (const char *sem_name) {
    send_sem_msg(DSM_MSG_POST_SEM, sem_name);
}

// Waits (down's) on the named semaphore. Semaphore is created if needed.
void dsm_wait_sem (const char *sem_name) {
    send_sem_msg(DSM_MSG_WAIT_SEM, sem_name);
    recv_post_sem(sem_name);
}

// Disconnects from shared memory system. Unmaps shared memory.
void dsm_exit (void) {

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

    // Reset shared map pointer.
    g_shared_map = NULL;

    // Reset signal handlers.
    dsm_sigdefault(SIGSEGV);
    dsm_sigdefault(SIGILL);
}