#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <curl/curl.h>
#include <jpeglib.h>
#include <png.h>

void usage(char **argv) {
	fprintf(stderr, "Usage: %s [-o outfile] http://whatever/whatever.{png,jpeg}\n", argv[0]);
}

// http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
void latlon2tile(double lat, double lon, int zoom, unsigned int *x, unsigned int *y) {
	double lat_rad = lat * M_PI / 180;
	unsigned long long n = 1LL << zoom;

	*x = n * ((lon + 180) / 360);
	*y = n * (1 - (log(tan(lat_rad) + 1/cos(lat_rad)) / M_PI)) / 2;
}

// http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
void tile2latlon(unsigned int x, unsigned int y, int zoom, double *lat, double *lon) {
	unsigned long long n = 1LL << zoom;
	*lon = 360.0 * x / n - 180.0;
	double lat_rad = atan(sinh(M_PI * (1 - 2.0 * y / n)));
	*lat = lat_rad * 180 / M_PI;
}

struct data {
	char *buf;
	int len;
	int nalloc;
};

struct image {
	unsigned char *buf;
	int depth;
	int width;
	int height;
};

size_t curl_receive(char *ptr, size_t size, size_t nmemb, void *v) {
	struct data *data = v;

	if (data->len + size * nmemb >= data->nalloc) {
		data->nalloc += size * nmemb + 50000;
		data->buf = realloc(data->buf, data->nalloc);
	}

	memcpy(data->buf + data->len, ptr, size * nmemb);
	data->len += size * nmemb;

	return size * nmemb;
};

struct image *read_jpeg(char *s, int len) {
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);
	jpeg_mem_src(&cinfo, (unsigned char *) s, len);
	jpeg_read_header(&cinfo, TRUE);
	jpeg_start_decompress(&cinfo);

	int row_stride = cinfo.output_width * cinfo.output_components;
	JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray) ((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

	struct image *i = malloc(sizeof(struct image));
	i->buf = malloc(cinfo.output_width * cinfo.output_height * cinfo.output_components);
	i->width = cinfo.output_width;
	i->height = cinfo.output_height;
	i->depth = cinfo.output_components;

	unsigned char *here = i->buf;
	while (cinfo.output_scanline < cinfo.output_height) {
		jpeg_read_scanlines(&cinfo, buffer, 1);
		memcpy(here, buffer[0], row_stride);
		here += row_stride;
	}

	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);

	return i;
}

static void fail(png_structp png_ptr, png_const_charp error_msg) {
	fprintf(stderr, "PNG error %s\n", error_msg);
	exit(EXIT_FAILURE);
}

struct read_state {
	char *base;
	int off;
	int len;
};

void user_read_data(png_structp png_ptr, png_bytep data, png_size_t length) {
	struct read_state *state = png_get_io_ptr(png_ptr);

	if (state->off + length > state->len) {
		length = state->len - state->off;
	}

	memcpy(data, state->base + state->off, length);
	state->off += length;
}

struct image *read_png(char *s, int len) {
	png_structp png_ptr;
	png_infop info_ptr;

	struct read_state state;
	state.base = s;
	state.off = 0;
	state.len = len;

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, fail, fail, fail);
	if (png_ptr == NULL) {
		fprintf(stderr, "PNG init failed\n");
		exit(EXIT_FAILURE);
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		fprintf(stderr, "PNG init failed\n");
		exit(EXIT_FAILURE);
	}

	png_set_read_fn(png_ptr, &state, user_read_data);
	png_set_sig_bytes(png_ptr, 0);

	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_PACKING | PNG_TRANSFORM_EXPAND, NULL);

	png_uint_32 width, height;
	int bit_depth;
	int color_type, interlace_type;

	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_type, NULL, NULL);

	struct image *i = malloc(sizeof(struct image));
	i->width = width;
	i->height = height; 
	i->depth = png_get_channels(png_ptr, info_ptr);
	i->buf = malloc(i->width * i->height * i->depth);

	unsigned int row_bytes = png_get_rowbytes(png_ptr, info_ptr);
	png_bytepp row_pointers = png_get_rows(png_ptr, info_ptr);

	int n;
	for (n = 0; n < i->height; n++) {
		memcpy(i->buf + row_bytes * n, row_pointers[n], row_bytes);
	}

	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	return i;
}

double g(x) {
	double a               = 1.20847  ;
	double u               = -1.67808 ;
	double o               = 1.04243  ;
	double a1              = 1.73992  ;
	double u1              = 1.29054  ;
	double o1              = 0.954128 ;

	return a / (o * sqrt(2 * M_PI)) * exp(-(pow(x - u, 2)) / (2 * o * o)) + a1 / (o1 * sqrt(2 * M_PI)) * exp(-(pow(x - u1, 2)) / (2 * o1 * o1));
}

double h(x) {
	return g(x) + g(x - 2 * M_PI) + g (x + 2 * M_PI);
}

double f(x) {
	double n               = 4.18685;
	double m               = -0.678404;

	double s = n * exp(log(x) * m);

	if (s > M_PI || isnan(s)) {
		return M_PI;
	}

	return s;
}

// http://rsb.info.nih.gov/ij/plugins/download/Color_Space_Converter.java

/**
* sRGB to XYZ conversion matrix
*/
double M[3][3] = { {0.4124, 0.3576,  0.1805},
          {0.2126, 0.7152,  0.0722},
          {0.0193, 0.1192,  0.9505} };

/**
* XYZ to sRGB conversion matrix
*/
double Mi[3][3] = { {3.2406, -1.5372, -0.4986},
           {-0.9689,  1.8758,  0.0415},
           { 0.0557, -0.2040,  1.0570} };

double whitePoint[3] = { 95.0429, 100.0, 108.8900 }; /* D65 */

double fpow(double base, double e) {
	return exp(log(base) * e);
}

void RGBtoXYZ(int R, int G, int B, double *x, double *y, double *z) {
	// convert 0..255 into 0..1
	double r = R / 255.0;
	double g = G / 255.0;
	double b = B / 255.0;

	// assume sRGB
	if (r <= 0.04045) {
		r = r / 12.92;
	} else {
		r = fpow(((r + 0.055) / 1.055), 2.4);
	}
	if (g <= 0.04045) {
		g = g / 12.92;
	} else {
		g = fpow(((g + 0.055) / 1.055), 2.4);
	}
	if (b <= 0.04045) {
		b = b / 12.92;
	} else {
		b = fpow(((b + 0.055) / 1.055), 2.4);
	}

	r *= 100.0;
	g *= 100.0;
	b *= 100.0;

	// [X Y Z] = [r g b][M]
	*x = (r * M[0][0]) + (g * M[0][1]) + (b * M[0][2]);
	*y = (r * M[1][0]) + (g * M[1][1]) + (b * M[1][2]);
	*z = (r * M[2][0]) + (g * M[2][1]) + (b * M[2][2]);
}

void XYZtoLAB(double X, double Y, double Z, double *l, double *a, double *b) {
	double x = X / whitePoint[0];
	double y = Y / whitePoint[1];
	double z = Z / whitePoint[2];

	if (x > 0.008856) {
		x = fpow(x, 1.0 / 3.0);
	} else {
		x = (7.787 * x) + (16.0 / 116.0);
	}
	if (y > 0.008856) {
		y = fpow(y, 1.0 / 3.0);
	} else {
		y = (7.787 * y) + (16.0 / 116.0);
	}
	if (z > 0.008856) {
		z = fpow(z, 1.0 / 3.0);
	} else {
		z = (7.787 * z) + (16.0 / 116.0);
	}

	*l = (116.0 * y) - 16.0;
	*a = 500.0 * (x - y);
	*b = 200.0 * (y - z);
}

void LABtoLCH(double L, double A, double B, double *l, double *c, double *h) {
	double C = sqrt(A * A + B * B);
	double H = atan2(B, A);

	*l = L;
	*c = C;
	*h = H;
}

void LCHtoLAB(double L, double C, double H, double *l, double *a, double *b) {
	double A = C * cos(H);
	double B = C * sin(H);

	*l = L;
	*a = A;
	*b = B;
}

void LABtoXYZ(double L, double a, double b, double *xo, double *yo, double *zo) {
	double y = (L + 16.0) / 116.0;
	double y3 = fpow(y, 3.0);
	double x = (a / 500.0) + y;
	double x3 = fpow(x, 3.0);
	double z = y - (b / 200.0);
	double z3 = fpow(z, 3.0);

	if (y3 > 0.008856) {
		y = y3;
	} else {
		y = (y - (16.0 / 116.0)) / 7.787;
	}
	if (x3 > 0.008856) {
		x = x3;
	} else {
		x = (x - (16.0 / 116.0)) / 7.787;
	}
	if (z3 > 0.008856) {
		z = z3;
	} else {
		z = (z - (16.0 / 116.0)) / 7.787;
	}

	*xo = x * whitePoint[0];
	*yo = y * whitePoint[1];
	*zo = z * whitePoint[2];
}

int XYZtoRGB(double X, double Y, double Z, int *R, int *G, int *B) {
	double x = X / 100.0;
	double y = Y / 100.0;
	double z = Z / 100.0;

	// [r g b] = [X Y Z][Mi]
	double r = (x * Mi[0][0]) + (y * Mi[0][1]) + (z * Mi[0][2]);
	double g = (x * Mi[1][0]) + (y * Mi[1][1]) + (z * Mi[1][2]);
	double b = (x * Mi[2][0]) + (y * Mi[2][1]) + (z * Mi[2][2]);

	// assume sRGB
	if (r > 0.0031308) {
		r = ((1.055 * fpow(r, 1.0 / 2.4)) - 0.055);
	} else {
		r = (r * 12.92);
	}
	if (g > 0.0031308) {
		g = ((1.055 * fpow(g, 1.0 / 2.4)) - 0.055);
	} else {
		g = (g * 12.92);
	}
	if (b > 0.0031308) {
		b = ((1.055 * fpow(b, 1.0 / 2.4)) - 0.055);
	} else {
		b = (b * 12.92);
	}

	if (r <= 0 || g <= 0 || b <= 0) {
		return 0;
	}
	if (r > 1 || g > 1 || b > 1) {
		return 0;
	}
	if (isnan(r) || isnan(g) || isnan(b)) {
		return 0;
	}

	*R = r * 255;
	*G = g * 255;
	*B = b * 255;

	return 1;
}

void sRGBtoRGB(int ir, int ig, int ib, double *R, double *G, double *B) {
	double r = ir / 255.0;
	double g = ig / 255.0;
	double b = ib / 255.0;

	if (r <= 0.04045) {
		r = r / 12.92;
	} else {
		r = fpow(((r + 0.055) / 1.055), 2.4);
	}
	if (g <= 0.04045) {
		g = g / 12.92;
	} else {
		g = fpow(((g + 0.055) / 1.055), 2.4);
	}
	if (b <= 0.04045) {
		b = b / 12.92;
	} else {
		b = fpow(((b + 0.055) / 1.055), 2.4);
	}

	*R = r;
	*G = g;
	*B = b;
}

void RGBtosRGB(double r, double g, double b, int *ir, int *ig, int *ib) {
	if (r > 0.0031308) {
		r = ((1.055 * fpow(r, 1.0 / 2.4)) - 0.055);
	} else {
		r = (r * 12.92);
	}
	if (g > 0.0031308) {
		g = ((1.055 * fpow(g, 1.0 / 2.4)) - 0.055);
	} else {
		g = (g * 12.92);
	}
	if (b > 0.0031308) {
		b = ((1.055 * fpow(b, 1.0 / 2.4)) - 0.055);
	} else {
		b = (b * 12.92);
	}

	*ir = r * 255;
	*ig = g * 255;
	*ib = b * 255;
}

void RGBtoLMS(double r, double g, double b, double *l, double *m, double *s) {
	// 2 deg
	*l = 1.021457 * r - 0.021553 * g + 0.000095 * b;
	*m = 1.573718 * r - 0.576264 * g + 0.002546 * b;
	*s = 0.033326 * r - 0.039022 * g + 1.005695 * b;

	// 10 deg
	*l = 1.017175 * r - 0.017223 * g + 0.000048 * b;
	*m = 1.409345 * r - 0.410486 * g + 0.001141 * b;
	*s = 0.057309 * r - 0.051593 * g + 0.994284 * b;

	// stockman-stiles 1999
	*l = 2.846201 * r + 11.092490 * g + b;
	*m = 0.168926 * r +  8.265895 * g + b;
	*s = 0.000000 * r +  0.010600 * g + b;
}

void LMStoRGB(double l, double m, double s, double *r, double *g, double *b) {
	// 2 deg
	*r =  0.941669 * l + 0.058331 * m + 0.000000 * s;
	*g =  0.481193 * l + 0.517322 * m + 0.001485 * s;
	*b =  0.034807 * l + 0.053464 * m + 0.911730 * s;

	// 10 deg
	*r =  0.942032 * l + 0.057968 * m + 0.000000 * s;
	*g =  0.485478 * l + 0.513799 * m + 0.000823 * s;
	*b =  0.050188 * l + 0.077054 * m + 0.872758 * s;

	// wolfram alpha inversion of stockman-stiles 1999
	*r =  0.381762     * l - 0.512476   * m + 0.130714 * s;
	*g = -0.00781889   * l + 0.131621   * m - 0.123809 * s;
	*b =  0.0000828061 * l - 0.00139518 * m + 1.00131 * s;

	if (*r < 0) { *r = 0; }
	if (*g < 0) { *g = 0; }
	if (*b < 0) { *b = 0; }
	if (*r > 1) { *r = 1; }
	if (*g > 1) { *g = 1; }
	if (*b > 1) { *b = 1; }
}

#if 0
double hollowU(int i, int j, double *v) {
	double num = v[j];

}

double v(double j, double *u, double *v) {
	int i, n;
	double num = 0;
	double denom = 0;

	for (i = 1; i <= 3; i++) {
		num += hollowU(i, j, v) * u[i];
	}

	for (n = 1; n <= 3; n++) {
		for (i = 1; i <= 3; i++) {
			denom += hollowU(i, n, v) * u[i];
		}
	}

	return num/denom;
}
#endif


void convert(unsigned char *buf, int width, int height) {
	int x, y;
	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			int r = buf[(y * width + x) * 4 + 0];
			int g = buf[(y * width + x) * 4 + 1];
			int b = buf[(y * width + x) * 4 + 2];

			double R, G, B;
			sRGBtoRGB(r, g, b, &R, &G, &B);

			double l, m, s;
			RGBtoLMS(R, G, B, &l, &m, &s);

			LMStoRGB(l, m, s, &R, &G, &B);
			RGBtosRGB(R, G, B, &r, &g, &b);

			buf[(y * width + x) * 4 + 0] = r;
			buf[(y * width + x) * 4 + 1] = g;
			buf[(y * width + x) * 4 + 2] = b;
		}
	}
}

int main(int argc, char **argv) {
	extern int optind;
	extern char *optarg;
	int i;

	char *outfile = NULL;

	while ((i = getopt(argc, argv, "o:")) != -1) {
		switch (i) {
		case 'o':
			outfile = optarg;
			break;

		default:
			usage(argv);
			exit(EXIT_FAILURE);
		}
	}

	if (argc - optind != 1) {
		usage(argv);
		exit(EXIT_FAILURE);
	}

	if (outfile == NULL && isatty(1)) {
		fprintf(stderr, "Didn't specify -o and standard output is a terminal\n");
		exit(EXIT_FAILURE);
	}
	FILE *outfp = stdout;
	if (outfile != NULL) {
		outfp = fopen(outfile, "wb");
		if (outfp == NULL) {
			perror(outfile);
			exit(EXIT_FAILURE);
		}
	}

	int width, height;
	unsigned char *buf;

	{
		{
			{
				char *url = argv[optind];

				CURL *curl = curl_easy_init();
				if (curl == NULL) {
					fprintf(stderr, "Curl won't start\n");
					exit(EXIT_FAILURE);
				}

				struct data data;
				data.buf = NULL;
				data.len = 0;
				data.nalloc = 0;

				curl_easy_setopt(curl, CURLOPT_URL, url);
				curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_receive);

				CURLcode res = curl_easy_perform(curl);
				if (res != CURLE_OK) {
					fprintf(stderr, "Can't retrieve %s: %s\n", url,
						curl_easy_strerror(res));
					exit(EXIT_FAILURE);
				}

				struct image *i;

				if (data.len >= 4 && memcmp(data.buf, "\x89PNG", 4) == 0) {
					i = read_png(data.buf, data.len);
				} else if (data.len >= 2 && memcmp(data.buf, "\xFF\xD8", 2) == 0) {
					i = read_jpeg(data.buf, data.len);
				} else {
					fprintf(stderr, "Don't recognize file format\n");

					free(data.buf);
					curl_easy_cleanup(curl);
					exit(EXIT_FAILURE);
				}

				free(data.buf);
				curl_easy_cleanup(curl);

				width = i->width;
				height = i->height;
				buf = malloc(i->width * i->height * 4);

				int x, y;
				for (y = 0; y < i->height; y++) {
					for (x = 0; x < i->width; x++) {
						if (i->depth == 4) {
							double as = buf[((y) * width + x) * 4 + 3] / 255.0;
							double rs = buf[((y) * width + x) * 4 + 0] / 255.0 * as;
							double gs = buf[((y) * width + x) * 4 + 1] / 255.0 * as;
							double bs = buf[((y) * width + x) * 4 + 2] / 255.0 * as;

							double ad = i->buf[(y * i->width + x) * 4 + 3] / 255.0;
							double rd = i->buf[(y * i->width + x) * 4 + 0] / 255.0 * ad;
							double gd = i->buf[(y * i->width + x) * 4 + 1] / 255.0 * ad;
							double bd = i->buf[(y * i->width + x) * 4 + 2] / 255.0 * ad;

							// https://code.google.com/p/pulpcore/wiki/TutorialBlendModes
							double ar = as * (1 - ad) + ad;
							double rr = rs * (1 - ad) + rd;
							double gr = gs * (1 - ad) + gd;
							double br = bs * (1 - ad) + bd;

							buf[((y) * width + x) * 4 + 3] = ar * 255.0;
							buf[((y) * width + x) * 4 + 0] = rr / ar * 255.0;
							buf[((y) * width + x) * 4 + 1] = gr / ar * 255.0;
							buf[((y) * width + x) * 4 + 2] = br / ar * 255.0;
						} else if (i->depth == 3) {
							buf[((y) * width + x) * 4 + 0] = i->buf[(y * i->width + x) * 3 + 0];
							buf[((y) * width + x) * 4 + 1] = i->buf[(y * i->width + x) * 3 + 1];
							buf[((y) * width + x) * 4 + 2] = i->buf[(y * i->width + x) * 3 + 2];
							buf[((y) * width + x) * 4 + 3] = 255;
						} else {
							buf[((y) * width) * 4 + 0] = i->buf[(y * i->width + x) * i->depth + 0];
							buf[((y) * width) * 4 + 1] = i->buf[(y * i->width + x) * i->depth + 0];
							buf[((y) * width) * 4 + 2] = i->buf[(y * i->width + x) * i->depth + 0];
							buf[((y) * width) * 4 + 3] = 255;
						}
					}
				}

				free(i->buf);
				free(i);
			}
		}
	}

	unsigned char *rows[height];
	for (i = 0; i < height; i++) {
		rows[i] = buf + i * (4 * width);
	}

	convert(buf, width, height);

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

	png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_set_rows(png_ptr, info_ptr, rows);
	png_init_io(png_ptr, outfp);
	png_write_png(png_ptr, info_ptr, 0, NULL);
	png_destroy_write_struct(&png_ptr, &info_ptr);

	if (outfile != NULL) {
		fclose(outfp);
	}

	return 0;
}
