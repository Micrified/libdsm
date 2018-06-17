#if !defined(DSM_MSG_H)
#define DSM_MSG_H

/*
 *******************************************************************************
 *                             Symbolic Constants                              *
 *******************************************************************************
*/


// Fixed size of packed (transmitted) messages.
#define DSM_MSG_SIZE			64

// Fixed-width string size for messages.
#define DSM_MSG_STR_SIZE		32


/*
 *******************************************************************************
 *                          Message Type Definitions                           *
 *******************************************************************************
*/


// Message Type Enumeration: (A: Arbiter; S: Server; D: Daemon; P: Process)
typedef enum {
	DSM_MSG_MIN_VAL = 0,
 	
	DSM_MSG_SET_SID,	// [S->D] 		Update connection info for session.
	DSM_MSG_DEL_SID,	// [S->D]		Delete session for given session-id.
	DSM_MSG_STP_ALL,	// [S->A] 		Stop all processes.
	DSM_MSG_CNT_ALL,	// [S->A] 		Resume all processes.
	DSM_MSG_REL_BAR,	// [S->A] 		Release processes waiting at barrier.
	DSM_MSG_WRT_NOW,	// [S->A->P]	Perform write-operation.
	DSM_MSG_SET_GID,	// [S->A->P]	Set a process global-identifier.

	DSM_MSG_GET_SID,	// [A->D] 		Request connection info for session.
	DSM_MSG_ALL_STP,	// [A->S]		All processes stopped.
	DSM_MSG_GOT_DATA,	// [A->S]		All data is received.

	DSM_MSG_ADD_PID,	// [P->A->S] 	Register process with server.
	DSM_MSG_REQ_WRT,	// [P->A->S]	Request write access.
	DSM_MSG_HIT_BAR,	// [P->A->S] 	Process is waiting on a barrier.
	DSM_MSG_WRT_DATA,	// [P->A->S]	Written data.
	DSM_MSG_POST_SEM,	// [P->A->S]	Post on a named semaphore.
	DSM_MSG_WAIT_SEM,	// [P->A->S]	Wait on a named semaphore.
	DSM_MSG_EXIT,		// [P->A->S]	Exiting.
	
	DSM_MSG_MAX_VAL
} dsm_msg_t;


/*
 *******************************************************************************
 *                         Message Payload Definitions                         *
 *******************************************************************************
*/


// For: [DSM_MSG_GET_SID, DSM_MSG_SET_SID, DSM_MSG_DEL_SID].
typedef struct dsm_payload_sid {
	char sid_name[DSM_MSG_STR_SIZE];
	union {
		int32_t port;
		int32_t nproc;
	};
} dsm_payload_sid;		// PACKED SIZE = 36B


// For: [DSM_MSG_ADD_PID, DSM_MSG_SET_GID, DSM_MSG_REQ_WRT, DSM_MSG_HIT_BAR,
//	DSM_MSG_WRT_NOW].
typedef struct dsm_payload_proc {
	int32_t pid;
	int32_t gid;
} dsm_payload_proc;		// PACKED SIZE = 8B


// For: [DSM_MSG_ALL_STP, DSM_MSG_GOT_DATA].
typedef struct dsm_payload_task {
	int32_t nproc;
} dsm_payload_task;		// PACKED SIZE = 4B


// For: [DSM_MSG_WRT_DATA].
typedef struct dsm_payload_data {
	int32_t offset;
	int32_t size;
	unsigned char bytes[8];
} dsm_payload_data;		// PACKED SIZE = 24B


// For: [DSM_MSG_POST_SEM, DSM_MSG_WAIT_SEM].
typedef struct dsm_payload_sem {
	char sem_name[DSM_MSG_STR_SIZE];
	int32_t pid;
} dsm_payload_sem;		// PACKED SIZE = 36B


/*
 *******************************************************************************
 *                          Message Type Definitions                           *
 *******************************************************************************
*/


// Type describing a message.
typedef struct dsm_msg {
	dsm_msg_t type;
	union {
		dsm_payload_sid sid;
		dsm_payload_proc proc;
		dsm_payload_task task;
		dsm_payload_data data;
		dsm_payload_sem sem;
	};
} dsm_msg;				// MAX PACKED SIZE =  40B


/*
 *******************************************************************************
 *                        Message Function Definitions                         *
 *******************************************************************************
*/


// Type describing a message-function (function bound to message).
typedef void (*dsm_msg_func) (int, dsm_msg *);


/*
 *******************************************************************************
 *                            Function Declarations                            *
 *******************************************************************************
*/

// Marshalls given message to buffer. Buffer must be at least DSM_MSG_SIZE.
void dsm_pack_msg (dsm_msg *mp, unsigned char *b);

// Unmarshalls message of type to buffer. Buffer must be at least DSM_MSG_SIZE.
void dsm_unpack_msg (dsm_msg *mp, unsigned char *b);

// [DEBUG] Prints the contents of a message.
void dsm_showMsg (dsm_msg *mp);

// Sets function for given message type in map. Exits on error.
void dsm_setMsgFunc (dsm_msg_t type, dsm_msg_func func, dsm_msg_func *fmap);

// Returns function pointer for given message type. Exits on error.
dsm_msg_func dsm_getMsgFunc (dsm_msg_t type, dsm_msg_func *fmap);

#endif
 