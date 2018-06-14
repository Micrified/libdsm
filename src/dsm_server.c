#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "dsm_inet.h"
#include "dsm_util.h"
#include "dsm_msg.h"
#include "dsm_poll.h"
#include "dsm_opqueue.h"
#include "dsm_ptab.h"
#include "dsm_stab.h"
#include "dsm_sem_htab.h"


/*
 *******************************************************************************
 *                             Symbolic Constants                              *
 *******************************************************************************
*/


// Default listener socket backlog
#define DSM_DEF_BACKLOG			32

// Minimum number of concurrent pollable connections.
#define DSM_MIN_POLLABLE		32

// Minimum number of queuable operation requests.
#define DSM_MIN_OPQUEUE_SIZE	32


/*
 *******************************************************************************
 *                              Global Variables                               *
 *******************************************************************************
*/


// Boolean flag indicating if program should continue polling.
int g_alive = 1;

// Boolean flag indicating if session has begun.
int g_started;

// The total number of expected processes.
unsigned int g_nproc;

// Pollable file-descriptor set.
pollset *g_pollSet;

// Function map.
dsm_msg_func g_fmap[DSM_MSG_MAX_VAL];

// Operation queue (current write state, who wants to write next, etc).
dsm_opqueue *g_opqueue;

// Process table.
dsm_ptab *g_proc_tab;

// String table.
dsm_stab *g_str_tab;

// Semaphore table.
dsm_sem_htab *g_sem_htab;

// The listener socket.
int g_sock_listen;


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

// Sends a message to all file-descriptors but except. Performs packing task.
static void send_all_msg (dsm_msg *mp, int except) {
    unsigned char buf[DSM_MSG_SIZE];

    // Pack message.
    dsm_pack_msg(mp, buf);

    // Send to all. Skip listener socket at index zero.
    for (int i = 1; i < (int)g_pollSet->fp; i++) {

        if (g_pollSet->fds[i].fd == except) {
            continue;
        }
        dsm_sendall(g_pollSet->fds[i].fd, buf, DSM_MSG_SIZE);
    }
}

// Sends basic message without payload. If fd == -1. Message is sent to all.
static void send_easy_msg (int fd, dsm_msg_t type) {
    dsm_msg msg = {.type = type};

    // If fd >= 0: Send to fd.
    if (fd >= 0) {
        send_msg(fd, &msg);
        return;
    }

    // Send to all.
    send_all_msg(&msg, -1);
}

// Informs the head of the queue it can write. Used twice so made it a function!
static void send_queue_wrt_now_msg (void) {
    dsm_msg msg = {.type = DSM_MSG_WRT_NOW};
    int fd, pid;

    // Get head of queue. Must exist.
    ASSERT_COND(dsm_isOpQueueEmpty(g_opqueue) == 0);
    
    // Get value. Extract file-descriptor and pid.
    uint64_t v = dsm_getOpQueueHead(g_opqueue);
    fd = DSM_MASK_FD(v);
    pid = DSM_MASK_PID(v);
    msg.proc.pid = pid;

    // Dispatch message.
    send_msg(fd, &msg);
}


/*
 *******************************************************************************
 *                          Message Handler Functions                          *
 *******************************************************************************
*/

// DSM_MSG_ALL_STP: Arbiter has stopped all local processes.
static void handler_all_stp (int fd, dsm_msg *mp) {
    int stopped = mp->task.nproc;
    UNUSED(fd);

    // Verify state.
    ASSERT_STATE(g_started == 1 && g_opqueue->step == STEP_WAITING_STOP_ACK);

    //printf("[%d] DSM_MSG_ALL_STP [%d remaining]\n", getpid(), g_nproc - (g_proc_tab->nstopped + stopped));

    // If all processes stopped. Inform writer and advance step.
    if ((g_proc_tab->nstopped += stopped) >= g_nproc) {

        // Inform head of queue it may write.
        send_queue_wrt_now_msg();

        // Set the next step.
        g_opqueue->step = STEP_WAITING_WRT_DATA;
        g_proc_tab->nstopped = 0;
    }
}

// DSM_MSG_GOT_DATA: Arbiter has updated all local processes.
static void handler_got_data (int fd, dsm_msg *mp) {
    int ready = mp->task.nproc;
    UNUSED(fd);

    // Verify state.
    ASSERT_STATE(g_started == 1 && g_opqueue->step == STEP_WAITING_SYNC_ACK);

    //printf("[%d] DSM_MSG_GOT_DATA [%d remaining]\n", getpid(), g_nproc - (g_proc_tab->nready + ready));

    // If all processes ready. Check queue for new operation.
    if ((g_proc_tab->nready += ready) >= g_nproc) {

        // Dequeue completed write-operation.
        dsm_dequeueOpQueue(g_opqueue);

        // Reset ready counter.
        g_proc_tab->nready = 0;

        // If new operation pending, jump to step 2 and inform writer.
        if (!dsm_isOpQueueEmpty(g_opqueue)) {
            //printf("[%d] Performing next queued operation!\n", getpid());
            g_opqueue->step = STEP_WAITING_WRT_DATA;
            send_queue_wrt_now_msg();
            return;
        }

        //printf("[%d] Informing all to continue!\n", getpid());
        send_easy_msg(-1, DSM_MSG_CNT_ALL);
        g_opqueue->step = STEP_READY;
    }
}

// DSM_MSG_ADD_PID: Process is checking in.
static void handler_add_pid (int fd, dsm_msg *mp) {
    int pid = mp->proc.pid;
    dsm_proc *proc_p;

    // Verify state.
    ASSERT_STATE(g_started == 0);

    // Register process in table.
    proc_p = dsm_setProcessTableEntry(g_proc_tab, fd, pid);

    //printf("[%d] DSM_MSG_ADD_PID: Registered %d\n", getpid(), pid);

    // Send response with process global identifier.
    mp->type = DSM_MSG_SET_GID;
    mp->proc.gid = proc_p->gid;
    send_msg(fd, mp);

    // If all processes ready. Start session.
    if ((g_proc_tab->nready += 1) >= g_nproc) {
        printf("[%d] Started.\n", getpid());

        // Set global started flag.
        g_started = 1;

        // Instruct all processes to begin.
        send_easy_msg(-1, DSM_MSG_CNT_ALL);

        // Reset the barrier.
        g_proc_tab->nready = 0;
    }
}

// DSM_MSG_REQ_WRT: Process wishes to write.
static void handler_req_wrt (int fd, dsm_msg *mp) {
    int isEmptyQueue = 0;
    int pid = mp->proc.pid;

    // Verify state.
    ASSERT_STATE(g_started == 1);

    // Verify PID and FD are valid.
    ASSERT_COND(fd >= 0 && pid >= 0);

    // Copy queue state.
    isEmptyQueue = dsm_isOpQueueEmpty(g_opqueue);

    // Queue request.
    dsm_enqueueOpQueue((uint32_t)fd, (uint32_t)pid, g_opqueue);

    //printf("[%d] DSM_MSG_REQ_WRT!\n", getpid());

    // Start new operation sequence if none other in progress.
    if (isEmptyQueue == 1) {

        // Verify step.
        ASSERT_COND(g_opqueue->step == STEP_READY);

        // Send stop signal and advance step.
        send_easy_msg(-1, DSM_MSG_STP_ALL);
        g_opqueue->step = STEP_WAITING_STOP_ACK;
    }
}

// DSM_MSG_HIT_BAR: Process is blocked on a barrier.
static void handler_hit_bar (int fd, dsm_msg *mp) {
    UNUSED(fd);
    UNUSED(mp);

    // Verify state.
    ASSERT_STATE(g_started == 1);

    //printf("[%d] HANDLER_HIT_BAR\n", getpid());

    // If all processes have reached barrier: Release and reset.
    if ((g_proc_tab->nblocked += 1) >= g_nproc) {
        printf("[%d] Releasing barrier!\n", getpid());

        // Inform blocked processes.
        send_easy_msg(-1, DSM_MSG_REL_BAR);

        // Reset barrier counter.
        g_proc_tab->nblocked = 0;
    }
}

// DSM_MSG_WRT_DATA: Process write information.
static void handler_wrt_data (int fd, dsm_msg *mp) {
    UNUSED(fd);

    // Verify state.
    ASSERT_STATE(g_started == 1 && g_opqueue->step == STEP_WAITING_WRT_DATA);

    // Verify sender is writer.
    ASSERT_COND(dsm_isOpQueueEmpty(g_opqueue) == 0 && fd > 0 &&
        DSM_MASK_FD(dsm_getOpQueueHead(g_opqueue)) == (uint32_t)fd);


    //printf("[%d] Sending data to all except sender!\n", getpid());

    // Otherwise: Forward data to all arbiters except the sender.
    send_all_msg(mp, fd);

    // Set new state step.
    g_opqueue->step = STEP_WAITING_SYNC_ACK;
}

// DSM_MSG_POST_SEM: Process is posting to a semaphore.
static void handler_post_sem (int fd, dsm_msg *mp) {
    char *sem_name = mp->sem.sem_name;
    dsm_sem_t *sem;
    dsm_proc *proc;
    int pfd;
    UNUSED(fd);

    // Verify State.
    ASSERT_STATE(g_started == 1);

    // If semaphore does not exist, create it.
    if ((sem = dsm_getHashTableEntry(g_sem_htab, sem_name)) == NULL) {
        sem = dsm_setSemHashTableEntry(g_sem_htab, sem_name);
        //printf("[%d] DSM_MSG_POST_SEM: Created \"%s\"\n", getpid(), sem_name);
    }

    // Get any process blocked on semaphore.
    proc = dsm_getProcessTableEntryWithSemID(g_proc_tab, sem->sem_id, &pfd);

    // If no process found: Increment value. Else: Unblock and reset process.
    if (proc == NULL) {
        //printf("[%d] DSM_MSG_POST_SEM: Incremented %s\n", getpid(), sem_name);
        sem->value += 1;
    } else {
        //printf("[%d] DSM_MSG_POST_SEM: Unblocked process %d waiting on %s!\n", getpid(), proc->pid, sem_name);
        proc->sem_id = -1;
        mp->sem.pid = proc->pid;
        send_msg(pfd, mp);
    }
}

// DSM_MSG_WAIT_SEM: Process is waiting on a semaphore.
static void handler_wait_sem (int fd, dsm_msg *mp) {
    char *sem_name = mp->sem.sem_name;
    dsm_sem_t *sem;
    dsm_proc *proc_p = dsm_getProcessTableEntry(g_proc_tab, fd, mp->sem.pid);

    // Verify state.
    ASSERT_STATE(g_started == 1);

    // Verify process exists in table.
    ASSERT_COND(proc_p != NULL);

    // If the semaphore does not exist, create it.
    if ((sem = dsm_getHashTableEntry(g_sem_htab, sem_name)) == NULL) {
        sem = dsm_setSemHashTableEntry(g_sem_htab, sem_name);
        //printf("[%d] DSM_MSG_WAIT_SEM: Created \"%s\"\n", getpid(), sem_name);
    }

    // If sem value > 0 : Send unblock and decrement. Else log sem under pid.
    if (sem->value > 0) {
        //printf("[%d] DSM_MSG_WAIT_SEM: Sent unblock right away (%d > 0)!\n", getpid(), sem->value);
        mp->type = DSM_MSG_POST_SEM; // Arbiter watches for POST_SEM only.
        send_msg(fd, mp);
        sem->value--;
    } else {
        //printf("[%d] DSM_MSG_WAIT_SEM: Registered %d as blocked under %s!\n", getpid(), proc_p->pid, sem_name);
        proc_p->sem_id = sem->sem_id;
    }
}

// DSM_MSG_EXIT: Process exit message.
static void handler_exit (int fd, dsm_msg *mp) {
    UNUSED(mp);

    // Verify state.
    ASSERT_STATE(g_started == 1);

    //printf("[%d] DSM_MSG_EXIT\n", getpid());

    // Close connection, remove from pollable set.
    dsm_removePollable(fd, g_pollSet);
    close(fd);

    // Remove process table entry.
    dsm_remProcessTableEntries(g_proc_tab, fd);

    // Destroy session if no active connections left.
    g_alive = (g_pollSet->fp > 1);
}


/*
 *******************************************************************************
 *                              Utility Functions                              *
 *******************************************************************************
*/


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
        dsm_cpanic("handle_new_msg", 
			"Participant lost connection. Unsafe state!");
    } else {
        dsm_unpack_msg(&msg, buf);
    }

    printf("[%d] New Message!\n", getpid());
    dsm_showMsg(&msg);

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
 *                                    Main                                     *
 *******************************************************************************
*/

int main (int argc, const char *argv[]) {
    int nproc = -1;             // Number of expected processes.
    int new = 0;                // Newly active connections (for poll syscall).
    struct pollfd *pfd = NULL;  // Pointer to a struct pollfd instance.

    // Verify arguments.
    if (argc != 3 || sscanf(argv[2], "%d", &nproc) != 1 || nproc < 2) {
        fprintf(stderr, "Usage: ./%s <port> <(nproc >= 2)>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // ------------------------------------------------------------------------

    // Set the expected process count.
    g_nproc = nproc;

    // Map message types to handlers.
    dsm_setMsgFunc(DSM_MSG_ALL_STP, handler_all_stp, g_fmap);
    dsm_setMsgFunc(DSM_MSG_GOT_DATA, handler_got_data, g_fmap);
    dsm_setMsgFunc(DSM_MSG_ADD_PID, handler_add_pid, g_fmap);
    dsm_setMsgFunc(DSM_MSG_REQ_WRT, handler_req_wrt, g_fmap);
    dsm_setMsgFunc(DSM_MSG_HIT_BAR, handler_hit_bar, g_fmap);
    dsm_setMsgFunc(DSM_MSG_WRT_DATA, handler_wrt_data, g_fmap);
    dsm_setMsgFunc(DSM_MSG_POST_SEM, handler_post_sem, g_fmap);
    dsm_setMsgFunc(DSM_MSG_WAIT_SEM, handler_wait_sem, g_fmap);
    dsm_setMsgFunc(DSM_MSG_EXIT, handler_exit, g_fmap);

    // Initialize pollable set.
    g_pollSet = dsm_initPollSet(DSM_MIN_POLLABLE);

    // Initialize operation queue.
    g_opqueue = dsm_initOpQueue(DSM_MIN_OPQUEUE_SIZE);

    // Initialize process table.
    g_proc_tab = dsm_initProcessTable(DSM_PTAB_NFD);

    // Initialize semaphore table.
    g_sem_htab = dsm_initSemHashTable();

    // Initialize string table.
    g_str_tab = dsm_initStringTable(DSM_STR_TAB_SIZE);

    // Initialize listener socket. Use any available port. [ARGV[1] for DEBUG]
    g_sock_listen = dsm_getBoundSocket(AI_PASSIVE, AF_UNSPEC, SOCK_STREAM, 
        argv[1]);

    // Register listener socket as pollable.
    dsm_setPollable(g_sock_listen, POLLIN, g_pollSet);

    // Listen on the socket.
    if (listen(g_sock_listen, DSM_DEF_BACKLOG) == -1) {
        dsm_panicf("Couldn't listen on socket (%d)!", g_sock_listen);
    }

    printf("[%d] Ready: ", getpid()); dsm_showSocketInfo(g_sock_listen);

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
        printf("[%d] State Update\n", getpid());
        dsm_showPollable(g_pollSet);
        dsm_showOpQueue(g_opqueue);
        dsm_showStringTable(g_str_tab);
        dsm_showProcessTable(g_proc_tab);
        printf("\n\n");
    }

    // ------------------------------------------------------------------------

    printf("[%d] Server Exiting...\n", getpid());

    // Unregister listener socket.
    dsm_removePollable(g_sock_listen, g_pollSet);

    // Close listener socket.
    close(g_sock_listen);

    // Free the process table.
    dsm_freeProcessTable(g_proc_tab);

    // Free semaphore table.
    dsm_freeHashTable(g_sem_htab);

    // Free string table.
    dsm_freeStringTable(g_str_tab);

    // Free the operation queue.
    dsm_freeOpQueue(g_opqueue);

    // Free pollable set.
    dsm_freePollSet(g_pollSet);

    return 0;
}