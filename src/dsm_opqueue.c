#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
	if ((oq->queue = malloc(queueSize * sizeof(int))) == NULL) {
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
int dsm_getOpQueueHead (dsm_opqueue *oq) {
	if (dsm_isOpQueueEmpty(oq) == 1) {
		dsm_cpanic("dsm_getOpQueueHead", "Can't get tail of empty queue!");
	}
	return oq->queue[oq->tail];
}

// Enqueues file-descriptor in operation-queue for write. Resizes if needed.
void dsm_enqueueOpQueue (int fd, dsm_opqueue *oq) {
	size_t new_queueSize;
	unsigned int i, j;
	int *new_queue;

	// Resize queue if full.
	if ((oq->head + 1) % oq->queueSize == oq->tail) {

		// Double queue size.
		new_queueSize = 2 * oq->queueSize;

		// Allocate the new queue.
		if ((new_queue = malloc(new_queueSize * sizeof(int))) == NULL) {
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

	// Enroll item.
	oq->queue[oq->head] = fd;
	oq->head = (oq->head + 1) % oq->queueSize;
}

// Dequeues file-descriptor from operation queue. Panics on error.
int dsm_dequeueOpQueue (dsm_opqueue *oq) {
	int val;

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
		printf("%d", oq->queue[i]);
		if (i < (oq->head - 1)) {
			putchar(',');
		}
	}
	printf("]\n");
}
