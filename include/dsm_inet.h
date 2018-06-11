#if !defined(DSM_INET_H)
#define DSM_INET_H


#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>


/*
 *******************************************************************************
 *                            Function Declarations                            *
 *******************************************************************************
*/


// [NON-REENTRANT] Converts port (< 99999) to string. Returns pointer.
const char *dsm_portToString (unsigned int port);

// Returns string describing address of addrinfo struct. Returns NULL on error.
const char *dsm_addrinfoToString (struct addrinfo *ap, char *b); 

// Returns a socket bound to the given port. Exits fatally on error.
int dsm_getBoundSocket (int flags, int family, int socktype, const char *port);

// Returns a socket connected to given address and port. Exits fatally on error.
int dsm_getConnectedSocket (const char *addr, const char *port);

// Gets socket address (use buffer length INET6_ADDRSTRLEN) and port.
void dsm_getSocketInfo (int s, char *addr_buf, size_t buf_size, 
	unsigned int *port);

// [DEBUG] Outputs socket's address and port.
void dsm_showSocketInfo (int s);

// Ensures 'size' data is sent to fd. Exits fatally on error.
void dsm_sendall (int fd, unsigned char *b, size_t size);

// Ensures 'size' data is received from fd. Exits fatally on error.
// Returns zero if all is normal. Returns nonzero if connection is closed.
int dsm_recvall (int fd, unsigned char *b, size_t size);


#endif