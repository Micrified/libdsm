#if !defined (DSM_MSG_H)
#define DSM_MSG_H


/*
 *******************************************************************************
 *                             Symbolic Constants                              *
 *******************************************************************************
*/


// Size of (packed) control messages.
#define DSM_MSG_SIZE                 64

// Size of data payloads.
#define DSM_MSG_DATA_SIZE            1024

// Fixed size for strings in messages.
#define DSM_MSG_STR_SIZE             32


/*
 *******************************************************************************
 *                       Type Definitions: Message Types                       *
 *******************************************************************************
*/


// Message Types: (A: Arbiter; S: Server; D: Daemon; P: Process).
typedef enum {
	DSM_MSG_MIN_VAL = 0,

	DSM_MSG_SET_SID,     // [S->D]       Update session connection details.
	DSM_MSG_DEL_SID,     // [S->D]       Remove session connection details.
	DSM_MSG_CNT_ALL,     // [S->A]       Resume any waiting processes.
	DSM_MSG_REL_BAR,     // [S->A]       Resume processes waiting at barrier.
	DSM_MSG_WRT_NOW,     // [S->A->P]    Approve process write request.
	DSM_MSG_SET_GID,     // [S->A->P]    Set process global identifier.

	DSM_MSG_GET_SID,     // [A->D]       Request session connection details.
	DSM_MSG_GOT_DATA,    // [A->S]       Arbiter has received all data.

	DSM_MSG_ADD_PID,     // [P->A->S]    Process registration.
	DSM_MSG_REQ_WRT,     // [P->A->S]    Process write-request.
	DSM_MSG_HIT_BAR,     // [P->A->S]    Process blocked at barrier.
	DSM_MSG_WRT_DATA,    // [P->A->S]    Process data transmission.
	DSM_MSG_WRT_END,     // [P->A->S]    Process end of data transmission.
	DSM_MSG_POST_SEM,    // [P->A->S]    Process posts to named semaphore.
	DSM_MSG_WAIT_SEM,    // [P->A->S]    Process waits on named semaphore.
	DSM_MSG_EXIT,        // [P->A->S]    Process exiting.

	DSM_MSG_MAX_VAL
} dsm_msg_t;


/*
 *******************************************************************************
 *                     Type Definitions: Message Payloads                      *
 *******************************************************************************
*/


// For: DSM_MSG_ + [GET_SID, SET_SID, DEL_SID].
typedef struct dsm_payload_sid {
	char sid_name[DSM_MSG_STR_SIZE];
	union {
		int32_t port;
		int32_t nproc;
	};
} dsm_payload_sid;     // PACKED SIZE = 36B


// For: DSM_MSG_ + [ADD_PID, SET_GID, REQ_WRT, HIT_BAR, WRT_NOW].
typedef struct dsm_payload_proc {
	int32_t pid;
	int32_t gid;
} dsm_payload_proc;    // PACKED SIZE = 8B


// For: DSM_MSG_ + [GOT_DATA].
typedef struct dsm_payload_task {
	int32_t nproc;
} dsm_payload_task;    // PACKED SIZE = 4B


// For: DSM_MSG_ + [POST_SEM, WAIT_SEM].
typedef struct dsm_payload_sem {
	char sem_name[DSM_MSG_STR_SIZE];
	int32_t pid;
} dsm_payload_sem;    // PACKED SIZE = 36B


// For: DSM_MSG_ + [WRT_DATA].
typedef struct dsm_payload_data {
	int64_t offset;
	int64_t size;
	unsigned char *bytes; // Note: Field is NOT automatically unpacked and set.
} dsm_payload_data;    // PACKED SIZE = 1040B, 


/*
 *******************************************************************************
 *                         Type Definitions: Messages                          *
 *******************************************************************************
*/


// Type describing a control message.
typedef struct dsm_msg {
	dsm_msg_t type;
	union {
		dsm_payload_sid     sid;
		dsm_payload_proc    proc;
		dsm_payload_task    task;
		dsm_payload_sem     sem;
		dsm_payload_data    data;
	};
} dsm_msg;     // PACKED SIZE = 40B


/*
 *******************************************************************************
 *                     Type Definitions: Message Function                      *
 *******************************************************************************
*/


// Describes a message-function (function mapped to message).
typedef void (*dsm_msg_func) (int, dsm_msg *);


/*
 *******************************************************************************
 *                            Function Declarations                            *
 *******************************************************************************
*/


// Marshalls message to buffer. Buffer size must be at least DSM_MSG_SIZE.
void dsm_pack_msg (dsm_msg *mp, unsigned char *b);

// Unmarshalls message from buffer. Buffer size must be at least DSM_MSG_SIZE.
void dsm_unpack_msg (dsm_msg *mp, unsigned char *b);

// [DEBUG] Prints message.
void dsm_show_msg (dsm_msg *mp);

// Assigns function to given message type. Exits on error.
void dsm_setMsgFunc (dsm_msg_t type, dsm_msg_func func, dsm_msg_func *fmap);

// Returns function for given message type. Exits on error.
dsm_msg_func dsm_getMsgFunc (dsm_msg_t type, dsm_msg_func *fmap);


#endif