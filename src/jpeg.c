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
#define LE               0     // Exif byte orders
#define BE               1

// Unfortunately we need a global variable in order to display the filename
// during libjpeg output messages
#define FILENAME_LEN 255
char filename[FILENAME_LEN + 1];

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

static void
image_jpeg_parse_exif_orientation(image *im, Buffer *exif)
{
  int bo, offset, num_entries;
  
  buffer_consume(exif, 6); // Exif\0\0
  bo = (buffer_get_short(exif) == 0x4949) ? LE : BE;
  
  buffer_consume(exif, 2); // 0x2a00
  
  offset = (bo == LE) ? buffer_get_int_le(exif) : buffer_get_int(exif);    
  buffer_consume(exif, offset - 8); // skip to offset (from the start of the byte order)
  
  num_entries = (bo == LE) ? buffer_get_short_le(exif) : buffer_get_short(exif);
  
  // All we care about is the orientation
  while (num_entries--) {
    int type_id = (bo == LE) ? buffer_get_short_le(exif) : buffer_get_short(exif);
    
    if (type_id == 0x112) {
      buffer_consume(exif, 6);
      im->orientation = (bo == LE) ? buffer_get_short_le(exif) : buffer_get_short(exif);
      
      DEBUG_TRACE("Exif Orientation: %d\n", im->orientation);
      break;
    }
    
    buffer_consume(exif, 10);
  }
}

jmp_buf setjmp_buffer;
static void
libjpeg_error_handler(j_common_ptr cinfo)
{
  cinfo->err->output_message(cinfo);
  longjmp(setjmp_buffer, 1);
  return;
}

static void
libjpeg_output_message(j_common_ptr cinfo)
{
  char buffer[JMSG_LENGTH_MAX];

  /* Create the message */
  (*cinfo->err->format_message) (cinfo, buffer);
  
  warn("Image::Scale error: %s (%s)\n", buffer, filename);
}

void
image_jpeg_read_header(image *im, const char *file)
{
  if (file != NULL) {
    if ( (im->stdio_fp = fopen(file, "rb")) == NULL ) {
      croak("Image::Scale could not open %s for reading", file);
    }
  }
  else {
    im->stdio_fp = NULL;
  }
  
  Newz(0, im->cinfo, sizeof(struct jpeg_decompress_struct), struct jpeg_decompress_struct);
  im->memory_used += sizeof(struct jpeg_decompress_struct);
  
  Newz(0, im->jpeg_error_pub, sizeof(struct jpeg_error_mgr), struct jpeg_error_mgr);

  im->cinfo->err = jpeg_std_error(im->jpeg_error_pub);
  im->jpeg_error_pub->error_exit = libjpeg_error_handler;
  im->jpeg_error_pub->output_message = libjpeg_output_message;
  
  if (setjmp(setjmp_buffer)) {
    return;
  }
  
  // Save filename in case any warnings/errors occur
  strncpy(filename, SvPVX(im->path), FILENAME_LEN);
  if (sv_len(im->path) > FILENAME_LEN)
    filename[FILENAME_LEN] = 0;
  
  jpeg_create_decompress(im->cinfo);
  
  if (file != NULL) {
    // Reading from file
    jpeg_stdio_src(im->cinfo, im->stdio_fp);
  }
  else {
    // Reading from SV
    // XXX jpeg_mem_src only available in libjpeg-8
    //jpeg_mem_src(im->cinfo, (unsigned char *)SvPVX(im->sv_data), sv_len(im->sv_data));
  }
  
  // Save APP1 marker for EXIF, only need the first 1024 bytes
  jpeg_save_markers(im->cinfo, 0xE1, 1024);
  
  jpeg_read_header(im->cinfo, TRUE);
  
  im->width    = im->cinfo->image_width;
  im->height   = im->cinfo->image_height;
  im->channels = im->cinfo->num_components;
  
  // Process Exif looking for orientation tag
  if (im->cinfo->marker_list != NULL) {
    jpeg_saved_marker_ptr marker = im->cinfo->marker_list;

    while (marker != NULL) {
      DEBUG_TRACE("Found marker: %x len %d\n", marker->marker, marker->data_length);
      
      if (marker->marker == 0xE1 
        && marker->data[0] == 'E' && marker->data[1] == 'x'
        && marker->data[2] == 'i' && marker->data[3] == 'f'
      ) {
        Buffer exif;
        
        buffer_init(&exif, marker->data_length);
        buffer_append(&exif, marker->data, marker->data_length);
        
        image_jpeg_parse_exif_orientation(im, &exif);
        
        buffer_free(&exif);
        break;
      }
      
      marker = marker->next;
    }
  }
}

void
image_jpeg_load(image *im)
{
  float scale_factor;
  int x, w, h, ofs;
  unsigned char *line[1], *ptr;
  
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
  
  // Save filename in case any warnings/errors occur
  strncpy(filename, SvPVX(im->path), FILENAME_LEN);
  if (sv_len(im->path) > FILENAME_LEN)
    filename[FILENAME_LEN] = 0;
  
  jpeg_start_decompress(im->cinfo);
  
  // Allocate storage for decompressed image
  image_alloc(im, w, h);
  
  ofs = 0;
  
  New(0, ptr, w * im->cinfo->output_components, unsigned char);
  line[0] = ptr;
  
  if (im->cinfo->output_components == 3) { // RGB
    while (im->cinfo->output_scanline < im->cinfo->output_height) {
      jpeg_read_scanlines(im->cinfo, line, 1);
      for (x = 0; x < w; x++) {
        im->pixbuf[ofs++] = COL(ptr[x + x + x], ptr[x + x + x + 1], ptr[x + x + x + 2]);
      }
    }
  }
  else if (im->cinfo->output_components == 4) { // CMYK inverted (Photoshop)
    while (im->cinfo->output_scanline < im->cinfo->output_height) {
      JSAMPROW row = *line;
      jpeg_read_scanlines(im->cinfo, line, 1);
      for (x = 0; x < w; x++) {
        int c = *row++;
        int m = *row++;
        int y = *row++;
        int k = *row++;
        
        im->pixbuf[ofs++] = COL((c * k) / MAXJSAMPLE, (m * k) / MAXJSAMPLE, (y * k) / MAXJSAMPLE);
      }
    }
  }
  else { // grayscale
    while (im->cinfo->output_scanline < im->cinfo->output_height) {
      jpeg_read_scanlines(im->cinfo, line, 1);
      for (x = 0; x < w; x++) {
        im->pixbuf[ofs++] = COL(ptr[x], ptr[x], ptr[x]);
      }
    }
  }
  
  Safefree(ptr);
}

static void
image_jpeg_compress(image *im, struct jpeg_compress_struct *cinfo, int quality)
{
  JSAMPROW row_pointer[1];
  int row_stride;
  unsigned char *data;
  int i, x;
  
  cinfo->image_width      = im->target_width;
  cinfo->image_height     = im->target_height;
  cinfo->input_components = 3;
  cinfo->in_color_space   = JCS_RGB; // output is always RGB even if source was grayscale
  
  jpeg_set_defaults(cinfo);
  jpeg_set_quality(cinfo, quality, TRUE);
  jpeg_start_compress(cinfo, TRUE);
  
  row_stride = cinfo->image_width * 3;
  New(0, data, row_stride, unsigned char);
  
  i = 0;
  while (cinfo->next_scanline < cinfo->image_height) {
    for (x = 0; x < cinfo->image_width; x++) {
      data[x + x + x]     = COL_RED(  im->outbuf[i]);
      data[x + x + x + 1] = COL_GREEN(im->outbuf[i]);
      data[x + x + x + 2] = COL_BLUE( im->outbuf[i]);
      i++;
    }
    row_pointer[0] = data;
    jpeg_write_scanlines(cinfo, row_pointer, 1);
  }
  
  jpeg_finish_compress(cinfo);
  jpeg_destroy_compress(cinfo);
  Safefree(data);
}

void
image_jpeg_save(image *im, const char *path, int quality)
{
  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  FILE *out;
  
  if ((out = fopen(path, "wb")) == NULL) {
    croak("Image::Scale cannot open %s for writing", path);
  }
  
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);
  jpeg_stdio_dest(&cinfo, out);
  
  image_jpeg_compress(im, &cinfo, quality);

  fclose(out);
}

void
image_jpeg_to_sv(image *im, int quality, SV *sv_buf)
{
  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  struct sv_dst_mgr dst;
  
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);
  image_jpeg_sv_dest(&cinfo, &dst, sv_buf);
  
  image_jpeg_compress(im, &cinfo, quality);
}

void
image_jpeg_finish(image *im)
{
  if (im->cinfo != NULL) {
    jpeg_destroy_decompress(im->cinfo);
    Safefree(im->cinfo);
    im->cinfo = NULL;
    im->memory_used -= sizeof(struct jpeg_decompress_struct);
    
    Safefree(im->jpeg_error_pub);
    im->jpeg_error_pub = NULL;
  }
  
  if (im->stdio_fp != NULL) {
    fclose(im->stdio_fp);
    im->stdio_fp = NULL;
  }
}
