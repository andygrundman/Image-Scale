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
  int ret = 1;
  
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
  
  im->pixbuf           = NULL;
  im->outbuf           = NULL;
  im->outbuf_size      = 0;
  im->type             = UNKNOWN;
  im->sv_offset        = 0;
  im->image_offset     = 0;
  im->image_length     = 0;
  im->width            = 0;
  im->height           = 0;
  im->width_padding    = 0;
  im->width_inner      = 0;
  im->height_padding   = 0;
  im->height_inner     = 0;
  im->flipped          = 0;
  im->bpp              = 0;
  im->channels         = 0;
  im->has_alpha        = 0;
  im->orientation      = ORIENTATION_NORMAL;
  im->orientation_orig = ORIENTATION_NORMAL;
  im->memory_limit     = 0;
  im->target_width     = 0;
  im->target_height    = 0;
  im->keep_aspect      = 0;
  im->resize_type      = IMAGE_SCALE_TYPE_GD_FIXED;
  im->filter           = 0;
  im->bgcolor          = 0;
  im->used             = 0;
  im->palette          = NULL;
  
#ifdef HAVE_JPEG
  im->cinfo            = NULL;
#endif
#ifdef HAVE_PNG
  im->png_ptr          = NULL;
  im->info_ptr         = NULL;
#endif
#ifdef HAVE_GIF
  im->gif              = NULL;
#endif

  // Read new() options
  if (my_hv_exists(self, "offset")) {
    im->image_offset = SvIV(*(my_hv_fetch(self, "offset")));
    if (im->fh != NULL)
      PerlIO_seek(im->fh, im->image_offset, SEEK_SET);
  }
  
  if (my_hv_exists(self, "length"))
    im->image_length = SvIV(*(my_hv_fetch(self, "length")));
  
  Newz(0, im->buf, sizeof(Buffer), Buffer);
  buffer_init(im->buf, BUFFER_SIZE);
  im->memory_used = BUFFER_SIZE;
  
  // Determine type of file from magic bytes
  if (im->fh != NULL) {
    if ( !_check_buf(im->fh, im->buf, 8, BUFFER_SIZE) ) {
      image_finish(im);
      croak("Unable to read image header for %s\n", file);
    }
  }
  else {
    im->sv_offset = MIN(sv_len(im->sv_data) - im->image_offset, BUFFER_SIZE);
    buffer_append(im->buf, SvPVX(im->sv_data) + im->image_offset, im->sv_offset);
  }
  
  bptr = buffer_ptr(im->buf);
  
  switch (bptr[0]) {
    case 0xff:
      if (bptr[1] == 0xd8 && bptr[2] == 0xff) {
#ifdef HAVE_JPEG
        im->type = JPEG;
#else
        image_finish(im);
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
          image_finish(im);
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
          image_finish(im);
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
      if ( !image_jpeg_read_header(im) ) {
        ret = 0;
        goto out;
      }
      break;
#endif
#ifdef HAVE_PNG
    case PNG:
      if ( !image_png_read_header(im) ) {
        ret = 0;
        goto out;
      }
      break;
#endif
#ifdef HAVE_GIF
    case GIF:
      if ( !image_gif_read_header(im) ) {
        ret = 0;
        goto out;
      }
      break;
#endif
    case BMP:
      image_bmp_read_header(im);
      break;
    case UNKNOWN:
      warn("Image::Scale unknown file type (%s), first 8 bytes were: %02x %02x %02x %02x %02x %02x %02x %02x\n",
        SvPVX(im->path), bptr[0], bptr[1], bptr[2], bptr[3], bptr[4], bptr[5], bptr[6], bptr[7]);
      ret = 0;
      break;
  }
  
  DEBUG_TRACE("Image dimenensions: %d x %d, channels %d\n", im->width, im->height, im->channels);
  
out:
  if (ret == 0)
    image_finish(im);
  
  return ret;
}

void
image_alloc(image *im, int width, int height)
{
  int size = width * height * sizeof(pix);
  
  if (im->memory_limit && im->memory_limit < im->memory_used + size) {
    image_finish(im);
    croak("Image::Scale memory_limit exceeded (wanted to allocate %d bytes)\n", im->memory_used + size);
  }
  
  DEBUG_TRACE("Allocating %d bytes for decompressed image\n", size);
  
  New(0, im->pixbuf, size, pix);
  im->memory_used += size;
}

void
image_bgcolor_fill(pix *buf, int size, int bgcolor)
{
  int alloc_size = size * sizeof(pix);
  int i;
  
  if (bgcolor != 0) {
    for (i = 0; i < alloc_size; i += sizeof(pix))
      memcpy( ((char *)buf) + i, &bgcolor, sizeof(pix) );
  }
  else {
    Zero(buf, size, pix);
  }
}

int
image_resize(image *im)
{
  int size;
  int ret = 1;
  
  // Check if we have already resized an image with this object,
  // if so, clear everything we've already done
  if (im->used) {
    DEBUG_TRACE("Object already used for a resize, resetting\n");
    if (im->outbuf != NULL) {
      Safefree(im->outbuf);
      im->outbuf = NULL;
      im->memory_used -= im->outbuf_size;
    }
    
#ifdef HAVE_JPEG
    // For a JPEG we have to reset the scaled size in case we're resizing larger than before
    if (im->type == JPEG) {
      im->width = im->cinfo->image_width;
      im->height = im->cinfo->image_height;
      
      DEBUG_TRACE("JPEG dimensions set back to original %d x %d\n", im->width, im->height);
    }
#endif
  }
  
  // Load the source image into memory
  switch (im->type) {
#ifdef HAVE_JPEG
    case JPEG:
      if ( !image_jpeg_load(im) ) {
        ret = 0;
        goto out;
      }
      break;
#endif
#ifdef HAVE_PNG
    case PNG:
      if ( !image_png_load(im) ) {
        ret = 0;
        goto out;
      }
      break;
#endif
#ifdef HAVE_GIF
    case GIF:
      if ( !image_gif_load(im) ) {
        ret = 0;
        goto out;
      }
      break;
#endif
    case BMP:
      if ( !image_bmp_load(im) ) {
        ret = 0;
        goto out;
      }
      break;
  }
  
  // Special case for equal size without resizing
  if (im->width == im->target_width && im->height == im->target_height) {
    im->outbuf = im->pixbuf;
    goto out;
  }
  
  // Allocate space for the resized image
  size = im->target_width * im->target_height;
  im->outbuf_size = size * sizeof(pix);
  
  if (im->memory_limit && im->memory_limit < im->memory_used + im->outbuf_size) {
    image_finish(im);
    croak("Image::Scale memory_limit exceeded (wanted to allocate %d bytes)\n", im->memory_used + im->outbuf_size);
  }
  
  DEBUG_TRACE("Allocating %d bytes for resized image of size %d x %d\n",
    im->outbuf_size, im->target_width, im->target_height);
  New(0, im->outbuf, size, pix);
  im->memory_used += im->outbuf_size;
  
  // Determine padding if necessary
  if (im->keep_aspect) {
    float source_ar = 1.0 * im->width / im->height;
    float dest_ar   = 1.0 * im->target_width / im->target_height;
    
    if (source_ar >= dest_ar) {
      im->height_padding = (int)((im->target_height - (im->target_width / source_ar)) / 2);
      im->height_inner   = (int)(im->target_width / source_ar);
      if (im->height_inner < 1) // Avoid divide by 0
        im->height_inner = 1;
    }
    else {
      im->width_padding = (int)((im->target_width - (im->target_height * source_ar)) / 2);
      im->width_inner   = (int)(im->target_height * source_ar);
      if (im->width_inner < 1) // Avoid divide by 0
        im->width_inner = 1;
    }
    
    // Fill new space with the bgcolor or zeros
    image_bgcolor_fill(im->outbuf, size, im->bgcolor);
    
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
      image_finish(im);
      croak("Image::Scale unknown resize type %d\n", im->resize_type);
  }
  
  // If the image was rotated, swap the width/height if necessary
  // This is needed for the save_*() functions to output the correct size
  if (im->orientation >= 5) {
    int tmp = im->target_height;
    im->target_height = im->target_width;
    im->target_width = tmp;
    
    DEBUG_TRACE("Image was rotated, output now %d x %d\n", im->target_width, im->target_height);
  }
  
  // After resizing we can release the source image memory
  Safefree(im->pixbuf);
  im->pixbuf = NULL;
  
out:
  im->used++;
  
  return ret;
}
    		  
void
image_finish(image *im)
{
  // Called at DESTROY-time to release all memory if needed.
  // Items here may be freed elsewhere so must check that they aren't NULL
  
  DEBUG_TRACE("image_finish\n");
  
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
      image_bmp_finish(im);
      break;
  }
  
  if (im->buf != NULL) {
    buffer_free(im->buf);
    Safefree(im->buf);
    im->buf = NULL;
  }
  
  if (im->pixbuf != NULL && im->pixbuf != im->outbuf) { // pixbuf = outbuf if resizing to same dimensions
    Safefree(im->pixbuf);
    im->pixbuf = NULL;
  }
  
  if (im->outbuf != NULL) {
    Safefree(im->outbuf);
    im->outbuf = NULL;
    im->outbuf_size = 0;
  }
  
  if (im->path != NULL) {
    SvREFCNT_dec(im->path);
    im->path = NULL;
  }
  
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
      if (x == 0 && y == 0 && im->orientation != 0) // An invalid orientation of 0 is often seen in non-rotated images
        warn("Image::Scale cannot rotate, unknown orientation value: %d (%s)\n", im->orientation, SvPVX(im->path));
      *ox = x;
      *oy = y;
      break;
  }
}
