#if !defined(DSM_SYNC_H)
#define DSM_SYNC_H

#include <signal.h>
#include "dsm_holes.h"


/*
 *******************************************************************************
 *                              Global Variables                               *
 *******************************************************************************
*/


// Pointer to the shared (and protected) memory map.
extern void *g_shared_map;

// Size of the shared map.
extern off_t g_map_size;

// Pointer to shared memory holes list.
extern dsm_hole *g_shm_holes;

// Communication socket.
extern int g_sock_io;


/*
 *******************************************************************************
 *                            Function Declarations                            *
 *******************************************************************************
*/


// Initializes the decoder tables necessary for use in the sync handlers.
void dsm_sync_init (void);

// Handler: Synchronization action for SIGSEGV.
void dsm_sync_sigsegv (int signal, siginfo_t *info, void *ucontext);

// Handler: Synchronization action for SIGILL.
void dsm_sync_sigill (int signal, siginfo_t *info, void *ucontext);

// [DEBUG] Handler: Synchronization action for SIGCONT.
void dsm_sync_sigcont (int signal, siginfo_t *info, void *ucontext);

// [DEBUG] Handler: Synchronization action for SIGTSTP.
void dsm_sync_sigtstp (int signal, siginfo_t *info, void *ucontext);

#endif