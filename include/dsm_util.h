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

// State-assertion macro.
#define ASSERT_STATE(E)         ((E) ? (void)0 : \
    (dsm_panicf("State Violation (%s:%d): %s", __FILE__, __LINE__, #E)))

// Condition-assertion macro.
#define ASSERT_COND(E)         ((E) ? (void)0 : \
    (dsm_panicf("Condition Unmet (%s:%d): %s", __FILE__, __LINE__, #E)))

// Max macro.
#define MAX(a,b)				((a) > (b) ? (a) : (b))

// Min macro.
#define MIN(a,b)				((a) < (b) ? (a) : (b))

// Unused macro.
#define UNUSED(x)               (void)(x)

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

// Exits fatally with formatted error. Supports tokens: {%s, %d, %f, %u, %p}.
void dsm_panicf (const char *fmt, ...);

// Outputs warning to stderr.
void dsm_warning (const char *msg);

// [DEBUG] Redirects output to named file. Returns new fd.
int dsm_setStdout (const char *filename);

// [DEBUG] Redirects output to xterm window. Returns old stdout.
int dsm_redirXterm (void);


/*
 *******************************************************************************
 *                        Wrapper Function Declarations                        *
 *******************************************************************************
*/


// Performs a fork. Exits fatally on error. 
int dsm_fork (void);

// Returns the current wall time in seconds.
double dsm_getWallTime (void);


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

// Creates or opens a shared file. Sets creator flag, returns file-descriptor.
int dsm_getSharedFile (const char *name, int *is_creator);

// Sets size of shared file. Returns size on success. Panics on error.
off_t dsm_setSharedFileSize (int fd, off_t size);

// Gets size of shared file. Panics on error.
off_t dsm_getSharedFileSize (int fd);

// Maps shared file of given size to memory with protections. Panics on error.
void *dsm_mapSharedFile (int fd, size_t size, int prot);

// Unlinks a shared memory file. Exits fatally on error.
void dsm_unlinkSharedFile (const char *name);

// Returns the size of the largest change between two byte buffers.
size_t dsm_memcmp (unsigned char a[], unsigned char b[], size_t len);


/*
 *******************************************************************************
 *                        Hashing Function Declarations                        *
 *******************************************************************************
*/


// Daniel J. Bernstein's hashing function. General Purpose.
unsigned int DJBHash (const char *s, size_t length);


#endif