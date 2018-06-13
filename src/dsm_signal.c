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


// Ignores specified signal (Except SIGKILL and SIGSTOP). 
void dsm_sigignore (int signal) {
	struct sigaction sa;

	// Set the ignore flag.
	sa.sa_handler = SIG_IGN;

	// Install action.
	if (sigaction(signal, &sa, NULL) == -1) {
		dsm_panic("Could not ignore signal!");
	}
}

// Restores default behavior for specified signal.
void dsm_sigdefault (int signal) {
	struct sigaction sa;

	// Set the default flag.
	sa.sa_handler = SIG_DFL;

	// Install action.
	if (sigaction(signal, &sa, NULL) == -1) {
		dsm_panic("Could not restore signal!");
	}
}

// Installs a handler for the given signal.
void dsm_sigaction (int signal, void (*f)(int, siginfo_t *, void *)) {
	struct sigaction sa;

	sa.sa_flags = SA_SIGINFO;		// Configure to receive additional info.
	sigemptyset(&sa.sa_mask);		// Zero mask to block no signals.
	sa.sa_sigaction = f;			// Set the signal handler.

	// Install sigaction and verify.
	if (sigaction(signal, &sa, NULL) == -1) {
		dsm_panic("Couldn't install handler!");
	}
}

// Sends signal to processes in group except caller. Signal must be ignorable.
void dsm_killpg (int signal) {

	// Ignore signal for calling process.
	dsm_sigignore(signal);

	// Send signal to process group.
	if (killpg(0, signal) == -1) {
		dsm_panic("Couldn't signal process group!");
	}

	// Restore signal for calling process.
	dsm_sigdefault(signal);
}