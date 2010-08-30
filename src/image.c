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

#include "image.h"
#include "fixed.h"

#include "bmp.c"
#ifdef HAVE_JPEG
#include "jpeg.c"
#endif
#ifdef HAVE_PNG
#include "png.c"
#endif
#ifdef HAVE_GIF
#include "gif.c"
#endif

// GD algorithm
#include "gd.c"

// Algorithms from GraphicsMagick
#include "magick.c"
#include "magick_fixed.c"

int
image_init(HV *self, image *im)
{
  unsigned char *bptr;
  char *file = NULL;
  
  if (my_hv_exists(self, "file")) {
    // Input from file
    SV *path = *(my_hv_fetch(self, "file"));
    file = SvPVX(path);
    im->fh = IoIFP(sv_2io(*(my_hv_fetch(self, "_fh"))));
    im->path = newSVsv(path);
  }
  else {
    // Input from scalar ref
    im->fh = NULL;
    im->path = newSVpv("(data)", 0);
    im->sv_data = *(my_hv_fetch(self, "data"));
    if (SvROK(im->sv_data))
      im->sv_data = SvRV(im->sv_data);
    else
      croak("data is not a scalar ref\n");
  }
  
  im->pixbuf         = NULL;
  im->outbuf         = NULL;
  im->type           = UNKNOWN;
  im->sv_offset      = 0;
  im->width          = 0;
  im->height         = 0;
  im->width_padding  = 0;
  im->width_inner    = 0;
  im->height_padding = 0;
  im->height_inner   = 0;
  im->flipped        = 0;
  im->bpp            = 0;
  im->channels       = 0;
  im->has_alpha      = 0;
  im->orientation    = ORIENTATION_NORMAL;
  im->memory_limit   = 0;
  im->target_width   = 0;
  im->target_height  = 0;
  im->keep_aspect    = 0;
  im->resize_type    = IMAGE_SCALE_TYPE_GD;
  im->filter         = 0;
  im->bgcolor        = 0;
  
#ifdef HAVE_JPEG
  im->cinfo         = NULL;
#endif
#ifdef HAVE_PNG
  im->png_ptr       = NULL;
  im->info_ptr      = NULL;
#endif
#ifdef HAVE_GIF
  im->gif           = NULL;
#endif
  
  Newz(0, im->buf, sizeof(Buffer), Buffer);
  buffer_init(im->buf, 1024);
  im->memory_used = 1024;
  
  // Determine type of file from magic bytes
  if (im->fh != NULL) {
    if ( !_check_buf(im->fh, im->buf, 8, 1024) ) {
      // Free mem in case croak is trapped
      buffer_free(im->buf);
      Safefree(im->buf);
      im->buf = NULL;
      croak("Unable to read image header for %s", file);
    }
  }
  else {
    im->sv_offset = MIN(sv_len(im->sv_data), 1024);
    buffer_append(im->buf, SvPVX(im->sv_data), im->sv_offset);
  }
  
  bptr = buffer_ptr(im->buf);
  
  switch (bptr[0]) {
    case 0xff:
      if (bptr[1] == 0xd8 && bptr[2] == 0xff) {
#ifdef HAVE_JPEG
        im->type = JPEG;
#else
        croak("Image::Scale was not built with JPEG support\n");
#endif
      }
      break;
    case 0x89:
      if (bptr[1] == 'P' && bptr[2] == 'N' && bptr[3] == 'G'
        && bptr[4] == 0x0d && bptr[5] == 0x0a && bptr[6] == 0x1a && bptr[7] == 0x0a) {
#ifdef HAVE_PNG
          im->type = PNG;
#else
          croak("Image::Scale was not built with PNG support\n");
#endif
      }
      break;
    case 'G':
      if (bptr[1] == 'I' && bptr[2] == 'F' && bptr[3] == '8'
        && (bptr[4] == '7' || bptr[4] == '9') && bptr[5] == 'a') {
#ifdef HAVE_GIF
          im->type = GIF;
#else
          croak("Image::Scale was not built with GIF support\n");
#endif
      }
      break;
    case 'B':
      if (bptr[1] == 'M') {
        im->type = BMP;
      }
      break;
  }
  
  DEBUG_TRACE("Image type: %d\n", im->type);
    
  // Read image header via type-specific function to determine dimensions 
  switch (im->type) {
#ifdef HAVE_JPEG
    case JPEG:
      image_jpeg_read_header(im, file);
      break;
#endif
#ifdef HAVE_PNG
    case PNG:
      image_png_read_header(im, file);
      break;
#endif
#ifdef HAVE_GIF
    case GIF:
      image_gif_read_header(im, file);
      break;
#endif
    case BMP:
      image_bmp_read_header(im, file);
      break;
  }
  
  DEBUG_TRACE("Image dimenensions: %d x %d, channels %d\n", im->width, im->height, im->channels);
  
  return 1;
}

void
image_alloc(image *im, int width, int height)
{
  int size = width * height * sizeof(pix);
  
  if (im->memory_limit && im->memory_limit < im->memory_used + size) {
    croak("Image::Scale memory_limit exceeded (wanted to allocate %d bytes)", im->memory_used + size);
  }
  
  DEBUG_TRACE("Allocating %d bytes for decompressed image\n", size);
  
  New(0, im->pixbuf, size, pix);
  im->memory_used += size;
}

void
image_resize(image *im)
{
  int size, alloc_size;
  
  // Load the source image into memory
  switch (im->type) {
#ifdef HAVE_JPEG
    case JPEG:
      image_jpeg_load(im);
      image_jpeg_finish(im);
      break;
#endif
#ifdef HAVE_PNG
    case PNG:
      image_png_load(im);
      image_png_finish(im);
      break;
#endif
#ifdef HAVE_GIF
    case GIF:
      image_gif_load(im);
      image_gif_finish(im);
      break;
#endif
    case BMP:
      image_bmp_load(im);
      break;
  }
  
  // Special case for equal size without resizing
  if (im->width == im->target_width && im->height == im->target_height) {
    im->outbuf = im->pixbuf;
    return;
  }
  
  // Allocate space for the resized image
  size = im->target_width * im->target_height;
  alloc_size = size * sizeof(pix);
  
  if (im->memory_limit && im->memory_limit < im->memory_used + alloc_size) {
    croak("Image::Scale memory_limit exceeded (wanted to allocate %d bytes)", im->memory_used + alloc_size);
  }
  
  DEBUG_TRACE("Allocating %d bytes for resized image of size %d x %d\n",
    alloc_size, im->target_width, im->target_height);
  New(0, im->outbuf, size, pix);
  im->memory_used += alloc_size;
  
  // Determine padding if necessary
  if (im->keep_aspect) {
    float source_ar = 1.0 * im->width / im->height;
    float dest_ar   = 1.0 * im->target_width / im->target_height;
    int i;
    
    if (source_ar >= dest_ar) {
      im->height_padding = (int)((im->target_height - (im->target_width / source_ar)) / 2);
      im->height_inner   = (int)(im->target_width / source_ar);
    }
    else {
      im->width_padding = (int)((im->target_width - (im->target_height * source_ar)) / 2);
      im->width_inner   = (int)(im->target_height * source_ar);
    }
    
    // Fill new space with the bgcolor
    if (im->bgcolor) {
      for (i = 0; i < size; i += sizeof(pix))
        memcpy( ((char *)im->outbuf) + i, &im->bgcolor, sizeof(pix) );
    }
    else {
      Zero(im->outbuf, size, pix);
    }
    
    DEBUG_TRACE("Using width padding %d, inner width %d, height padding %d, inner height %d, bgcolor %x\n",
      im->width_padding, im->width_inner, im->height_padding, im->height_inner, im->bgcolor);
  }

  // Resize
  switch (im->resize_type) {
    case IMAGE_SCALE_TYPE_GD:
      image_downsize_gd(im);
      break;
    case IMAGE_SCALE_TYPE_GD_FIXED:
      image_downsize_gd_fixed_point(im);
      break;
    case IMAGE_SCALE_TYPE_GM:
      image_downsize_gm(im);
      break;
    case IMAGE_SCALE_TYPE_GM_FIXED:
      image_downsize_gm_fixed_point(im);
      break;
    default:
      croak("Image::Scale unknown resize type %d\n", im->resize_type);
  }
  
  // If the image was rotated, swap the width/height if necessary
  if (im->orientation >= 5) {
    int tmp = im->target_height;
    im->target_height = im->target_width;
    im->target_width = tmp;
    
    DEBUG_TRACE("Image was rotated, output now %d x %d\n", im->target_width, im->target_height);
  }
  
  // After resizing we can release the source image memory
  Safefree(im->pixbuf);
  im->pixbuf = NULL;
}
    		  
void
image_finish(image *im)
{
  // Called at DESTROY-time to release all memory if needed.
  // Items here may be freed elsewhere so must check that they aren't NULL
  
  switch (im->type) {
#ifdef HAVE_JPEG
    case JPEG:
      image_jpeg_finish(im);
      break;
#endif
#ifdef HAVE_PNG
    case PNG:
      image_png_finish(im);
      break;
#endif
#ifdef HAVE_GIF
    case GIF:
      image_gif_finish(im);
      break;
#endif
    case BMP:
      // Nothing
      break;
  }
  
  if (im->buf != NULL) {
    buffer_free(im->buf);
    Safefree(im->buf);
  }
  
  if (im->pixbuf != NULL && im->pixbuf != im->outbuf) // pixbuf = outbuf if resizing to same dimensions
    Safefree(im->pixbuf);
  
  if (im->outbuf != NULL)
    Safefree(im->outbuf);
  
  DEBUG_TRACE("Freed all memory, total used: %d\n", im->memory_used);
  im->memory_used = 0;
}

inline void
image_get_rotated_coords(image *im, int x, int y, int *ox, int *oy)
{
  switch (im->orientation) {
    case ORIENTATION_MIRROR_HORIZ: // 2
      *ox = im->target_width - 1 - x;
      *oy = y;
      break;
    case ORIENTATION_180: // 3
      *ox = im->target_width - 1 - x;
      *oy = im->target_height - 1 - y;
      break;
    case ORIENTATION_MIRROR_VERT: // 4
      *ox = x;
      *oy = im->target_height - 1 - y;
      break;
    case ORIENTATION_MIRROR_HORIZ_270_CCW: // 5
      *ox = y;
      *oy = x;
      break;
    case ORIENTATION_90_CCW: // 6
      *ox = im->target_height - 1 - y;
      *oy = x;
      break;
    case ORIENTATION_MIRROR_HORIZ_90_CCW: // 7
      *ox = im->target_height - 1 - y;
      *oy = im->target_width - 1 - x;
      break;
    case ORIENTATION_270_CCW: // 8
      *ox = y;
      *oy = im->target_width - 1 - x;
      break;
    default:
      croak("Image::Scale cannot rotate, unknown orientation value: %d\n", im->orientation);
      break;
  }
}
