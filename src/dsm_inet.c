#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#include "dsm_inet.h"
#include "dsm_util.h"


/*
 *******************************************************************************
 *                            Function Definitions                             *
 *******************************************************************************
*/


// [NON-REENTRANT] Converts port (< 99999) to string. Returns pointer.
const char *dsm_portToString (unsigned int port) {
	static char b[6]; // Five digits for 65536 + one for null character.
	snprintf(b, 6, "%u", port);
	return b;
}

// Returns string describing address of addrinfo struct. Returns NULL on error.
const char *dsm_addrinfoToString (struct addrinfo *ap, char *b) {
	void *addr;

	// If IPv4, use sin_addr. Otherwise use sin6_addr for IPv6.
	if (ap->ai_family == AF_INET) {
		struct sockaddr_in *ipv4 = (struct sockaddr_in *)ap->ai_addr;
		addr = &(ipv4->sin_addr);
	} else {
		struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)ap->ai_addr;
		addr = &(ipv6->sin6_addr);
	}

	// Convert format to string: Buffer must be INET6_ADDRSTRLEN large.
	return inet_ntop(ap->ai_family, addr, b, INET6_ADDRSTRLEN);
}

// Returns a socket bound to the given port. Exits fatally on error.
int dsm_getBoundSocket (int flags, int family, int socktype, const char *port) {
	struct addrinfo hints, *res, *p;
	int s, stat, y = 1;

	// Configure hints.
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = flags;
	hints.ai_family = family;
	hints.ai_socktype = socktype;

	// Lookup connection options.
	if ((stat = getaddrinfo(NULL, port, &hints, &res)) != 0) {
		dsm_panicf("getaddrinfo failed: %s", gai_strerror(stat));
	}

	// Bind to first suitable result.
	for (p = res; p != NULL; p = p->ai_next) {

		// Try initializing a socket.
		if ((s = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			dsm_warning("Socket init failed on getaddrinfo result!");
			continue;
		}

		// Try to reuse port.
		if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &y, sizeof(y)) == -1) {
			dsm_panicf("Couldn't reuse port (%s)", port);
		}

		// Try binding to the socket.
		if (bind(s, p->ai_addr, p->ai_addrlen) == -1) {
			dsm_panicf("Couldn't bind to port (%s)", port);
		}

		break;
	}

	// Free linked-list of results.
	freeaddrinfo(res);

	// Return result.
	return ((p == NULL) ? -1 : s);	
}


// Returns a socket connected to given address and port. Exits fatally on error.
int dsm_getConnectedSocket (const char *addr, const char *port) {
	struct addrinfo hints, *res, *p;
	int s, stat;

	// Setup hints. 
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	// Lookup connection options.
	if ((stat = getaddrinfo(addr, port, &hints, &res)) != 0) {
		dsm_panicf("getaddrinfo failed: %s", gai_strerror(stat));
	}

	// Bind to first suitable result.
	for (p = res; p != NULL; p = p->ai_next) {

		// Try initializing a socket.
		if ((s = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			dsm_warning("Socket init failed on getaddrinfo result!");
			continue;
		}

		// Try connecting to the socket.
		if (connect(s, p->ai_addr, p->ai_addrlen) == -1) {
			dsm_panicf("Couldn't connect to %s on %s", addr, port);
		}

		break;
	}

	// Free linked-list of results.
	freeaddrinfo(res);

	// Return result.
	return ((p == NULL) ? -1 : s);
}

// Gets socket address (use buffer length INET6_ADDRSTRLEN) and port.
void dsm_getSocketInfo (int s, char *addr_buf, size_t buf_size, 
	unsigned int *port) {
	struct sockaddr_storage addrinfo;
	socklen_t size = sizeof(addrinfo);
	void *addr;
	int family;

	// Verify input.
	if (addr_buf != NULL && buf_size < INET6_ADDRSTRLEN) {
		dsm_cpanic("dsm_getSocketInfo failed", "buf_size too small!"); 
	}
	
	// Extract socket information.
	if (getsockname(s, (struct sockaddr *)&addrinfo, &size) != 0) {
		dsm_panic("getsockname failed!");
	}

	// Verify family, cast to appropriate structure. Assign port.
	if (addrinfo.ss_family == AF_INET) {
		struct sockaddr_in *ipv4 = (struct sockaddr_in *)&addrinfo;
		family = ipv4->sin_family;
		addr = &(ipv4->sin_addr);
		if (port != NULL) {
			*port = ntohs(ipv4->sin_port);
		}
	} else {
		struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)&addrinfo;
		family = ipv6->sin6_family;
		addr = &(ipv6->sin6_addr);
		if (port != NULL) {
			*port = ntohs(ipv6->sin6_port);
		}
	}

	// Convert address to string, write to buffer.
	if (addr_buf != NULL && 
		inet_ntop(family, addr, addr_buf, buf_size) == NULL) {
		dsm_panic("inet_ntop failed!");
	}
}

// [DEBUG] Outputs socket's address and port.
void dsm_showSocketInfo (int s) {
	unsigned int port;
	char b[INET6_ADDRSTRLEN];

	dsm_getSocketInfo(s, b, INET6_ADDRSTRLEN, &port);

	printf("FD: %d, ADDR: \"%s\", PORT: %u\n", s, b, port);
}

// Ensures 'size' data is sent to fd. Exits fatally on error.
void dsm_sendall (int fd, unsigned char *b, size_t size) {
	size_t sent = 0;
	int n;

	do {
		if ((n = send(fd, (b + sent), size - sent, 0)) == -1) {
			dsm_panic("Syscall error on send!");
		}
		sent += n;
	} while (sent < size);
}

// Ensures 'size' data is received from fd. Exits fatally on error.
// Returns zero if all is normal. Returns nonzero if connection is closed.
int dsm_recvall (int fd, unsigned char *b, size_t size) {
	size_t received = 0;
	int n;

	do {
		if ((n = recv(fd, (b + received), size - received, 0)) == -1) {
			dsm_panic("Syscall error on recv!");
		}
		
		// Check if socket is closed.
		if (n == 0) {
			return -1;
		}

		received += n;
	} while (received < size);

	return 0;
}
