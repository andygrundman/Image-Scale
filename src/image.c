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

#include "jpeg.c"
#include "bmp.c"
#include "gif.c"
#include "png.c"

// Algorithms from GraphicsMagick
#include "magick.c"
#include "magick_fixed.c"

int
image_init(HV *self, image *im)
{
  unsigned char *bptr;
  char *file = SvPVX(*(my_hv_fetch(self, "file")));
  
  im->pixbuf        = NULL;
  im->outbuf        = NULL;
  im->fh            = IoIFP(sv_2io(*(my_hv_fetch(self, "_fh"))));
  im->type          = UNKNOWN;
  im->width         = 0;
  im->height        = 0;
  im->flipped       = 0;
  im->bpp           = 0;
  im->channels      = 0;
  im->has_alpha     = 0;
  im->memory_limit  = 0;
  im->target_width  = 0;
  im->target_height = 0;
  im->keep_aspect   = 0;
  im->rotate        = 0;
  im->resize_type   = IMAGE_SCALE_TYPE_DEFAULT;
  im->filter        = 0;
  
  im->cinfo         = NULL;
  im->png_ptr       = NULL;
  im->info_ptr      = NULL;
  
  Newz(0, im->buf, sizeof(Buffer), Buffer);
  buffer_init(im->buf, 1024);
  im->memory_used = 1024;
  
  // Determine type of file from magic bytes
  
  if ( !_check_buf(im->fh, im->buf, 8, 1024) ) {
    // Free mem in case croak is trapped
    buffer_free(im->buf);
    Safefree(im->buf);
    im->buf = NULL;
    croak("Unable to read image header for %s", file);
  }
  
  bptr = buffer_ptr(im->buf);
  
  switch (bptr[0]) {
    case 0xff:
      if (bptr[1] == 0xd8 && bptr[2] == 0xff) {
        im->type = JPEG;
      }
      break;
    case 0x89:
      if (bptr[1] == 'P' && bptr[2] == 'N' && bptr[3] == 'G'
        && bptr[4] == 0x0d && bptr[5] == 0x0a && bptr[6] == 0x1a && bptr[7] == 0x0a) {
          im->type = PNG;
      }
      break;
    case 'G':
      if (bptr[1] == 'I' && bptr[2] == 'F' && bptr[3] == '8'
        && (bptr[4] == '7' || bptr[4] == '9') && bptr[5] == 'a') {
          im->type = GIF;
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
    case JPEG:
      image_jpeg_read_header(im, file);
      break;
    case PNG:
      image_png_read_header(im, file);
      break;
    case GIF:
      image_gif_read_header(im, file);
      break;
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
  int size;
  
  // Load the source image into memory
  switch (im->type) {
    case JPEG:
      image_jpeg_load(im);
      image_jpeg_finish(im);
      break;
    case PNG:
      image_png_load(im);
      image_png_finish(im);
      break;
    case GIF:
      image_gif_load(im);
      break;
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
  size = im->target_width * im->target_height * sizeof(pix);
  
  if (im->memory_limit && im->memory_limit < im->memory_used + size) {
    croak("Image::Scale memory_limit exceeded (wanted to allocate %d bytes)", im->memory_used + size);
  }
  
  DEBUG_TRACE("Allocating %d bytes for resized image of size %d x %d\n",
    size, im->target_width, im->target_height);
  New(0, im->outbuf, size, pix);
  im->memory_used += size;
  
  // Resize
  switch (im->resize_type) {
    case IMAGE_SCALE_TYPE_DEFAULT:
      image_downsize(im);
      break;
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
  
  // After resizing we can release the source image memory
  Safefree(im->pixbuf);
  im->pixbuf = NULL;
}

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

void
image_downsize(image *im)
{
  int32_t vx, vy;
	pix vcol;
	int32_t i, j;
	
  float rx, ry;
  float width_scale, height_scale;
	float red, green, blue, alpha;
	int32_t half_square_width, half_square_height;
  float round_width, round_height;

	if ( (im->outbuf == NULL) || (im->pixbuf == NULL) )
		return;

	width_scale  = (float)im->width  / (float)im->target_width;
	height_scale = (float)im->height / (float)im->target_height;

	half_square_width  = (int32_t)(width_scale  / 2.0);
	half_square_height = (int32_t)(height_scale / 2.0);
	round_width  = (width_scale  / 2.0) - (float)half_square_width;
	round_height = (height_scale / 2.0) - (float)half_square_height;
	if (round_width  > 0.0)
		half_square_width++;
	else
		round_width = 1.0;
	if (round_height > 0.0)
		half_square_height++;
	else
		round_height = 1.0;
	
	DEBUG_TRACE("width_scale %.2f, height_scale %.2f\n", width_scale, height_scale);
  DEBUG_TRACE("half_square_width %d, half_square_height %d\n", half_square_width, half_square_height);
  DEBUG_TRACE("round_width %.2f, round_height %.2f\n", round_width, round_height);

	for (vy = 0; vy < im->target_height; vy++) {
		for (vx = 0; vx < im->target_width; vx++) {
      float spixels = 0.0;
      red = green = blue = alpha = 0.0;
      
			rx  = vx * width_scale;
			ry  = vy * height_scale;

      DEBUG_TRACE("rx %.2f, ry %.2f ->\n", rx, ry);

			for (j = 0; j < half_square_height << 1; j++)	{
				for (i = 0; i < half_square_width << 1; i++) {
          float pcontribution;
          
          int qx = ((int32_t)rx) - half_square_width + i;
          int qy = ((int32_t)ry) - half_square_height + j;
          
          // Do not interpolate with negative pixels
				  if (qx < 0 || qy < 0) {
            continue;
          }

					if (((j == 0) || (j == (half_square_height << 1) - 1)) && 
					   ((i == 0) || (i == (half_square_width << 1) - 1)))
					{
            pcontribution = round_width * round_height;
					}
					else if ((j == 0) || (j == (half_square_height << 1) - 1)) {
            pcontribution = round_height;
					}
				  else if ((i == 0) || (i == (half_square_width << 1) - 1))	{
            pcontribution = round_width;
					}
					else {
            pcontribution = 1.0;
					}
					
					vcol = get_pix(im, qx, qy);
					
					DEBUG_TRACE("  merging with pix %d, %d: src %x (%d %d %d %d), pcontribution: %.2f\n",
            qx, qy, vcol,
            COL_RED(vcol), COL_GREEN(vcol), COL_BLUE(vcol), COL_ALPHA(vcol), pcontribution);
                  
          red   += (float)COL_RED  (vcol) * pcontribution;
          green += (float)COL_GREEN(vcol) * pcontribution;
          blue  += (float)COL_BLUE (vcol) * pcontribution;
          alpha += (float)COL_ALPHA(vcol) * pcontribution;
          spixels += pcontribution;
				}
			}
			
			if (spixels != 0.0) {
        //DEBUG_TRACE("  spixels %.2f\n", spixels);
        spixels = 1 / spixels;
  			red   *= spixels;
  			green *= spixels;
  			blue  *= spixels;
  			alpha *= spixels;
  		}
  		
      if (red > 255.0)   red = 255.0;
      if (green > 255.0) green = 255.0;
      if (blue > 255.0)  blue = 255.0;
      if (alpha > 255.0) alpha = 255.0;
			
			DEBUG_TRACE("  -> %d, %d %x (%d %d %d %d)\n",
        vx, vy, COL_FULL((int)red, (int)green, (int)blue, (int)alpha),
        (int)red, (int)green, (int)blue, (int)alpha);

			put_pix(
			  im, vx, vy,
				COL_FULL((int)red, (int)green, (int)blue, (int)alpha)
			);
		}
	}
}

// Port of GD copyResampled
#define floor2(exp) ((int) exp)

void
image_downsize_gd(image *im)
{
  int x, y;
  float sy1, sy2, sx1, sx2;
  int dstX = 0, dstY = 0, srcX = 0, srcY = 0;
  float width_scale, height_scale;
  
  int dstW = im->target_width;
  int dstH = im->target_height;
  int srcW = im->width;
  int srcH = im->height;
  
  width_scale = (float)srcW / dstW;
  height_scale = (float)srcH / dstH;
  
  for (y = dstY; (y < dstY + dstH); y++) {
    sy1 = (float)(y - dstY) * height_scale;
    sy2 = (float)((y + 1) - dstY) * height_scale;
    
    for (x = dstX; (x < dstX + dstW); x++) {
      float sx, sy;
  	  float spixels = 0;
  	  float red = 0.0, green = 0.0, blue = 0.0, alpha = 0.0;
  	  
  	  sx1 = (float)(x - dstX) * width_scale;
  	  sx2 = (float)((x + 1) - dstX) * width_scale;
  	  sy = sy1;
  	  
      //DEBUG_TRACE("sx1 %.2f, sx2 %.2f, sy1 %.2f, sy2 %.2f\n", sx1, sx2, sy1, sy2); 
  	  
  	  do {
        float yportion;
        
        //DEBUG_TRACE("  yportion(sy %.2f, sy1 %.2f, sy2 %.2f) = ", sy, sy1, sy2);
        if (floor2(sy) == floor2(sy1)) {
          yportion = 1.0 - (sy - floor2(sy));
    		  if (yportion > sy2 - sy1) {
            yportion = sy2 - sy1;
    		  }
    		  sy = floor2(sy);
    		}
    		else if (sy == floor2(sy2)) {
          yportion = sy2 - floor2(sy2);
        }
        else {
          yportion = 1.0;
        }
        //DEBUG_TRACE("%.2f\n", yportion);
        
        sx = sx1;
        
        do {
          float xportion = 1.0;
    		  float pcontribution;
    		  pix p;

    		  //DEBUG_TRACE("  xportion(sx %.2f, sx1 %.2f, sx2 %.2f) = ", sx, sx1, sx2);
    		  if (floor2(sx) == floor2(sx1)) {
    	      xportion = 1.0 - (sx - floor2(sx));
    	      if (xportion > sx2 - sx1)	{
              xportion = sx2 - sx1;
    			  }
    		    sx = floor2(sx);
    		  }
    		  else if (sx == floor2(sx2)) {
            xportion = sx2 - floor2(sx2);
          }
    		  else {
    		    xportion = 1.0;
    		  }
    		  //DEBUG_TRACE("%.2f\n", xportion);
  		  
    		  pcontribution = xportion * yportion;
  		  
    		  p = get_pix(im, (int32_t)sx + srcX, (int32_t)sy + srcY);
    		  
    		  /*
    		  DEBUG_TRACE("  merging with pix %d, %d: src %x (%d %d %d %d), pcontribution %.2f\n",
            (int32_t)sx + srcX, (int32_t)sy + srcY,
            p, COL_RED(p), COL_GREEN(p), COL_BLUE(p), COL_ALPHA(p), pcontribution);
          */
  		    
    		  red   += COL_RED(p)   * pcontribution;
    		  green += COL_GREEN(p) * pcontribution;
    		  blue  += COL_BLUE(p)  * pcontribution;
    		  alpha += COL_ALPHA(p) * pcontribution;
    		  spixels += pcontribution;
    		  sx += 1.0;
    		} while (sx < sx2);
    		
    		sy += 1.0;
      } while (sy < sy2);
      
      if (spixels != 0.0) {
        //DEBUG_TRACE("  rgba (%.2f %.2f %.2f %.2f) spixels %.2f\n", red, green, blue, alpha, spixels);
        spixels = 1 / spixels;
	      red   *= spixels;
	      green *= spixels;
	      blue  *= spixels;
	      alpha *= spixels;
	    }
	    
	    /* Clamping to allow for rounding errors above */
      if (red > 255.0)   red = 255.0;
      if (green > 255.0) green = 255.0;
      if (blue > 255.0)  blue = 255.0;
      if (alpha > 255.0) alpha = 255.0;
      
      /*
      DEBUG_TRACE("  -> %d, %d %x (%d %d %d %d)\n",
        x, y, COL_FULL((int)red, (int)green, (int)blue, (int)alpha),
        (int)red, (int)green, (int)blue, (int)alpha);
      */
      
      put_pix(
			  im, x, y,
				COL_FULL((int)red, (int)green, (int)blue, (int)alpha)
			);
	  }
	}
}

void
image_downsize_gd_fixed_point(image *im)
{
  int x, y;
  fixed_t sy1, sy2, sx1, sx2;
  int dstX = 0, dstY = 0, srcX = 0, srcY = 0;
  fixed_t width_scale, height_scale;
  
  int dstW = im->target_width;
  int dstH = im->target_height;
  int srcW = im->width;
  int srcH = im->height;
  
  width_scale = fixed_div(int_to_fixed(srcW), int_to_fixed(dstW));
  height_scale = fixed_div(int_to_fixed(srcH), int_to_fixed(dstH));
  
  for (y = dstY; (y < dstY + dstH); y++) {
    sy1 = fixed_mul(int_to_fixed(y - dstY), height_scale);
    sy2 = fixed_mul(int_to_fixed((y + 1) - dstY), height_scale);
    
    for (x = dstX; (x < dstX + dstW); x++) {
      fixed_t sx, sy;
  	  fixed_t spixels = 0;
  	  fixed_t red = 0, green = 0, blue = 0, alpha = 0;
  	  
      sx1 = fixed_mul(int_to_fixed(x - dstX), width_scale);
      sx2 = fixed_mul(int_to_fixed((x + 1) - dstX), width_scale);  	  
  	  sy = sy1;
  	  
  	  /*
      DEBUG_TRACE("sx1 %f, sx2 %f, sy1 %f, sy2 %f\n",
        fixed_to_float(sx1), fixed_to_float(sx2), fixed_to_float(sy1), fixed_to_float(sy2));
      */
  	  
  	  do {
        fixed_t yportion;
        
        //DEBUG_TRACE("  yportion(sy %f, sy1 %f, sy2 %f) = ", fixed_to_float(sy), fixed_to_float(sy1), fixed_to_float(sy2));
        
        if (fixed_floor(sy) == fixed_floor(sy1)) {
          yportion = FIXED_1 - (sy - fixed_floor(sy));
    		  if (yportion > sy2 - sy1) {
            yportion = sy2 - sy1;
    		  }
    		  sy = fixed_floor(sy);
    		}
    		else if (sy == fixed_floor(sy2)) {
          yportion = sy2 - fixed_floor(sy2);
        }
        else {
          yportion = FIXED_1;
        }
        
        //DEBUG_TRACE("%f\n", fixed_to_float(yportion));
        
        sx = sx1;
        
        do {
          fixed_t xportion;
    		  fixed_t pcontribution;
    		  pix p;
    		  
    		  //DEBUG_TRACE("  xportion(sx %f, sx1 %f, sx2 %f) = ", fixed_to_float(sx), fixed_to_float(sx1), fixed_to_float(sx2));
  		  
    		  if (fixed_floor(sx) == fixed_floor(sx1)) {
    	      xportion = FIXED_1 - (sx - fixed_floor(sx));
    	      if (xportion > sx2 - sx1)	{
              xportion = sx2 - sx1;
    			  }
    		    sx = fixed_floor(sx);
    		  }
    		  else if (sx == fixed_floor(sx2)) {
            xportion = sx2 - fixed_floor(sx2);
          }
    		  else {
    		    xportion = FIXED_1;
    		  }
    		  
    		  //DEBUG_TRACE("%f\n", fixed_to_float(xportion));
  		  
    		  pcontribution = fixed_mul(xportion, yportion);
  		  
    		  p = get_pix(im, fixed_to_int(sx + srcX), fixed_to_int(sy + srcY));
    		  
    		  /*
    		  DEBUG_TRACE("  merging with pix %d, %d: src %x (%d %d %d %d), pcontribution %f\n",
            fixed_to_int(sx + srcX), fixed_to_int(sy + srcY),
            p, COL_RED(p), COL_GREEN(p), COL_BLUE(p), COL_ALPHA(p), fixed_to_float(pcontribution));
          */
  		    
          red   += fixed_mul(int_to_fixed(COL_RED(p)), pcontribution);
          green += fixed_mul(int_to_fixed(COL_GREEN(p)), pcontribution);
    		  blue  += fixed_mul(int_to_fixed(COL_BLUE(p)), pcontribution);
    		  alpha += fixed_mul(int_to_fixed(COL_ALPHA(p)), pcontribution);
    		  
    		  spixels += pcontribution;
    		  sx += FIXED_1;
    		} while (sx < sx2);
    		
        sy += FIXED_1;
      } while (sy < sy2);
      
  	  // If rgba get too large for the fixed-point representation, fallback to the floating point routine
		  // This should only happen with very large images
		  if (red < 0 || green < 0 || blue < 0 || alpha < 0) {
        warn("fixed-point overflow: %d %d %d %d\n", red, green, blue, alpha);
        return image_downsize_gd(im);
      }
      
      if (spixels != 0) {
        /*
        DEBUG_TRACE("  rgba (%f %f %f %f) spixels %f\n",
          fixed_to_float(red), fixed_to_float(green), fixed_to_float(blue), fixed_to_float(alpha), fixed_to_float(spixels));
        */
        
        spixels = fixed_div(FIXED_1, spixels);
        
        red   = fixed_mul(red, spixels);
        green = fixed_mul(green, spixels);
        blue  = fixed_mul(blue, spixels);
        alpha = fixed_mul(alpha, spixels);
	    }
	    
	    /* Clamping to allow for rounding errors above */
      if (red > FIXED_255)   red = FIXED_255;
      if (green > FIXED_255) green = FIXED_255;
      if (blue > FIXED_255)  blue = FIXED_255;
      if (alpha > FIXED_255) alpha = FIXED_255;
      
      /*
      DEBUG_TRACE("  -> %d, %d %x (%d %d %d %d)\n",
        x, y, COL_FULL(fixed_to_int(red), fixed_to_int(green), fixed_to_int(blue), fixed_to_int(alpha)),
        fixed_to_int(red), fixed_to_int(green), fixed_to_int(blue), fixed_to_int(alpha));
      */
      
      put_pix(
			  im, x, y,
				COL_FULL(fixed_to_int(red), fixed_to_int(green), fixed_to_int(blue), fixed_to_int(alpha))
			);
	  }
	}
}
    		  
void
image_finish(image *im)
{
  // Called at DESTROY-time to release all memory if needed.
  // Items here may be freed elsewhere so must check that they aren't NULL
  
  switch (im->type) {
    case JPEG:
      image_jpeg_finish(im);
      break;
    case PNG:
      image_png_finish(im);
      break;
    case GIF:
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
