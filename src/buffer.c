// Derived from:

/* $OpenBSD: buffer.c,v 1.31 2006/08/03 03:34:41 deraadt Exp $ */
/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Functions for manipulating fifo buffers (that can grow if needed).
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#include "buffer.h"

#define  BUFFER_MAX_CHUNK       0x1400000
#define  BUFFER_MAX_LEN         0x1400000
#define  BUFFER_ALLOCSZ         0x002000
#define  BUFFER_COMPACT_PERCENT 0.8

#define UnsignedToFloat(u) (((double)((long)(u - 2147483647L - 1))) + 2147483648.0)

/* Initializes the buffer structure. */

void
buffer_init(Buffer *buffer, uint32_t len)
{
  if (!len) len = BUFFER_ALLOCSZ;

  buffer->alloc = 0;
  New(0, buffer->buf, (int)len, u_char);
  buffer->alloc = len;
  buffer->offset = 0;
  buffer->end = 0;
  buffer->cache = 0;
  buffer->ncached = 0;

#ifdef AUDIO_SCAN_DEBUG
  PerlIO_printf(PerlIO_stderr(), "Buffer allocated with %d bytes\n", len);
#endif
}

/* Allows easy reuse of a buffer, will init or clear buffer if it already exists */

void
buffer_init_or_clear(Buffer *buffer, uint32_t len)
{
  if (!buffer->alloc) {
    buffer_init(buffer, len);
  }
  else {
    buffer_clear(buffer);
  }
}

/* Frees any memory used for the buffer. */

void
buffer_free(Buffer *buffer)
{
  if (buffer->alloc > 0) {
#ifdef AUDIO_SCAN_DEBUG
    PerlIO_printf(PerlIO_stderr(), "Buffer high water mark: %d\n", buffer->alloc);
#endif
    memset(buffer->buf, 0, buffer->alloc);
    buffer->alloc = 0;
    Safefree(buffer->buf);
  }
}

/*
 * Clears any data from the buffer, making it empty.  This does not actually
 * zero the memory.
 */

void
buffer_clear(Buffer *buffer)
{
  buffer->offset = 0;
  buffer->end = 0;
  buffer->cache = 0;
  buffer->ncached = 0;
}

/* Appends data to the buffer, expanding it if necessary. */

void
buffer_append(Buffer *buffer, const void *data, uint32_t len)
{
  void *p;
  p = buffer_append_space(buffer, len);
  Copy(data, p, (int)len, u_char);
}

static int
buffer_compact(Buffer *buffer)
{
  /*
   * If the buffer is at least BUFFER_COMPACT_PERCENT empty, move the
   * data to the beginning.
   */
  if (buffer->offset * 1.0 / buffer->alloc >= BUFFER_COMPACT_PERCENT ) {
#ifdef AUDIO_SCAN_DEBUG
    PerlIO_printf(PerlIO_stderr(), "Buffer compacting (%d -> %d)\n", buffer->offset + buffer_len(buffer), buffer_len(buffer));
#endif
    Move(buffer->buf + buffer->offset, buffer->buf, (int)(buffer->end - buffer->offset), u_char);
    buffer->end -= buffer->offset;
    buffer->offset = 0;
    return (1);
  }

  return (0);
}

/*
 * Appends space to the buffer, expanding the buffer if necessary. This does
 * not actually copy the data into the buffer, but instead returns a pointer
 * to the allocated region.
 */

void *
buffer_append_space(Buffer *buffer, uint32_t len)
{
  uint32_t newlen;
  void *p;

  if (len > BUFFER_MAX_CHUNK)
    croak("buffer_append_space: len %u too large (max %u)", len, BUFFER_MAX_CHUNK);

  /* If the buffer is empty, start using it from the beginning. */
  if (buffer->offset == buffer->end) {
    buffer->offset = 0;
    buffer->end = 0;
  }

restart:
  /* If there is enough space to store all data, store it now. */
  if (buffer->end + len <= buffer->alloc) {
    p = buffer->buf + buffer->end;
    buffer->end += len;
    return p;
  }

  /* Compact data back to the start of the buffer if necessary */
  if (buffer_compact(buffer))
    goto restart;

  /* Increase the size of the buffer and retry. */
  if (buffer->alloc + len < 4096)
    newlen = (buffer->alloc + len) * 2;
  else
    newlen = buffer->alloc + len + 4096;
  
  if (newlen > BUFFER_MAX_LEN)
    croak("buffer_append_space: alloc %u too large (max %u)",
        newlen, BUFFER_MAX_LEN);
#ifdef AUDIO_SCAN_DEBUG
  PerlIO_printf(PerlIO_stderr(), "Buffer extended to %d\n", newlen);
#endif
  Renew(buffer->buf, (int)newlen, u_char);
  buffer->alloc = newlen;
  goto restart;
  /* NOTREACHED */
}

/*
 * Check whether an allocation of 'len' will fit in the buffer
 * This must follow the same math as buffer_append_space
 */
int
buffer_check_alloc(Buffer *buffer, uint32_t len)
{
  if (buffer->offset == buffer->end) {
    buffer->offset = 0;
    buffer->end = 0;
  }
 restart:
  if (buffer->end + len < buffer->alloc)
    return (1);
  if (buffer_compact(buffer))
    goto restart;
  if (roundup(buffer->alloc + len, BUFFER_ALLOCSZ) <= BUFFER_MAX_LEN)
    return (1);
  return (0);
}

/* Returns the number of bytes of data in the buffer. */

uint32_t
buffer_len(Buffer *buffer)
{
  return buffer->end - buffer->offset;
}

/* Gets data from the beginning of the buffer. */

int
buffer_get_ret(Buffer *buffer, void *buf, uint32_t len)
{
  if (len > buffer->end - buffer->offset) {
    warn("buffer_get_ret: trying to get more bytes %d than in buffer %d", len, buffer->end - buffer->offset);
    return (-1);
  }

  Copy(buffer->buf + buffer->offset, buf, (int)len, char);
  buffer->offset += len;
  return (0);
}

void
buffer_get(Buffer *buffer, void *buf, uint32_t len)
{
  if (buffer_get_ret(buffer, buf, len) == -1)
    croak("buffer_get: buffer error");
}

/* Consumes the given number of bytes from the beginning of the buffer. */

int
buffer_consume_ret(Buffer *buffer, uint32_t bytes)
{
  if (bytes > buffer->end - buffer->offset) {
    warn("buffer_consume_ret: trying to get more bytes %d than in buffer %d", bytes, buffer->end - buffer->offset);
    return (-1);
  }

  buffer->offset += bytes;
  return (0);
}

void
buffer_consume(Buffer *buffer, uint32_t bytes)
{
  if (buffer_consume_ret(buffer, bytes) == -1)
    croak("buffer_consume: buffer error");
}

/* Consumes the given number of bytes from the end of the buffer. */

int
buffer_consume_end_ret(Buffer *buffer, uint32_t bytes)
{
  if (bytes > buffer->end - buffer->offset)
    return (-1);

  buffer->end -= bytes;
  return (0);
}

void
buffer_consume_end(Buffer *buffer, uint32_t bytes)
{
  if (buffer_consume_end_ret(buffer, bytes) == -1)
    croak("buffer_consume_end: trying to get more bytes %d than in buffer %d", bytes, buffer->end - buffer->offset);
}

/* Returns a pointer to the first used byte in the buffer. */

void *
buffer_ptr(Buffer *buffer)
{
  return buffer->buf + buffer->offset;
}

// Dumps the contents of the buffer to stderr.
// Based on: http://sws.dett.de/mini/hexdump-c/
#ifdef AUDIO_SCAN_DEBUG
void
buffer_dump(Buffer *buffer, uint32_t size)
{
  unsigned char *data = buffer->buf;
  unsigned char c;
  int i = 1;
  int n;
  char bytestr[4] = {0};
  char hexstr[ 16*3 + 5] = {0};
  char charstr[16*1 + 5] = {0};
  
  if (!size) {
    size = buffer->end - buffer->offset;
  }
  
  for (n = buffer->offset; n < buffer->offset + size; n++) {
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

// Useful functions from bufaux.c

/*
 * Returns a character from the buffer (0 - 255).
 */
int
buffer_get_char_ret(char *ret, Buffer *buffer)
{
  if (buffer_get_ret(buffer, ret, 1) == -1) {
    warn("buffer_get_char_ret: buffer_get_ret failed");
    return (-1);
  }

  return (0);
}

int
buffer_get_char(Buffer *buffer)
{
  char ch;

  if (buffer_get_char_ret(&ch, buffer) == -1)
    croak("buffer_get_char: buffer error");
  return (u_char) ch;
}

uint32_t
get_u32le(const void *vp)
{
  const u_char *p = (const u_char *)vp;
  uint32_t v;

  v  = (uint32_t)p[3] << 24;
  v |= (uint32_t)p[2] << 16;
  v |= (uint32_t)p[1] << 8;
  v |= (uint32_t)p[0];

  return (v);
}

int
buffer_get_int_le_ret(uint32_t *ret, Buffer *buffer)
{
  u_char buf[4];

  if (buffer_get_ret(buffer, (char *) buf, 4) == -1)
    return (-1);
  *ret = get_u32le(buf);
  return (0);
}

uint32_t
buffer_get_int_le(Buffer *buffer)
{
  uint32_t ret;

  if (buffer_get_int_le_ret(&ret, buffer) == -1)
    croak("buffer_get_int_le: buffer error");

  return (ret);
}

uint32_t
get_u32(const void *vp)
{
  const u_char *p = (const u_char *)vp;
  uint32_t v;

  v  = (uint32_t)p[0] << 24;
  v |= (uint32_t)p[1] << 16;
  v |= (uint32_t)p[2] << 8;
  v |= (uint32_t)p[3];

  return (v);
}

int
buffer_get_int_ret(uint32_t *ret, Buffer *buffer)
{
  u_char buf[4];

  if (buffer_get_ret(buffer, (char *) buf, 4) == -1)
    return (-1);
  *ret = get_u32(buf);
  return (0);
}

uint32_t
buffer_get_int(Buffer *buffer)
{
  uint32_t ret;

  if (buffer_get_int_ret(&ret, buffer) == -1)
    croak("buffer_get_int: buffer error");

  return (ret);
}

uint32_t
get_u24(const void *vp)
{
  const u_char *p = (const u_char *)vp;
  uint32_t v;

  v  = (uint32_t)p[0] << 16;
  v |= (uint32_t)p[1] << 8;
  v |= (uint32_t)p[2];

  return (v);
}

int
buffer_get_int24_ret(uint32_t *ret, Buffer *buffer)
{
  u_char buf[3];

  if (buffer_get_ret(buffer, (char *) buf, 3) == -1)
    return (-1);
  *ret = get_u24(buf);
  return (0);
}

uint32_t
buffer_get_int24(Buffer *buffer)
{
  uint32_t ret;

  if (buffer_get_int24_ret(&ret, buffer) == -1)
    croak("buffer_get_int24: buffer error");

  return (ret);
}

uint32_t
get_u24le(const void *vp)
{
  const u_char *p = (const u_char *)vp;
  uint32_t v;

  v  = (uint32_t)p[2] << 16;
  v |= (uint32_t)p[1] << 8;
  v |= (uint32_t)p[0];

  return (v);
}

int
buffer_get_int24_le_ret(uint32_t *ret, Buffer *buffer)
{
  u_char buf[3];

  if (buffer_get_ret(buffer, (char *) buf, 3) == -1)
    return (-1);
  *ret = get_u24le(buf);
  return (0);
}

uint32_t
buffer_get_int24_le(Buffer *buffer)
{
  uint32_t ret;

  if (buffer_get_int24_le_ret(&ret, buffer) == -1)
    croak("buffer_get_int24_le: buffer error");

  return (ret);
}

uint64_t
get_u64le(const void *vp)
{
  const u_char *p = (const u_char *)vp;
  uint64_t v;

  v  = (uint64_t)p[7] << 56;
  v |= (uint64_t)p[6] << 48;
  v |= (uint64_t)p[5] << 40;
  v |= (uint64_t)p[4] << 32;
  v |= (uint64_t)p[3] << 24;
  v |= (uint64_t)p[2] << 16;
  v |= (uint64_t)p[1] << 8;
  v |= (uint64_t)p[0];

  return (v);
}

int
buffer_get_int64_le_ret(uint64_t *ret, Buffer *buffer)
{
  u_char buf[8];

  if (buffer_get_ret(buffer, (char *) buf, 8) == -1)
    return (-1);
  *ret = get_u64le(buf);
  return (0);
}

uint64_t
buffer_get_int64_le(Buffer *buffer)
{
  uint64_t ret;

  if (buffer_get_int64_le_ret(&ret, buffer) == -1)
    croak("buffer_get_int64_le: buffer error");

  return (ret);
}

uint64_t
get_u64(const void *vp)
{
  const u_char *p = (const u_char *)vp;
  uint64_t v;

  v  = (uint64_t)p[0] << 56;
  v |= (uint64_t)p[1] << 48;
  v |= (uint64_t)p[2] << 40;
  v |= (uint64_t)p[3] << 32;
  v |= (uint64_t)p[4] << 24;
  v |= (uint64_t)p[5] << 16;
  v |= (uint64_t)p[6] << 8;
  v |= (uint64_t)p[7];

  return (v);
}

int
buffer_get_int64_ret(uint64_t *ret, Buffer *buffer)
{
  u_char buf[8];

  if (buffer_get_ret(buffer, (char *) buf, 8) == -1)
    return (-1);
  *ret = get_u64(buf);
  return (0);
}

uint64_t
buffer_get_int64(Buffer *buffer)
{
  uint64_t ret;

  if (buffer_get_int64_ret(&ret, buffer) == -1)
    croak("buffer_get_int64_le: buffer error");

  return (ret);
}

uint16_t
get_u16le(const void *vp)
{
  const u_char *p = (const u_char *)vp;
  uint16_t v;

  v  = (uint16_t)p[1] << 8;
  v |= (uint16_t)p[0];

  return (v);
}

int
buffer_get_short_le_ret(uint16_t *ret, Buffer *buffer)
{
  u_char buf[2];

  if (buffer_get_ret(buffer, (char *) buf, 2) == -1)
    return (-1);
  *ret = get_u16le(buf);
  return (0);
}

uint16_t
buffer_get_short_le(Buffer *buffer)
{
  uint16_t ret;

  if (buffer_get_short_le_ret(&ret, buffer) == -1)
    croak("buffer_get_short_le: buffer error");

  return (ret);
}

uint16_t
get_u16(const void *vp)
{
  const u_char *p = (const u_char *)vp;
  uint16_t v;

  v  = (uint16_t)p[0] << 8;
  v |= (uint16_t)p[1];

  return (v);
}

int
buffer_get_short_ret(uint16_t *ret, Buffer *buffer)
{
  u_char buf[2];

  if (buffer_get_ret(buffer, (char *) buf, 2) == -1)
    return (-1);
  *ret = get_u16(buf);
  return (0);
}

uint16_t
buffer_get_short(Buffer *buffer)
{
  uint16_t ret;

  if (buffer_get_short_ret(&ret, buffer) == -1)
    croak("buffer_get_short: buffer error");

  return (ret);
}

/*
 * Stores a character in the buffer.
 */
void
buffer_put_char(Buffer *buffer, int value)
{
  char ch = value;

  buffer_append(buffer, &ch, 1);
}

// Read a null-terminated UTF-8 string
// Caller must manage utf8 buffer (init/free)
uint32_t
buffer_get_utf8(Buffer *buffer, Buffer *utf8, uint32_t len_hint)
{
  int i = 0;
  unsigned char *bptr = buffer_ptr(buffer);
  
  if (!len_hint) return 0;
  
  for (i = 0; i < len_hint; i++) {
    uint8_t c = bptr[i];
    
    buffer_put_char(utf8, c);
    
    if (c == 0) {
      i++;
      break;
    }
  }
  
  // Consume string + null
  buffer_consume(buffer, i);
  
  // Add null if one wasn't provided
  if ( (utf8->buf + utf8->end - 1)[0] != 0 ) {
    buffer_put_char(utf8, 0);
  }
  
#ifdef AUDIO_SCAN_DEBUG
  //DEBUG_TRACE("utf8 buffer:\n");
  //buffer_dump(utf8, 0);
#endif
  
  return i;
}

// Read a null-terminated latin1 string, converting to UTF-8 in supplied buffer
// len_hint is the length of the latin1 string, utf8 may end up being larger
// or possibly less if we hit a null.
// Caller must manage utf8 buffer (init/free)
uint32_t
buffer_get_latin1_as_utf8(Buffer *buffer, Buffer *utf8, uint32_t len_hint)
{
  int i = 0;
  unsigned char *bptr = buffer_ptr(buffer);
  uint8_t is_utf8;
  
  if (!len_hint) return 0;
  
  // We may get a valid UTF-8 string in here from ID3v1 or
  // elsewhere, if so we don't want to translate from ISO-8859-1
  is_utf8 = is_utf8_string(bptr, len_hint);
  
  for (i = 0; i < len_hint; i++) {
    uint8_t c = bptr[i];
    
    if (is_utf8) {
      buffer_put_char(utf8, c);
    }
    else {
      // translate high chars from ISO-8859-1 to UTF-8
      if (c < 0x80) {
        buffer_put_char(utf8, c);
      }
      else if (c < 0xc0) {
        buffer_put_char(utf8, 0xc2);
        buffer_put_char(utf8, c);
      }
      else {
        buffer_put_char(utf8, 0xc3);
        buffer_put_char(utf8, c - 64);
      }
    }
    
    if (c == 0) {
      i++;
      break;
    }
  }
  
  // Consume string + null
  buffer_consume(buffer, i);
  
  // Add null if one wasn't provided
  if ( (utf8->buf + utf8->end - 1)[0] != 0 ) {
    buffer_put_char(utf8, 0);
  }
  
#ifdef AUDIO_SCAN_DEBUG
  //DEBUG_TRACE("utf8 buffer:\n");
  //buffer_dump(utf8, 0);
#endif
  
  return i;
}

// Read a null-terminated UTF-16 string, converting to UTF-8 in the supplied buffer
// Caller must manage utf8 buffer (init/free)
// XXX supports U+0000 ~ U+FFFF only.
uint32_t
buffer_get_utf16_as_utf8(Buffer *buffer, Buffer *utf8, uint32_t len, uint8_t byteorder)
{
  int i = 0;
  uint16_t wc = 0;
  
  if (!len) return 0;
  
  for (i = 0; i < len; i += 2) {
    // Check that we are not reading past the end of the buffer
    if (len - i >= 2) {
      wc = (byteorder == UTF16_BYTEORDER_LE)
        ? buffer_get_short_le(buffer)
        : buffer_get_short(buffer);
    }
    else {
      DEBUG_TRACE("    UTF-16 text has an odd number of bytes, skipping final byte\n");
      buffer_consume(buffer, 1);
      wc = 0;
    }

    if (wc < 0x80) {
      buffer_put_char(utf8, wc & 0xff);      
    }
    else if (wc < 0x800) {
      buffer_put_char(utf8, 0xc0 | (wc>>6));
      buffer_put_char(utf8, 0x80 | (wc & 0x3f));
    }
    else {
      buffer_put_char(utf8, 0xe0 | (wc>>12));
      buffer_put_char(utf8, 0x80 | ((wc>>6) & 0x3f));
      buffer_put_char(utf8, 0x80 | (wc & 0x3f));
    }
    
    if (wc == 0) {
      i += 2;
      break;
    }
  }
  
  // Add null if one wasn't provided
  if ( (utf8->buf + utf8->end - 1)[0] != 0 ) {
    buffer_put_char(utf8, 0);
  }
  
#ifdef AUDIO_SCAN_DEBUG
  //DEBUG_TRACE("utf8 buffer:\n");
  //buffer_dump(utf8, 0);
#endif
  
  return i;
}

#ifdef HAS_GUID
void
buffer_get_guid(Buffer *buffer, GUID *g)
{
  g->Data1 = buffer_get_int_le(buffer);
  g->Data2 = buffer_get_short_le(buffer);
  g->Data3 = buffer_get_short_le(buffer);
  
  buffer_get(buffer, g->Data4, 8);
}
#endif

int
buffer_get_float32_le_ret(float *ret, Buffer *buffer)
{
  u_char buf[4];

  if (buffer_get_ret(buffer, (char *) buf, 4) == -1)
    return (-1);
  *ret = get_f32le(buf);
  return (0);
}

float
buffer_get_float32_le(Buffer *buffer)
{
  float ret;

  if (buffer_get_float32_le_ret(&ret, buffer) == -1)
    croak("buffer_get_float32_le_ret: buffer error");

  return (ret);
}

// From libsndfile
float
get_f32le(const void *vp)
{
  const u_char *p = (const u_char *)vp;
  float v;
  int exponent, mantissa, negative;
  
  negative = p[3] & 0x80;
  exponent = ((p[3] & 0x7F) << 1) | ((p[2] & 0x80) ? 1 : 0);
  mantissa = ((p[2] & 0x7F) << 16) | (p[1] << 8) | (p[0]);
  
  if ( !(exponent || mantissa) ) {
    return 0.0;
  }
  
  mantissa |= 0x800000;
  exponent = exponent ? exponent - 127 : 0;
  
  v = mantissa ? ((float)mantissa) / ((float)0x800000) : 0.0;
  
  if (negative) {
    v *= -1;
  }
  
  if (exponent > 0) {
    v *= pow(2.0, exponent);
  }
  else if (exponent < 0) {
    v /= pow(2.0, abs(exponent));
  }

  return (v);
}

int
buffer_get_float32_ret(float *ret, Buffer *buffer)
{
  u_char buf[4];

  if (buffer_get_ret(buffer, (char *) buf, 4) == -1)
    return (-1);
  *ret = get_f32(buf);
  return (0);
}

float
buffer_get_float32(Buffer *buffer)
{
  float ret;

  if (buffer_get_float32_ret(&ret, buffer) == -1)
    croak("buffer_get_float32_ret: buffer error");

  return (ret);
}

// From libsndfile
float
get_f32(const void *vp)
{
  const u_char *p = (const u_char *)vp;
  float v;
  int exponent, mantissa, negative;
  
  negative = p[0] & 0x80;
  exponent = ((p[0] & 0x7F) << 1) | ((p[1] & 0x80) ? 1 : 0);
  mantissa = ((p[1] & 0x7F) << 16) | (p[2] << 8) | (p[3]);
  
  if ( !(exponent || mantissa) ) {
    return 0.0;
  }
  
  mantissa |= 0x800000;
  exponent = exponent ? exponent - 127 : 0;
  
  v = mantissa ? ((float)mantissa) / ((float)0x800000) : 0.0;
  
  if (negative) {
    v *= -1;
  }
  
  if (exponent > 0) {
    v *= pow(2.0, exponent);
  }
  else if (exponent < 0) {
    v /= pow(2.0, abs(exponent));
  }

  return (v);
}

// http://www.onicos.com/staff/iz/formats/aiff.html
// http://www.onicos.com/staff/iz/formats/ieee.c
double
buffer_get_ieee_float(Buffer *buffer)
{
  double f;
  int expon;
  unsigned long hiMant, loMant;
  
  unsigned char *bptr = buffer_ptr(buffer);
  
  expon  = ((bptr[0] & 0x7F) << 8) | (bptr[1] & 0xFF);
  hiMant = ((unsigned long)(bptr[2] & 0xFF) << 24)
      |    ((unsigned long)(bptr[3] & 0xFF) << 16)
      |    ((unsigned long)(bptr[4] & 0xFF) << 8)
      |    ((unsigned long)(bptr[5] & 0xFF));
  loMant = ((unsigned long)(bptr[6] & 0xFF) << 24)
      |    ((unsigned long)(bptr[7] & 0xFF) << 16)
      |    ((unsigned long)(bptr[8] & 0xFF) << 8)
      |    ((unsigned long)(bptr[9] & 0xFF));

  if (expon == 0 && hiMant == 0 && loMant == 0) {
    f = 0;
  }
  else {
    if (expon == 0x7FFF) {    /* Infinity or NaN */
      f = HUGE_VAL;
    }
    else {
      expon -= 16383;
      f  = ldexp(UnsignedToFloat(hiMant), expon-=31);
      f += ldexp(UnsignedToFloat(loMant), expon-=32);
    }
  }
  
  buffer_consume(buffer, 10);

  if (bptr[0] & 0x80)
    return -f;
  else
    return f;
}

void
put_u16(void *vp, uint16_t v)
{
  u_char *p = (u_char *)vp;
  
	p[0] = (u_char)(v >> 8) & 0xff;
	p[1] = (u_char)v & 0xff;
}

void
put_u24(void *vp, uint32_t v)
{
	u_char *p = (u_char *)vp;

	p[0] = (u_char)(v >> 16) & 0xff;
	p[1] = (u_char)(v >> 8) & 0xff;
	p[2] = (u_char)v & 0xff;
}

void
put_u32(void *vp, uint32_t v)
{
	u_char *p = (u_char *)vp;

	p[0] = (u_char)(v >> 24) & 0xff;
	p[1] = (u_char)(v >> 16) & 0xff;
	p[2] = (u_char)(v >> 8) & 0xff;
	p[3] = (u_char)v & 0xff;
}

void
buffer_put_int(Buffer *buffer, u_int value)
{
	char buf[4];

	put_u32(buf, value);
	buffer_append(buffer, buf, 4);
}

// Warnings:
// Do not request more than 32 bits at a time.
// Be careful if using other buffer functions without reading a multiple of 8 bits.
uint32_t
buffer_get_bits(Buffer *buffer, uint32_t bits)
{
  uint32_t mask = CacheMask[bits];
  
  //PerlIO_printf(PerlIO_stderr(), "get_bits(%d), in cache %d\n", bits, buffer->ncached);
  
  while (buffer->ncached < bits) {
    // Need to read more data
     
    //PerlIO_printf(PerlIO_stderr(), "reading: ");
    //buffer_dump(buffer, 1);
    
    buffer->cache = (buffer->cache << 8) | buffer_get_char(buffer);
    buffer->ncached += 8;
  }
  
  buffer->ncached -= bits;
  
  //PerlIO_printf(PerlIO_stderr(), "cache %x, ncached %d\n", buffer->cache, buffer->ncached);
  //PerlIO_printf(PerlIO_stderr(), "return %x\n", (buffer->cache >> buffer->ncached) & mask);
  
  return (buffer->cache >> buffer->ncached) & mask;
}

uint32_t
buffer_get_syncsafe(Buffer *buffer, uint8_t bytes)
{
  uint32_t value = 0;
  unsigned char *bptr = buffer_ptr(buffer);

  switch (bytes) {
  case 5: value = (value << 4) | (*bptr++ & 0x0f);
  case 4: value = (value << 7) | (*bptr++ & 0x7f);
          value = (value << 7) | (*bptr++ & 0x7f);
          value = (value << 7) | (*bptr++ & 0x7f);
          value = (value << 7) | (*bptr++ & 0x7f);
  }
  
  buffer_consume(buffer, bytes);

  return value;
}
