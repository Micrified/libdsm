#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "dsm_holes.h"

// Shared memory holes.
dsm_hole *g_shm_holes;


// Main test program.
int main (void) {
	int a, b, c, d, e;

	// Register 0 -> 2 as "a".
	assert((a = dsm_new_hole(0, 2, &g_shm_holes)) != -1);

	// Register 2 -> 4 as "b".
	assert((b = dsm_new_hole(2, 2, &g_shm_holes)) != -1);

	// Register 6 -> 7 as "c".
	assert((c = dsm_new_hole(6, 1, &g_shm_holes)) != -1);

	// Assert that "a" is occupied.
	assert(dsm_overlaps_hole(0, 3, g_shm_holes) != 0);

	// Assert that "c" is occupied.
	assert(dsm_overlaps_hole(5, 4, g_shm_holes) != 0);

	// Assert space 7-9 is not occupied.
	assert(dsm_overlaps_hole(7, 2, g_shm_holes) == 0);

	// Ensure registering 5-8 fails.
	assert(dsm_new_hole(5, 3, &g_shm_holes) == -1);

	// Remove "a", ensure space is clear.
	assert(dsm_del_hole(a, &g_shm_holes) != -1);
	assert(dsm_overlaps_hole(0, 2, g_shm_holes) == 0);

	// Ensure space 2-4 is completely within a hole (b).
	assert(dsm_in_hole(2, 2, 0, g_shm_holes) != 0);

	// Ensure space 1-3 is not completely within a hole.
	assert(dsm_in_hole(1, 2, 0, g_shm_holes) == 0);

	// Ensure space 3-7 is not completely within a hole.
	assert(dsm_in_hole(3, 4, 0, g_shm_holes) == 0);

	// Ensure registering 1 -> 5 fails.
	assert((d = dsm_new_hole(1, 4, &g_shm_holes)) == -1);

	// Ensure registering 3 -> 5 fails.
	assert((d = dsm_new_hole(3, 2, &g_shm_holes)) == -1);

	// Ensure registering 1 -> 3 fails.
	assert((d = dsm_new_hole(1, 2, &g_shm_holes)) == -1);

	// Ensure "b" can be removed, and space is clear.
	assert(dsm_del_hole(b, &g_shm_holes) != -1);
	assert(dsm_overlaps_hole(0, 5, g_shm_holes) == 0);
	assert(dsm_overlaps_hole(0, 6, g_shm_holes) == 0);

	// Register 0 -> 6 as "d".
	assert((d = dsm_new_hole(0, 6, &g_shm_holes)) != -1);

	// Ensure registering 2 -> 3 fails.
	assert((e = dsm_new_hole(2, 1, &g_shm_holes)) == -1);

	// Free holes.
	dsm_free_holes(g_shm_holes);

	printf("Ok!\n");

	return 0;
}
