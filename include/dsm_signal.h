#if !defined(DSM_SIGNAL_H)
#define DSM_SIGNAL_H

#include <signal.h>


/*
 *******************************************************************************
 *                            Function Declarations                            *
 *******************************************************************************
*/


// Ignores specified signal (Except SIGKILL and SIGSTOP). 
void dsm_sigignore (int signal);

// Restores default behavior for specified signal.
void dsm_sigdefault (int signal);

// Installs a handler for the given signal.
void dsm_sigaction (int signal, void (*f)(int, siginfo_t *, void *));

// Sends signal to processes in group except caller. Signal must be ignorable.
void dsm_killpg (int signal);


#endif