#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>


// The message group tag for MPI send and recv.
#define MPI_GROUP_TAG          42

int main (int argc, char *argv[]) {
    int n, r;
    double step, chunk, sum = 0.0;
    MPI_Status stat;

    // Validate arguments.
    if (argc != 2 || sscanf(argv[1], "%lf", &step) != 1) {
        fprintf(stderr, "Usage: %s <step-size>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Initialize MPI, assign number of processes and rank.
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &n);
    MPI_Comm_rank(MPI_COMM_WORLD, &r);

    // Compute portion
    for (double x = r * step; x < 1.0; x += n * step) {
        sum += (4.0 / (x * x + 1.0));
    }

    // Send result and terminate: If not rank zero.
    if (r != 0) {
        MPI_Send(&sum, 1, MPI_DOUBLE, 0, MPI_GROUP_TAG, MPI_COMM_WORLD);
        goto done;
    }

    // Collect results.
    for (int i = 0; i < (n - 1); i++) {
        MPI_Recv(&chunk, 1, MPI_DOUBLE, MPI_ANY_SOURCE, MPI_GROUP_TAG, MPI_COMM_WORLD, &stat);
        sum += chunk;
    }

    // Print result.
    printf("pi = %lf\n", sum * step);

    // Cleanup.
    done:
    MPI_Finalize();

    return 0;
}