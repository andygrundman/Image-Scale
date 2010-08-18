/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <jpeglib.h>
#include <gif_lib.h>

#define DEFAULT_JPEG_QUALITY 90

#define COL(red, green, blue) (((red) << 24) | ((green) << 16) | ((blue) << 8) | 0xFF)
#define COL_FULL(red, green, blue, alpha) (((red) << 24) | ((green) << 16) | ((blue) << 8) | (alpha))
#define COL_RED(col)   ((col >> 24) & 0xFF)
#define COL_GREEN(col) ((col >> 16) & 0xFF)
#define COL_BLUE(col)  ((col >> 8) & 0xFF)
#define COL_ALPHA(col) (col & 0xFF)

#ifndef MAX
#define MAX(x,y) (x) > (y) ? (x) : (y)
#endif
#ifndef MIN
#define MIN(x,y) (x) < (y) ? (x) : (y)
#endif
#ifndef ABS
#define ABS(x)  ((x) < 0 ? -(x) : (x))
#endif

#define ROUND_FLOAT_TO_INT(x) ((int)(x < 0.0 ? 0 : (x > 255.0) ? 255 : x + 0.5))

#define EPSILON 1.0e-12
#define PI      3.14159265358979323846264338327950288419716939937510

#define ARGUNUSED(arg) arg __attribute__((unused))

typedef uint32_t pix;

enum image_type {
  UNKNOWN,
  JPEG,
  GIF,
  PNG,
  BMP
};

// Resize algorithms
enum resize_type {
  IMAGE_SCALE_TYPE_GD,
  IMAGE_SCALE_TYPE_GD_FIXED,
  IMAGE_SCALE_TYPE_GM,
  IMAGE_SCALE_TYPE_GM_FIXED,
};

typedef struct {
  Buffer  *buf;
  PerlIO  *fh;
  FILE    *stdio_fp;
  int32_t type;
	int32_t width;
	int32_t height;
  int32_t width_padding;  // empty padding pixels to leave to maintain aspect
  int32_t width_inner;    // width of inner area when maintaining aspect
  int32_t height_padding;
  int32_t height_inner;
  int32_t flipped;
  int32_t bpp;
  int32_t channels;
  int32_t has_alpha;
  int32_t memory_used;
	pix     *pixbuf; // Source image
  pix     *outbuf; // Resized image
  pix     *tmpbuf; // Temporary intermediate image
	
	// Resize options
  int32_t memory_limit;
  int32_t target_width;
  int32_t target_height;
  int32_t keep_aspect;
  int32_t rotate;
  int32_t resize_type;
  int32_t filter;
	
	struct jpeg_decompress_struct *cinfo;
	
  png_structp png_ptr;
  png_infop info_ptr;
  
  GifFileType *gif;
} image;

static inline pix
get_pix(image *im, int32_t x, int32_t y)
{
	return (im->pixbuf[(y * im->width) + x]);
}

static inline void
put_pix(image *im, int32_t x, int32_t y, pix col)
{
	im->outbuf[(y * im->target_width) + x] = col;
}

int image_init(HV *self, image *im);
void image_resize(image *im);
void image_downsize_gd(image *im);
void image_downsize_gd_fixed_point(image *im);
void image_downsize_gm(image *im);
void image_alloc(image *im, int width, int height);
void image_finish(image *im);

void image_jpeg_read_header(image *im, const char *file);
void image_jpeg_load(image *im);
void image_jpeg_save(image *im, const char *path, int quality);
void image_jpeg_to_sv(image *im, int quality, SV *sv_buf);
void image_jpeg_finish(image *im);

void image_bmp_read_header(image *im, const char *file);
void image_bmp_load(image *im);

void image_gif_read_header(image *im, const char *file);
void image_gif_load(image *im);

void image_png_read_header(image *im, const char *file);
void image_png_load(image *im);
void image_png_save(image *im, const char *path);
void image_png_to_sv(image *im, SV *sv_buf);
void image_png_finish(image *im);
