#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "dsm_opqueue.h"
#include "dsm_util.h"


/*
 *******************************************************************************
 *                            Function Definitions                             *
 *******************************************************************************
*/


// Allocates and initializes an operation-queue.
dsm_opqueue *dsm_initOpQueue (size_t queueSize) {
	dsm_opqueue *oq;

	// Allocate opqueue.
	if ((oq = malloc(sizeof(dsm_opqueue))) == NULL) {
		dsm_cpanic("dsm_initOpQueue failed!", "Allocation error");
	}

	// Set queue itself.
	if ((oq->queue = malloc(queueSize * sizeof(uint64_t))) == NULL) {
		dsm_cpanic("dsm_initOpQueue failed!", "Allocation error");
	}

	// Set remaining fields.
	oq->step = STEP_READY;
	oq->queueSize = queueSize;
	oq->head = oq->tail = 0;

	return oq;
}

// Free's given operation-queue.
void dsm_freeOpQueue (dsm_opqueue *oq) {
	if (oq == NULL) {
		return;
	}
	free(oq->queue);
	free(oq);
}

// Returns true (1) if the given operation-queue is empty.
int dsm_isOpQueueEmpty (dsm_opqueue *oq) {
	return (oq->head == oq->tail);
}

// Returns tail of operation-queue. Exits fatally on error.
uint64_t dsm_getOpQueueHead (dsm_opqueue *oq) {
	if (dsm_isOpQueueEmpty(oq) == 1) {
		dsm_cpanic("dsm_getOpQueueHead", "Can't get tail of empty queue!");
	}
	return oq->queue[oq->tail];
}

// Enqueues {machine + process} in operation-queue for write.
void dsm_enqueueOpQueue (uint32_t fd, uint32_t pid, dsm_opqueue *oq) {
	size_t new_queueSize;
	unsigned int i, j;
	uint64_t *new_queue;

	// Resize queue if full.
	if ((oq->head + 1) % oq->queueSize == oq->tail) {

		// Double queue size.
		new_queueSize = 2 * oq->queueSize;

		// Allocate the new queue.
		if ((new_queue = malloc(new_queueSize * sizeof(uint64_t))) == NULL) {
			dsm_cpanic("dsm_enqueueOpQueue", "Couldn't resize queue");
		}

		// Copy over the data.
		for (i = oq->tail, j = 0; i != oq->head; 
			i = (i + 1) % oq->queueSize, j++) {
			new_queue[j] = oq->queue[i];
		}

		// Free the old queue and replace it.
		free(oq->queue);
		oq->queue = new_queue;

		// Set the size, new head and tail.
		oq->queueSize = new_queueSize;
		oq->head = j;
		oq->tail = 0;
	}

	// Compute new item.
	uint64_t long_fd = fd;
	uint64_t long_pid = pid;

	// Enroll item.
	oq->queue[oq->head] = (long_fd | (long_pid << 32));
	oq->head = (oq->head + 1) % oq->queueSize;
}

// Dequeues file-descriptor from operation queue. Panics on error.
uint64_t dsm_dequeueOpQueue (dsm_opqueue *oq) {
	int64_t val;

	// Error out if queue is empty.
	if (oq->tail == oq->head) {
		dsm_cpanic("dsm_dequeueOpQueue", "Can't dequeue from empty!");
	}

	// Extract value, move tail up, then return value.
	val = oq->queue[oq->tail];
	oq->tail = (oq->tail + 1) % oq->queueSize;
	return val;
}

// Prints the operation-queue.
void dsm_showOpQueue (dsm_opqueue *oq) {
	printf("Operation Step = %d\n", oq->step);
	printf("Operation Queue = [");
	for (unsigned i = oq->tail; i != oq->head; i = (i + 1) % oq->queueSize) {
		uint64_t v = oq->queue[i];
		printf("{%" PRIu32 ": %" PRIu32 "}", DSM_MASK_FD(v), DSM_MASK_PID(v));
		if (i < (oq->head - 1)) {
			putchar(',');
		}
	}
	printf("]\n");
}
