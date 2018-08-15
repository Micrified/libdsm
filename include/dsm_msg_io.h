#if !defined (DSM_MSG_IO_H)
#define DSM_MSG_IO_H

#include "dsm_msg.h"


/*
 *******************************************************************************
 *                            Function Declarations                            *
 *******************************************************************************
*/


/*
 * [NON-REENTRANT] Receives message from socket and unpacks it. The message
 * pointer is then configured. This function is non-reentrant, meaning that it 
 * cannot be called recursively or in parallel without all but the latest
 * invoker losing information. This specifically affects messages of type
 * DSM_MSG_WRT_DATA, where the data is stored in a static buffer within
 * the function.
*/
void dsm_recv_msg (int fd, dsm_msg *m);

/*
 * Packs message and writes it to the given socket. All messages have a
 * standard size of DSM_MSG_SIZE bytes, with a potential extension in the
 * case of DSM_MSG_WRT_DATA. In DSM_MSG_WRT_DATA, the buffer is appended to
 * the end of the message (at an offset of DSM_MSG_SIZE bytes). Data size is 
 * capped at DSM_MAX_DATA_SIZE. If a larger size is specified, then it is
 * automatically chunked and sent in a sequence of messages.
*/
void dsm_send_msg (int fd, dsm_msg *mp);


#endif