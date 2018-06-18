#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <unistd.h>
#include "dsm/dsm.h"

#define FALSE   0
#define TRUE    1


// Configuration struct: dsm_arbiter.h
dsm_cfg cfg = {
    .nproc = 8,             // Total number of expected processes.
    .sid_name = "Foo",      // Session-identifier: Doesn't do anything here.
    .d_addr = "127.0.0.1",  // Daemon-address. 
    .d_port = "4200",       // Daemon-port.
    .map_size = 4096        // Size of shared memory to reserve.
};


// Returns the wall-time.
static double get_wall_time (void) {
    struct timeval time;

    if (gettimeofday(&time, NULL) != 0) {
        fprintf(stderr, "Error: Couldn't get wall time!\n");
        exit(EXIT_FAILURE);
    }
    return (double)time.tv_sec + (double)time.tv_usec * 0.000001;
}

// Returns zero if the natural number is not prime. Otherwise returns one.
static int isPrime (unsigned int p) {
    int i, root;

    if (p == 1) return FALSE;
    if (p == 2) return TRUE;
    if ((p % 2)  == 0) return FALSE;

    root = (int)(1.0 + sqrt(p));

    for (i = 3; (i < root) && (p % i != 0); i += 2);
    return i < root ? FALSE : TRUE;
}

int main (void) {
    unsigned int i, j;  // Loop iterators.
    unsigned int a, b;  // Range of primes.
    unsigned int nproc; // Number of participant processes.
    unsigned int rank;  // Rank of current process.
    unsigned int cnt;   // Number of primes in interval.
    double t = 0;       // Time to execute.
    unsigned int *sum;  // Shared variable.

    // Set nproc.
    nproc = cfg.nproc;

    // Scan input.
    printf("Enter integer numbers <a,b> such that: 1 <= a <= b: ");
    if (scanf("%u%u", &a, &b) != 2) {
        fprintf(stderr, "Error: Bad input!\n");
        exit(EXIT_FAILURE);
    }

    // Setup timer.
    t = get_wall_time();

    // Fork to create nproc processes.
    for (unsigned int i = 1; i < cfg.nproc; i++) {
        if (fork() == 0) break;
    }

    // Initialize DSM.
    sum = (unsigned int *)dsm_init(&cfg);

    // Set rank.
    rank = dsm_getgid();

    if (a <= 2) {
        cnt = 1 * (rank == 0); // Only add once!
        a = 3;
    }

    if (a % 2 == 0) {
        a++;
    }

    // Compute number of primes. j tracks who does what interval.
    for (i = a, j = 0; i <= b; i += 2, j++) {
        if ((j % nproc) == rank) {
            cnt += isPrime(i);
        }
    }

    // Perform a reduce. Requires synchronization semaphore for now.
    dsm_wait_sem("x");
    *sum += cnt;
    dsm_post_sem("x");

    // Wait for all processes to catch up.
    dsm_barrier();

    // Stop timer.
    t = get_wall_time() - t;

    // Print result if rank is zero.
    if (rank == 0) {
        printf("\n#primes = %u (in %fs)\n", *sum, t);
    }

    // Exit DSM.
    dsm_exit();

    return EXIT_SUCCESS;
}