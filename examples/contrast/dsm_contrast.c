#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "dsm/dsm.h"
#include "dsm/dsm_util.h"
#include "image.h"


/*
 *******************************************************************************
 *                             Symbolic Constants                              *
 *******************************************************************************
*/


// Named semaphore constant.
#define S				"foobar"


/*
 *******************************************************************************
 *                                  Functions                                  *
 *******************************************************************************
*/


// Rounds given size up to next page multiple.
size_t page_rounded (size_t size) {
	size_t pagesize = getpagesize();
	size_t multiple = (size / pagesize) + 1;
	return multiple * pagesize;
}

// Computes min and max over range. Saves to given pointers.
void setMinMax (int *max_p, int *min_p, int len, int *data) {
	int min, max;

	// Set initial values.
	min = max = data[0];

	// Compute the local minimum and maximum.
	for (int i = 1; i < len; i++) {
		max = MAX(data[i], max);
		min = MIN(data[i], min);
	}

	// Update the global minimum and maximum.
	dsm_wait_sem(S);
	*max_p = MAX(*max_p, max);
	*min_p = MIN(*min_p, min);
	dsm_post_sem(S);
}

// Stretches contrast across an image data segment.
void stretchContrast (int low, int high, int max, int min, int len, int *data) {
	float scale = (float)(high - low) / (max - min);
	for (int i = 0; i < len; i++) {
		data[i] = scale * (data[i] - min);
	}
}


/*
 *******************************************************************************
 *                                    Main                                     *
 *******************************************************************************
*/


int main (int argc, char *argv[]) {
	Image image;
	size_t imageSize, shmSize;
	int p, rank, hole;
	int *max_p, *min_p, *width_p, *height_p, *shm, *imdata, *shm_imdata;
	double t;

	// Verify arguments.
	if (argc != 4 || sscanf(argv[1], "%d", &p) != 1) {
		fprintf(stderr, "usage: %s <nproc> <in-file> <out-file>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	// Load image, set imageSize, assign local data pointer.
	image = readImage(argv[2]);
	imageSize = image->width * image->height * sizeof(int);
	imdata = image->imdata[0];

	// Compute total space needed: (4 variables + the image data).
	shmSize = page_rounded(imageSize + 4 * sizeof(int));
	
	// Show startup text.
	printf("Boosting contrast for \"%s\". This may take a while ...\n", 
		argv[2]);

	// Initialize DSM + Assign rank. 
	shm = dsm_init("contrast", p, p, shmSize);
	rank = dsm_get_gid();

	// Start wall-time.
	t = dsm_getWallTime();

	// Assign the first four integer addresses to the four variables.
	max_p      = shm;
	min_p      = shm + 1;
	width_p    = shm + 2;
	height_p   = shm + 3;
	shm_imdata = shm + 4;

	// Rank 0: Make a hole, set all data, then update at once.
	if (rank == 0) {
		hole = dsm_dig_hole(shm, shmSize);
		*width_p = image->width;
		*height_p = image->height;
		*max_p = *min_p = imdata[0];
		memcpy(shm_imdata, imdata, imageSize);
		dsm_fill_hole(hole);
	}

	// Synchronize.
	dsm_barrier();

	// Each process computes work segment: Last process does leftover work.
	size_t span = (*width_p) * (*height_p), offset = rank * (span / p);
	size_t len = (span / p) + (rank == (p - 1)) * (span % p);
	size_t span_size = len * sizeof(int);

	// Compute minimum and maximum.
	setMinMax(max_p, min_p, len, shm_imdata + offset);

	// Synchronize.
	dsm_barrier();

	// All: Make a hole, apply contrast stretch, then synchronize.
	hole = dsm_dig_hole(shm_imdata + offset, span_size);
	stretchContrast(0, 255, *max_p, *min_p, len, shm_imdata + offset);
	dsm_fill_hole(hole);

	// Synchronize.
	dsm_barrier();

	// Rank 0: Copy the data back to the local version, and write it out.
	if (rank == 0) {
		memcpy(imdata, shm_imdata, imageSize);
		writeImage(image, argv[3]);
		printf("Task done by %d processes in (%.3lfs)\n", p, dsm_getWallTime() - t);
	}

	// Exit.
	dsm_exit();

	return 0;
}