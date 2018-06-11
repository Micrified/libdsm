#if !defined(DSM_POLL_H)
#define DSM_POLL_H

#include <sys/poll.h>


/*
 *******************************************************************************
 *                              Type Definitions                               *
 *******************************************************************************
*/


// Describes a set of pollable file-descriptors.
typedef struct pollset {
	unsigned int fp;		// File-descriptor array pointer.
	unsigned int len;		// Length of the file-descriptor array (capacity).
	struct pollfd *fds;		// File-descriptor array.		
} pollset;


/*
 *******************************************************************************
 *                            Function Declarations                            *
 *******************************************************************************
*/


// Initializes and returns empty pollset. Exits fatally on error.
pollset *dsm_initPollSet (unsigned int len);

// Free's given pollset. 
void dsm_freePollSet (pollset *p);

// Adds or updates fd with events to pollset. Exits fatally on error.
void dsm_setPollable (int fd, short events, pollset *p);

// Removes fd from pollset. Remaing sets are shuffled down.
void dsm_removePollable (int fd, pollset *p);

// [DEBUG] Prints the pollset.
void dsm_showPollable (pollset *p);


#endif