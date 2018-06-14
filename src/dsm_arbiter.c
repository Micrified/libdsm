#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <semaphore.h>

#include "dsm_arbiter.h"
#include "dsm_util.h"
#include "dsm_msg.h"
#include "dsm_poll.h"
#include "dsm_inet.h"
#include "dsm_ptab.h"

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

// [EXTERN] Initialization semaphore. 
extern sem_t *g_sem_start;

// [EXTERN] Pointer to the shared (and protected) memory map.
extern void *g_shared_map;

// [EXTERN] Size of the shared map.
extern off_t g_map_size;



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

// Sends a message to all file-descriptors. Performs packing task.
static void send_all_msg (dsm_msg *mp) {
    unsigned char buf[DSM_MSG_SIZE];

    // Pack message.
    dsm_pack_msg(mp, buf);

    // Send to all. Skip listner socket at zero + server socket at one.
    for (unsigned int i = 2; i < g_pollSet->fp; i++) {
        dsm_sendall(g_pollSet->fds[i].fd, buf, DSM_MSG_SIZE);
    }
}

// Sends basic message without payload. 
static void send_easy_msg (int fd, dsm_msg_t type) {
    dsm_msg msg = {.type = type};
    send_msg(fd, &msg);
}

// Sends message for payload: dsm_payload_task. Fills out number of processes.
static void send_task_msg (int fd, dsm_msg_t type) {
    dsm_msg msg = {.type = type};
    msg.task.nproc = g_proc_tab->nproc;
    send_msg(fd, &msg);
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
    send_msg(fd, &msg);
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

// Unsets stopped bit on process. Signals if not blocked or queued.
static void map_cnt_all (int fd, dsm_proc *proc_p) {
    UNUSED(fd);

    // Unset stopped bit.
    proc_p->flags.is_stopped = 0;

    // If not blocked or queued, then signal.
    if (proc_p->flags.is_blocked == 0 && proc_p->flags.is_queued == 0) {
        signalProcess(proc_p->pid, SIGCONT);
    }
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

// DSM_MSG_STP_ALL: Stop all processes instruction.
static void handler_stp_all (int fd, dsm_msg *mp) {
    UNUSED(mp);

    // Verify state + sender.
    ASSERT_STATE(g_started == 1 && fd == g_sock_server);

    // For all processes: Set stopped bit. Signal if not yet stopped or blocked.
    dsm_mapFuncToProcessTableEntries(g_proc_tab, map_stp_all);

    // Dispatch DSM_MSG_ALL_STP. Provide number of processes stopped.
    send_task_msg(g_sock_server, DSM_MSG_ALL_STP);
}

// DSM_MSG_CNT_ALL: Resume all processes instruction.
static void handler_cnt_all (int fd, dsm_msg *mp) {
    UNUSED(mp);

    // Verify sender.
    ASSERT_COND(fd == g_sock_server);


    // Otherwise, set as started. Destroy shared files. send start message.
    if (g_started == 0) {
        g_started = 1;
        dsm_unlinkSharedFile(DSM_SHM_FILE_NAME);
        dsm_unlinkNamedSem(DSM_SEM_INIT_NAME);

        // Send each process it's GID to start it. 
        dsm_mapFuncToProcessTableEntries(g_proc_tab, map_gid_all);
        return;
    }

    // For all processes: Unset stopped bit. Signal if not blocked.
    dsm_mapFuncToProcessTableEntries(g_proc_tab, map_cnt_all);
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
    send_msg(proc_fd, mp);
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
    send_msg(g_sock_server, mp);
}

// DSM_MSG_REQ_WRT: Process requesting to write.
static void handler_req_wrt (int fd, dsm_msg *mp) {
    int pid = mp->proc.pid;
    dsm_proc *proc_p;

    // Verify state + sender.
    ASSERT_STATE(g_started == 1 && fd != g_sock_server);

    // Verify PID is registered.
    ASSERT_COND((proc_p = dsm_getProcessTableEntry(g_proc_tab, fd, pid)) != NULL);

    // Set process to queued.
    proc_p->flags.is_queued = 1;

    // Forward request to server.
    send_msg(g_sock_server, mp);
}

// DSM_MSG_HIT_BAR: Process is waiting on a barrier.
static void handler_hit_bar (int fd, dsm_msg *mp) {
    int pid = mp->proc.pid;
    dsm_proc *proc_p;

    // Verify state + sender.
    ASSERT_STATE(g_started == 1 && fd != g_sock_server);

    // Verify PID is registered.
    ASSERT_COND((proc_p = dsm_getProcessTableEntry(g_proc_tab, fd, pid)) != NULL);

    // Set process to blocked.
    proc_p->flags.is_blocked = 1;

    // Forward message to server.
    send_msg(g_sock_server, mp);
}

// DSM_MSG_WRT_DATA: Process data message.
static void handler_wrt_data (int fd, dsm_msg *mp) {

    // Verify state.
    ASSERT_STATE(g_started == 1);

    // If it's coming from a local process, forward to server.
    if (fd != g_sock_server) {
        send_msg(g_sock_server, mp);

    } else {

        // Otherwise synchronize the shared memory.
        dsm_mprotect(g_shared_map, g_map_size, PROT_WRITE);
        void *dest = (void *)((uintptr_t)g_shared_map + mp->data.offset);
        void *src = (void *)mp->data.bytes;
        memcpy(dest, src, 8);
        dsm_mprotect(g_shared_map, g_map_size, PROT_READ);
    }

    // Send synchronization message.
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
        send_msg(proc_fd, mp);

        return;
    }

    // Otherwise: Ensure process actually exists.
    ASSERT_COND((proc_p = dsm_getProcessTableEntry(g_proc_tab, fd, pid)) != NULL);

    // Foward POST to server.
    send_msg(g_sock_server, mp);
}

// DSM_MSG_WAIT_SEM: Process waiting on a named semaphore.
static void handler_wait_sem (int fd, dsm_msg *mp) {
    int pid = mp->sem.pid;
    dsm_proc *proc_p;

    // Verify state + sender.
    ASSERT_STATE(g_started == 1 && fd != g_sock_server);

    // Verify process exists.
    ASSERT_COND((proc_p = dsm_getProcessTableEntry(g_proc_tab, fd, pid)) != NULL);

    // Set blocked bit.
    proc_p->flags.is_blocked = 1;

    // Forward message to server.
    send_msg(g_sock_server, mp);
}

// DSM_MSG_EXIT: Process exiting.
static void handler_exit (int fd, dsm_msg *mp) {
    UNUSED(mp);

    // Verify state + sender.
    ASSERT_STATE(g_started == 1 && fd != g_sock_server);

    // Remove process from table.
    dsm_remProcessTableEntries(g_proc_tab, fd);

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
        dsm_panicf("Could not send signal: %d to process %d!\n", signal, pid);
    }
}

// Contacts daemon with sid, sets session details. Exits fatally on error.
static int getServerSocket (dsm_cfg *cfg) {
    char abuf[INET6_ADDRSTRLEN], pbuf[6];
    UNUSED(cfg);

    // Get server address.
    //printf("Server Address: "); scanf("%s", abuf);
    abuf[INET6_ADDRSTRLEN - 1] = '\0';

    snprintf(abuf, INET6_ADDRSTRLEN, "%s", cfg->d_addr);
    snprintf(pbuf, 6, "%s", cfg->d_port);

    // Get port.
    //printf("Server Port: "); scanf("%s", pbuf);
    //pbuf[5] = '\0';

    // Connect.
    return dsm_getConnectedSocket(abuf, pbuf);
}

// Handles a connection to listener socket.
static void handle_new_connection (int fd) {
    struct sockaddr_storage newAddr;
    socklen_t newAddrSize = sizeof(newAddr);
    int sock_new;

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
    unsigned char buf[DSM_MSG_SIZE];
    dsm_msg msg = {0};
    void (*handler)(int, dsm_msg *);

    // Read in data. Abort if sender disconnected (recvall returns nonzero).
    if (dsm_recvall(fd, buf, DSM_MSG_SIZE) != 0) {
        dsm_cpanic("handle_new_message", 
            "Participant lost connection. Unsafe state!");
    } else {
        dsm_unpack_msg(&msg, buf);
    }

    //printf("[%d] New Message!\n", getpid());
    //dsm_showMsg(&msg);

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
void dsm_arbiter (dsm_cfg *cfg) {
    int new = 0;                // Newly active connections (for poll syscall).
    struct pollfd *pfd = NULL;  // Pointer to a struct pollfd instance.

    // Register functions.
    dsm_setMsgFunc(DSM_MSG_STP_ALL, handler_stp_all, g_fmap);
    dsm_setMsgFunc(DSM_MSG_CNT_ALL, handler_cnt_all, g_fmap);
    dsm_setMsgFunc(DSM_MSG_REL_BAR, handler_rel_bar, g_fmap);
    dsm_setMsgFunc(DSM_MSG_WRT_NOW, handler_wrt_now, g_fmap);
    dsm_setMsgFunc(DSM_MSG_SET_GID, handler_set_gid, g_fmap);

    dsm_setMsgFunc(DSM_MSG_ADD_PID, handler_add_pid, g_fmap);
    dsm_setMsgFunc(DSM_MSG_REQ_WRT, handler_req_wrt, g_fmap);
    dsm_setMsgFunc(DSM_MSG_HIT_BAR, handler_hit_bar, g_fmap);
    dsm_setMsgFunc(DSM_MSG_WRT_DATA, handler_wrt_data, g_fmap);
    dsm_setMsgFunc(DSM_MSG_POST_SEM, handler_post_sem, g_fmap);
    dsm_setMsgFunc(DSM_MSG_WAIT_SEM, handler_wait_sem, g_fmap);
    dsm_setMsgFunc(DSM_MSG_EXIT, handler_exit, g_fmap);

    // Initialize pollable set.
    g_pollSet = dsm_initPollSet(DSM_MIN_POLLABLE);

    // Initialize process table.
    g_proc_tab = dsm_initProcessTable(DSM_PTAB_NFD);

    // Initialize server socket.
    g_sock_server = getServerSocket(cfg);

    // Initialize listener socket.
    g_sock_listen = dsm_getBoundSocket(AI_PASSIVE, AF_UNSPEC, SOCK_STREAM,
        DSM_ARB_PORT);

    // Listen on listener socket.
    if (listen(g_sock_listen, DSM_DEF_BACKLOG) == -1) {
        dsm_panic("Couldn't listen on listener socket!");
    }

    // Register listener socket as pollable at index zero.
    dsm_setPollable(g_sock_listen, POLLIN, g_pollSet);

    // Register server socket as pollable at index one.
    dsm_setPollable(g_sock_server, POLLIN, g_pollSet);

    // Release any waiting processes.
    for (unsigned int i = 0; i < cfg->nproc; i++) {
        dsm_up(g_sem_start);
    }

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

            //printf("State Update:\n");
            //dsm_showPollable(g_pollSet);
            //dsm_showProcessTable(g_proc_tab);
            //printf("\n\n");
        }
    }

    // ------------------------------------------------------------------------

    // Send exit message.
    send_easy_msg(g_sock_server, DSM_MSG_EXIT);

    // Close and remove server socket.
    dsm_removePollable(g_sock_server, g_pollSet);
    close(g_sock_server);

    // Close and remove listener socket.
    dsm_removePollable(g_sock_listen, g_pollSet);
    close(g_sock_listen);

    // Free the process table.
    dsm_freeProcessTable(g_proc_tab);

    // Free the pollable set.
    dsm_freePollSet(g_pollSet);

    // Exit.
    exit(EXIT_SUCCESS);
}