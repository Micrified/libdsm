#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <inttypes.h>
#include <sys/wait.h>

#include "dsm_msg.h"
#include "dsm_inet.h"
#include "dsm_util.h"

// Communication socket.
int sock;

// Session names to use.
const char *session_names[5] = {"Ajax", "Bjorn", "Calypso", "Demeter", "Earl"};

// Receives a message. Compares against expected message. Returns zero if matching.
int recv_message (dsm_msg *mp, dsm_msg *cp) {
    int different = 0;
    dsm_msg msg;
    unsigned char buf[DSM_MSG_SIZE], exp[DSM_MSG_SIZE];

    // Pack expected message to exp.
    dsm_pack_msg(mp, exp);

    // Read incoming message.
    dsm_recvall(sock, buf, DSM_MSG_SIZE);

    // Unpack and output message.
    dsm_unpack_msg(&msg, buf);
    dsm_showMsg(&msg);

    // Compare the message.
    for (int i = 0; i < DSM_MSG_SIZE; i++) {
        if (buf[i] != exp[i]) {
            different = 1;
            break;
        }
    }

    // Copy result to cp if set.
    if (cp != NULL) {
        memcpy(cp, &msg, sizeof(msg));
    }

    return different;
}

// Send a message to the server.
void send_message (dsm_msg *mp) {
    unsigned char buf[DSM_MSG_SIZE];
    dsm_pack_msg(mp, buf);
    dsm_sendall(sock, buf, DSM_MSG_SIZE);
}

int main (int argc, char *argv[]) {
    int rank = 0, nproc = 15;
    dsm_msg msg = {0}, res = {0};
    char buf[32];

    // Verify arguments.
    if (argc != 3) {
        fprintf(stderr, "usgae: ./%s <address> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Perform five forks, leaving three processes per session.
    for (int i = 1; i < nproc; i++) {
        if (fork() == 0) {
            rank = i;
            break;
        }
    }

    // Session is rank % 5.
    const char *sid_name = session_names[rank % 5];

    // Connect to the server.
    sock = dsm_getConnectedSocket(argv[1], argv[2]);

    // Send: DSM_GET_SID.
    msg.type = DSM_MSG_GET_SID;
    snprintf(msg.sid.sid_name, DSM_MSG_STR_SIZE, "%s", sid_name);
    msg.sid.nproc = 3;
    send_message(&msg);

    // Verify: Reply is DSM_SET_SID for sid_name.
    recv_message(&msg, &res);
    ASSERT_COND(res.type == DSM_MSG_SET_SID && 
        strcmp(sid_name, res.sid.sid_name) == 0);

    // Write the port to the buffer.
    snprintf(buf, 32, "%" PRId32, res.sid.port);

    // Close existing socket. Try connecting to session server.
    close(sock);
    sock = dsm_getConnectedSocket(argv[1], buf);

    // Dispatch a check-in message.
    msg.type = DSM_MSG_ADD_PID;
    msg.proc.pid = rank;
    send_message(&msg);

    // Verify a DSM_MSG_SET_GID was sent back.
    recv_message(&msg, &res);
    ASSERT_COND(res.type == DSM_MSG_SET_GID);

    // Verify a start message was received.
    recv_message(&msg, &res);
    ASSERT_COND(res.type == DSM_MSG_CNT_ALL);

    // Send exit message.
    msg.type = DSM_MSG_EXIT;
    send_message(&msg);

    return 0;
}