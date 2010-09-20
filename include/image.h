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

#ifdef HAVE_JPEG
#include <jpeglib.h>
#endif
#ifdef HAVE_GIF
#include <gif_lib.h>
#endif

#define BUFFER_SIZE 4096

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

// MSVC does not have lrintf
#ifdef _MSC_VER
static inline int lrintf(float f) {
#ifdef _M_X64
  return (int)((f > 0.0f) ? (f + 0.5f) : (f -0.5f));
#else
  int i;
  _asm {
    fld f
    fistp i
  };
  
  return i;
#endif
}
#endif

#define ROUND_FLOAT_TO_INT(x) lrintf(x)

#define EPSILON 1.0e-12
#define PI      3.14159265358979323846264338327950288419716939937510

#ifdef _MSC_VER
#define ARGUNUSED(arg) arg
#else
#define ARGUNUSED(arg) arg __attribute__((unused))
#endif

typedef uint32_t pix;

enum image_type {
  UNKNOWN = 0,
  JPEG,
  GIF,
  PNG,
  BMP
};

// Resize algorithms
enum resize_type {
  IMAGE_SCALE_TYPE_GD = 0,
  IMAGE_SCALE_TYPE_GD_FIXED,
  IMAGE_SCALE_TYPE_GM,
  IMAGE_SCALE_TYPE_GM_FIXED
};

// Exif Orientation
enum orientation {
  ORIENTATION_NORMAL = 1,
  ORIENTATION_MIRROR_HORIZ,
  ORIENTATION_180,
  ORIENTATION_MIRROR_VERT,
  ORIENTATION_MIRROR_HORIZ_270_CCW,
  ORIENTATION_90_CCW,
  ORIENTATION_MIRROR_HORIZ_90_CCW,
  ORIENTATION_270_CCW
};

// BMP Compression methods
enum bmp_compression {
  BMP_BI_RGB = 0,
  BMP_BI_RLE8,
  BMP_BI_RLE4,
  BMP_BI_BITFIELDS,
  BMP_BI_JPEG,
  BMP_BI_PNG
};

typedef struct {
  int colors[256];
} palette;

typedef struct {
  Buffer  *buf;
  SV      *path;
  PerlIO  *fh;
  SV      *sv_data;
  int32_t sv_offset;
  int32_t image_offset;
  int32_t image_length;
  int32_t type;
  int32_t width;
  int32_t height;
  int32_t width_padding;  // empty padding pixels to leave to maintain aspect
  int32_t width_inner;    // width of inner area when maintaining aspect
  int32_t height_padding;
  int32_t height_inner;
  int32_t flipped;
  int32_t bpp;
  int32_t compression;
  int32_t channels;
  int32_t has_alpha;
  int32_t orientation;
  int32_t orientation_orig;
  int32_t memory_used;
  int32_t outbuf_size;
  int32_t used; // How many times the object has been used to resize
  pix     *pixbuf; // Source image
  pix     *outbuf; // Resized image
  pix     *tmpbuf; // Temporary intermediate image
  palette *palette;
  
  // Resize options
  int32_t memory_limit;
  int32_t target_width;
  int32_t target_height;
  int32_t keep_aspect;
  int32_t resize_type;
  int32_t filter;
  int32_t bgcolor;
  
#ifdef HAVE_JPEG
  struct jpeg_decompress_struct *cinfo;
  struct jpeg_error_mgr *jpeg_error_pub;
#endif

#ifdef HAVE_PNG
  png_structp png_ptr;
  png_infop info_ptr;
#endif

#ifdef HAVE_GIF
  GifFileType *gif;
#endif
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

static inline void
put_pix_rotated(image *im, int32_t x, int32_t y, int32_t rotated_width, pix col)
{
  im->outbuf[(y * rotated_width) + x] = col;
}

int image_init(HV *self, image *im);
int image_resize(image *im);
void image_downsize_gd(image *im);
void image_downsize_gd_fixed_point(image *im);
void image_downsize_gm(image *im);
void image_alloc(image *im, int width, int height);
void image_bgcolor_fill(pix *buf, int size, int bgcolor);
void image_finish(image *im);
inline void image_get_rotated_coords(image *im, int x, int y, int *ox, int *oy);

#ifdef HAVE_JPEG
int image_jpeg_read_header(image *im);
int image_jpeg_load(image *im);
void image_jpeg_save(image *im, const char *path, int quality);
void image_jpeg_to_sv(image *im, int quality, SV *sv_buf);
void image_jpeg_finish(image *im);
#endif

int image_bmp_read_header(image *im);
int image_bmp_load(image *im);
void image_bmp_finish(image *im);

#ifdef HAVE_GIF
int image_gif_read_header(image *im);
int image_gif_load(image *im);
void image_gif_finish(image *im);
#endif

#ifdef HAVE_PNG
int image_png_read_header(image *im);
int image_png_load(image *im);
void image_png_save(image *im, const char *path);
void image_png_to_sv(image *im, SV *sv_buf);
void image_png_finish(image *im);
#endif
