#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "dsm/dsm.h"

int main (int argc, char *argv[]) {
    int r, p;
    double step, chunk = 0.0;
    volatile double *sum, sum_copy;

	// Validate arguments.
	if (argc != 3 || sscanf(argv[1], "%lf", &step) != 1 ||
		sscanf(argv[2], "%d", &p) != 1) {
		fprintf(stderr, "usage: %s <step-size> <nproc>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	// Initialize shared memory system (forks diverge).
	sum = (double *)dsm_init("Pi", p, p, 4096);

    // Assign "rank".
    r = dsm_get_gid();

    // Compute portion
    for (double x = r * step; x < 1.0; x += p * step) {
        chunk += (4.0 / (x * x + 1.0));
    }

	// Add to shared sum.
    dsm_wait_sem("sum");
    *sum += chunk;
    dsm_post_sem("sum");

	// Wait for everyone to synchronize.
	dsm_barrier();

	// [Critical]: Always copy out data you need before exit.
	sum_copy = *sum;

    // Cleanup (forks converge).
    dsm_exit();

	printf("pi = %lf\n", sum_copy * step);

    return EXIT_SUCCESS;
}