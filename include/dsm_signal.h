#if !defined(DSM_SIGNAL_H)
#define DSM_SIGNAL_H

#include <signal.h>


/*
 *******************************************************************************
 *                            Function Declarations                            *
 *******************************************************************************
*/


// Installs signal handler via a struct sigaction pointer.
void dsm_sigaction_restore (int signal, struct sigaction *sa);

// Restores default behavior for specified signal. Saves old sigaction in old.
void dsm_sigdefault (int signal, struct sigaction *old);

// Installs a handler for the given signal.
void dsm_sigaction (int signal, void (*f)(int, siginfo_t *, void *),
	struct sigaction *old);


#endif