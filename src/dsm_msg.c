#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <inttypes.h>
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


// Table mapping dsm_msg_t to dsm_marshall_func.
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
 * String (char):   	 	"s" (fixed to DSM_MSG_STR_SIZE bytes).
 * Returns total number of bytes written. Truncates strings if too long.
*/
static size_t pack (unsigned char *b, const char *fmt, ...) {
	va_list ap;
	size_t size = 0, written = 0;

	char *s;			// String.
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
 * String (char):   	 	"s" (fixed to DSM_MSG_STR_SIZE bytes).
 * Returns total number of bytes written. Truncates strings if too long.
*/
static size_t unpack (unsigned char *b, const char *fmt, ...) {
	va_list ap;
	size_t size = 0, read = 0;

	char *s;			// String.
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


// Marshalls: [CNT_ALL, REL_BAR, WRT_END, EXIT].
static void marshall_payload_none (int dir, dsm_msg *mp, unsigned char *b) {
	const char *fmt = "l";
	if (dir == 0) {
		pack(b, fmt, mp->type); 
	} else {
		unpack(b, fmt, &(mp->type));
	}
}

// Marshalls: [GET_SID, SET_SID, DEL_SID].
static void marshall_payload_sid (int dir, dsm_msg *mp, unsigned char *b) {
	const char *fmt = "lsl";
	if (dir == 0) {
		pack(b, fmt, mp->type, mp->sid.sid_name, mp->sid.port);
	} else {
		unpack(b, fmt, &(mp->type), mp->sid.sid_name, &(mp->sid.port));
	}
}

// Marshalls: [ADD_PID, SET_GID, REQ_WRT, WRT_NOW].
static void marshall_payload_proc (int dir, dsm_msg *mp, unsigned char *b) {
	const char *fmt = "lll";
	if (dir == 0) {
		pack(b, fmt, mp->type, mp->proc.pid, mp->proc.gid);
	} else {
		unpack(b, fmt, &(mp->type), &(mp->proc.pid), &(mp->proc.gid));
	}
}

// Marshalls: [GOT_DATA].
static void marshall_payload_task (int dir, dsm_msg *mp, unsigned char *b) {
	const char *fmt = "ll";
	if (dir == 0) {
		pack(b, fmt, mp->type, mp->task.nproc);
	} else {
		unpack(b, fmt, &(mp->type), &(mp->task.nproc));
	}
}

// Marshalls: [WRT_DATA]. (the buf field is NOT packed).
static void marshall_payload_data (int dir, dsm_msg *mp, unsigned char *b) {
	const char *fmt = "lqq";
	if (dir == 0) {
		pack(b, fmt, mp->type, mp->data.offset, mp->data.size);
	} else {
		unpack(b, fmt, &(mp->type), &(mp->data.offset), &(mp->data.size));
	}
}

// Marshalls: [POST_SEM, WAIT_SEM].
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


// Initializes the function maps. 
void init_fmaps (void) {
	static int init = 0;

	// Only initialize once.
	if (init == 1) {
		return;
	} else {
		init = 1;
	}

	// Marshalling: No payloads.
	fmap[DSM_MSG_CNT_ALL] = fmap[DSM_MSG_REL_BAR]
		= fmap[DSM_MSG_WRT_END] = fmap[DSM_MSG_EXIT] = marshall_payload_none;

	// Marshalling: dsm_payload_sid.
	fmap[DSM_MSG_SET_SID] = fmap[DSM_MSG_GET_SID]
		= fmap[DSM_MSG_DEL_SID] = marshall_payload_sid;

	// Marshalling: dsm_payload_proc.
	fmap[DSM_MSG_ADD_PID] = fmap[DSM_MSG_SET_GID]
		= fmap[DSM_MSG_HIT_BAR] = fmap[DSM_MSG_REQ_WRT]
		= fmap[DSM_MSG_WRT_NOW] = marshall_payload_proc;

	// Marshalling: dsm_paylaod_task.
	fmap[DSM_MSG_GOT_DATA] = marshall_payload_task;

	// Marshalling: dsm_payload_data.
	fmap[DSM_MSG_WRT_DATA] = marshall_payload_data;

	// Marshalling: dsm_payload_sem.
	fmap[DSM_MSG_POST_SEM] = fmap[DSM_MSG_WAIT_SEM] = marshall_payload_sem;

}


/*
 *******************************************************************************
 *                            Function Definitions                             *
 *******************************************************************************
*/


// Marshalls message to buffer. Buffer size must be at least DSM_MSG_SIZE.
void dsm_pack_msg (dsm_msg *mp, unsigned char *b) {
	init_fmaps();
	dsm_marshall_func f;

	// Verify input.
	ASSERT_COND((mp != NULL) && (b != NULL));
	ASSERT_COND((mp->type > DSM_MSG_MIN_VAL) && (mp->type < DSM_MSG_MAX_VAL));

	// Sanitize buffer.
	memset(b, 0, DSM_MSG_SIZE);

	// Execute mapped function.
	f = fmap[mp->type];
	f(0, mp, b);
}

// Unmarshalls message from buffer. Buffer size must be at least DSM_MSG_SIZE.
void dsm_unpack_msg (dsm_msg *mp, unsigned char *b) {
	init_fmaps();
	dsm_marshall_func f;

	// Verify input.
	ASSERT_COND((mp != NULL) && (b != NULL));

	// Extract and set type.
	unpack(b, "l", &(mp->type));

	// Verify message type.
	ASSERT_COND((mp->type > DSM_MSG_MIN_VAL) && (mp->type < DSM_MSG_MAX_VAL));

	// Execute mapped function.
	f = fmap[mp->type];
	f(1, mp, b);
}


// [DEBUG] Prints message.
void dsm_show_msg (dsm_msg *mp) {
	printf("=============== MSG ===============\n");
	
	// Null Messages.
	if (mp == NULL) {
		printf("NULL\n");
		goto end;
	}

	switch (mp->type) {
		case DSM_MSG_SET_SID:
			printf("Type: DSM_MSG_SET_SID\n");
			printf("sid = \"%.*s\"\n", DSM_MSG_STR_SIZE, 
				mp->sid.sid_name);
			printf("port = %" PRId32 "\n", mp->sid.port);
			break;
		case DSM_MSG_DEL_SID:
			printf("Type: DSM_MSG_DEL_SID\n");
			printf("sid = \"%.*s\"\n", DSM_MSG_STR_SIZE,
				mp->sid.sid_name);
			printf("nproc = %" PRId32 "\n", mp->sid.nproc);
			break;
		case DSM_MSG_CNT_ALL:
			printf("Type: DSM_MSG_CNT_ALL\n");
			break;
		case DSM_MSG_REL_BAR:
			printf("Type: DSM_MSG_REL_BAR\n");
			break;
		case DSM_MSG_WRT_NOW:
			printf("Type: DSM_MSG_WRT_NOW\n");
			printf("pid = %" PRId32 "\n", mp->proc.pid);
			printf("gid = %" PRId32 "\n", mp->proc.gid);
			break;
		case DSM_MSG_SET_GID:
			printf("Type: DSM_MSG_SET_GID\n");
			printf("pid = %" PRId32 "\n", mp->proc.pid);
			printf("gid = %" PRId32 "\n", mp->proc.gid);
			break;
		case DSM_MSG_GET_SID:
			printf("Type: DSM_MSG_GET_SID\n");
			printf("sid = \"%.*s\"\n", DSM_MSG_STR_SIZE,
				mp->sid.sid_name);
			printf("nproc = %" PRId32 "\n", mp->sid.nproc);
			break;
		case DSM_MSG_GOT_DATA:
			printf("Type: DSM_MSG_GOT_DATA\n");
			printf("nproc = %" PRId32 "\n", mp->task.nproc);
			break;
		case DSM_MSG_ADD_PID:
			printf("Type: DSM_MSG_ADD_PID\n");
			printf("pid = %" PRId32 "\n", mp->proc.pid);
			break;
		case DSM_MSG_REQ_WRT:
			printf("Type: DSM_MSG_REQ_WRT\n");
			printf("pid = %" PRId32 "\n", mp->proc.pid);
			break;
		case DSM_MSG_HIT_BAR:
			printf("Type: DSM_MSG_HIT_BAR\n");
			printf("pid = %" PRId32 "\n", mp->proc.pid);
			break;
		case DSM_MSG_WRT_DATA:
			printf("Type: DSM_MSG_WRT_DATA\n");
			printf("offset = %" PRId64 "\n", mp->data.offset);
			printf("size = %" PRId64 "\n", mp->data.size);
			for (int i = 0; i < MIN(8, mp->data.size); ++i) {
				printf("0x%02x ", mp->data.buf[i]);
			}
			printf("\n");
			break;
		case DSM_MSG_WRT_END:
			printf("Type: DSM_MSG_WRT_END\n");
			break;
		case DSM_MSG_POST_SEM:
			printf("Type: DSM_MSG_POST_SEM\n");
			printf("pid = %" PRId32 "\n", mp->sem.pid);
			printf("sem_name = \"%.*s\"\n", DSM_MSG_STR_SIZE,
				mp->sem.sem_name);
			break;
		case DSM_MSG_WAIT_SEM:
			printf("Type: DSM_MSG_WAIT_SEM\n");
			printf("pid = %" PRId32 "\n", mp->sem.pid);
			printf("sem_name = \"%.*s\"\n", DSM_MSG_STR_SIZE, 
				mp->sem.sem_name);
			break;
		case DSM_MSG_EXIT:
			printf("Type: DSM_MSG_EXIT\n");
			break;
		default:
			printf("<UNKNOWN>\n");
	}

	end:
	printf("===================================\n");
}

// Assigns function to given message type. Exits on error.
void dsm_setMsgFunc (dsm_msg_t type, dsm_msg_func func, dsm_msg_func *fmap) {

	// Verify type.
	ASSERT_COND((type > DSM_MSG_MIN_VAL) && (type < DSM_MSG_MAX_VAL));

	// Install function.
	fmap[type] = func;
}

// Returns function for given message type. Exits on error.
dsm_msg_func dsm_getMsgFunc (dsm_msg_t type, dsm_msg_func *fmap) {

	// Verify type.
	ASSERT_COND((type > DSM_MSG_MIN_VAL) && (type < DSM_MSG_MAX_VAL));

	// Install function.
	return fmap[type];
}