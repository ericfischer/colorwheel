#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <png.h>
#include <math.h>

#define WIDTH 1000
#define HEIGHT 1000

#define MID (WIDTH / 2)

void fail() {

}

int main() {
	unsigned char buf[WIDTH * HEIGHT * 4] = { 0 };
	int x, y;

	for (x = 0; x < WIDTH; x++) {
		for (y = 0; y < HEIGHT; y++) {
			double xd = x - MID;
			double yd = y - MID;
			double d = sqrt(xd * xd + yd * yd);

			if (d <= MID) {
				double h = atan2(1 - yd, xd) / (2 * M_PI);
				double sat = d / MID;
				double l = .5;

				h *= 2 * M_PI;
				// put red at the right
				h = h + (M_PI / 2 - (M_PI - 2));

				double r1 = sin(h + M_PI - 2.0) * 0.417211 * sat + l;
				double g1 = sin(h + M_PI + 1.5) * 0.158136 * sat + l;
				double b1 = sin(h + M_PI      ) * 0.455928 * sat + l;

				double midr = exp(log(r1 * 0.923166 + 0.0791025) * 1.25) * 255;
				double midg = exp(log(g1 * 0.923166 + 0.0791025) * 1.25) * 255;
				double midb = exp(log(b1 * 0.923166 + 0.0791025) * 1.25) * 255;

				buf[(y * HEIGHT + x) * 4 + 0] = midr;
				buf[(y * HEIGHT + x) * 4 + 1] = midg;
				buf[(y * HEIGHT + x) * 4 + 2] = midb;
				buf[(y * HEIGHT + x) * 4 + 3] = 255;
			}
		}
	}

	unsigned char *rows[HEIGHT];
	int i;
	for (i = 0 ; i < HEIGHT; i++) {
		rows[i] = buf + i * (4 * WIDTH);
	}

	png_structp png_ptr;
	png_infop info_ptr;

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, fail, fail, fail);
	if (png_ptr == NULL) {
		fprintf(stderr, "PNG failure (write struct)\n");
		exit(EXIT_FAILURE);
	}
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		png_destroy_write_struct(&png_ptr, NULL);
		fprintf(stderr, "PNG failure (info struct)\n");
		exit(EXIT_FAILURE);
	}

	png_set_IHDR(png_ptr, info_ptr, WIDTH, HEIGHT, 8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_set_rows(png_ptr, info_ptr, rows);
	png_init_io(png_ptr, stdout);
	png_write_png(png_ptr, info_ptr, 0, NULL);
	png_destroy_write_struct(&png_ptr, &info_ptr);
}
