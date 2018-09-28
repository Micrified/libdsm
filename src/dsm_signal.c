#include <unistd.h>
#include <signal.h>
#include <semaphore.h>

#include "dsm_signal.h"
#include "dsm_util.h"


/*
 *******************************************************************************
 *                            Function Definitions                             *
 *******************************************************************************
*/

// Installs signal handler via a struct sigaction pointer.
void dsm_sigaction_restore (int signal, struct sigaction *sa) {
	if (sigaction(signal, sa, NULL) == -1) {
		dsm_panic("Could not restore signal!");
	}
}

// Ignores specified signal (Except SIGKILL and SIGSTOP). 
void dsm_sigignore (int signal, struct sigaction *old) {
	struct sigaction sa = {0};

	// Set the ignore flag.
	sa.sa_handler = SIG_IGN;

	// Install action.
	if (sigaction(signal, &sa, old) == -1) {
		dsm_panic("Could not ignore signal!");
	}
}

// Restores default behavior for specified signal. Saves old sigaction in old.
void dsm_sigdefault (int signal, struct sigaction *old) {
	struct sigaction sa = {0};

	// Set the default flag.
	sa.sa_handler = SIG_DFL;

	// Install action.
	if (sigaction(signal, &sa, old) == -1) {
		dsm_panic("Could not set default signal!");
	}
}

// Installs a handler for the given signal.
void dsm_sigaction (int signal, void (*f)(int, siginfo_t *, void *),
	struct sigaction *old) {
	struct sigaction sa = {0};

	sa.sa_flags = SA_SIGINFO;		// Configure to receive additional info.
	sigemptyset(&sa.sa_mask);		// Zero mask to block no signals.
	sa.sa_sigaction = f;			// Set the signal handler.

	// Install sigaction and verify.
	if (sigaction(signal, &sa, old) == -1) {
		dsm_panic("Couldn't install handler!");
	}
}