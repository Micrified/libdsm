// File: image.h
// Written by Arnold Meijster and Rob de Bruin.
// Restructured by Yannick Stoffers.
// 
// Part of the contrast stretching program. 

#ifndef IMAGE_H
#define IMAGE_H

// Data type for storing 2D greyscale image.
typedef struct imagestruct
{
	int width, height;
	int **imdata;
} *Image;

Image makeImage (int w, int h);
void freeImage (Image im);
Image readImage (char *filename);
void writeImage (Image im, char *filename);

#endif
