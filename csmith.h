#include "stdint.h"
#include "limits.h"
#include "/usr/include/csmith/safe_math_macros.h"

extern int putchar (int);

static inline void platform_main_begin(void)
{
}

static inline void crc32_gentab (void)
{
}

uint64_t crc32_context;

static inline
int strcmp (const char *s1, const char *s2)
{
  for(; *s1 == *s2; ++s1, ++s2)
    if(*s1 == 0)
      return 0;
  return *(unsigned char *)s1 < *(unsigned char *)s2 ? -1 : 1;
}

static inline void
transparent_crc (uint64_t val, char* vname, int flag)
{
  crc32_context += val;
}

static void
transparent_crc_bytes (char *ptr, int nbytes, char* vname, int flag)
{
  int i;
  for (i=0; i<nbytes; i++) {
    crc32_context += ptr[i];
  }
}

static inline void
my_puts (char *p)
{
  int i = 0;
  while (p[i]) {
    putchar (p[i]);
    i++;
  }
}

static inline void
my_puthex (int x)
{
    putchar("0123456789abcdef"[x]);
}

static inline void
platform_main_end (uint64_t x, int flag)
{
  if (!flag) {
    int i;
    my_puts ("checksum = ");
    for (i=0; i<16; i++) {
      my_puthex (x & 0xf);
      x >>= 4;
    }
    putchar ('\n');
  }
}
