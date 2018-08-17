#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <semaphore.h>

#include "dsm_arbiter.h"
#include "dsm_util.h"
#include "dsm_msg.h"
#include "dsm_poll.h"
#include "dsm_inet.h"
#include "dsm_ptab.h"
#include "dsm_msg_io.h"


/*
 *******************************************************************************
 *                             Symbolic Constants                              *
 *******************************************************************************
*/


// Default listener socket backlog
#define DSM_DEF_BACKLOG			32

// Minimum number of concurrent pollable connections.
#define DSM_MIN_POLLABLE		32


/*
 *******************************************************************************
 *                              Global Variables                               *
 *******************************************************************************
*/


// Boolean flag indicating if program should continue polling.
int g_alive = 1;

// Boolean flag indicating if session has begun.
int g_started;

// [DEBUG] Global message counter.
static unsigned int g_msg_count;

// [DEBUG] Global timer.
static double g_seconds_elapsed;

// Pollable file-descriptor set.
pollset *g_pollSet;

// Function map.
dsm_msg_func g_fmap[DSM_MSG_MAX_VAL];

// Process table.
dsm_ptab *g_proc_tab;

// The listener socket.
int g_sock_listen;

// The server socket.
int g_sock_server;

// Pointer to the shared (and protected) memory map.
void *g_shared_map;

// Size of the shared map.
off_t g_map_size;

// Global configuration settings (set through program arguments).
dsm_cfg g_cfg;


/*
 *******************************************************************************
 *                          Message Wrapper Functions                          *
 *******************************************************************************
*/


// Sends basic message without payload. 
static void send_easy_msg (int fd, dsm_msg_t type) {
    dsm_msg msg = {.type = type};
    dsm_send_msg(fd, &msg);
}

// Sends message for payload: dsm_payload_task. Fills out number of processes.
static void send_task_msg (int fd, dsm_msg_t type) {
    dsm_msg msg = {.type = type};
    msg.task.nproc = g_proc_tab->nproc;
    dsm_send_msg(fd, &msg);
}

/*
 *******************************************************************************
 *                        Mapping Function Definitions                         *
 *******************************************************************************
*/


// Forward declaration of signalProcess.
static void signalProcess (int pid, int signal);

// Sends the process it's GID.
static void map_gid_all (int fd, dsm_proc *proc_p) {
    dsm_msg msg = {.type = DSM_MSG_SET_GID};

    // Unset stopped bit (set at start).
    proc_p->flags.is_stopped = 0;

    msg.proc.pid = proc_p->pid;
    msg.proc.gid = proc_p->gid;
    dsm_send_msg(fd, &msg);
}

// Sets stopped bit on process. Signals if not blocked, stopped, or queued.
static void map_stp_all (int fd, dsm_proc *proc_p) {
    UNUSED(fd);

    // If not stopped, blocked, or queued, then signal.
    if (proc_p->flags.is_stopped == 0 && proc_p->flags.is_blocked == 0
        && proc_p->flags.is_queued == 0) {
        signalProcess(proc_p->pid, SIGTSTP);
    }

    // Set stopped bit.
    proc_p->flags.is_stopped = 1;
}

// Unsets blocked bit on process. Sends release message.
static void map_rel_bar (int fd, dsm_proc *proc_p) {
    UNUSED(fd);
    
    // Unset blocked bit.
    proc_p->flags.is_blocked = 0;

    // No process should be stopped or queued here. A barrier is a barrier.
    ASSERT_COND(proc_p->flags.is_stopped == 0 && proc_p->flags.is_queued == 0);

    // Allow process to continue.
    signalProcess(proc_p->pid, SIGCONT);
}


/*
 *******************************************************************************
 *                          Message Handler Functions                          *
 *******************************************************************************
*/


// DSM_MSG_CNT_ALL: Resume all processes instruction.
static void handler_cnt_all (int fd, dsm_msg *mp) {
    UNUSED(mp);

    // Verify sender.
    ASSERT_COND(fd == g_sock_server && g_started == 0);

	// Set the started flag.
    g_started = 1;

	// Destroy the shared file.
	dsm_unlinkSharedFile(DSM_SHM_FILE_NAME);

    // Start global timer.
    g_seconds_elapsed = dsm_getWallTime();

    // Send each process it's GID to start it. 
    dsm_mapFuncToProcessTableEntries(g_proc_tab, map_gid_all);
}

// DSM_MSG_REL_BAR: Release processes waiting at barrier.
static void handler_rel_bar (int fd, dsm_msg *mp) {
    UNUSED(mp);

    // Verify state + sender.
    ASSERT_STATE(g_started == 1 && fd == g_sock_server);

    // For all processes: Unset blocked bit. Signal if not stopped.
    dsm_mapFuncToProcessTableEntries(g_proc_tab, map_rel_bar);
}

// DSM_MSG_WRT_NOW: Perform write-operation.
static void handler_wrt_now (int fd, dsm_msg *mp) {
    int proc_fd, pid = mp->proc.pid;
    dsm_proc *proc_p;

    // Verify state + sender.
    ASSERT_STATE(g_started == 1 && fd == g_sock_server);

    // Locate writing process.
    ASSERT_COND((proc_p = dsm_findProcessTableEntry(g_proc_tab, pid, &proc_fd))
        != NULL);

    // Unset as queued. This process has write go-ahead.
    proc_p->flags.is_queued = 0;

    // Forward message to writer.
    dsm_send_msg(proc_fd, mp);
}

// DSM_MSG_SET_GID: Set a process global-identifier.
static void handler_set_gid (int fd, dsm_msg *mp) {
    int pid = mp->proc.pid, gid = mp->proc.gid, proc_fd;
    dsm_proc *proc_p;

    // Verify state and sender.
    ASSERT_STATE(g_started == 0 && fd == g_sock_server);

    // Verify PID exists.
    ASSERT_COND((proc_p = 
        dsm_findProcessTableEntry(g_proc_tab, pid, &proc_fd)) != NULL);

    // Update GID for PID in table.
    proc_p->gid = gid;
}

// DSM_MSG_ADD_PID: Process checking in. 
static void handler_add_pid (int fd, dsm_msg *mp) {
    int pid = mp->proc.pid;
    dsm_proc *proc_p;

    // Verify state and sender.
    ASSERT_STATE(g_started == 0 && fd != g_sock_server);

    // Verify PID does not already exist.
    ASSERT_COND(dsm_findProcessTableEntry(g_proc_tab, pid, NULL) == NULL);

    // Register PID in table.
    proc_p = dsm_setProcessTableEntry(g_proc_tab, fd, pid);

    // Overwrite GID (default behavior is to set it, but we're not server).
    proc_p->gid = -1;

    // Set process as stopped by default (until start signal is received).
    proc_p->flags.is_stopped = 1;

    // Forward message to server.
    dsm_send_msg(g_sock_server, mp);
}

// DSM_MSG_REQ_WRT: Process requesting to write.
static void handler_req_wrt (int fd, dsm_msg *mp) {
    int pid = mp->proc.pid;
    dsm_proc *proc_p;

    // Verify state + sender.
    ASSERT_STATE(g_started == 1 && fd != g_sock_server);

    // Verify PID is registered.
    ASSERT_COND((proc_p = dsm_getProcessTableEntry(g_proc_tab, fd, pid))
		!= NULL);

    // Set process to queued.
    proc_p->flags.is_queued = 1;

    // Forward request to server.
    dsm_send_msg(g_sock_server, mp);
}

// DSM_MSG_HIT_BAR: Process is waiting on a barrier.
static void handler_hit_bar (int fd, dsm_msg *mp) {
    int pid = mp->proc.pid;
    dsm_proc *proc_p;

    // Verify state + sender.
    ASSERT_STATE(g_started == 1 && fd != g_sock_server);

    // Verify PID is registered.
    ASSERT_COND((proc_p = dsm_getProcessTableEntry(g_proc_tab, fd, pid)) 
        != NULL);

    // Set process to blocked.
    proc_p->flags.is_blocked = 1;

    // Forward message to server.
    dsm_send_msg(g_sock_server, mp);
}

// DSM_MSG_WRT_DATA: Process data message.
static void handler_wrt_data (int fd, dsm_msg *mp) {

    // Verify state.
    ASSERT_STATE(g_started == 1);

    // If it's coming from a local process, forward to server.
    if (fd != g_sock_server) {
        dsm_send_msg(g_sock_server, mp);

    } else {
		
		// Map_end is used to ensure nothing is written off shared map.
		size_t len = 0, map_end = (size_t)g_shared_map + (size_t)g_map_size;

        // Otherwise synchronize the shared memory.
        dsm_mprotect(g_shared_map, g_map_size, PROT_WRITE);
        void *dest = (void *)((intptr_t)g_shared_map + mp->data.offset);
        void *src = (void *)mp->data.buf;
		len = MIN(MAX(len, map_end - (size_t)dest), 8);
        memcpy(dest, src, len);
        dsm_mprotect(g_shared_map, g_map_size, PROT_READ);
    }

}

// DSM_MSG_WRT_END: End of data transmission.
static void handler_wrt_end (int fd, dsm_msg *mp) {
	UNUSED(mp);

	// Verify state.
	ASSERT_STATE(g_started == 1);

	// If internal: Forward to server.
	if (fd != g_sock_server) {
		dsm_send_msg(g_sock_server, mp);
	}

	// Always send acknowledgment to server.
	send_task_msg(g_sock_server, DSM_MSG_GOT_DATA);
}

// DSM_MSG_POST_SEM: Process posted to a named semaphore.
static void handler_post_sem (int fd, dsm_msg *mp) {
    int proc_fd, pid = mp->sem.pid;
    dsm_proc *proc_p;

    // Verify state.
    ASSERT_STATE(g_started == 1);

    // If from server: Find and unblock process.
    if (fd == g_sock_server) {
        ASSERT_COND((proc_p = dsm_findProcessTableEntry(g_proc_tab, pid,
            &proc_fd)));

        // Ensure process was blocked: Could verify semaphore name but costly.
        ASSERT_COND(proc_p->flags.is_blocked == 1);
        
        // Unset blocked bit.
        proc_p->flags.is_blocked = 0;

        // Forward message to process.
        dsm_send_msg(proc_fd, mp);

        return;
    }

    // Otherwise: Ensure process actually exists.
    ASSERT_COND((proc_p = dsm_getProcessTableEntry(g_proc_tab, fd, pid)) 
        != NULL);

    // Foward POST to server.
    dsm_send_msg(g_sock_server, mp);
}

// DSM_MSG_WAIT_SEM: Process waiting on a named semaphore.
static void handler_wait_sem (int fd, dsm_msg *mp) {
    int pid = mp->sem.pid;
    dsm_proc *proc_p;

    // Verify state + sender.
    ASSERT_STATE(g_started == 1 && fd != g_sock_server);

    // Verify process exists.
    ASSERT_COND((proc_p = dsm_getProcessTableEntry(g_proc_tab, fd, pid)) 
        != NULL);

    // Set blocked bit.
    proc_p->flags.is_blocked = 1;

    // Forward message to server.
    dsm_send_msg(g_sock_server, mp);
}

// DSM_MSG_EXIT: Process exiting.
static void handler_exit (int fd, dsm_msg *mp) {
    UNUSED(mp);

    // Verify state + sender.
    ASSERT_STATE(g_started == 1 && fd != g_sock_server);

    // Close socket.
    close(fd);

    // Remove from pollable set.
    dsm_removePollable(fd, g_pollSet);

    // If no more connections remain, then stop polling.
    if (g_pollSet->fp <= 2) {
        g_alive = 0;
    }
}


/*
 *******************************************************************************
 *                              Utility Functions                              *
 *******************************************************************************
*/


// Sends signal to 'pid'. 
static void signalProcess (int pid, int signal) {
    if (kill(pid, signal) == -1) {
        dsm_panicf("(%s:%d) Could not send signal: \"%s\" to process %d!\n", 
			__FILE__, __LINE__, strsignal(signal), pid);
    }
}

// Contacts daemon with sid, sets session details. Exits fatally on error.
static int getServerSocket (dsm_cfg *cfg) {
	dsm_msg msg;
    int sock;

	// Attempt to connect to the daemon.
	if ((sock = dsm_getConnectedSocket(cfg->d_addr, cfg->d_port)) == -1) {
		dsm_panicf("Couldn't reach session daemon (%s:%s)", cfg->d_addr, 
			cfg->d_port);
	}

	// Configure session request.
	msg.type = DSM_MSG_GET_SID;
	snprintf(msg.sid.sid_name, DSM_MSG_STR_SIZE, "%s", cfg->sid_name);
	msg.sid.nproc = cfg->tproc;
	
	// Dispatch session request.
	dsm_send_msg(sock, &msg);

	// Read back response.
	dsm_recv_msg(sock, &msg);

	// Close connection.
	close(sock);

	// Connect to the session-server.
	if ((sock = dsm_getConnectedSocket(cfg->d_addr, 
		dsm_portToString(msg.sid.port))) == -1) {
		dsm_panicf("Couldn't reach session server (%s:%s)", cfg->d_addr,
			dsm_portToString(msg.sid.port));
	}

	return sock;
}

// Handles a connection to listener socket.
static void handle_new_connection (int fd) {
    struct sockaddr_storage newAddr;
    socklen_t newAddrSize = sizeof(newAddr);
    int sock_new;

	// If the session already started. Drop connection and issue warning.
	if (g_started == 1) {
		dsm_cpanic("Connection attempt after session start!",
			"Likely from running sessions in quick succession");
	}

    // Panic if can't accept connection.
    if ((sock_new = accept(fd, (struct sockaddr *)&newAddr, &newAddrSize)) 
        == -1) {
        dsm_panic("handle_new_connection: Couldn't accept new connection!");
    }

    // Add to pollable set.
    dsm_setPollable(sock_new, POLLIN, g_pollSet);
}


// Handles a message from a pollable socket.
static void handle_new_message (int fd) {
    dsm_msg msg = {0};
    void (*handler)(int, dsm_msg *);

	// Read in data.
	dsm_recv_msg(fd, &msg);

	// Increment the message count.
	g_msg_count++;

    // Get handler. Abort if none set.
    if ((handler = dsm_getMsgFunc(msg.type, g_fmap)) == NULL) {
        dsm_warning("Unknown message received!");
        dsm_removePollable(fd, g_pollSet);
        close(fd);
    } else {
        handler(fd, &msg);
    }
}


/*
 *******************************************************************************
 *                                   Arbiter                                   *
 *******************************************************************************
*/


// Runs the arbiter main loop. Exits on success.
int main (int argc, const char *argv[]) {
	int fd;						// File-descriptor for shared memory map.
    int new = 0;                // Newly active connections (for poll syscall).
    struct pollfd *pfd = NULL;  // Pointer to a struct pollfd instance.

	// Parse program arguments.
	if (argc != 6 || sscanf(argv[1], "%u", &g_cfg.tproc) != 1 || 
		sscanf(argv[5], "%zu", &g_cfg.map_size) != 1) {
		dsm_cpanic("Usage: ./dsm_arbiter <nproc> <sid_name> <d_addr> "\
			"<d_port> <map_size>", "Bad arguments!");
	} else {
		g_cfg.sid_name = argv[2];
		g_cfg.d_addr = argv[3];
		g_cfg.d_port = argv[4];
	}

	// Initialize listener socket: If already occupied, yield.
    if ((g_sock_listen = dsm_getBoundSocket(AI_PASSIVE, AF_UNSPEC, SOCK_STREAM,
        DSM_ARB_PORT)) == -1 || listen(g_sock_listen, DSM_DEF_BACKLOG) == -1) {
		exit(EXIT_SUCCESS);
	}

	// Redirect to Xterm.
	dsm_redirXterm();

	// Create or open the shared file.
	fd = dsm_getSharedFile(DSM_SHM_FILE_NAME, NULL);

	// Set the file size.
	g_map_size = dsm_setSharedFileSize(fd, MAX(DSM_SHM_FILE_SIZE, 
		g_cfg.map_size));

	// Map shared file to memory.
	g_shared_map = dsm_mapSharedFile(fd, g_map_size, PROT_READ|PROT_WRITE);


    // Register functions.
    dsm_setMsgFunc(DSM_MSG_CNT_ALL, handler_cnt_all, g_fmap);
    dsm_setMsgFunc(DSM_MSG_REL_BAR, handler_rel_bar, g_fmap);
    dsm_setMsgFunc(DSM_MSG_WRT_NOW, handler_wrt_now, g_fmap);
    dsm_setMsgFunc(DSM_MSG_SET_GID, handler_set_gid, g_fmap);
    dsm_setMsgFunc(DSM_MSG_ADD_PID, handler_add_pid, g_fmap);
    dsm_setMsgFunc(DSM_MSG_REQ_WRT, handler_req_wrt, g_fmap);
    dsm_setMsgFunc(DSM_MSG_HIT_BAR, handler_hit_bar, g_fmap);
    dsm_setMsgFunc(DSM_MSG_WRT_DATA, handler_wrt_data, g_fmap);
	dsm_setMsgFunc(DSM_MSG_WRT_END, handler_wrt_end, g_fmap);
    dsm_setMsgFunc(DSM_MSG_POST_SEM, handler_post_sem, g_fmap);
    dsm_setMsgFunc(DSM_MSG_WAIT_SEM, handler_wait_sem, g_fmap);
    dsm_setMsgFunc(DSM_MSG_EXIT, handler_exit, g_fmap);

    // Initialize pollable set.
    g_pollSet = dsm_initPollSet(DSM_MIN_POLLABLE);

    // Initialize process table.
    g_proc_tab = dsm_initProcessTable(DSM_PTAB_NFD);

    // Initialize server socket.
    g_sock_server = getServerSocket(&g_cfg);

    // Register listener socket as pollable at index zero.
    dsm_setPollable(g_sock_listen, POLLIN, g_pollSet);

    // Register server socket as pollable at index one.
    dsm_setPollable(g_sock_server, POLLIN, g_pollSet);


    // ------------------------------------------------------------------------

    // Keep polling as long as no errors occur, or alive flag not false.
    while (g_alive && (new = poll(g_pollSet->fds, g_pollSet->fp, -1)) != -1) {
        for (unsigned int i = 0; i < g_pollSet->fp; i++) {
            pfd = g_pollSet->fds + i;

            // Skip file-descriptors without input.
            if ((pfd->revents & POLLIN) == 0) continue;

            // Accept connections on listener socket. Otherwise process as msg.
            if (pfd->fd == g_sock_listen) {
                handle_new_connection(g_sock_listen);
            } else {
                handle_new_message(pfd->fd);
            }
        }

        printf("\rExchanged Messages: %u", g_msg_count); fflush(stdout);
    }

    // ------------------------------------------------------------------------

    // Send exit message.
    send_easy_msg(g_sock_server, DSM_MSG_EXIT);

	// Close and remove listener socket.
    close(g_sock_listen);
    dsm_removePollable(g_sock_listen, g_pollSet);

    // Close and remove server socket.
	close(g_sock_server);
    dsm_removePollable(g_sock_server, g_pollSet);

    // Free the process table.
    dsm_freeProcessTable(g_proc_tab);

    // Free the pollable set.
    dsm_freePollSet(g_pollSet);

    // Compute elapsed time, display momentarily.
    g_seconds_elapsed = dsm_getWallTime() - g_seconds_elapsed;
    printf("\nElapsed Seconds: %f\n", g_seconds_elapsed);
    fflush(stdout);
	close(1);
	
	// End.
    return EXIT_SUCCESS;
}
