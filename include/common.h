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

// Enable to support clock cycle test counts (x86 only)
//#define CLOCK_CYCLE_TESTS

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
#ifdef AUDIO_SCAN_DEBUG
void hexdump(unsigned char *data, uint32_t size);
#endif

// Based on code from http://www.agner.org/optimize/
// Test code to count clock cycles of some portion of the code
// Wrap code to be tested with CLOCK_START(n) ... CLOCK_END(n)
// where n is a count of iterations to run.  Code must support
// being run more than once.
#ifdef CLOCK_CYCLE_TESTS

// Read time stamp counter
// The return value is the internal clock count
int64_t ReadTSC() {
   int res[2];                              // store 64 bit result here
   
   #if defined (_LP64)                      // 64 bit mode
      __asm__ __volatile__  (               // serialize (save rbx)
      "xorl %%eax,%%eax \n push %%rbx \n cpuid \n"
       ::: "%rax", "%rcx", "%rdx");
      __asm__ __volatile__  (               // read TSC, store edx:eax in res
      "rdtsc\n"
       : "=a" (res[0]), "=d" (res[1]) );
      __asm__ __volatile__  (               // serialize again
      "xorl %%eax,%%eax \n cpuid \n pop %%rbx \n"
       ::: "%rax", "%rcx", "%rdx");
   #else                                    // 32 bit mode
      __asm__ __volatile__  (               // serialize (save ebx)
      "xorl %%eax,%%eax \n pushl %%ebx \n cpuid \n"
       ::: "%eax", "%ecx", "%edx");
      __asm__ __volatile__  (               // read TSC, store edx:eax in res
      "rdtsc\n"
       : "=a" (res[0]), "=d" (res[1]) );
      __asm__ __volatile__  (               // serialize again
      "xorl %%eax,%%eax \n cpuid \n popl %%ebx \n"
       ::: "%eax", "%ecx", "%edx");
   #endif
   
   return *(int64_t*)res;                   // return result
}

#define CLOCK_START(count) { \
  int ci; \
  int64_t before, overhead; \
  int64_t clocklist[count]; \
  for (ci=0; ci < count; ci++) { \
    before = ReadTSC(); \
    clocklist[ci] = ReadTSC() - before; \
  } \
  overhead = clocklist[0]; \
  for (ci=0; ci < count; ci++) { \
    if (clocklist[ci] < overhead) overhead = clocklist[ci]; \
  } \
  for (ci=0; ci < count; ci++) { \
    before = ReadTSC(); \

#define CLOCK_END(count) \
    clocklist[ci] = ReadTSC() - before; \
  } \
  for (ci=0; ci < count; ci++) { \
    clocklist[ci] -= overhead; \
  } \
  fprintf(stderr, "\n  test     clock cycles (overhead %lld)\n", overhead); \
  for (ci = 0; ci < count; ci++) { \
    fprintf(stderr, "%6i  %14G\n", ci+1, (double)(clocklist[ci])); \
  } \
}

#else
#define CLOCK_START(count)
#define CLOCK_END(count)
#endif
