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

void
image_bmp_read_header(image *im, const char *file)
{
  int offset;
  int compression;
  
  buffer_consume(im->buf, 10);
  
  offset = buffer_get_int_le(im->buf);
  buffer_consume(im->buf, 4);
  im->width  = buffer_get_int_le(im->buf);
  im->height = buffer_get_int_le(im->buf);  
  buffer_consume(im->buf, 2);
  im->bpp = buffer_get_short_le(im->buf);
  compression = buffer_get_int_le(im->buf);
  
  if (im->bpp < 24) { // XXX
    croak("Image::Scale unsupported BMP bit depth: %d\n", im->bpp);
  }
  
  if (compression != 0) { // XXX
    croak("Image::Scale unsupported BMP compression type: %d\n", compression);
  }
  
  // Negative height indicates a flipped image
  if (im->height < 0) {
    im->flipped = 1;
    im->height = abs(im->height);
  }
  
  im->channels = im->bpp / 8;
  
  // Skip to the start of the image data
  buffer_consume(im->buf, offset - 34);
}

void
image_bmp_load(image *im)
{
  int offset = 0;
  int padding = 0;
  int i, x, y, blen;
  int starty, lasty, incy;
  unsigned char *bptr;
  
  bptr = buffer_ptr(im->buf);
  blen = buffer_len(im->buf);
  
  // Allocate storage for decompressed image
	image_alloc(im, im->width, im->height);
	
	if (im->flipped) {
    starty = 0;
    lasty  = im->height;
    incy   = 1;
  }
  else {
    starty = im->height - 1;
    lasty  = -1;
    incy   = -1;
  }
  
  y = starty;
  
  if ((im->width * im->channels) & 3) {
    padding = (im->width * im->channels) % 4;
  }
  
  while (y != lasty) {
    for (x = 0; x < im->width; x++) {
      if (blen < im->channels) {
        // Load more from the buffer
        buffer_consume(im->buf, buffer_len(im->buf) - blen);
        
        if (im->fh != NULL) {
          // Read from file
          if ( !_check_buf(im->fh, im->buf, im->channels, 8192) ) {
            croak("Image::Scale unable to read entire BMP file\n");
          }
        }
        else {
          // Read from scalar
          int svbuflen = MIN(sv_len(im->sv_data) - im->sv_offset, 8192);
          
          if (!svbuflen) {
            croak("Image::Scale unable to read entire BMP file\n");
          }
          buffer_append(im->buf, SvPVX(im->sv_data) + im->sv_offset, svbuflen);
          im->sv_offset += svbuflen;
        }
        
        bptr = buffer_ptr(im->buf);
        blen = buffer_len(im->buf);
        offset = 0;
      }
      
      i = x + (y * im->width);
      if (im->channels == 4) {
        // 32-bit BGRA
        im->pixbuf[i] = COL_FULL(bptr[offset+2], bptr[offset+1], bptr[offset], bptr[offset+3]);
      }
      else {
        // 24-bit BGR
        im->pixbuf[i] = COL(bptr[offset+2], bptr[offset+1], bptr[offset]);
      }
      
      //DEBUG_TRACE("x %d / y %d / i %d, offset %d, blen %d, pix %x\n", x, y, i, offset, blen, im->pixbuf[i]);
      
      offset += im->channels;
      blen -= im->channels;
    }
    
    if (padding) {
      if (blen < padding) {
        // Load more from the buffer
        buffer_consume(im->buf, buffer_len(im->buf) - blen);
        if ( !_check_buf(im->fh, im->buf, im->channels, 8192) ) {
          croak("Image::Scale unable to read entire BMP file\n");
        }
        bptr = buffer_ptr(im->buf);
        blen = buffer_len(im->buf);
        offset = 0;
      }
        
      offset += padding;
      blen -= padding;
    }
    
    y += incy;
  }
}
