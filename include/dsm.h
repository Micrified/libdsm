#if !defined(DSM_H)
#define DSM_H

#include "dsm_arbiter.h"


/*
 *******************************************************************************
 *                            Function Declarations                            *
 *******************************************************************************
*/


/*
 * Forks local processes; initializes DSM. Returns shared memory pointer.
 * - sid:      Session identifier.
 * - lproc:    Number of local processes to fork.
 * - tproc:    Total expected process count.
 * - map_size: Desired map size. Rounded to page size multiple (minimum 1 page).
*/
void *dsm_init (const char *sid, unsigned int lproc, unsigned int tproc,
	size_t map_size);

/*
 * Forks local processes; initializes DSM. Returns shared memory pointer.
 * - cfg: Configuration structure. See dsm_arbiter.h.
*/
void *dsm_init2 (dsm_cfg *cfg);

// Returns the global process identifier (GID) to the caller.
int dsm_get_gid (void);

// Blocks process until all other processes are synchronized at the same point.
void dsm_barrier (void);

/*
 * Performs a post (up) on named semaphore. Target created if nonexistant.
 * - sem_name: Named semaphore identifier.
*/
void dsm_post_sem (const char *sem_name);

/*
 * Performs a wait (down) on named semaphore. Target created if nonexistent.
 * - sem_name: Named semaphore identifier.
*/
void dsm_wait_sem (const char *sem_name);

// Disconnects from DSM. Unmaps shared memory. Collects local process forks.
void dsm_exit (void);


#endif