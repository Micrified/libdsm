#if !defined(DSM_OPQUEUE_H)
#define DSM_OPQUEUE_H

#include <stdlib.h>

/*
 *******************************************************************************
 *                             Symbolic Constants                              *
 *******************************************************************************
*/


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
	int *queue;
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
int dsm_getOpQueueHead (dsm_opqueue *oq);

// Enqueues file-descriptor in operation-queue for write. Resizes if needed.
void dsm_enqueueOpQueue (int fd, dsm_opqueue *oq);

// Dequeues file-descriptor from operation queue. Panics on error.
int dsm_dequeueOpQueue (dsm_opqueue *oq);

// Prints the operation-queue.
void dsm_showOpQueue (dsm_opqueue *oq);


#endif