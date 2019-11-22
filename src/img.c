#include <stddef.h>
#include <stdio.h>
#include <jpeglib.h>
#include <png.h>
#include <stdint.h>
#include <stdlib.h>
#include <wlr/util/log.h>
#include "img.h"

static void *load_png_image(FILE *f, int *width_ptr, int *height_ptr,
		bool *has_alpha_ptr) {
	png_structp png =
		png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	png_infop info = png_create_info_struct(png);
	png_init_io(png, f);

	png_read_info(png, info);
	int width = png_get_image_width(png, info);
	int height = png_get_image_height(png, info);
	int color_type = png_get_color_type(png, info);
	int bit_depth = png_get_bit_depth(png, info);

	if (bit_depth == 16) {
		png_set_strip_16(png);
	}
	if (color_type == PNG_COLOR_TYPE_PALETTE) {
		png_set_palette_to_rgb(png);
	}
	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
		png_set_expand_gray_1_2_4_to_8(png);
	}
	if (png_get_valid(png, info, PNG_INFO_tRNS)) {
		png_set_tRNS_to_alpha(png);
	}
	if (color_type == PNG_COLOR_TYPE_RGB ||
			color_type == PNG_COLOR_TYPE_GRAY ||
			color_type == PNG_COLOR_TYPE_PALETTE) {
		png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
	}
	if (color_type == PNG_COLOR_TYPE_GRAY ||
			color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
		png_set_gray_to_rgb(png);
	}

	int row_bytes = width * 4;
	uint8_t *data = malloc(row_bytes * height);
	png_bytep *row_pointers = malloc(height * sizeof(png_bytep));
	for (int i = 0; i < height; i++) {
		row_pointers[i] = data + row_bytes * i;
	}
	png_read_image(png, row_pointers);
	free(row_pointers);

	png_destroy_read_struct(&png, &info, NULL);

	*width_ptr = width;
	*height_ptr = height;
	*has_alpha_ptr = true;
	return data;
}

static bool is_jpeg_header(uint8_t buf[static 3]) {
	return buf[0] == 0xFF && buf[1] == 0xD8 && buf[2] == 0xFF;
}

static void *load_jpeg_image(FILE *f, int *width_ptr, int *height_ptr,
		bool *has_alpha_ptr) {
	struct jpeg_error_mgr err = {0};
	struct jpeg_decompress_struct info = {
		.err = jpeg_std_error(&err),
	};
	jpeg_create_decompress(&info);

	jpeg_stdio_src(&info, f);
	jpeg_read_header(&info, TRUE);

	jpeg_start_decompress(&info);

	size_t data_size =
		info.output_width * info.output_height * info.num_components;
	uint8_t *data = malloc(data_size);
	while (info.output_scanline < info.output_height) {
		uint8_t *row = data + info.num_components * info.output_width *
			info.output_scanline;
		jpeg_read_scanlines(&info, &row, 1);
	}

	jpeg_finish_decompress(&info);

	*width_ptr = info.output_width;
	*height_ptr = info.output_width;
	*has_alpha_ptr = info.num_components == 4;
	return data;
}

void *wxrc_load_image(FILE *f, const char *mime_type,
		int *width_ptr, int *height_ptr, bool *has_alpha_ptr) {
	if (mime_type == NULL) {
		uint8_t header[8];
		if (fread(header, 1, sizeof(header), f) != sizeof(header)) {
			wlr_log(WLR_ERROR, "Failed to read image header");
			return NULL;
		}
		if (fseek(f, -sizeof(header), SEEK_CUR) != 0) {
			wlr_log_errno(WLR_ERROR, "fseek failed");
			return NULL;
		}

		if (png_sig_cmp(header, 0, sizeof(header)) == 0) {
			mime_type = "image/png";
		} else if (is_jpeg_header(header)) {
			mime_type = "image/jpeg";
		} else {
			wlr_log(WLR_ERROR, "Unsupported image format");
			return NULL;
		}
	}

	if (strcmp(mime_type, "image/png") == 0) {
		return load_png_image(f, width_ptr, height_ptr, has_alpha_ptr);
	} else if (strcmp(mime_type, "image/jpeg") == 0) {
		return load_jpeg_image(f, width_ptr, height_ptr, has_alpha_ptr);
	} else {
		wlr_log(WLR_ERROR, "Image MIME type unsupported: %s", mime_type);
		return NULL;
	}
}
