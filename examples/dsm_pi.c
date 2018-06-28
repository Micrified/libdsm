#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "dsm/dsm.h"

/* Make then run the server with: "./bin/dsm_server 4200 4" */

// Configuration struct: dsm_arbiter.h
dsm_cfg cfg = {
    .nproc = 4,             // Total number of expected processes.
    .sid_name = "Foo",      // Session-identifier: Doesn't do anything here.
    .d_addr = "127.0.0.1",  // Daemon-address. TEMP: Used for server address.
    .d_port = "4200",       // Daemon-port. TEMP: Used for server port.
    .map_size = 4096        // Size of shared memory to reserve.
};

int main (int argc, char *argv[]) {
    int r;
    double step, chunk = 0.0;
    volatile double *sum;

    // Validate arguments.
    if (argc != 2 || sscanf(argv[1], "%lf", &step) != 1) {
        fprintf(stderr, "Usage: %s <step-size>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Each process must call dsm_init at least once. Fork before call.
    for (unsigned int i = 1; i < cfg.nproc; i++) {
        if (fork() == 0) break;
    }

    // Initialize shared memory.
    sum = (double *)dsm_init(&cfg);

    // Assign "rank".
    r = dsm_get_gid();

    // Compute portion
    for (double x = r * step; x < 1.0; x += cfg.nproc * step) {
        chunk += (4.0 / (x * x + 1.0));
    }

    // Write result: Need a semaphore weirdly enough.
    // I think it's because everyone gets suspended waiting to write,
    // and the write instruction already has the value zero cached.
    // If you remove the semaphore, they all just get: 0 + chunk.
    // Will investigate getting around this. But might not be possible.
    dsm_wait_sem("x");
    *sum += chunk;
    dsm_post_sem("x");

    // Wait for everyone to finish.
    dsm_barrier();

    // Output result if rank is zero.
    if (r == 0) {
        printf("pi = %lf\n", (*sum) * step);
    }

    // Cleanup.
    dsm_exit();

    // If rank is zero, then wait for others.
    if (r == 0) {
        for (unsigned int i = 0; i < (cfg.nproc - 1); i++) {
            waitpid(-1, NULL, 0);
        }
    }

    return 0;
}