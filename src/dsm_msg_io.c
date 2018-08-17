#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dsm_msg.h"
#include "dsm_util.h"
#include "dsm_inet.h"
#include "dsm_msg_io.h"


/*
 *******************************************************************************
 *                            Function Definitions                             *
 *******************************************************************************
*/


// See header file for description.
void dsm_recv_msg (int fd, dsm_msg *mp) {
	unsigned char buf[DSM_MSG_SIZE];
	static unsigned char data[DSM_MAX_DATA_SIZE];

	// Receive message from socket.
	if (dsm_recvall(fd, buf, DSM_MSG_SIZE) != 0) {
		goto err;
	}

	// Unpack the message.
	dsm_unpack_msg(mp, buf);

	// If not DSM_MSG_WRT_DATA, then return.
	if (mp->type != DSM_MSG_WRT_DATA) {
		return;
	}
	
	// Otherwise verify data payload size.
	ASSERT_COND(mp->data.size <= DSM_MAX_DATA_SIZE);
	
	// Receive remaining data from socket.
	if (dsm_recvall(fd, data, mp->data.size) != 0) {
		goto err;
	}

	// Attach buffer pointer to message, and return it.
	mp->data.buf = data;
	return;

	err: dsm_panicf("(%s:%d) Lost connection to [%d]!", 
			__FILE__, __LINE__, fd);
}

// See header file for description.
void dsm_send_msg (int fd, dsm_msg *mp) {
	unsigned char buf[DSM_MSG_SIZE];
	size_t next_size, send_size;
	unsigned int isData;

	// Verify input.
	ASSERT_COND(mp != NULL);

	// If message type is DSM_MSG_WRT_DATA, adjust details.
	if ((isData = (mp->type == DSM_MSG_WRT_DATA)) == 1) {
		send_size = MIN(DSM_MAX_DATA_SIZE, mp->data.size);
		next_size = mp->data.size - send_size;
		mp->data.size = send_size;
	}

	// Pack the message to buffer.
	dsm_pack_msg(mp, buf);

	// Dispatch message.
	dsm_sendall(fd, buf, DSM_MSG_SIZE);

	// If type is not DSM_MSG_WRT_DATA. Return.
	if (isData == 0) {
		return;
	}

	// Send the data.
	dsm_sendall(fd, mp->data.buf, mp->data.size);

	// If more remains, continue sending.
	if (next_size > 0) {
		mp->data.size = next_size;
		mp->data.offset += send_size;
		mp->data.buf = (void *)((intptr_t)mp->data.buf + (intptr_t)send_size);
		dsm_send_msg(fd, mp);
	}
}
