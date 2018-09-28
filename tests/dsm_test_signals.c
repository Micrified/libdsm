#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include "dsm/dsm.h"

// Unused macro.
#define UNUSED(x)               (void)(x)

// Counter: Number of default signal handlers that should work once done.
int g_signal_check[3];

// Default Handler for SIGSEGV.
void handler_sigsegv (int sig, siginfo_t *si, void *ucontext) {
	UNUSED(sig); UNUSED(si); UNUSED(ucontext);
	g_signal_check[0] = 1;
}

// Default Handler for SIGILL.
void handler_sigill (int sig, siginfo_t *si, void *ucontext) {
	UNUSED(sig); UNUSED(si); UNUSED(ucontext);
	g_signal_check[1] = 1;
}

// Default Handler for SIGTSTP.
void handler_sigtstp (int sig, siginfo_t *si, void *ucontext) {
	UNUSED(sig); UNUSED(si); UNUSED(ucontext);
	g_signal_check[2] = 1;
}

// Main test program.
int main (void) {
	struct sigaction sa = {0};
	sa.sa_flags = SA_SIGINFO;
	int self = getpid();

	// Install SIGSEGV handler.
	sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = handler_sigsegv;
	sigaction(SIGSEGV, &sa, NULL);

	// Install SIGILL handler.
	sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = handler_sigill;
	sigaction(SIGILL, &sa, NULL);

	// Install SIGTSTP handler.
	sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = handler_sigtstp;
	sigaction(SIGTSTP, &sa, NULL);

	// Run a libdsm session.
	int *p = dsm_init("foo", 4, 4, 4096);
	dsm_wait_sem("foo");
	*p += 1;
	dsm_post_sem("foo");
	dsm_barrier();
	dsm_exit();

	// Fire all handlers.
	kill(self, SIGSEGV);
	kill(self, SIGILL);
	kill(self, SIGTSTP);

	// Verify all former signals have been restored.
	assert(g_signal_check[0] == 1);
	assert(g_signal_check[1] == 1);
	assert(g_signal_check[2] == 1);

	printf("Ok!\n");

	return 0;
}
