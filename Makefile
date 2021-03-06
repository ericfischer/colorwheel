CC := $(CC)
CXX := $(CXX)
CXXFLAGS := $(CXXFLAGS)
LDFLAGS := $(LDFLAGS)

CURL_CFLAGS := $(shell pkg-config --cflags libcurl)
PNG_CFLAGS := $(shell pkg-config --cflags libpng)
CURL_LIBS := $(shell pkg-config --libs libcurl)
PNG_LIBS := $(shell pkg-config --libs libpng)
JPEG_CFLAGS = -I/usr/local/Cellar/jpeg/8d/include/
JPEG_LIBS = -L/usr/local/Cellar/jpeg/8d/lib/

lms: lms.c
	$(CC) -g -Wall -O3 $(CFLAGS) $(LDFLAGS) -o lms lms.c $(CURL_CFLAGS) $(PNG_CFLAGS) $(JPEG_CFLAGS) $(CURL_LIBS) $(PNG_LIBS) $(JPEG_LIBS) -ljpeg -lm

simulate: simulate.c
	$(CC) -g -Wall -O3 $(CFLAGS) $(LDFLAGS) -o simulate simulate.c $(CURL_CFLAGS) $(PNG_CFLAGS) $(JPEG_CFLAGS) $(CURL_LIBS) $(PNG_LIBS) $(JPEG_LIBS) -ljpeg -lm

cielch: cielch.c
	$(CC) -g -Wall -O3 $(CFLAGS) $(LDFLAGS) -o cielch cielch.c $(CURL_CFLAGS) $(PNG_CFLAGS) $(JPEG_CFLAGS) $(CURL_LIBS) $(PNG_LIBS) $(JPEG_LIBS) -ljpeg -lm

onehue: onehue.c
	$(CC) -g -Wall -O3 $(CFLAGS) $(LDFLAGS) -o onehue onehue.c $(CURL_CFLAGS) $(PNG_CFLAGS) $(JPEG_CFLAGS) $(CURL_LIBS) $(PNG_LIBS) $(JPEG_LIBS) -ljpeg -lm

onechroma: onechroma.c
	$(CC) -g -Wall -O3 $(CFLAGS) $(LDFLAGS) -o onechroma onechroma.c $(CURL_CFLAGS) $(PNG_CFLAGS) $(JPEG_CFLAGS) $(CURL_LIBS) $(PNG_LIBS) $(JPEG_LIBS) -ljpeg -lm

deutchroma: deutchroma.c
	$(CC) -g -Wall -O3 $(CFLAGS) $(LDFLAGS) -o deutchroma deutchroma.c $(CURL_CFLAGS) $(PNG_CFLAGS) $(JPEG_CFLAGS) $(CURL_LIBS) $(PNG_LIBS) $(JPEG_LIBS) -ljpeg -lm

onelightness: onelightness.c
	$(CC) -g -Wall -O3 $(CFLAGS) $(LDFLAGS) -o onelightness onelightness.c $(CURL_CFLAGS) $(PNG_CFLAGS) $(JPEG_CFLAGS) $(CURL_LIBS) $(PNG_LIBS) $(JPEG_LIBS) -ljpeg -lm

adjust-chroma: adjust-chroma.c
	$(CC) -g -Wall -O3 $(CFLAGS) $(LDFLAGS) -o adjust-chroma adjust-chroma.c $(CURL_CFLAGS) $(PNG_CFLAGS) $(JPEG_CFLAGS) $(CURL_LIBS) $(PNG_LIBS) $(JPEG_LIBS) -ljpeg -lm

neutralpoint: neutralpoint.c
	$(CC) -g -Wall -O3 $(CFLAGS) $(LDFLAGS) -o neutralpoint neutralpoint.c $(CURL_CFLAGS) $(PNG_CFLAGS) $(JPEG_CFLAGS) $(CURL_LIBS) $(PNG_LIBS) $(JPEG_LIBS) -ljpeg -lm


ramp: ramp.c
	$(CC) -g -Wall -O3 $(CFLAGS) $(LDFLAGS) -o ramp ramp.c $(CURL_CFLAGS) $(PNG_CFLAGS) $(JPEG_CFLAGS) $(CURL_LIBS) $(PNG_LIBS) $(JPEG_LIBS) -ljpeg -lm

dichromat: dichromat.c
	$(CC) -g -Wall -O3 $(CFLAGS) $(LDFLAGS) -o dichromat dichromat.c $(CURL_CFLAGS) $(PNG_CFLAGS) $(JPEG_CFLAGS) $(CURL_LIBS) $(PNG_LIBS) $(JPEG_LIBS) -ljpeg -lm

deut: deut.c
	$(CC) -g -Wall -O3 $(CFLAGS) $(LDFLAGS) -o deut deut.c $(CURL_CFLAGS) $(PNG_CFLAGS) $(JPEG_CFLAGS) $(CURL_LIBS) $(PNG_LIBS) $(JPEG_LIBS) -ljpeg -lm

xyy: xyy.c
	$(CC) -g -Wall -O3 $(CFLAGS) $(LDFLAGS) -o xyy xyy.c $(CURL_CFLAGS) $(PNG_CFLAGS) $(JPEG_CFLAGS) $(CURL_LIBS) $(PNG_LIBS) $(JPEG_LIBS) -ljpeg -lm

clean:
	rm -f stitch
