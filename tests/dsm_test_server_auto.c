#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "dsm_msg.h"
#include "dsm_inet.h"
#include "dsm_util.h"

// The socket connected to the server.
int sock;

// Receive a message and ensure it matches the expected message.
int recv_message (dsm_msg *mp) {
    unsigned char buf[DSM_MSG_SIZE];
    unsigned char expected[DSM_MSG_SIZE];
    dsm_msg msg;

    // Pack expected message into buf2.
    dsm_pack_msg(mp, expected);

    // Read in message.
    dsm_recvall(sock, buf, DSM_MSG_SIZE);

    dsm_unpack_msg(&msg, buf);
    dsm_showMsg(&msg);

    // Compare the message.
    for (int i = 0; i < DSM_MSG_SIZE; i++) {
        if (buf[i] != expected[i]) {
            return -1;
        }
    }

    return 0;
}

// Send a message to the server.
void send_message (dsm_msg *mp) {
    unsigned char buf[DSM_MSG_SIZE];
    dsm_pack_msg(mp, buf);
    dsm_sendall(sock, buf, DSM_MSG_SIZE);
}


int main (int argc, char *argv[]) {
    int nproc = 2;
    dsm_msg msg = {0};

    // Verify arguments.
    if (argc != 3) {
        fprintf(stderr, "usage: ./%s <address> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Connect to server.
    sock = dsm_getConnectedSocket(argv[1], argv[2]);
    printf("Connected...\n");


    // Register two processes. Verify the replies.
    for (int i = 0; i < nproc; i++) {

        // Sending message.
        memset(&msg, 0, sizeof(dsm_msg));
        msg.type = DSM_MSG_ADD_PID;
        msg.proc.pid = i;

        send_message(&msg);

        printf("%d check-in:\n", i); dsm_showMsg(&msg);

        // Receiving message.
        memset(&msg, 0, sizeof(dsm_msg));
        msg.type = DSM_MSG_SET_GID;
        msg.proc.pid = i;
        msg.proc.gid = i;

        // Verify.
        assert(recv_message(&msg) == 0);

        printf("%d check-in got:\n", i); dsm_showMsg(&msg);
    }

    // Verify start message received.
    memset(&msg, 0, sizeof(dsm_msg));
    msg.type = DSM_MSG_CNT_ALL; 
    assert(recv_message(&msg) == 0);
    printf("Start message received:\n"); dsm_showMsg(&msg);

    // Have both processes issue simultaneous write requests.
    for (int i = 0; i < nproc; i++) {
        memset(&msg, 0, sizeof(dsm_msg));
        msg.type = DSM_MSG_REQ_WRT;
        msg.proc.pid = i;
        send_message(&msg);
        printf("%d Sent write request!:\n", i); dsm_showMsg(&msg);
    }

    // Accept a stop response message.
    memset(&msg, 0, sizeof(dsm_msg));
    msg.type = DSM_MSG_STP_ALL;
    assert(recv_message(&msg) == 0);
    printf("STOP message received:\n"); dsm_showMsg(&msg);

    // Reply with an all stopped ack.
    memset(&msg, 0, sizeof(dsm_msg));
    msg.type = DSM_MSG_ALL_STP;
    msg.task.nproc = nproc;
    send_message(&msg);
    printf("STOP ack sent:\n"); dsm_showMsg(&msg);

    // For all processes: Dispatch a write request.
    for (int i = 0; i < nproc; i++) {

        // Accept a write go-ahead message.
        memset(&msg, 0, sizeof(dsm_msg));
        msg.type = DSM_MSG_WRT_NOW;
        msg.proc.pid = i;
        msg.proc.gid = 0;
        assert(recv_message(&msg) == 0);
        printf("%d: Accept write go-ahead!\n", i); dsm_showMsg(&msg);

        // Send write data.
        memset(&msg, 0, sizeof(dsm_msg));
        msg.type = DSM_MSG_WRT_DATA;
        msg.data.offset = 0;
        msg.data.size = 8;
        send_message(&msg);
        printf("%d: Sent write data!\n", i); dsm_showMsg(&msg);
        // Single arbtier: Skip (wrt data and confirm msg).
    }

    // Have all processes block on a semaphore.
    memset(&msg, 0, sizeof(dsm_msg));
    msg.type = DSM_MSG_WAIT_SEM;
    memcpy(msg.sem.sem_name, "x", 1);
    for (int i = 0; i < nproc; i++) {
        msg.sem.pid = i;
        send_message(&msg);
    }

    // Expect an unblock message for every post on x.

    for (int i = 0; i < nproc; i++) {
        memset(&msg, 0, sizeof(dsm_msg));
        msg.type = DSM_MSG_POST_SEM;
        memcpy(msg.sem.sem_name, "x", 1);
        msg.sem.pid = i;
        send_message(&msg);

        // Expect a reply: Hack: I know it sends ok in rev order.
        memset(&msg, 0, sizeof(dsm_msg));
        msg.type = DSM_MSG_POST_SEM;
        memcpy(msg.sem.sem_name, "x", 1);
        msg.sem.pid = nproc - i - 1;
        recv_message(&msg);
    }

    // Close down.
    memset(&msg, 0, sizeof(dsm_msg));
    msg.type = DSM_MSG_EXIT;
    send_message(&msg);
    printf("Sent EXIT message:\n"); dsm_showMsg(&msg);

    return 0;
}