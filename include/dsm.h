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

/*
 * Creates a hole in the shared memory space. Returns a positive hole ID
 * on success, and -1 on error. Memory within this space will not be
 * synchronized until the hole is filled. Any access that overlaps with a 
 * active (synchronized) memory space will result in the entire access being
 * synchronized.
 * - addr: The starting address of the hole (must be in shared memory map).
 * - size: The size (in bytes) of the hole. Must be > 0.
*/
int dsm_dig_hole (void *addr, size_t size);

/*
 * Fills a hole in the shared memory space. Panics on error.
 * Filling a hole results in the hole memory space being
 * synchronized. The hole then no longer exists.
 * - id: The hole ID returned from dsm_dig_hole.
*/
void dsm_fill_hole (int id);

// Disconnects from DSM. Unmaps shared memory. Collects local process forks.
void dsm_exit (void);


#endif