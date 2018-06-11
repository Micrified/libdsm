#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dsm_poll.h"
#include "dsm_util.h"


/*
 *******************************************************************************
 *                            Function Definitions                             *
 *******************************************************************************
*/


// Initializes and returns empty pollset. Exits fatally on error.
pollset *dsm_initPollSet (unsigned int len) {
	pollset *p;

	// Allocate set.
	if ((p = malloc(sizeof(pollset))) == NULL) {
		dsm_cpanic("Couldn't allocate pollset!", "malloc failed");
	}

	// Allocate file-descriptor list.
	if ((p->fds = malloc(len * sizeof(struct pollfd))) == NULL) {
		dsm_cpanic("Couldn't allocate pollset!", "malloc failed");
	}

	// Setup remaining fields and return.
	p->fp = 0;
	p->len = len;

	return p;
}

// Free's given pollset. 
void dsm_freePollSet (pollset *p) {
	if (p == NULL) {
		return;
	}
	free(p->fds);
	free(p);
}

// Adds or updates fd with events to pollset. Exits fatally on error.
void dsm_setPollable (int fd, short events, pollset *p) {
	
	// Verify argument.
	if (p == NULL) {
		dsm_cpanic("dsm_setPollable failed", "NULL pointer argument");
	}

	// Check if fd exists. If so, update events.
	for (unsigned int i = 0; i < p->fp; i++) {
		if (p->fds[i].fd == fd) {
			p->fds[i].events = events;
			return;
		}
	}

	// Check if room exists. Expand set if necessary.
	if (p->fp >= p->len) {
		p->len *= 2;
		if ((p->fds = realloc(p->fds, p->len)) == NULL) {
			dsm_cpanic("Couldn't realloc pollset!", "Unknown");
		}
	}

	// Install fd.
	p->fds[p->fp++] = (struct pollfd) {
		.fd = fd,
		.events = events,
		.revents = 0
	};
}

// Removes fd from pollset. Remaing sets are shuffled down.
void dsm_removePollable (int fd, pollset *p) {
	unsigned int i, j;

	// Verify argument.
	if (p == NULL) {
		dsm_cpanic("dsm_removePollable failed", "NULL pointer argument");
	}

	// Search for target.
	for (i = 0; (i < p->fp && p->fds[i].fd != fd); i++)
		;

	// If target didn't exist, return early.
	if (i == p->fp) {
		return;
	}

	// Overwrite and shuffle.
	for (j = i; j < (p->fp - 1); j++) {
		p->fds[j] = p->fds[j + 1];
	}

	// Decrement pointer.
	p->fp--;
}

// [DEBUG] Prints the pollset.
void dsm_showPollable (pollset *p) {

	// Verify argument.
	if (p == NULL) {
		printf("Pollset = NULL\n");
		return;
	}

	// Print set.
	printf("Pollset = [");
	for (unsigned int i = 0; i < p->fp; i++) {
		printf("%d", p->fds[i].fd);
		if (i < (p->fp - 1)) {
			putchar(',');
		}
	}
	printf("]\n");
}
