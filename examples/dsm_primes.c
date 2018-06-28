#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <math.h>
#include <sys/wait.h>
#include <unistd.h>
#include "dsm/dsm.h"

#define FALSE   0
#define TRUE    1


// Configuration struct: dsm_arbiter.h
dsm_cfg cfg = {
    .nproc = 4,             // Total number of expected processes.
    .sid_name = "Foo",      // Session-identifier: Doesn't do anything here.
    .d_addr = "127.0.0.1",  // Daemon-address. 
    .d_port = "4200",       // Daemon-port.
    .map_size = 4096        // Size of shared memory to reserve.
};


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

int main (int argc, const char *argv[]) {
    unsigned int i, j;  // Loop iterators.
    unsigned int a, b;  // Range of primes.
    unsigned int nproc; // Number of participant processes.
    unsigned int rank;  // Rank of current process.
    unsigned int cnt;   // Number of primes in interval.
    unsigned int *sum;  // Shared variable.

    // Scan process count fromp program args.
    if (argc != 2 || sscanf(argv[1], "%d", &nproc) != 1) {
        fprintf(stderr, "Usage: %s <nproc>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Set config nproc.
    cfg.nproc = nproc;

    // Scan input.
    printf("Enter integer numbers <a,b> such that: 1 <= a <= b: ");
    if (scanf("%u%u", &a, &b) != 2) {
        fprintf(stderr, "Error: Bad input!\n");
        exit(EXIT_FAILURE);
    }

    // Fork to create nproc processes.
    for (unsigned int i = 1; i < nproc; i++) {
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

    // Print result if rank is zero.
    if (rank == 0) {
        printf("\n#primes = %u\n", *sum);
    }

    // Exit DSM.
    dsm_exit();

    // Rank 0: Clean up zombies.
    if (rank == 0) {
        for (unsigned int i = 1; i < cfg.nproc; i++) {
            waitpid(-1, NULL, 0);
        }
    }

    return EXIT_SUCCESS;
}