#if !defined(DSM_H)
#define DSM_H

#include "dsm_arbiter.h"


/*
 *******************************************************************************
 *                            Function Declarations                            *
 *******************************************************************************
*/


// Initializes the shared memory system. dsm_cfg is defined in dsm_arbiter.h.
// Returns a pointer to the shared map.
void *dsm_init (dsm_cfg *cfg);

// Returns the global process identifier (GID) to the caller.
int dsm_getgid (void);

// Blocks process until all other processes are synchronized at the same point.
void dsm_barrier (void);

// Posts (up's) on the named semaphore. Semaphore is created if needed.
void dsm_post (const char *sem_name);

// Waits (down's) on the named semaphore. Semaphore is created if needed.
void dsm_wait (const char *sem_name);

// Disconnects from shared memory system. Unmaps shared memory.
void dsm_exit (void);


#endif