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

jmp_buf setjmp_buffer;
static void
libjpeg_error_handler(j_common_ptr cinfo)
{
	cinfo->err->output_message(cinfo);
	longjmp(setjmp_buffer, 1);
	return;
}

void
image_jpeg_read_header(image *im, const char *file)
{
  struct jpeg_error_mgr pub;
 
  if ( (im->stdio_fp = fopen(file, "rb")) == NULL ) {
		croak("Image::Scale could not open %s for reading", file);
	}
  
  Newz(0, im->cinfo, sizeof(struct jpeg_decompress_struct), struct jpeg_decompress_struct);
  im->memory_used += sizeof(struct jpeg_decompress_struct);

  im->cinfo->err = jpeg_std_error(&pub);
	pub.error_exit = libjpeg_error_handler;
	
	if (setjmp(setjmp_buffer)) {
		return;
	}
  
  jpeg_create_decompress(im->cinfo);
  jpeg_stdio_src(im->cinfo, im->stdio_fp);  
  jpeg_read_header(im->cinfo, TRUE);
  
  im->width    = im->cinfo->image_width;
  im->height   = im->cinfo->image_height;
  im->channels = im->cinfo->num_components;
}

void
image_jpeg_load(image *im)
{
  float scale_factor;
  int x, y, i, w, h, ofs, maxbuf;
  unsigned char *line[16], *ptr;
  
  im->cinfo->do_fancy_upsampling = FALSE;
	im->cinfo->do_block_smoothing = FALSE;
	
	// Choose optimal scaling factor
	jpeg_calc_output_dimensions(im->cinfo);
	scale_factor = (float)im->cinfo->output_width / im->target_width;
	if (scale_factor > ((float)im->cinfo->output_height / im->target_height))
    scale_factor = (float)im->cinfo->output_height / im->target_height;
  im->cinfo->scale_denom *= (unsigned int)scale_factor;
  jpeg_calc_output_dimensions(im->cinfo);
  
  w = im->cinfo->output_width;
  h = im->cinfo->output_height;
  
  // Change the original values to the scaled size
  im->width  = w;
  im->height = h;
  
  DEBUG_TRACE("Using JPEG scale factor %d/%d, new output dimensions %d x %d\n",
    im->cinfo->scale_num, im->cinfo->scale_denom, w, h);
  
  if (setjmp(setjmp_buffer)) {
		return;
	}
  
  jpeg_start_decompress(im->cinfo);
  
  if (im->cinfo->rec_outbuf_height > 16) {
    warn("Image::Scale JPEG uses line buffers > 16, cannot load\n");
    return;
	}
  
  // Allocate storage for decompressed image
	image_alloc(im, w, h);
	
  maxbuf = w * h;
  
  if (im->cinfo->output_components == 3) {
		ofs = 0;
		
    New(0, ptr, w * 3 * im->cinfo->rec_outbuf_height, unsigned char);

		for (y = 0; y < h; y += im->cinfo->rec_outbuf_height)	{
			for (i = 0; i < im->cinfo->rec_outbuf_height; i++) {
				line[i] = ptr + (w * 3 * i);
			}
			jpeg_read_scanlines(im->cinfo, line, im->cinfo->rec_outbuf_height);
			for (x = 0; x < w * im->cinfo->rec_outbuf_height; x++)	{
				if (ofs < maxbuf) {
				  im->pixbuf[ofs] = COL(ptr[x + x + x], ptr[x + x + x + 1], ptr[x + x + x + 2]);
					ofs++;
				}
			}
		}
		
		Safefree(ptr);
	}
	else if (im->cinfo->output_components == 1) {
	  ofs = 0;
	  
		for (i = 0; i < im->cinfo->rec_outbuf_height; i++) {
      New(0, line[i], w, unsigned char);
		}
		
		for (y = 0; y < h; y += im->cinfo->rec_outbuf_height)	{
			jpeg_read_scanlines(im->cinfo, line, im->cinfo->rec_outbuf_height);
			for (i = 0; i < im->cinfo->rec_outbuf_height; i++)	{
				for (x = 0; x < w; x++)	{
					im->pixbuf[ofs++] = COL(line[i][x], line[i][x], line[i][x]);
				}
			}
		}
		
		for (i = 0; i < im->cinfo->rec_outbuf_height; i++) {
		  Safefree(line[i]);
		}
	}
}

void
image_jpeg_save(image *im, const char *path, int quality)
{
  struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	JSAMPROW row_pointer[1];
  FILE *out;
  int row_stride;
	unsigned char *data;
  int i, x;
  
  if ((out = fopen(path, "wb")) == NULL) {
    croak("Image::Scale cannot open %s for writing", path);
  }
	
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);
  jpeg_stdio_dest(&cinfo, out);
  
  cinfo.image_width      = im->target_width;
  cinfo.image_height     = im->target_height;
  cinfo.input_components = 3; // XXX grayscale?
  cinfo.in_color_space   = JCS_RGB; // XXX grayscale?
  
  jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, TRUE);
	jpeg_start_compress(&cinfo, TRUE);
	
	row_stride = cinfo.image_width * 3;
  New(0, data, row_stride, unsigned char);
  
  i = 0;
  while (cinfo.next_scanline < cinfo.image_height) {
		for (x = 0; x < cinfo.image_width; x++)	{
			data[x + x + x]     = COL_RED(  im->outbuf[i]);
			data[x + x + x + 1] = COL_GREEN(im->outbuf[i]);
			data[x + x + x + 2] = COL_BLUE( im->outbuf[i]);
			i++;
		}
		row_pointer[0] = data;
		jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}
	
	jpeg_finish_compress(&cinfo);
  fclose(out);
  
  Safefree(data);
  jpeg_destroy_compress(&cinfo);
}

void
image_jpeg_finish(image *im)
{
  if (im->cinfo != NULL) {
    jpeg_destroy_decompress(im->cinfo);
    Safefree(im->cinfo);
    im->cinfo = NULL;
    im->memory_used -= sizeof(struct jpeg_decompress_struct);
  
    fclose(im->stdio_fp);
  }
}
