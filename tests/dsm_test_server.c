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


/* Test Description:
 * This program mimicks several arbiters, each with a single process, 
 * communicating with a session-server. The program forks multiple
 * processes. Each oddly ranked process initiates a write-request,
 * and waits to receive a go-ahead. All other processes simply
 * wait to receive data. The system tests whether the server
 * is correctly handling concurrent write-requests. 
*/

#define SESSION_NAME	"test"

// Server socket.
int sock;

// Contains the last message received by recv_message.
dsm_msg recv_msg;

// Receives message. Returns nonzero if not matching expected.
int recv_message (dsm_msg *exp_msg) {
	unsigned char recv_buf[DSM_MSG_SIZE], exp_buf[DSM_MSG_SIZE];

	// Receive message.
	dsm_recvall(sock, recv_buf, DSM_MSG_SIZE);
	dsm_unpack_msg(&recv_msg, recv_buf);

	// Do not compare, if given message is NULL.
	if (exp_msg == NULL) return -1;

	// Pack expected message.
	dsm_pack_msg(exp_msg, exp_buf);
	
	return memcmp(recv_buf, exp_buf, DSM_MSG_SIZE);
}

// Send message to socket.
void send_message (dsm_msg *mp) {
	unsigned char buf[DSM_MSG_SIZE];
	dsm_pack_msg(mp, buf);
	dsm_sendall(sock, buf, DSM_MSG_SIZE);
}

int main (int argc, const char *argv[]) {
	int gid = -1, rank = 0, narb = 4;
	dsm_msg msg = {0};
	char port[6] = {0};

	// Verify arguments.
	if (argc != 3) {
		fprintf(stderr, "usage: %s <daemon_address> <daemon_port>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	// Fork processes.
	for (int i = 1; i < narb; i++) {
		if (fork() == 0) {
			rank = i;
			break;
		}
	}

	// Connect to session-daemon.
	sock = dsm_getConnectedSocket(argv[1], argv[2]);

	// All forks ask for the same session server.
	msg.type = DSM_MSG_GET_SID;
	snprintf(msg.sid.sid_name, DSM_MSG_STR_SIZE, "%s", SESSION_NAME);
	msg.sid.nproc = narb;
	send_message(&msg);

	// Expect back a reply. Ignore full comparison (we only check type and SID).
	recv_message(&msg);
	assert(recv_msg.type == DSM_MSG_SET_SID && 
		strcmp(recv_msg.sid.sid_name, SESSION_NAME) == 0);

	// Copy port to buffer.
	snprintf(port, 6, "%" PRId32, recv_msg.sid.port);

	// Close connection, prepare to begin new one with session server.
	close(sock);
	sock = dsm_getConnectedSocket(argv[1], port);

	// Each arbiter sends an ADD_PID message, and expects back a SET_GID message.
	msg.type = DSM_MSG_ADD_PID;
	msg.proc.pid = rank;
	send_message(&msg);
	
	// Receive SET_GID. Verify PID and GID are correct. 
	memset(&msg, 0, sizeof(dsm_msg));
	msg.type = DSM_MSG_SET_GID;
	msg.proc.pid = rank;
	msg.proc.gid = rank;
	recv_message(&msg);
	gid = recv_msg.proc.gid;
	assert(recv_msg.proc.pid == rank && gid >= 0 && gid < 4);

	// Each arbiter expects a continue message.
	memset(&msg, 0, sizeof(dsm_msg));
	msg.type = DSM_MSG_CNT_ALL;
	recv_message(&msg);

	
	// Have oddly ranked processes/arbiter issue write requests.
	if ((rank % 2) == 1) {
		msg.type = DSM_MSG_REQ_WRT;
		msg.proc.pid = rank;
		send_message(&msg);
	}


	// Accept incoming messages. If odd, take special actions.
	for (int i = 0; i < 2; i++) {

		// Receive any kind of message.
		recv_message(NULL);

		// If go-ahead: verify rank and send data.
		if (recv_msg.type == DSM_MSG_WRT_NOW) {
			assert((rank % 2) == 1);
			memset(&msg, 0, sizeof(dsm_msg));
			msg.type = DSM_MSG_WRT_DATA;
			send_message(&msg);
		} else {
			assert(recv_msg.type == DSM_MSG_WRT_DATA);
		}

		// Send acknowledgment.
		msg.type = DSM_MSG_GOT_DATA;
		msg.task.nproc = 1;
		send_message(&msg);


	}

	// All processes send exit message.
	memset(&msg, 0, sizeof(dsm_msg));
	msg.type = DSM_MSG_EXIT;
	send_message(&msg);

	close(sock);

	// If process rank is 0: collect zombies.
	if (rank == 0) {
		for (int i = 1; i < narb; i++) {
			waitpid(-1, NULL, 0);
		}
		printf("Ok!\n");
	}

	return 0;
}