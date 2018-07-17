#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <math.h>
#include <sys/wait.h>
#include <unistd.h>
#include "dsm/dsm.h"
#include "dsm/dsm_util.h"

#define FALSE   0
#define TRUE    1


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
	unsigned int n = 0;		// Maximum number to check if prime.
	unsigned int p = 0;		// Number of processes.
	unsigned int r = 0; 	// Number of rounds.
	unsigned int c = 0;		// Number of locally computed primes.
	unsigned int *sum;		// Shared sum.
	unsigned int rank = -1;	// Process rank.
    double t;               // Elapsed execution time.
    unsigned int is_child;  // Is process child or not?

	// Get process count, rounds from argument vector.
	if (argc != 4 || sscanf(argv[1], "%u", &p) != 1 || 
		sscanf(argv[2], "%u", &n) != 1 || sscanf(argv[3], "%u", &r) != 1) {
		fprintf(stderr, "Usage: %s <nproc> <max> <nrounds>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

    // Run several sessions in sequence.
    while (r--) {

        // Reset count to zero.
        c = 0;

        // Fork to create "p" total processes.
        for (unsigned int i = 1; i < p; i++) {
            if ((is_child = fork()) == 0) break;
        }

        // Start time.
        t = dsm_getWallTime();

        // Initialize DSM.
        sum = (unsigned int *)dsm_init2("primes", p, getpagesize());

        // Set rank.
        rank = (unsigned int)dsm_get_gid();

        // Compute number of primes. j tracks who does what interval.
        for (unsigned int i = 2 * rank + 1; i <= n; i += 2 * p) {
            c += isPrime(i);
        }

        // Add to the shared sum. Requires semaphore.
        dsm_wait_sem("sum_guard"); *sum += c; dsm_post_sem("sum_guard");

        // Wait for everyone to compute.
        dsm_barrier();

        // Stop wall time.
        t = dsm_getWallTime() - t;

        // Output result if rank is zero.
        if (rank == 0) {
            printf("#primes in [0,%u] = %u (%.3lfs)\n", n, *sum + 1, t); 
        }

        // Exit DSM.
        dsm_exit();

        // Parent: Clean up zombies.
        if (is_child != 0) {
            for (unsigned int i = 1; i < p; i++) {
                waitpid(-1, NULL, 0);
            }
        } else {
            break;
        }

    }

    return EXIT_SUCCESS;
}