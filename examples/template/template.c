#include <stdio.h>
#include <stdlib.h>
#include "dsm/dsm.h"

// Link source with libraries: -ldsm -lpthread -lrt -lxed


// Before running, make sure the daemon (dsm_daemon) is running!
int main (void) {
	const char *sid = "template";    // Session identifier.
	unsigned int lproc = 4;          // Number of local processes.
	unsigned int tproc = 4;          // Total number of processes.
	size_t map_size = 4096;          // Shared memory map size (bytes).
	void *shared_map_ptr;            // Shared memory map pointer.

	// Initialize the shared memory system (forks spawn and diverge).
	shared_map_ptr = dsm_init(sid, lproc, tproc, map_size);

	// Say something (do work here). 
	printf("Hello from process #%d\n", dsm_get_gid());

	// Exit the shared memory system (forks converge).
	dsm_exit();

	return EXIT_SUCCESS;
}