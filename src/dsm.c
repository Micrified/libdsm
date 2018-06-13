#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <semaphore.h>

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


// State-assertion macro.
#define ASSERT_STATE(E)         ((E) ? (void)0 : \
    (dsm_panicf("State Violation (%s:%d): %s", __FILE__, __LINE__, #E)))

// Condition-assertion macro.
#define ASSERT_COND(E)         ((E) ? (void)0 : \
    (dsm_panicf("Condition Unmet (%s:%d): %s", __FILE__, __LINE__, #E)))

// The name of the shared initialization semaphore.
#define DSM_SEM_INIT_NAME			"dsm_start"


/*
 *******************************************************************************
 *                              Global Variables                               *
 *******************************************************************************
*/


// Initialization semaphore. Used to synchronize startup between processes.
sem_t *g_sem_start;

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
 *                        Private Function Definitions                         *
 *******************************************************************************
*/


// Opens or creates semaphore with initial value 'val'. Returns semaphore ptr.
static sem_t *getSem (const char *name, unsigned int val) {
	sem_t *sp = NULL;

	// Try creating exclusive semaphore. Defer EEXIST error.
	if ((sp = sem_open(name, O_CREAT|O_EXCL|O_RDWR, S_IRUSR|S_IWUSR, val))
		== SEM_FAILED && errno != EEXIST) {
		dsm_panicf("Couldn't create named-semaphore: \"%s\"!", name);
	}

	// Try opening existing semaphore. Panic on error.
	if (sp == SEM_FAILED && (sp = sem_open(name, O_RDWR)) == SEM_FAILED) {
		dsm_panicf("Couldn't open named-semaphore: \"%s\"!", name);
	}

	return sp;
}

// Creates or opens a shared file. Sets owner flag, returns file-descriptor.
static int getSharedFile (const char *name, int *is_owner) {
	int fd, mode = S_IRUSR|S_IWUSR, owner = 0;

	// Try creating exclusive file. Defer EEXIST error.
	if ((fd = shm_open(name, O_CREAT|O_EXCL|O_RDWR, mode)) == -1 &&
		errno != EEXIST) {
		dsm_panicf("Couldn't created shared file \"%s\"!", name);
	}

	// Set owner flag.
	owner = (fd != -1);

	// Try opening existing file. Panic on error.
	if (!owner && (fd = shm_open(name, O_RDWR, mode)) == -1) {
		dsm_panicf("Couldn't open shared file \"%s\"!", name);
	}

	// Set owner pointer.
	if (is_owner != NULL) {
		*is_owner = owner;	
	}

	return fd;
}

// Sets size of shared file. Returns given size on success. Panics on error.
static off_t setSharedFileSize (int fd, off_t size) {

    // Round size up to next multiple of a page.
    if (size % DSM_PAGESIZE != 0) {
        size += (size % DSM_PAGESIZE);
    }

	if (ftruncate(fd, size) == -1) {
		dsm_panicf("Couldn't resize shared file (fd = %d)!", fd);
	}
	return size;
}

// Returns the size of a shared file. Panics on error.
static off_t getSharedFileSize (int fd) {
	struct stat sb;

	if (fstat(fd, &sb) == -1) {
		dsm_panicf("Couldn't get size of shared file (fd = %d)!", fd);
	}

	return sb.st_size;
}

// Maps shared file of given size to memory with protections. Panics on error.
static void *mapSharedFile (int fd, size_t size, int prot) {
	void *map;

	// Let operating system select starting address: PAGE ALIGNED
	if ((map = mmap(NULL, size, prot, MAP_SHARED, fd, 0)) == MAP_FAILED) {
		dsm_panicf("Couldn't map shared file to memory (fd = %d)!", fd);
	}

	// Zero the memory.
	memset(map, 0, size);

	return map;
}


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


// Initializes the shared memory system. dsm_cfg is defined in dsm_arbiter.h.
// Returns a pointer to the shared map.
void *dsm_init (dsm_cfg *cfg) {
    int fd, first;

    // Verify: Initializer not already called.
    ASSERT_STATE(g_sock_io != -1 || g_shared_map != NULL);

    // Create or open init-semaphore.
    g_sem_start = getSem(DSM_SEM_INIT_NAME, 0);

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

    // If first: Fork arbiter.
    if (first && fork() == 0) {
        printf("[%d] Arbiter launching...\n", getpid());
        dsm_arbiter(cfg);
    }
    dsm_redirXterm();
    printf("[%d] Waiting to begin...\n", getpid());

    // Wait on initialization semaphore for release by arbiter.
    dsm_down(g_sem_start);

    // Connect to arbiter.
    g_sock_io = dsm_getConnectedSocket(DSM_LOOPBACK_ADDR, DSM_ARB_PORT);

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
    printf("[%d] I got gid: %d\n", getpid(), dsm_getgid());

    // Return shared map pointer.
    printf("[%d] Starting...!\n", getpid());
    return g_shared_map;
}

// Initializes the shared memory system with default configuration.
// Returns a pointer to the shared map.
void *dsm_init2 (unsigned int nproc, size_t map_size) {

    // Default configuration.
    dsm_cfg cfg = {
        .nproc = nproc,
        .sid = "Tulip",
        .d_addr = "127.0.0.1",
        .d_port = "4800",
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
void dsm_post (const char *sem_name) {
    send_sem_msg(DSM_MSG_POST_SEM, sem_name);
}

// Waits (down's) on the named semaphore. Semaphore is created if needed.
void dsm_wait (const char *sem_name) {
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

int main (void) {

    // Fork.
    fork(); printf("[%d] Starting...\n", getpid());

    // Get the shared map.
    void *map = dsm_init2(2, DSM_PAGESIZE * 2);
    printf("[%d] map: %p -> %p\n", getpid(), map, (void *)((uintptr_t)map + DSM_PAGESIZE * 2));
    // Cast shared integer.
    int *turn = (int *)map;

    // Ping pong.
    for (int i = 0; i < 5; i++) {
        while (*turn != dsm_getgid());
        if (dsm_getgid() == 0) {
            printf("Ping! ...\n");
        } else {
            printf("... Pong!\n");
        }
        sleep(1);
        *turn = 1 - *turn;
    }

    // Call de-initializer.
    dsm_exit();


    return 0;
}
