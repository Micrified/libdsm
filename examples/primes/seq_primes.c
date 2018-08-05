#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>

#define FALSE   0
#define TRUE    1

// Returns the current wall time in seconds.
double dsm_getWallTime (void) {
	struct timeval time;

	// Attempt to extract time.
	if (gettimeofday(&time, NULL) != 0) {
		fprintf(stderr, "Error: Could not get time!\n");
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
    unsigned int a, b, sum = 0;
    double t;

    // Scan input.
    printf("Enter integer numbers <a,b> such that: 1 <= a <= b: ");
    if (scanf("%u%u", &a, &b) != 2) {
        fprintf(stderr, "Error: Bad input!\n");
        exit(EXIT_FAILURE);
    }

    // Start timer.
    t = dsm_getWallTime();

    // Compute number of primes.
    for (unsigned int i = a; i < b; i++) {
        sum += isPrime(i);
    }

    // End timer.
    t = dsm_getWallTime() - t;

    // Print result.
    printf("#primes = %u (%fs)\n", sum, t);

    return 0;

}