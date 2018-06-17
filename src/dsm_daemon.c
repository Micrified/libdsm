#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "dsm_daemon.h"
#include "dsm_inet.h"
#include "dsm_util.h"
#include "dsm_msg.h"
#include "dsm_poll.h"
#include "dsm_ptab.h"
#include "dsm_stab.h"
#include "dsm_sid_htab.h"


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
 

// Pollable file-descriptor set.
pollset *g_pollSet;

// Function map.
dsm_msg_func g_fmap[DSM_MSG_MAX_VAL];

// String table.
dsm_stab *g_str_tab;

// Session table.
dsm_sid_htab *g_sid_htab;

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


/*
 *******************************************************************************
 *                          Message Handler Functions                          *
 *******************************************************************************
*/


// DSM_MSG_GET_SID: Arbiter wishes to obtain session server details.
static void handler_get_sid (int fd, dsm_msg *mp) {
	dsm_sid_t *session = dsm_getHashTableEntry(g_sid_htab, mp->sid.sid_name);

	// If the session doesn't exist, create one and fork a session server.
	if (session == NULL) {
		dsm_setSIDHashTableEntry(g_sid_htab, mp->sid.sid_name);
		printf("[%d] Forking an arbiter for %d processes!\n", getpid(),
			mp->sid.nproc);
		return;
	}

	// If it does exist, and the port is set, then send a reply.
	if (session->port != -1) {
		printf("[%d] Sending session info!\n", getpid());
		mp->type = DSM_MSG_SET_SID;
		mp->sid.port = session->port;
		send_msg(fd, mp);
		return;
	}

	// Otherwise do nothing.
}

// DSM_MSG_SET_SID: Session server wishes to update it's connection info.
static void handler_set_sid (int fd, dsm_msg *mp) {
	dsm_sid_t *session;
	UNUSED(fd);

	// Extract session.
	ASSERT_COND((session = dsm_getHashTableEntry(g_sid_htab, 
		mp->sid.sid_name)) != NULL);

	// Verify port.
	ASSERT_COND(session->port > 0);

	// Update the session port.
	session->port = session->port;

	// Notify all waiting arbiters.
	printf("[%d] Notifying all waiting arbiters...\n", getpid());


}

// DSM_MSG_DEL_SID: Session server wishes to terminate session.
static void handler_del_sid (int fd, dsm_msg *mp) {
	dsm_sid_t *session;
	UNUSED(fd);

	// Ensure session exists.
	ASSERT_COND((session = dsm_getHashTableEntry(g_sid_htab, 
		mp->sid.sid_name)) != NULL);

	// Remove entry.
	dsm_remHashTableEntry(g_sid_htab, mp->sid.sid_name);
	
	// Remove any waiting connections for said session.
	printf("[%d] Destroyed session: \"%s\". Removing pending!\n", getpid(), 
		mp->sid.sid_name);
}


/*
 *******************************************************************************
 *                              Utility Functions                              *
 *******************************************************************************
*/


// Handles a connection to the listener socket.
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
 *                                    Main                                     *
 *******************************************************************************
*/


int main (int argc, const char *argv[]) {
	int new = 0;				// Newly active connections (for poll syscall).
	struct pollfd *pfd = NULL;	// Pointer to a struct pollfd instance.
	UNUSED(argc); UNUSED(argv);

	// ------------------------------------------------------------------------

	// Map message types to handlers.
	dsm_setMsgFunc(DSM_MSG_GET_SID, handler_get_sid, g_fmap);
	dsm_setMsgFunc(DSM_MSG_SET_SID, handler_set_sid, g_fmap);
	dsm_setMsgFunc(DSM_MSG_DEL_SID, handler_del_sid, g_fmap);

	// Initialize pollable set.
	g_pollSet = dsm_initPollSet(DSM_MIN_POLLABLE);

	// Initialize session table.
	g_sid_htab = dsm_initSIDHashTable();

	// Initialize string table.
	g_str_tab = dsm_initStringTable(DSM_STR_TAB_SIZE);

	// Initialize listener socket. Use default port.
	g_sock_listen = dsm_getBoundSocket(AI_PASSIVE, AF_UNSPEC, SOCK_STREAM, 
		DSM_DAEMON_PORT);

	// Register listener socket as pollable.
	dsm_setPollable(g_sock_listen, POLLIN, g_pollSet);

	// Listen on the socket.
	if (listen(g_sock_listen, DSM_DEF_BACKLOG) == -1) {
		dsm_panicf("Couldn't listen on port (%s)!", DSM_DAEMON_PORT);
	}

	printf("[%d] Ready: ", getpid()); dsm_showSocketInfo(g_sock_listen);

	// ------------------------------------------------------------------------

	// Keep polling as long as no errors occur.
	while ((new = poll(g_pollSet->fds, g_pollSet->fp, -1)) != -1) {

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
	}

	// ------------------------------------------------------------------------

	printf("[%d] Daemon Exiting...\n", getpid());

	// Unregister listener socket.
	dsm_removePollable(g_sock_listen, g_pollSet);

	// Close listener socket.
	close(g_sock_listen);

	// Free the session table.
	dsm_freeHashTable(g_sid_htab);

	// Free string table.
	dsm_freeStringTable(g_str_tab);

	// Free pollable set.
	dsm_freePollSet(g_pollSet);

	return 0;
}