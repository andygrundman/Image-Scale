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
 
// 16-bit color masks and shifts, default is 5-5-5
static uint32_t masks[3]  = { 0x7c00, 0x3e0, 0x1f };
static uint32_t shifts[3] = { 10, 5, 0 };
static uint32_t ncolors[3] = { (1 << 5) - 1, (1 << 5) - 1, (1 << 5) - 1 };

int
image_bmp_read_header(image *im)
{
  int offset, palette_colors;
  
  buffer_consume(im->buf, 10);
  
  offset = buffer_get_int_le(im->buf);
  buffer_consume(im->buf, 4);
  im->width  = buffer_get_int_le(im->buf);
  im->height = buffer_get_int_le(im->buf);  
  buffer_consume(im->buf, 2);
  im->bpp = buffer_get_short_le(im->buf);
  im->compression = buffer_get_int_le(im->buf);
  
  DEBUG_TRACE("BMP offset %d, width %d, height %d, bpp %d, compression %d\n",
    offset, im->width, im->height, im->bpp, im->compression);
  
  if (im->compression > 3) { // JPEG/PNG
    warn("Image::Scale unsupported BMP compression type: %d (%s)\n", im->compression, SvPVX(im->path));
    return 0;
  }
  
  // Negative height indicates a flipped image
  if (im->height < 0) {
    croak("flipped\n");
    im->flipped = 1;
    im->height = abs(im->height);
  }
  
  // Not used during reading, but lets output PNG be correct
  im->channels = 4;
  
  // Skip BMP size, resolution
  buffer_consume(im->buf, 12);
  
  palette_colors = buffer_get_int_le(im->buf);
  
  // Skip number of important colors
  buffer_consume(im->buf, 4);
  
  // < 16-bit always has a palette
  if (!palette_colors && im->bpp < 16) {
    switch (im->bpp) {
      case 8:
        palette_colors = 256;
        break;
      case 4:
        palette_colors = 16;
        break;
      case 1:
        palette_colors = 2;
        break;
    }
  }
  
  DEBUG_TRACE("palette_colors %d\n", palette_colors);
  if (palette_colors) {
    // Read palette
    int i;
    
    if (palette_colors > 256) {
      warn("Image::Scale cannot read BMP with palette > 256 colors (%s)\n", SvPVX(im->path));
      return 0;
    }
    
    New(0, im->palette, 1, palette);
    
    for (i = 0; i < palette_colors; i++) {
      int b = buffer_get_char(im->buf);
      int g = buffer_get_char(im->buf);
      int r = buffer_get_char(im->buf);
      buffer_consume(im->buf, 1);
      
      im->palette->colors[i] = COL(r, g, b);
      DEBUG_TRACE("palette %d = %08x\n", i, im->palette->colors[i]);
    }
  }
  else if (im->compression == BMP_BI_BITFIELDS) {
    int pos, bit, i;
    
    if (im->bpp == 16) {
      // Read 16-bit bitfield masks
      for (i = 0; i < 3; i++) {
        masks[i] = buffer_get_int_le(im->buf);
        
        // Determine shift value
        pos = 0;
        bit = masks[i] & -masks[i];
        while (bit) {
          pos++;
          bit >>= 1;
        }
        shifts[i] = pos - 1;
        
        // green can be 6 bits
        if (i == 1) {
          if (masks[1] == 0x7e0)
            ncolors[1] = (1 << 6) - 1;
          else
            ncolors[1] = (1 << 5) - 1;
        }
        
        DEBUG_TRACE("16bpp mask %d: %08x >> %d, ncolors %d\n", i, masks[i], shifts[i], ncolors[i]);
      }
    }
    else { // 32-bit bitfields
      // Read 32-bit bitfield masks
      for (i = 0; i < 3; i++) {
        masks[i] = buffer_get_int_le(im->buf);
        
        // Determine shift value
        pos = 0;
        bit = masks[i] & -masks[i];
        while (bit) {
          pos++;
          bit >>= 1;
        }
        shifts[i] = pos - 1;
        
        DEBUG_TRACE("32bpp mask %d: %08x >> %d\n", i, masks[i], shifts[i]);
      }
    }
  }
  
  // XXX make sure to skip to offset
  
  return 1;
}

int
image_bmp_load(image *im)
{
  int offset = 0;
  int paddingbits = 0;
  int mask = 0;
  int i, x, y, blen;
  int starty, lasty, incy, linebytes;
  unsigned char *bptr;
  
  // If reusing the object a second time, reset buffer
  if (im->used) {
    DEBUG_TRACE("Resetting BMP state\n");
    image_bmp_finish(im);
    
    buffer_clear(im->buf);
    
    if (im->fh != NULL) {
      // reset file to begining of image
      PerlIO_seek(im->fh, im->image_offset, SEEK_SET);
      
      if ( !_check_buf(im->fh, im->buf, 8, BUFFER_SIZE) ) {
        warn("Image::Scale unable to read BMP header (%s)\n", SvPVX(im->path));
        image_bmp_finish(im);
        return 0;
      }
    }
    else {
      // reset SV read
      im->sv_offset = MIN(sv_len(im->sv_data) - im->image_offset, BUFFER_SIZE);
      buffer_append(im->buf, SvPVX(im->sv_data) + im->image_offset, im->sv_offset);
    }
    
    image_bmp_read_header(im);
  }
  
  // Calculate bits of padding per line
  paddingbits = 32 - (im->width * im->bpp) % 32;
  if (paddingbits == 32)
    paddingbits = 0;
  
  // Bytes per line
  linebytes = ((im->width * im->bpp) + paddingbits) / 8;
  
  // No padding if RLE compressed
  if (paddingbits && (im->compression == BMP_BI_RLE4 || im->compression == BMP_BI_RLE8))
    paddingbits = 0;
    
  // XXX Don't worry about RLE support yet
  if (im->compression == BMP_BI_RLE4 || im->compression == BMP_BI_RLE8) {
    warn("Image::Scale does not support BMP RLE compression yet\n");
    image_bmp_finish(im);
    return 0;
  }
  
  DEBUG_TRACE("linebits %d, paddingbits %d, linebytes %d\n", im->width * im->bpp, paddingbits, linebytes);
  
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
  
  if (im->bpp == 1)
    mask = 0x80;
  else if (im->bpp == 4)
    mask = 0xF0;
  
  while (y != lasty) {
    for (x = 0; x < im->width; x++) {
      if (blen <= 0 || blen < im->bpp / 8) {
        // Load more from the buffer
        if (blen < 0)
          blen = 0;
        
        buffer_consume(im->buf, buffer_len(im->buf) - blen);
        
        if (im->fh != NULL) {
          // Read from file
          if ( !_check_buf(im->fh, im->buf, im->channels, 8192) ) {
            image_bmp_finish(im);
            warn("Image::Scale unable to read entire BMP file (%s)\n", SvPVX(im->path));
            return 0;
          }
        }
        else {
          // Read from scalar
          int svbuflen = MIN(sv_len(im->sv_data) - im->sv_offset, 8192);
          
          if (!svbuflen) {
            image_bmp_finish(im);
            warn("Image::Scale unable to read entire BMP file (%s)\n", SvPVX(im->path));
            return 0;
          }
          buffer_append(im->buf, SvPVX(im->sv_data) + im->sv_offset, svbuflen);
          im->sv_offset += svbuflen;
        }
        
        bptr = buffer_ptr(im->buf);
        blen = buffer_len(im->buf);
        offset = 0;
      }
      
      i = x + (y * im->width);
      
      switch (im->bpp) {
        case 32: // XXX how to detect alpha channel?
          //im->pixbuf[i] = COL_FULL(bptr[offset+2], bptr[offset+1], bptr[offset], bptr[offset+3]);
          im->pixbuf[i] = COL(bptr[offset+2], bptr[offset+1], bptr[offset]);
          offset += 4;
          blen -= 4;
          linebytes -= 4;
          break;

        case 24: // 24-bit BGR
          im->pixbuf[i] = COL(bptr[offset+2], bptr[offset+1], bptr[offset]);
          offset += 3;
          blen -= 3;
          linebytes -= 3;
          break;
          
        case 16:
        {
          int p = (bptr[offset+1] << 8) | bptr[offset];
          DEBUG_TRACE("p %x (r %02x g %02x b %02x)\n", p,
              ((p & masks[0]) >> shifts[0]) * 255 / ncolors[0],
              ((p & masks[1]) >> shifts[1]) * 255 / ncolors[1],
              ((p & masks[2]) >> shifts[2]) * 255 / ncolors[2]);
                        
          im->pixbuf[i] = COL(
            ((p & masks[0]) >> shifts[0]) * 255 / ncolors[0],
            ((p & masks[1]) >> shifts[1]) * 255 / ncolors[1],
            ((p & masks[2]) >> shifts[2]) * 255 / ncolors[2]
          );
          
          offset += 2;
          blen -= 2;
          linebytes -= 2;
          break;
        }
        
        case 8:
          im->pixbuf[i] = im->palette->colors[ bptr[offset] ];
          offset++;
          blen--;
          linebytes--;
          break;
          
        case 4:
          // uncompressed
          if (mask == 0xF0) {
            im->pixbuf[i] = im->palette->colors[ (bptr[offset] & mask) >> 4 ];
            mask = 0xF;
          }
          else {
            im->pixbuf[i] = im->palette->colors[ (bptr[offset] & mask) ];
            offset++;
            blen--;
            linebytes--;
            mask = 0xF0;
          }
          break;
          
        case 1:
          im->pixbuf[i] = im->palette->colors[ (bptr[offset] & mask) ? 1 : 0 ];
          mask >>= 1;
          if (!mask) {
            offset++;
            blen--;
            linebytes--;
            mask = 0x80;
          }
          break;
      }
      
      DEBUG_TRACE("x %d / y %d, linebytes left %d, pix %08x\n", x, y, linebytes, im->pixbuf[i]);
    }
    
    if (linebytes) {
      DEBUG_TRACE("Consuming %d bytes of padding\n", linebytes);
      
      if (blen < linebytes) {
        // Load more from the buffer
        buffer_consume(im->buf, buffer_len(im->buf) - blen);
        if ( !_check_buf(im->fh, im->buf, im->channels, 8192) ) {
          image_bmp_finish(im);
          warn("Image::Scale unable to read entire BMP file (%s)\n", SvPVX(im->path));
          return 0;
        }
        bptr = buffer_ptr(im->buf);
        blen = buffer_len(im->buf);
        offset = 0;
      }
      
      offset += linebytes;
      blen -= linebytes;
      
      // Reset mask for next line
      if (im->bpp == 4)
        mask = 0xF0;
      else if (im->bpp == 1)
        mask = 0x80;
    }
    
    linebytes = ((im->width * im->bpp) + paddingbits) / 8;
    
    y += incy;
  }
  
  // Set channels to 4 so we write a color PNG, unless bpp is 1
  if (im->bpp > 1)
    im->channels = 4;
  
  return 1;
}

void
image_bmp_finish(image *im)
{
  if (im->palette != NULL) {
    Safefree(im->palette);
    im->palette = NULL;
  }
}
