/**
 * Copyright (C) 2006-2018 Kentaro Fukuchi
 *
 * This library is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation; either version 2.1 of the License, or any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License along with this library; if not, write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include "image.h"
#include "libb64/include/b64/cdecode.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"
#include "xmalloc.h"
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int __get_resize_width(int oldwidth)
{
	struct winsize w;
	int i, min_w;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) {
		error("ioctl failed to get terminal's size");
		return oldwidth;
	}
	min_w = w.ws_col <= w.ws_row ? w.ws_col : w.ws_row;
	min_w *= 1.5;

	if (oldwidth <= min_w)
		return oldwidth;

	for (i = min_w - 1; i >= 1; i--) {
		// the resize ratio must be an even number
		if (oldwidth % i == 0 && (oldwidth / i) % 2 == 0)
			return i;
	}

	error("failed to find a fit size for current terminal,"
	      "using origin width:%d",
	      oldwidth);
	return oldwidth;
}

static int __resize_img_rawdata(unsigned char **dest, unsigned char *src, int oldwidth, int newwidth)
{
	unsigned char *_dest;

	_dest = stbir_resize(src, oldwidth, oldwidth, 0,
			     0, newwidth, newwidth, 0,
			     STBIR_1CHANNEL,
			     STBIR_TYPE_UINT8, STBIR_EDGE_CLAMP, STBIR_FILTER_BOX);

	if (_dest == NULL) {
		*dest = NULL;
		return -1;
	}

	*dest = _dest;
	return 0;
}

static FILE *__openfile(const char *outfile)
{
	FILE *fp;

	if (outfile == NULL || (outfile[0] == '-' && outfile[1] == '\0')) {
		fp = stdout;
	} else {
		fp = fopen(outfile, "wb");
		if (fp == NULL) {
			panic("failed to create file: %s\n", outfile);
		}
	}

	return fp;
}

static void __writeQRUTF8_margin(FILE *fp, int realwidth, int margin, const char *white,
				 const char *reset, const char *full)
{
	int x, y;

	for (y = 0; y < margin / 2; y++) {
		fputs(white, fp);
		for (x = 0; x < realwidth; x++)
			fputs(full, fp);
		fputs(reset, fp);
		fputc('\n', fp);
	}
}

/**
 * display QR code in terminal with UTF8 characters.
 * **MODIFY FROM libqrencode** and the source is in
 * https://github.com/fukuchi/libqrencode/blob/master/qrenc.c (line 847 starting)
 */
static int __writeQRUTF8(unsigned char *data, int width, int margin, const char *outfile, int use_ansi, int invert)
{
	FILE *fp;
	int x, y;
	int realwidth;
	const char *white, *reset;
	const char *empty, *lowhalf, *uphalf, *full;

	empty = " ";
	lowhalf = "\342\226\204";
	uphalf = "\342\226\200";
	full = "\342\226\210";

	if (invert) {
		const char *tmp;

		tmp = empty;
		empty = full;
		full = tmp;

		tmp = lowhalf;
		lowhalf = uphalf;
		uphalf = tmp;
	}

	if (use_ansi) {
		if (use_ansi == 2) {
			white = "\033[38;5;231m\033[48;5;16m";
		} else {
			white = "\033[40;37;1m";
		}
		reset = "\033[0m";
	} else {
		white = "";
		reset = "";
	}

	fp = __openfile(outfile);

	realwidth = (width + margin * 2);

	__writeQRUTF8_margin(fp, realwidth, margin, white, reset, full);

	for (y = 0; y < width; y += 2) {
		unsigned char *row1, *row2;
		row1 = data + y * width;
		row2 = row1 + width;

		fputs(white, fp);

		for (x = 0; x < margin; x++) {
			fputs(full, fp);
		}

		for (x = 0; x < width; x++) {
			if (row1[x] & 1) {
				if (y < width - 1 && row2[x] & 1) {
					fputs(empty, fp);
				} else {
					fputs(lowhalf, fp);
				}
			} else if (y < width - 1 && row2[x] & 1) {
				fputs(uphalf, fp);
			} else {
				fputs(full, fp);
			}
		}

		for (x = 0; x < margin; x++)
			fputs(full, fp);

		fputs(reset, fp);
		fputc('\n', fp);
	}

	__writeQRUTF8_margin(fp, realwidth, margin, white, reset, full);

	fclose(fp);
	return 0;
}

const char *b64rawdata(const char *b64data)
{
	if (b64data == NULL)
		return NULL;

	const char *p = strchr(b64data, ',');
	if (p == NULL)
		return NULL;
	return p + 1;
}

char *b64decode(const char *b64rawdata, size_t *outlen)
{
	base64_decodestate state;
	base64_init_decodestate(&state);

	size_t inlen = strlen(b64rawdata);

	size_t max_outlen = inlen / 4 * 3 + inlen % 4;
	char *output = xmalloc(max_outlen + 1);

	size_t written = base64_decode_block(b64rawdata, inlen,
					     output, &state);

	if (outlen)
		*outlen = written;
	return output;
}

int b64writeQR(const char *b64data, const char *outfile, int margin, int use_ansi, int invert, int resize)
{
	int x, y, comp, ret, width, rwidth;
	unsigned char *imgrawdata, *resizedata;
	char *imgdata;
	size_t imglen;

	imgdata = b64decode(b64rawdata(b64data), &imglen);

	imgrawdata = stbi_load_from_memory((unsigned char *)imgdata, imglen, &x, &y, &comp, 1L);

	width = x;
	assert(width == y);

	if (resize) {
		rwidth = __get_resize_width(width);
		ret = __resize_img_rawdata(&resizedata, imgrawdata, width, rwidth);
		if (ret < 0)
			mxrec_cleanup(cleanup, ret, ret);
		ret = __writeQRUTF8(resizedata, rwidth, margin, outfile, use_ansi, invert);
	} else {
		resizedata = NULL;
		ret = __writeQRUTF8(imgrawdata, width, margin, outfile, use_ansi, invert);
	}

cleanup:
	xfree(imgdata);
	stbi_image_free(imgrawdata);
	xfree(resizedata);

	return ret;
}
