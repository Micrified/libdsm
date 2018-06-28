#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "dsm/dsm.h"


// Configuration struct: dsm_arbiter.h
dsm_cfg cfg = {
    .nproc = 2,             // Total number of expected processes.
    .sid_name = "Foo",      // Session-identifier: Doesn't do anything here.
    .d_addr = "127.0.0.1",  // Daemon-address.
    .d_port = "4200",		// Daemon-port.
    .map_size = 4096        // Size of shared memory to reserve.
};

int main (void) {
    int *turn;

    // Each process must call dsm_init at least once. Fork before call.
    if (fork() == -1) {
        fprintf(stderr, "Oh no! Time to reboot!\n");
        exit(EXIT_FAILURE);
    }

    // Initialize shared memory. Returns pointer to map.
    turn = (int *)dsm_init(&cfg);

    // Play ping pong.
    for (int i = 0; i < 5; i++) {
        while (*turn != dsm_get_gid());
        if (dsm_get_gid() == 0) {
            printf("Ping! ...\n");
        } else {
            printf("... Pong!\n");
        }
        *turn = (1 - *turn);
    }

    // De-initialize the shared map.
    dsm_exit();

    return 0;
}