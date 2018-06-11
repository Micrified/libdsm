#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <arpa/inet.h>

#include "dsm_msg.h"
#include "dsm_util.h"

/*
 *******************************************************************************
 *                              Type Definitions                               *
 *******************************************************************************
*/


// Type describing a marshalling function.
typedef void (*dsm_marshall_func) (int dir, dsm_msg *, unsigned char *);


/*
 *******************************************************************************
 *                              Global Variables                               *
 *******************************************************************************
*/


// Map: dsm_msg_t -> dsm_marshall_func.
static dsm_marshall_func fmap[DSM_MSG_MAX_VAL];


/*
 *******************************************************************************
 *                        Packing Function Definitions                         *
 *******************************************************************************
*/


// Writes 32-bit integer to buffer. Returns number of bytes written.
static size_t pack_i32 (unsigned char *b, uint32_t i) {

    // Write each byte to the buffer.
    *b++ = i >> 24;
    *b++ = i >> 16;
    *b++ = i >> 8;
    *b++ = i;

    return sizeof(uint32_t);
}

// Writes 64-bit integer to buffer. Returns number of bytes written.
static size_t pack_i64 (unsigned char *b, uint64_t i) {

    // Write each byte to the buffer.
    *b++ = i >> 56;
    *b++ = i >> 48;
    *b++ = i >> 40;
    *b++ = i >> 32;
    *b++ = i >> 24;
    *b++ = i >> 16;
    *b++ = i >> 8;
    *b++ = i;
    
    return sizeof(uint64_t);
}

// Reads 32-bit integer from buffer. Returns number of bytes read.
static size_t unpack_i32 (unsigned char *b, int32_t *ip) {
	int32_t i;
    uint32_t ui = 0;

	// Reconstructs packed integer as unsigned.
    ui = ((uint32_t)b[0] << 24) |
         ((uint32_t)b[1] << 16) |
         ((uint32_t)b[2] << 8)  |
         ((uint32_t)b[3])       ;

	// Converts unsigned to signed.
	if (ui <= 0x7fffffffu) {
		i = ui;
	} else {
		i = -1 - (int32_t)(0xffffffffu - ui);
	}

	// Assign ip and return size.
    *ip = i;

    return sizeof(uint32_t);
}

// Reads 64-bit integer from buffer. Returns number of bytes read.
static int64_t unpack_i64 (unsigned char *b, int64_t *ip) {
	int64_t i;
    uint64_t ui = 0;

	// Reconstruct packed integer as unsigned.
    ui = ((uint64_t)b[0] << 56) |
         ((uint64_t)b[1] << 48) |
         ((uint64_t)b[2] << 40) |
         ((uint64_t)b[3] << 32) |
         ((uint64_t)b[4] << 24) |
         ((uint64_t)b[5] << 16) |
         ((uint64_t)b[6] << 8)  |
         ((uint64_t)b[7])       ;
	
	// Converts unsigned to signed.
	if (ui <= 0x7fffffffffffffffu) {
		i = ui;
	} else {
		i = -1 - (int64_t)(0xffffffffffffffffu - ui);
	}

	// Assign ip and return size.
    *ip = i;

    return sizeof(uint64_t);
}

/* 
 * Packs type-sequence to buffer sequentially according to format string.
 * Supported tokens are:
 * Long (int32_t):  	 	"l"
 * Quad (int64_t):  	 	"q"
 * String (char):   	 	"s" (fixed size).
 * Bytes (unsigned char):	"b" (fixed size of 8 bytes).
 * Returns total number of bytes written. Truncates strings if too long.
*/
static size_t pack (unsigned char *b, const char *fmt, ...) {
	va_list ap;
	size_t size = 0, written = 0;

	char *s;			// String.
	unsigned char *y;	// Byte buffer.
	uint32_t l;			// 32-bit signed integer.
	uint64_t q;			// 64-bit signed integer.

	// Initialize variable-argument list.
	va_start(ap, fmt);

	// Parse variable list, write elements to buffer.
	while (*fmt != '\0') {
		switch (*fmt) {

			case 'l':
				l = (uint32_t)va_arg(ap, int32_t);
                written = pack_i32(b, l);
				break;

			case 'q':
				q = (uint64_t)va_arg(ap, int64_t);
				written = pack_i64(b, q);
				break;

			case 's':
				s = va_arg(ap, char *);
                memset(b, 0, DSM_MSG_STR_SIZE);
				memcpy(b, s, MIN(strlen(s), DSM_MSG_STR_SIZE));
				written = DSM_MSG_STR_SIZE;
				break;

			case 'b':
				y = va_arg(ap, unsigned char *);
				memcpy(b, y, 8);
				written = 8;
				break;

            default:
                fprintf(stderr, "Error: Unknown token: %c!\n", *fmt);
                exit(EXIT_FAILURE);
		}

        // Increase buffer pointer and size.
        b += written;
        size += written;

		// Increment token pointer.
		fmt++;
	}

	va_end(ap);

	return size;
}

/* 
 * Unpacks type-sequence from buffer sequentially according to format string.
 * Supported tokens are:
 * Long (int32_t):  	 	"l"
 * Quad (int64_t):  	 	"q"
 * String (char):   	 	"s" (fixed size).
 * Bytes (unsigned char):	"b" (fixed size of 8 bytes).
 * Returns total number of bytes written. Truncates strings if too long.
*/
static size_t unpack (unsigned char *b, const char *fmt, ...) {
	va_list ap;
	size_t size = 0, read = 0;

	char *s;			// String.
	unsigned char *y;	// Byte buffer.
	int32_t *l;			// 32-bit signed integer.
	int64_t *q;			// 64-bit signed integer.

	// Initialize variable-argument list.
	va_start(ap, fmt);

	// Parse variable list, write elements to buffer.
	while (*fmt != '\0') {
		switch (*fmt) {

			case 'l':
				l = va_arg(ap, int32_t *);
                read = unpack_i32(b, l);
				break;

			case 'q':
				q = va_arg(ap, int64_t *);
				read = unpack_i64(b, q);
				break;

			case 's':
				s = va_arg(ap, char *);
				memcpy(s, b, DSM_MSG_STR_SIZE);
				read = DSM_MSG_STR_SIZE;
				break;

			case 'b':
				y = va_arg(ap, unsigned char *);
				memcpy(y, b, 8);
				read = 8;
				break;

            default:
                fprintf(stderr, "Error: Unknown token: %c!\n", *fmt);
                exit(EXIT_FAILURE);
		}

        // Increase buffer-pointer and size.
        b += read;
        size += read;

        // Increment token pointer.
		fmt++;
	}

	va_end(ap);

    return size;
}

/*
 *******************************************************************************
 *                      Marshalling Function Definitions                       *
 *******************************************************************************
*/


// Marshalls: [DSM_MSG_STP_ALL, DSM_MSG_CNT_ALL, DSM_MSG_REL_BAR, DSM_MSG_WRT
//	_NOW, DSM_MSG_ALL_STP, DSM_MSG_GOT_DATA, DSM_MSG_REQ_WRT, DSM_MSG_EXIT].
static void marshall_payload_none (int dir, dsm_msg *mp, unsigned char *b) {
	const char *fmt = "l";
	if (dir == 0) {
		pack(b, fmt, mp->type);
	} else {
		unpack(b, fmt, &(mp->type));
	}
}

// Marshalls: [DSM_MSG_GET_SID, DSM_MSG_SET_SID, DSM_MSG_DEL_SID].
static void marshall_payload_sid (int dir, dsm_msg *mp, unsigned char *b) {
	const char *fmt = "lsl";
	if (dir == 0) {
		pack(b, fmt, mp->type, mp->sid.sid, mp->sid.port);
	} else {
		unpack(b, fmt, &(mp->type), mp->sid.sid, &(mp->sid.port));
	}
}

// Marshalls: [DSM_MSG_ADD_PID, DSM_MSG_SET_GID, DSM_MSG_REQ_WRT, 
//	DSM_MSG_HIT_BAR].
static void marshall_payload_proc (int dir, dsm_msg *mp, unsigned char *b) {
	const char *fmt = "lll";
	if (dir == 0) {
		pack(b, fmt, mp->type, mp->proc.pid, mp->proc.gid);
	} else {
		unpack(b, fmt, &(mp->type), &(mp->proc.pid), &(mp->proc.gid));
	}
}

// Marshalls: [DSM_MSG_ALL_STP, DSM_MSG_GOT_DATA].
static void marshall_payload_task (int dir, dsm_msg *mp, unsigned char *b) {
	const char *fmt = "ll";
	if (dir == 0) {
		pack(b, fmt, mp->type, mp->task.nproc);
	} else {
		unpack(b, fmt, &(mp->type), &(mp->task.nproc));
	}
}

// Marshalls: [DSM_MSG_GOT_DATA].
static void marshall_payload_data (int dir, dsm_msg *mp, unsigned char *b) {
	const char *fmt = "lqqb";
	if (dir == 0) {
		pack(b, fmt, mp->type, mp->data.offset, mp->data.size, mp->data.bytes);
	} else {
		unpack(b, fmt, &(mp->type), &(mp->data.offset), &(mp->data.size), 
			&(mp->data.bytes));
	}
}

// Marshalls: [DSM_MSG_POST_SEM, DSM_MSG_WAIT_SEM].
static void marshall_payload_sem (int dir, dsm_msg *mp, unsigned char *b) {
	const char *fmt = "lsl";
	if (dir == 0) {
		pack(b, fmt, mp->type, mp->sem.sem_name, mp->sem.pid);
	} else {
		unpack(b, fmt, &(mp->type), mp->sem.sem_name, &(mp->sem.pid));
	}
}


/*
 *******************************************************************************
 *                        Private Function Definitions                         *
 *******************************************************************************
*/


// Initializes all function maps.
void init_fmaps (void) {
	static int init = 0;

	// Don't re-initialize.
	if (init == 1) {
		return;
	}

	// Marshalling for: No payload.
	fmap[DSM_MSG_STP_ALL] = fmap[DSM_MSG_CNT_ALL] = fmap[DSM_MSG_REL_BAR] = 
		fmap[DSM_MSG_WRT_NOW] = fmap[DSM_MSG_REQ_WRT] = fmap[DSM_MSG_EXIT] =
		marshall_payload_none;

	// Marshalling for: dsm_payload_sid.
	fmap[DSM_MSG_SET_SID] = fmap[DSM_MSG_DEL_SID] = fmap[DSM_MSG_GET_SID] = 
		marshall_payload_sid;

	// Marshalling for: dsm_payload_proc.
	fmap[DSM_MSG_ADD_PID] = fmap[DSM_MSG_SET_GID] = fmap[DSM_MSG_HIT_BAR] =
		marshall_payload_proc;

	// Marshalling for: dsm_payload_task.
	fmap[DSM_MSG_GOT_DATA] = fmap[DSM_MSG_ALL_STP] = marshall_payload_task;

	// Marshalling for: dsm_payload_sem.
	fmap[DSM_MSG_POST_SEM] = fmap[DSM_MSG_WAIT_SEM] = marshall_payload_sem;
	
	// Set as initialized.
	init = 1;
}


/*
 *******************************************************************************
 *                         Public Function Definitions                         *
 *******************************************************************************
*/


// Marshalls given message to buffer. Buffer must be at least DSM_MSG_SIZE.
void dsm_pack_msg (dsm_msg *mp, unsigned char *b) {
	init_fmaps();
	dsm_marshall_func f;

	// Verify input.
	if (mp == NULL || b == NULL) {
		dsm_cpanic("dsm_pack_msg", "NULL parameter(s)!");
	}
	if (mp->type <= DSM_MSG_MIN_VAL || mp->type >= DSM_MSG_MAX_VAL) {
		dsm_cpanic("dsm_pack_msg", "Invalid message type!");
	}

	// Sanitize buffer.
	memset(b, 0, DSM_MSG_SIZE);

	// Execute mapped function.
	f = fmap[mp->type];
	f(0, mp, b);
}

// Unmarshalls message of type to mp. Buffer must be at least DSM_MSG_SIZE.
void dsm_unpack_msg (dsm_msg *mp, unsigned char *b) {
	init_fmaps();
	dsm_marshall_func f;

	// Extract and set type.
	unpack(b, "l", &(mp->type));

	// Verify input.
	if (mp == NULL || b == NULL) {
		dsm_cpanic("dsm_unpack_msg", "NULL parameter(s)!");
	}
	if (mp->type <= DSM_MSG_MIN_VAL || mp->type >= DSM_MSG_MAX_VAL) {
		dsm_cpanic("dsm_unpack_msg", "Invalid message type!");
	}

	// Execute mapped function.
	f = fmap[mp->type];
	f(1, mp, b);
}

// [DEBUG] Prints the contents of a message.
void dsm_showMsg (dsm_msg *mp) {
	printf("======================== MSG ========================\n");

	// Don't access NULL messages.
	if (mp == NULL) {
		printf("NULL\n");
		return;
	}

	// Print contents.
	switch (mp->type) {
		case DSM_MSG_SET_SID:
			printf("Type: DSM_MSG_SET_SID\n");
			printf("sid = \"%.*s\"\n", DSM_MSG_STR_SIZE, 
				mp->sid.sid);
			printf("port = %d\n", mp->sid.port);
			break;

		case DSM_MSG_STP_ALL:
			printf("Type: DSM_MSG_STP_ALL\n");
			break;

		case DSM_MSG_CNT_ALL:
			printf("Type: DSM_MSG_CNT_ALL\n");
			break;

		case DSM_MSG_REL_BAR:
			printf("Type: DSM_MSG_REL_BAR\n");
			break;

		case DSM_MSG_WRT_NOW:
			printf("Type: DSM_MSG_WRT_NOW\n");
			break;

		case DSM_MSG_SET_GID:
			printf("Type: DSM_MSG_SET_GID\n");
			printf("pid = %d\n", mp->proc.pid);
			printf("gid = %d\n", mp->proc.gid);
			break;

		case DSM_MSG_GET_SID:
			printf("Type: DSM_MSG_GET_SID\n");
			printf("sid = \"%.*s\"\n", DSM_MSG_STR_SIZE,
				mp->sid.sid);
			printf("nproc = %d\n", mp->sid.nproc);
			break;

		case DSM_MSG_ALL_STP:
			printf("Type: DSM_MSG_ALL_STP\n");
			printf("nproc = %d\n", mp->task.nproc);
			break;

		case DSM_MSG_GOT_DATA:
			printf("Type: DSM_MSG_GOT_DATA\n");
			printf("nproc = %d\n", mp->task.nproc);
			break;

		case DSM_MSG_ADD_PID:
			printf("Type: DSM_MSG_ADD_PID\n");
			printf("pid = %d\n", mp->proc.pid);
			break;

		case DSM_MSG_REQ_WRT:
			printf("Type: DSM_MSG_REQ_WRT\n");
			printf("pid = %d\n", mp->proc.pid);
			break;

		case DSM_MSG_HIT_BAR:
			printf("Type: DSM_MSG_HIT_BAR\n");
			printf("pid = %d\n", mp->proc.pid);
			break;

		case DSM_MSG_WRT_DATA: {
			printf("Type: DSM_MSG_WRT_DATA\n");
			printf("offset = %ld\n", mp->data.offset);
			printf("size = %zu\n", mp->data.size);
			printf("data = ");
			for (unsigned int i = 0; i < mp->data.size; i++) {
				printf("%02x ", mp->data.bytes[i]);
			}
			printf("\n");
			break;
		}

		case DSM_MSG_POST_SEM:
			printf("Type: DSM_MSG_POST_SEM\n");
			printf("sem_name = \"%.*s\"\n", DSM_MSG_STR_SIZE,
				mp->sem.sem_name);
			break;

		case DSM_MSG_WAIT_SEM:
			printf("Type: DSM_MSG_WAIT_SEM\n");
			printf("sem_name = \"%.*s\"\n", DSM_MSG_STR_SIZE, 
				mp->sem.sem_name);
			break;

		case DSM_MSG_EXIT:
			printf("Type: DSM_MSG_EXIT\n");
			break;

		default:
			printf("Type: <UNKNOWN>\n");		
	}
	
	printf("=====================================================\n");
}

// Sets function for given message type in map. Returns nonzero on error.
void dsm_setMsgFunc (dsm_msg_t type, dsm_msg_func func, dsm_msg_func *fmap) {
	
	// Verify type.
	if (type <= DSM_MSG_MIN_VAL || type >= DSM_MSG_MAX_VAL) {
		dsm_cpanic("dsm_setMsgFunc", "Message type out of range!");
	}

	// Install function.
	fmap[type] = func;
}

// Returns function pointer for given message type. Returns NULL on error.
dsm_msg_func dsm_getMsgFunc (dsm_msg_t type, dsm_msg_func *fmap) {

	// Verify type.
	if (type <= DSM_MSG_MIN_VAL || type >= DSM_MSG_MAX_VAL) {
		dsm_cpanic("dsm_setMsgFunc", "Message type out of range!");
	}

	// Return function.
	return fmap[type];
}
