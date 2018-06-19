#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include "dsm/dsm.h"
#include "image.h"

// Pointer to shared memory map.
int *shared_data;

// Size of shared memory map.
size_t shared_data_size;

// Computes the maximum and mimimum over an image.
void getRange (int *max_p, int *min_p, int length, int *segment) {
    int min, max;

    // Initialize min and max.
    min = max = *segment;

    // Compute min and max over segment.
    for (int i = 0; i < length; i++) {
        min = segment[i] < min ? segment[i] : min;
        max = segment[i] > max ? segment[i] : max;
    }
	printf("[%d] local max = %d, local min = %d\n", getpid(), max, min);

    // Assign max and min.
    dsm_wait_sem("range");
    *max_p = (*max_p > max) ? *max_p : max;
    *min_p = (*min_p < min) ? *min_p : min;
    dsm_post_sem("range");
    printf("[%d] Ended up with: max = %d, min = %d\n", getpid(), *max_p, *min_p);
}

// Stretches the contrast of an image.
void contrastStretch (int low, int high, int max, int min, int length, int *segment) {
    float scale = (float)(high - low) / (max - min);
	printf("[%d] Contrast Stretch (low = %d, high = %d, max = %d, min = %d) scale = %f\n", getpid(), low, high, max, min, scale);
    // Perform stretch.
    for (int i = 0; i < length; i++) {
        segment[i] = scale * (segment[i] - min);
    }
}

int main (int argc, char *argv[]) {
    Image image;
    size_t imageSize;
    int rank, nproc = 4;
    int *max_p, *min_p, *width_p, *height_p, *data;

    // Verify arguments.
    if (argc != 3) {
        fprintf(stderr, "Usage: %s input.pgm output.pgm\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Fork to create a total of nproc processes.
    for (int i = 1; i < nproc; i++) {
        if (fork() == 0) break;
    }

    // Initialize DSM, set rank.
    shared_data_size = 6400 * 4096;
    shared_data = dsm_init2("contrast", nproc, shared_data_size);
    rank = dsm_getgid();

    // Configure pointers.
    max_p = shared_data + 0;
    min_p = shared_data + 1;
    width_p = shared_data + 2;
    height_p = shared_data + 3;
    data = shared_data + 4;

    // If rank 0: Read in image and setup pointers.
    if (rank == 0) {
        image = readImage(argv[1]);
        imageSize = image->width * image->height * sizeof(int);
		printf("imageSize = %zu\n", imageSize);
        assert((imageSize + 4) <= shared_data_size);

        // Write width and height to first two bytes.
        *width_p = image->width;
        *height_p = image->height;

        // Set max_p and min_p to initial values.
        *max_p = *min_p = image->imdata[0][0];

        // Check rest of data fits, then write to shared memory.
        memcpy(data, image->imdata[0], imageSize);
    }

    // Everyone waits to start.
    dsm_barrier();

    // Everyone computes their new data segment.
    int n = (*width_p) * (*height_p);
    int offset = rank * (n / nproc);
    int length = (n / nproc) + (rank == (nproc - 1)) * (n % nproc);
    printf("[%d] I'm handling [%d -> %d)\n", getpid(), offset, offset + length);
	fflush(stdout);

    // Compute the max and min.
    getRange(max_p, min_p, length, data + offset);

	// Everyone waits for next stage.
	dsm_barrier();
     
    // Perform the contrast stretch.
    contrastStretch(0, 255, *max_p, *min_p, length, data + offset);

    // Everyone waits to end.
    dsm_barrier();

    // If rank 0: Copy data back and write out image.
    if (rank == 0) {
        memcpy(image->imdata[0], shared_data + 4, imageSize);
        writeImage(image, argv[2]);
    }

	// DSM exit.
	dsm_exit();

	// If rank 0: Clean up zombies.
	if (rank == 0) {
        for (int i = 1; i < nproc; i++) {
            waitpid(-1, NULL, 0);
        }
	}

    return 0;
}