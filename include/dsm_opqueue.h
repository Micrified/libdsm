#if !defined(DSM_OPQUEUE_H)
#define DSM_OPQUEUE_H

#include <stdlib.h>

/*
 *******************************************************************************
 *                             Symbolic Constants                              *
 *******************************************************************************
*/

// Extracts the file-descriptor from the operation-queue storage value.
#define DSM_MASK_FD(v)			((uint32_t)((v) & 0xFFFFFFFF))

// Extracts the pid from the operation-queue return value.
#define DSM_MASK_PID(v)			((uint32_t)((v) >> 32))

// Minimum number of queuable items.
#define DSM_MIN_OPQUEUE_SIZE	32


/*
 *******************************************************************************
 *                              Type Definitions                               *
 *******************************************************************************
*/


// Enumeration of server states.
typedef enum dsm_syncStep {
	STEP_READY = 0,					// No write request is pending. All normal.
	STEP_WAITING_STOP_ACK,			// Waiting for stop ack from all arbiters.
	STEP_WAITING_WRT_DATA,			// Waiting for write data information.
	STEP_WAITING_SYNC_ACK			// Waiting for data received acks.
} dsm_syncStep;

// Describes the current server state, and contains queued write-requests.
typedef struct dsm_opqueue {
	dsm_syncStep step;
	uint64_t *queue;
	size_t queueSize;
	unsigned int head, tail;
} dsm_opqueue;


/*
 *******************************************************************************
 *                            Function Declarations                            *
 *******************************************************************************
*/


// Allocates and initializes an operation-queue.
dsm_opqueue *dsm_initOpQueue (size_t queueSize);

// Free's given operation-queue.
void dsm_freeOpQueue (dsm_opqueue *oq);

// Returns true (1) if the given operation-queue is empty.
int dsm_isOpQueueEmpty (dsm_opqueue *oq);

// Returns head of operation-queue. Exits fatally on error.
uint64_t dsm_getOpQueueHead (dsm_opqueue *oq);

// Enqueues {machine + process} in operation-queue for write.
void dsm_enqueueOpQueue (uint32_t fd, uint32_t pid, dsm_opqueue *oq);

// Dequeues file-descriptor from operation queue. Panics on error.
uint64_t dsm_dequeueOpQueue (dsm_opqueue *oq);

// Prints the operation-queue.
void dsm_showOpQueue (dsm_opqueue *oq);


#endif