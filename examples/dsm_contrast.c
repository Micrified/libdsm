#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>

#include "dsm/dsm.h"
#include "image.h"

#define PAGESIZE    4096

// Compute minimum and maximum over range.
void setMinMax (int *max_p, int *min_p, int length, int *segment) {
    int min, max;

    // Initialize min and max.
    min = max = *segment;

    // Compute local min and max.
    for (int i = 0; i < length; i++) {
        max = (segment[i] > max ? segment[i] : max);
        min = (segment[i] < min ? segment[i] : min);
    }

    // Update global min and max. 
    dsm_wait_sem("u");
    *max_p = (max > *max_p ? max : *max_p);
    *min_p = (min < *min_p ? min : *min_p);
    dsm_post_sem("u");
}

// Stretch image contrast.
void stretchContrast (int low, int high, int max, int min, int length, int *segment) {
    float scale = (float)(high - low) / (max - min);

    for (int i = 0; i < length; i++) {
        segment[i] = scale * (segment[i] - min);
    }
}

int main (int argc, char *argv[]) {
    Image image;
    size_t imageSize, sharedSize = (6400 * PAGESIZE); 
    int nproc, rank;
    int *max_p, *min_p, *width_p, *height_p, *shared, *data;
    
    // Verify arguments.
    if (argc != 4 || sscanf(argv[1], "%d", &nproc) != 1) {
        fprintf(stderr, "Usage: %s <nproc> <in.pgm> <out.pgm>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Fork processes.
    for (int i = 1; i < nproc; i++) {
        if (fork() == 0) break;
    }

    // Initialize DSM and rank.
    shared = dsm_init2("contrast", nproc, sharedSize);
    rank = dsm_getgid();

    // Set shared addresses.
    max_p = shared + 0; min_p = shared + 1; width_p = shared + 2;
    height_p = shared + 3; data = shared + 4;

    // Rank 0: Read in image and assign values.
    if (rank == 0) {
        image = readImage(argv[2]);
        imageSize = image->width * image->height * sizeof(int);
        assert((imageSize + 4) <= sharedSize);
        *width_p = image->width; *height_p = image->height;
        *max_p = *min_p = image->imdata[0][0];
        memcpy(data, image->imdata[0], imageSize);
    }

    // Synchronize.
    dsm_barrier();

    // Everyone computes their own segment.
    int n = (*width_p) * (*height_p), offset = rank * (n / nproc);
    int length = (n / nproc) + (rank == (nproc - 1)) * (n % nproc);

    // Compute min and max.
    setMinMax(max_p, min_p, length, data + offset);

    // Synchronize
    dsm_barrier();

    // Perform contrast stretch.
    stretchContrast(0, 255, *max_p, *min_p, length, data + offset);

    // Synchronize.
    dsm_barrier();

    // Rank 0: Copy data back and write out image.
    if (rank == 0) {
        memcpy(image->imdata[0], shared + 4, imageSize);
        writeImage(image, argv[3]);
    }

    // Exit.
    dsm_exit();

    // Rank 0: Clean up zombies.
    if (rank == 0) {
        for (int i = 1; i < nproc; i++) {
            waitpid(-1, NULL, 0);
        }
    }
    
    return 0;
}