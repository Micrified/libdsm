#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/wait.h>
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
#define DSM_MIN_POLLABLE		4


/*
 *******************************************************************************
 *                              Global Variables                               *
 *******************************************************************************
*/
 

// Pollable file-descriptor set.
pollset *g_pollSet;

// Set of session-identifiers paired to file-descriptors by index.
int *g_fd_sids;

// Length of g_fd_sids.
size_t g_fd_sids_length;

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
 *                   File-Descriptor to Session-ID Functions                   *
 *******************************************************************************
*/


// Initializes the file-descriptor to session-ID set.
static int *init_fd_sids (size_t size) {
	int *fd_sids;

	// Allocate the array.
	if ((fd_sids = malloc(size * sizeof(int))) == NULL) {
		dsm_panic("Allocation failed!");
	}

	// Set the array values to the unset value.
	memset(fd_sids, -1, size * sizeof(int));

	// Set global size.
	g_fd_sids_length = size;

	return fd_sids;
}

// Frees the file-descriptor to session-ID set.
static void free_fd_sids (int *fd_sids) {
	free(fd_sids);
}

// Sets the session-identifier for the given file-descriptor.
static void set_fd_sid (int fd, int sid) {
	size_t old_length = g_fd_sids_length;

	// Verify arguments.
	ASSERT_COND(g_fd_sids != NULL && fd > 0);

	// Resize array if necessary. Remember to unset new slots.
	if ((size_t)fd >= g_fd_sids_length) {
		g_fd_sids_length = MAX(g_fd_sids_length * 2, (size_t)fd + 1);
		g_fd_sids = realloc(g_fd_sids, g_fd_sids_length * sizeof(int));
		memset(g_fd_sids + old_length, -1, (g_fd_sids_length - old_length)
			 * sizeof(int));
	}

	// Insert sid.
	g_fd_sids[fd] = sid;
}

// [DEBUG] Prints all session-identifiers for their given file-descriptors.
static void show_fd_sids (void) {
	printf("FD: ");
	for (unsigned int i = 0; i < g_fd_sids_length; i++) {
		printf("%d ", i);
	}
	printf("\n");
	printf("SI: ");
	for (unsigned int i = 0; i < g_fd_sids_length; i++) {
		printf("%d ", g_fd_sids[i]);
	}
	printf("\n");
}

// Removes the session-identifier for the given file-descriptor.
static void rem_fd_sid (int fd) {
	
	// Verify arguments.
	ASSERT_COND(g_fd_sids != NULL && fd > 0 && (size_t)fd < g_fd_sids_length);

	// Unset the entry.
	g_fd_sids[fd] = -1;
}


/*
 *******************************************************************************
 *                          Message Wrapper Functions                          *
 *******************************************************************************
*/


// Sends a message to target fd for SID payload. Performs packing task.
static void send_sid_msg (int fd, dsm_msg_t type, char *sid_name, int port) {
	dsm_msg msg;
    unsigned char buf[DSM_MSG_SIZE];

	// Create message.
	msg.type = type;
	msg.sid.port = port;
	snprintf(msg.sid.sid_name, DSM_MSG_STR_SIZE, "%s", sid_name);
	
    // Pack message.
    dsm_pack_msg(&msg, buf);

    // Send message.
    dsm_sendall(fd, buf, DSM_MSG_SIZE);
}


/*
 *******************************************************************************
 *                          Message Handler Functions                          *
 *******************************************************************************
*/


// Forks a server session for given session name and number of processes.
static void fork_session_server (const char *sid_name, unsigned int nproc);

// DSM_MSG_GET_SID: Arbiter wishes to obtain session server details.
static void handler_get_sid (int fd, dsm_msg *mp) {
	dsm_sid_t *session = dsm_getHashTableEntry(g_sid_htab, mp->sid.sid_name);

	// If invalid number of processes specified: Send rejection response.
	if (mp->sid.nproc < 2) {
		send_sid_msg(fd, DSM_MSG_DEL_SID, mp->sid.sid_name, 0);		
	}

	// If session doesn't exist: Create one, fork server, and update sender.
	if (session == NULL) {
		session = dsm_setSIDHashTableEntry(g_sid_htab, mp->sid.sid_name);
		fork_session_server(mp->sid.sid_name, mp->sid.nproc);
	}

	// Otherwise, if port is set: Send sender reply and disconnect them.
	if (session->port != -1) {

		// Dispatch reply.
		send_sid_msg(fd, DSM_MSG_SET_SID, mp->sid.sid_name, session->port);

		// Close sender socket.
		close(fd);

		// Remove sender from pollable set.
		dsm_removePollable(fd, g_pollSet);

		return;
	}

	// Update sender as waiting for SID.
	set_fd_sid(fd, session->sid);
}

// DSM_MSG_SET_SID: Session server wishes to update it's connection info.
static void handler_set_sid (int fd, dsm_msg *mp) {
	dsm_sid_t *session;
	UNUSED(fd);

	// Extract session.
	ASSERT_COND((session = dsm_getHashTableEntry(g_sid_htab, 
		mp->sid.sid_name)) != NULL);

	// Verify port.
	ASSERT_COND(mp->sid.port > 0);

	// Update the session port.
	session->port = mp->sid.port;

	// Notify all waiting arbiters, then remove them.
	for (unsigned int i = 0; i < g_fd_sids_length; i++) {
		if (g_fd_sids[i] == session->sid) {
			send_sid_msg(i, DSM_MSG_SET_SID, mp->sid.sid_name, session->port);
			rem_fd_sid(i);
			dsm_removePollable(i, g_pollSet);
		}
	}

	// Close sender socket and remove from pollables.
	close(fd);
	dsm_removePollable(fd, g_pollSet);
}

// DSM_MSG_DEL_SID: Session server wishes to terminate session.
static void handler_del_sid (int fd, dsm_msg *mp) {
	dsm_sid_t *session;
	UNUSED(fd);

	// Ensure session exists.
	ASSERT_COND((session = dsm_getHashTableEntry(g_sid_htab, 
		mp->sid.sid_name)) != NULL);
	
	// Remove any waiting connections for said session.
	for (unsigned int i = 0; i < g_fd_sids_length; i++) {
		if (g_fd_sids[i] == session->sid) {
			rem_fd_sid(i);
			dsm_removePollable(i, g_pollSet);			
		}
	}

	// Remove entry.
	dsm_remHashTableEntry(g_sid_htab, mp->sid.sid_name);

	// Close sender socket and remove from pollables.
	close(fd);
	dsm_removePollable(fd, g_pollSet);
}


/*
 *******************************************************************************
 *                              Utility Functions                              *
 *******************************************************************************
*/


// Forks a server session for given session name and number of processes.
static void fork_session_server (const char *sid_name, unsigned int nproc) {
	int pid;
	char buf[16] = {0};
	snprintf(buf, 16, "%u", nproc);

	// Fork once and exit to orphan server to init.
	if ((pid = dsm_fork()) == 0) {

		// Set as new session group leader to remove terminal.
		if (dsm_fork() == 0) {
			setsid();
			execl("./bin/dsm_server", "dsm_server", sid_name, buf, 
				(char *)NULL);
		}

		// Exit the fork.
		exit(EXIT_SUCCESS);
	}

	waitpid(pid, NULL, 0);
}

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

	// Initialize session-identifier set.
	g_fd_sids = init_fd_sids(DSM_MIN_POLLABLE);

	// Initialize session table.
	g_sid_htab = dsm_initSIDHashTable();

	// Initialize string table.
	g_str_tab = dsm_initStringTable(DSM_STR_TAB_SIZE);

	// Initialize listener socket. Use default port.
	if ((g_sock_listen = dsm_getBoundSocket(AI_PASSIVE, AF_UNSPEC, SOCK_STREAM, 
		DSM_DAEMON_PORT)) == -1) {
		dsm_panicf("Couldn't bind to port %d!", DSM_DAEMON_PORT);
	}

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

	// Free session-identifier table.
	free_fd_sids(g_fd_sids);

	// Free pollable set.
	dsm_freePollSet(g_pollSet);

	return 0;
}