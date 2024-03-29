#include "stdint.h"
#include "limits.h"
#include <safe_math_macros.h>

typedef __SIZE_TYPE__ size_t;

extern int putchar (int);

extern void *memcpy (void *restrict, const void *restrict, size_t);
extern void *memset (void *, int, size_t);
extern int strcmp (const char *, const char *);

static inline void platform_main_begin(void)
{
}

static uint32_t crc32_tab[256];
static uint32_t crc32_context = 0xFFFFFFFFUL;

static void 
crc32_gentab (void)
{
	uint32_t crc;
	const uint32_t poly = 0xEDB88320UL;
	int i, j;
	
	for (i = 0; i < 256; i++) {
		crc = i;
		for (j = 8; j > 0; j--) {
			if (crc & 1) {
				crc = (crc >> 1) ^ poly;
			} else {
				crc >>= 1;
			}
		}
		crc32_tab[i] = crc;
	}
}

static void 
crc32_byte (uint8_t b)
{
	crc32_context = 
		((crc32_context >> 8) & 0x00FFFFFF) ^ 
		crc32_tab[(crc32_context ^ b) & 0xFF];
}

static void 
crc32_8bytes (uint64_t val)
{
	crc32_byte ((val>>0) & 0xff);
	crc32_byte ((val>>8) & 0xff);
	crc32_byte ((val>>16) & 0xff);
	crc32_byte ((val>>24) & 0xff);
	crc32_byte ((val>>32) & 0xff);
	crc32_byte ((val>>40) & 0xff);
	crc32_byte ((val>>48) & 0xff);
	crc32_byte ((val>>56) & 0xff);
}

static void 
transparent_crc (uint64_t val, char* vname, int flag)
{
	crc32_8bytes(val);
}

static inline void
puthex (int x)
{
    putchar ("0123456789abcdef"[x]);
}

static inline void
platform_main_end (uint32_t x, int flag)
{
  if (!flag) {
    const char *s;
    int i;
    for (s = "checksum = "; *s; ++s)
      putchar (*s);
    for (i=0; i<8; i++) {
      puthex (x & 0xf);
      x >>= 4;
    }
    putchar ('\n');
  }
}
