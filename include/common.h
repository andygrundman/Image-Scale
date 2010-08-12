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
 
// Enable for debug output
//#define AUDIO_SCAN_DEBUG

#ifdef AUDIO_SCAN_DEBUG
# define DEBUG_TRACE(...) PerlIO_printf(PerlIO_stderr(), __VA_ARGS__)
#else
# define DEBUG_TRACE(...)
#endif

/* for PRIu64 */
#ifdef _MSC_VER
#include "pinttypes.h"
#else
#include <inttypes.h>
#endif

#include "buffer.h"

/* strlen the length automatically */
#define my_hv_store(a,b,c)     hv_store(a,b,strlen(b),c,0)
#define my_hv_store_ent(a,b,c) hv_store_ent(a,b,c,0)
#define my_hv_fetch(a,b)       hv_fetch(a,b,strlen(b),0)
#define my_hv_exists(a,b)      hv_exists(a,b,strlen(b))
#define my_hv_exists_ent(a,b)  hv_exists_ent(a,b,0)
#define my_hv_delete(a,b)      hv_delete(a,b,strlen(b),0)

#define GET_INT32BE(b) \
(i = (b[0] << 24) | (b[1] << 16) | b[2] << 8 | b[3], b += 4, i)

int _check_buf(PerlIO *infile, Buffer *buf, int size, int min_size);
off_t _file_size(PerlIO *infile);
