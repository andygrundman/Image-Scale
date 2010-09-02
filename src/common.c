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

#include "common.h"
#include "buffer.c"

int
_check_buf(PerlIO *infile, Buffer *buf, int min_wanted, int max_wanted)
{
  int ret = 1;
  
  // Do we have enough data?
  if ( buffer_len(buf) < min_wanted ) {
    // Read more data
    uint32_t read;
    uint32_t actual_wanted;
    unsigned char *tmp;

#ifdef _MSC_VER
    uint32_t pos_check = PerlIO_tell(infile);
#endif
    
    if (min_wanted > max_wanted) {
      max_wanted = min_wanted;
    }
    
    // Adjust actual amount to read by the amount we already have in the buffer
    actual_wanted = max_wanted - buffer_len(buf);

    New(0, tmp, actual_wanted, unsigned char);
    
    DEBUG_TRACE("Buffering from file @ %d (min_wanted %d, max_wanted %d, adjusted to %d)\n",
      (int)PerlIO_tell(infile), min_wanted, max_wanted, actual_wanted
    );

    if ( (read = PerlIO_read(infile, tmp, actual_wanted)) <= 0 ) {
      if ( PerlIO_error(infile) ) {
        warn("Error reading: %s (wanted %d)\n", strerror(errno), actual_wanted);
      }
      else {
        // Avoid printing this warning, let image format routines handle warnings and errors
        //warn("Error: Unable to read at least %d bytes from file.\n", min_wanted);
      }

      ret = 0;
      goto out;
    }

    buffer_append(buf, tmp, read);

    // Make sure we got enough
    if ( buffer_len(buf) < min_wanted ) {
      warn("Error: Unable to read at least %d bytes from file (only read %d).\n", min_wanted, read);
      ret = 0;
      goto out;
    }

#ifdef _MSC_VER
    // Bug 16095, weird off-by-one bug seen only on Win32 and only when reading a filehandle
    if (PerlIO_tell(infile) != pos_check + read) {
      //PerlIO_printf(PerlIO_stderr(), "Win32 bug, pos should be %d, but was %d\n", pos_check + read, PerlIO_tell(infile));
      PerlIO_seek(infile, pos_check + read, SEEK_SET);
    }
#endif

    DEBUG_TRACE("Buffered %d bytes, new pos %d\n", read, (int)PerlIO_tell(infile));

out:
    Safefree(tmp);
  }

  return ret;
}

off_t
_file_size(PerlIO *infile)
{
#ifdef _MSC_VER
  // Win32 doesn't work right with fstat
  off_t file_size;
  
  PerlIO_seek(infile, 0, SEEK_END);
  file_size = PerlIO_tell(infile);
  PerlIO_seek(infile, 0, SEEK_SET);
  
  return file_size;
#else
  struct stat buf;
  
  if ( !fstat( PerlIO_fileno(infile), &buf ) ) {
    return buf.st_size;
  }
  
  warn("Unable to stat: %s\n", strerror(errno));
  
  return 0;
#endif
}

#ifdef AUDIO_SCAN_DEBUG
void
hexdump(unsigned char *data, uint32_t size)
{
  unsigned char c;
  int i = 1;
  int n;
  char bytestr[4] = {0};
  char hexstr[ 16*3 + 5] = {0};
  char charstr[16*1 + 5] = {0};
  
  if (!size) {
    return;
  }
  
  for (n = 0; n < size; n++) {
    c = data[n];

    /* store hex str (for left side) */
    snprintf(bytestr, sizeof(bytestr), "%02x ", c);
    strncat(hexstr, bytestr, sizeof(hexstr)-strlen(hexstr)-1);

    /* store char str (for right side) */
    if (c < 21 || c > 0x7E) {
      c = '.';
    }
    snprintf(bytestr, sizeof(bytestr), "%c", c);
    strncat(charstr, bytestr, sizeof(charstr)-strlen(charstr)-1);

    if (i % 16 == 0) { 
      /* line completed */
      PerlIO_printf(PerlIO_stderr(), "%-50.50s  %s\n", hexstr, charstr);
      hexstr[0] = 0;
      charstr[0] = 0;
    }
    i++;
  }

  if (strlen(hexstr) > 0) {
    /* print rest of buffer if not empty */
    PerlIO_printf(PerlIO_stderr(), "%-50.50s  %s\n", hexstr, charstr);
  }
}
#endif
