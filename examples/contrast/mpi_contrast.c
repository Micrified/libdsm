#include <stdio.h>
#include <stdlib.h>
#include "image.h"

// Stretches the contrast of an image.
void contrastStretch (int low, int high, Image image) {
    int row, col, min, max;
    int width = image->width, height = image->height, **im = image->imdata;
    float scale;

    // Find minimum and maximum.
    min = max = im[0][0];

    for (row = 0; row < height; row++) {
        for (col = 0; col < width; col++) {
            min = (im[row][col] < min) ? im[row][col] : min;
            max = (im[row][col] > max) ? im[row][col] : max;
        }
    }

    // Compute scale factor.
    scale = (float)(high - low) / (max - min);
    printf("max = %d, min = %d, scale = %f\n", max, min, scale);

    // Stretch image.
    for (row = 0; row < height; row++) {
        for (col = 0; col < width; col++) {
            im[row][col] = scale * (im[row][col] - min);
        }
    }
}

int main (int argc, char *argv[]) {
    Image image;

    // Verify arguments.
    if (argc != 3) {
        fprintf(stderr, "Usage: %s input.pgm output.pgm\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Read the image in.
    image = readImage(argv[1]);

    // Perform the contrast stretch.
    contrastStretch(0, 255, image);

    // Write image out.
    writeImage(image, argv[2]);

    return 0;
}