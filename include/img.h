#ifndef WXRC_IMG_H
#define WXRC_IMG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/**
 * Reads an image from f. mime_type is optional. Returns a pointer to a
 * malloc'ed chunk of memory of data_size bytes. The pixels are laid out in
 * tightly-packed big-endian RGBA 8:8:8:8 or RGB 8:8:8.
 */
void *wxrc_load_image(FILE *f, const char *mime_type,
	int *width, int *height, bool *has_alpha);

#endif
