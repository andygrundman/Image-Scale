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

#define JPEG_BUFFER_SIZE 4096
 
struct sv_dst_mgr {
  struct jpeg_destination_mgr jdst;
  SV *sv_buf;
  JOCTET *buf;
  JOCTET *off;
};

// Destination manager to copy compressed data to an SV
static void
sv_dst_mgr_init(j_compress_ptr cinfo)
{
  struct sv_dst_mgr *dst = (void *)cinfo->dest;
  
  New(0, dst->buf, JPEG_BUFFER_SIZE, JOCTET);
  
  dst->off = dst->buf;
  dst->jdst.next_output_byte = dst->off;
  dst->jdst.free_in_buffer = JPEG_BUFFER_SIZE;
}

static boolean
sv_dst_mgr_empty(j_compress_ptr cinfo)
{
  struct sv_dst_mgr *dst = (void *)cinfo->dest;
  
  // Copy buffer to SV
  sv_catpvn(dst->sv_buf, (char *)dst->buf, JPEG_BUFFER_SIZE);

  // Reuse the buffer for the next chunk
  dst->off = dst->buf;
  dst->jdst.next_output_byte = dst->off;
  dst->jdst.free_in_buffer = JPEG_BUFFER_SIZE;
  
  DEBUG_TRACE("sv_dst_mgr_empty, copied %d bytes\n", JPEG_BUFFER_SIZE);
  
  return TRUE;
}

static void
sv_dst_mgr_term(j_compress_ptr cinfo)
{
  struct sv_dst_mgr *dst = (void *)cinfo->dest;

  size_t sz = JPEG_BUFFER_SIZE - dst->jdst.free_in_buffer;
  
  if (sz > 0) {
    // Copy buffer to SV
    sv_catpvn(dst->sv_buf, (char *)dst->buf, sz);
  }
  
  Safefree(dst->buf);

  DEBUG_TRACE("sv_dst_mgr_term, copied %ld bytes\n", sz);
}

static void
image_jpeg_sv_dest(j_compress_ptr cinfo, struct sv_dst_mgr *dst, SV *sv_buf)
{
  dst->sv_buf = sv_buf;
  dst->jdst.init_destination = sv_dst_mgr_init;
  dst->jdst.empty_output_buffer = sv_dst_mgr_empty;
  dst->jdst.term_destination = sv_dst_mgr_term;
  cinfo->dest = (void *)dst;
}

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
image_jpeg_to_sv(image *im, int quality, SV *sv_buf)
{
  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  JSAMPROW row_pointer[1];
  int row_stride;
  unsigned char *data;
  int i, x;
  struct sv_dst_mgr dst;
  
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);
  image_jpeg_sv_dest(&cinfo, &dst, sv_buf);
  
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
    for (x = 0; x < cinfo.image_width; x++) {
      data[x + x + x]     = COL_RED(  im->outbuf[i]);
      data[x + x + x + 1] = COL_GREEN(im->outbuf[i]);
      data[x + x + x + 2] = COL_BLUE( im->outbuf[i]);
      i++;
    }
    row_pointer[0] = data;
    jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }
  
  jpeg_finish_compress(&cinfo);
  
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
