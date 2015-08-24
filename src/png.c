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

static void
image_png_read_buf(png_structp png_ptr, png_bytep data, png_size_t len)
{
 image *im = (image *)png_get_io_ptr(png_ptr);

 DEBUG_TRACE("PNG read_buf wants %ld bytes, %d in buffer\n", len, buffer_len(im->buf));
 
 if (im->fh != NULL) {
   if ( !_check_buf(im->fh, im->buf, len, MAX(len, BUFFER_SIZE)) ) {
     goto eof;
   }
 }
 else {
   if (len > buffer_len(im->buf)) {
     // read from SV into buffer
     int sv_readlen = len - buffer_len(im->buf);
   
     if (sv_readlen > sv_len(im->sv_data) - im->sv_offset)
       goto eof;
   
     DEBUG_TRACE("  Reading %d bytes of SV data @ %d\n", sv_readlen, im->sv_offset);    
     buffer_append(im->buf, SvPVX(im->sv_data) + im->sv_offset, sv_readlen);
     im->sv_offset += sv_readlen;
    }
 }

 memcpy(data, buffer_ptr(im->buf), len);
 buffer_consume(im->buf, len);
 
 goto ok;
 
eof:
  png_error(png_ptr, "Not enough PNG data");
 
ok:
  return;
}

static void
image_png_error(png_structp png_ptr, png_const_charp error_msg)
{
  image *im = (image *)png_get_error_ptr(png_ptr);
  
  warn("Image::Scale libpng error: %s (%s)\n", error_msg, SvPVX(im->path));
  
  longjmp(png_jmpbuf(png_ptr), 1);
}

static void
image_png_warning(png_structp png_ptr, png_const_charp warning_msg)
{
  image *im = (image *)png_get_error_ptr(png_ptr);
  
  warn("Image::Scale libpng warning: %s (%s)\n", warning_msg, SvPVX(im->path));
}

int
image_png_read_header(image *im)
{
  im->png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, (png_voidp)im, image_png_error, image_png_warning);
  if ( !im->png_ptr )
    croak("Image::Scale could not initialize libpng\n");
  
  im->info_ptr = png_create_info_struct(im->png_ptr);
  if ( !im->info_ptr ) {
    png_destroy_read_struct(&im->png_ptr, (png_infopp)NULL, (png_infopp)NULL);
    croak("Image::Scale could not initialize libpng\n");
  }
  
  if ( setjmp( png_jmpbuf(im->png_ptr) ) ) {
    image_png_finish(im);
    return 0;
  }
  
  png_set_read_fn(im->png_ptr, im, image_png_read_buf);
  
  png_read_info(im->png_ptr, im->info_ptr);
  
  im->width     = png_get_image_width(im->png_ptr, im->info_ptr);
  im->height    = png_get_image_height(im->png_ptr, im->info_ptr);
  im->channels  = png_get_channels(im->png_ptr, im->info_ptr);
  im->has_alpha = 1;
  
  return 1;
}

static void
image_png_interlace_pass_gray(image *im, unsigned char *ptr, int start_y, int stride_y, int start_x, int stride_x)
{
  int x, y;
  
  for (y = 0; y < im->height; y++) {
    png_read_row(im->png_ptr, ptr, NULL);
    if (start_y == 0) {
      start_y = stride_y;
      for (x = start_x; x < im->width; x += stride_x) {
        im->pixbuf[y * im->width + x] = COL_FULL(
          ptr[x * 2], ptr[x * 2], ptr[x * 2], ptr[x * 2 + 1]
        );
      }
    }
    start_y--;
  }
}

static void
image_png_interlace_pass(image *im, unsigned char *ptr, int start_y, int stride_y, int start_x, int stride_x)
{
  int x, y;
  
  for (y = 0; y < im->height; y++) {
    png_read_row(im->png_ptr, ptr, NULL);
    if (start_y == 0) {
      start_y = stride_y;
      for (x = start_x; x < im->width; x += stride_x) {
        im->pixbuf[y * im->width + x] = COL_FULL(
          ptr[x * 4], ptr[x * 4 + 1], ptr[x * 4 + 2], ptr[x * 4 + 3]
        );
      }
    }
    start_y--;
  }
}

int
image_png_load(image *im)
{
  int bit_depth, color_type, num_passes, x, y;
  int ofs;
  volatile unsigned char *ptr = NULL; // volatile = won't be rolled back if longjmp is called
  
  if ( setjmp( png_jmpbuf(im->png_ptr) ) ) {
    if (ptr != NULL)
      Safefree(ptr);
    image_png_finish(im);
    return 0;
  }
  
  // If reusing the object a second time, we need to completely create a new png struct
  if (im->used) {
    DEBUG_TRACE("Recreating libpng objects\n");
    image_png_finish(im);
    
    if (im->fh != NULL) {
      // reset file to begining of image
      PerlIO_seek(im->fh, im->image_offset, SEEK_SET);
    }
    else {
      // reset SV read
      im->sv_offset = im->image_offset;
    }
    
    buffer_clear(im->buf);
    
    image_png_read_header(im);
  }
  
  bit_depth  = png_get_bit_depth(im->png_ptr, im->info_ptr);
  color_type = png_get_color_type(im->png_ptr, im->info_ptr);
  
  if (color_type == PNG_COLOR_TYPE_PALETTE) {
    png_set_expand(im->png_ptr); // png_set_palette_to_rgb(im->png_ptr);
    im->channels = 4;
  }
  else if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
    png_set_expand(im->png_ptr); // png_set_expand_gray_1_2_4_to_8(im->png_ptr);
  else if (png_get_valid(im->png_ptr, im->info_ptr, PNG_INFO_tRNS))
    png_set_expand(im->png_ptr); // png_set_tRNS_to_alpha(im->png_ptr);
  
  if (bit_depth == 16)
    png_set_strip_16(im->png_ptr);
  else if (bit_depth < 8)
    png_set_packing(im->png_ptr);
  
  // Make non-alpha RGB/Palette 32-bit and Gray 16-bit for easier handling
  if ( !(color_type & PNG_COLOR_MASK_ALPHA) ) {
    png_set_add_alpha(im->png_ptr, 0xFF, PNG_FILLER_AFTER);
  }
  
  num_passes = png_set_interlace_handling(im->png_ptr);
  
  DEBUG_TRACE("png bit_depth %d, color_type %d, channels %d, num_passes %d\n", bit_depth, color_type, im->channels, num_passes);
  
  png_read_update_info(im->png_ptr, im->info_ptr);
  
  image_alloc(im, im->width, im->height);
  
  New(0, ptr, png_get_rowbytes(im->png_ptr, im->info_ptr), unsigned char);
  
  ofs = 0;
  
  if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) { // Grayscale (Alpha)
    if (num_passes == 1) { // Non-interlaced
      for (y = 0; y < im->height; y++) {
        png_read_row(im->png_ptr, (unsigned char *)ptr, NULL);
        for (x = 0; x < im->width; x++) {
  			  im->pixbuf[ofs++] = COL_FULL(ptr[x * 2], ptr[x * 2], ptr[x * 2], ptr[x * 2 + 1]);
  			}
      }
    }
    else if (num_passes == 7) { // Interlaced
      image_png_interlace_pass_gray(im, (unsigned char *)ptr, 0, 8, 0, 8);
      image_png_interlace_pass_gray(im, (unsigned char *)ptr, 0, 8, 4, 8);
      image_png_interlace_pass_gray(im, (unsigned char *)ptr, 4, 8, 0, 4);
      image_png_interlace_pass_gray(im, (unsigned char *)ptr, 0, 4, 2, 4);
      image_png_interlace_pass_gray(im, (unsigned char *)ptr, 2, 4, 0, 2);
      image_png_interlace_pass_gray(im, (unsigned char *)ptr, 0, 2, 1, 2);
      image_png_interlace_pass_gray(im, (unsigned char *)ptr, 1, 2, 0, 1);
    }
  }
  else { // RGB(A)
    if (num_passes == 1) { // Non-interlaced
      for (y = 0; y < im->height; y++) {
        png_read_row(im->png_ptr, (unsigned char *)ptr, NULL);
        for (x = 0; x < im->width; x++) {
  			  im->pixbuf[ofs++] = COL_FULL(ptr[x * 4], ptr[x * 4 + 1], ptr[x * 4 + 2], ptr[x * 4 + 3]);
  			}
      }
    }
    else if (num_passes == 7) { // Interlaced
      // The first pass will return an image 1/8 as wide as the entire image
      // (every 8th column starting in column 0)
      // and 1/8 as high as the original (every 8th row starting in row 0)
      image_png_interlace_pass(im, (unsigned char *)ptr, 0, 8, 0, 8);
    
      // The second will be 1/8 as wide (starting in column 4)
      // and 1/8 as high (also starting in row 0)
      image_png_interlace_pass(im, (unsigned char *)ptr, 0, 8, 4, 8);
    
      // The third pass will be 1/4 as wide (every 4th pixel starting in column 0)
      // and 1/8 as high (every 8th row starting in row 4)
      image_png_interlace_pass(im, (unsigned char *)ptr, 4, 8, 0, 4);
    
      // The fourth pass will be 1/4 as wide and 1/4 as high
      // (every 4th column starting in column 2, and every 4th row starting in row 0)
      image_png_interlace_pass(im, (unsigned char *)ptr, 0, 4, 2, 4);
    
      // The fifth pass will return an image 1/2 as wide,
      // and 1/4 as high (starting at column 0 and row 2)
      image_png_interlace_pass(im, (unsigned char *)ptr, 2, 4, 0, 2);
    
      // The sixth pass will be 1/2 as wide and 1/2 as high as the original
      // (starting in column 1 and row 0)
      image_png_interlace_pass(im, (unsigned char *)ptr, 0, 2, 1, 2);
    
      // The seventh pass will be as wide as the original, and 1/2 as high,
      // containing all of the odd numbered scanlines.
      image_png_interlace_pass(im, (unsigned char *)ptr, 1, 2, 0, 1);
    }
    else {
      croak("Image::Scale unsupported PNG interlace type (%d passes)\n", num_passes);
    }
  }
  
  Safefree(ptr);
  
  // This is not required, so we can save some time by not reading post-image chunks
  //png_read_end(im->png_ptr, im->info_ptr);
  
  return 1;
}

static void
image_png_compress(image *im, png_structp png_ptr, png_infop info_ptr)
{
  int i, x, y;
  int color_space = PNG_COLOR_TYPE_RGB_ALPHA;
  volatile unsigned char *ptr = NULL;
  
  if (setjmp( png_jmpbuf(png_ptr) )) {
    if (ptr != NULL)
      Safefree(ptr);
    return;
  }
  
  // Match output color space with input file
  switch (im->channels) {
    case 4:
    case 3:
      DEBUG_TRACE("PNG output color space set to RGBA\n");
      color_space = PNG_COLOR_TYPE_RGB_ALPHA;
      break;
    case 2:
    case 1:
      DEBUG_TRACE("PNG output color space set to gray alpha\n");
      color_space = PNG_COLOR_TYPE_GRAY_ALPHA;
      break;
  }
  
  png_set_IHDR(png_ptr, info_ptr, im->target_width, im->target_height, 8, color_space,
    PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
  
  png_write_info(png_ptr, info_ptr);
  
  New(0, ptr, png_get_rowbytes(png_ptr, info_ptr), unsigned char);
  
  i = 0;
  
  if (color_space == PNG_COLOR_TYPE_GRAY_ALPHA) {
    for (y = 0; y < im->target_height; y++) {
      for (x = 0; x < im->target_width; x++)  {
        ptr[x * 2]     = COL_BLUE(im->outbuf[i]);
        ptr[x * 2 + 1] = COL_ALPHA(im->outbuf[i]);
        i++;
      }
      png_write_row(png_ptr, (png_bytep)ptr);
    }
  }
  else { // RGB  
    for (y = 0; y < im->target_height; y++) {
      for (x = 0; x < im->target_width; x++)  {
        ptr[x * 4]     = COL_RED(im->outbuf[i]);
        ptr[x * 4 + 1] = COL_GREEN(im->outbuf[i]);
        ptr[x * 4 + 2] = COL_BLUE(im->outbuf[i]);
        ptr[x * 4 + 3] = COL_ALPHA(im->outbuf[i]);
        i++;
      }
      png_write_row(png_ptr, (png_bytep)ptr);
    }
  }
	
  Safefree(ptr);
  
  png_write_end(png_ptr, info_ptr);
}

void
image_png_save(image *im, const char *path)
{
  png_structp png_ptr;
  png_infop info_ptr;
  FILE *out;
  
  if (im->outbuf == NULL)
    croak("Image::Scale cannot write PNG with no output data\n");
  
  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png_ptr) {
    croak("Image::Scale could not initialize libpng\n");
  }
  
  info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    png_destroy_write_struct(&png_ptr, NULL);
    croak("Image::Scale could not initialize libpng\n");
  }

  if ((out = fopen(path, "wb")) == NULL) {
    png_destroy_write_struct(&png_ptr, &info_ptr);
    croak("Image::Scale cannot open %s for writing\n", path);
  }
  
  png_init_io(png_ptr, out);
  
  image_png_compress(im, png_ptr, info_ptr);
  
  fclose(out);
  png_destroy_write_struct(&png_ptr, &info_ptr);
}

static void
image_png_write_sv(png_structp png_ptr, png_bytep data, png_size_t len)
{
  SV *sv_buf = (SV *)png_get_io_ptr(png_ptr);
  
  // Copy buffer to SV
  sv_catpvn(sv_buf, (char *)data, len);
  
  DEBUG_TRACE("image_png_write_sv copied %ld bytes\n", len);
}

static void
image_png_flush_sv(png_structp png_ptr)
{
  // Nothing
}

void
image_png_to_sv(image *im, SV *sv_buf)
{
  png_structp png_ptr;
  png_infop info_ptr;
  
  if (im->outbuf == NULL)
    croak("Image::Scale cannot write PNG with no output data\n");
  
  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png_ptr) {
    croak("Image::Scale could not initialize libpng\n");
  }
  
  info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    png_destroy_write_struct(&png_ptr, NULL);
    croak("Image::Scale could not initialize libpng\n");
  }
  
  png_set_write_fn(png_ptr, sv_buf, image_png_write_sv, image_png_flush_sv);
  
  image_png_compress(im, png_ptr, info_ptr);
  
  png_destroy_write_struct(&png_ptr, &info_ptr);
}

void
image_png_finish(image *im)
{
  if (im->png_ptr != NULL) {
    png_destroy_read_struct(&im->png_ptr, &im->info_ptr, NULL);
    im->png_ptr = NULL;
    DEBUG_TRACE("libpng destroy\n");
  }
}
