#if !defined(DSM_UTIL_H)
#define DSM_UTIL_H

#include <stdlib.h>
#include <sys/poll.h>
#include <semaphore.h>

/*
 *******************************************************************************
 *                             Symbolic Constants                              *
 *******************************************************************************
*/


// Max macro.
#define MAX(a,b)				((a) > (b) ? (a) : (b))

// Min macro.
#define MIN(a,b)				((a) < (b) ? (a) : (b))

// Size of system memory page.
#define DSM_PAGESIZE			sysconf(_SC_PAGESIZE)

// Loopback address.
#define DSM_LOOPBACK_ADDR		"127.0.0.1"


/*
 *******************************************************************************
 *                          I/O Function Declarations                          *
 *******************************************************************************
*/


// Exits fatally with given error message. Also outputs errno.
void dsm_panic (const char *msg);

// Exits fatally with given error message. Outputs custom errno.
void dsm_cpanic (const char *msg, const char *reason);

// Exits fatally with formatted error. Supports tokens: {%s, %d, %f, %u}.
void dsm_panicf (const char *fmt, ...);

// Outputs warning to stderr.
void dsm_warning (const char *msg);

// [DEBUG] Redirects output to named file. Returns new fd.
int dsm_setStdout (const char *filename);

// [DEBUG] Redirects output to xterm window. Returns old stdout.
int dsm_redirXterm (void);


/*
 *******************************************************************************
 *                       Semaphore Function Declarations                       *
 *******************************************************************************
*/


// Increments a semaphore. Panics on error.
void dsm_up (sem_t *sp);

// Decrements a semaphore. Panics on error.
void dsm_down (sem_t *sp);

// Returns a semaphore's value. Panics on error.
int dsm_getSemValue (sem_t *sp);

// Unlinks a named semaphore. Exits fatally on error.
void dsm_unlinkNamedSem (const char *name);


/*
 *******************************************************************************
 *                        Memory Function Declarations                         *
 *******************************************************************************
*/

// Allocates a zeroed block of memory. Exits fatally on error.
void *dsm_zalloc (size_t size);

// Sets the given protections to a memory page. Exits fatally on error.
void dsm_mprotect (void *address, size_t size, int flags);

// Allocates a page-aligned slice of memory. Exits fatally on error.
void *dsm_pageAlloc (void *address, size_t size);

// Unlinks a shared memory file. Exits fatally on error.
void dsm_unlinkSharedFile (const char *name);


/*
 *******************************************************************************
 *                        Hashing Function Declarations                        *
 *******************************************************************************
*/


// Daniel J. Bernstein's hashing function. General Purpose.
unsigned int DJBHash (const char *s, size_t length);


#endif