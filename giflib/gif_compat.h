#ifndef GIF_COMPAT_H
#define GIF_COMPAT_H

/*
 * gif_compat.h - stdint / stdbool shims for GCC 2.95 (AmigaOS native).
 *
 * GCC 2.95 pre-dates stdint.h and stdbool.h.  On Amiga m68k the sizes
 * are fixed: char=8, short=16, int=32, long=32.
 *
 * GCC 3+ and the Bebbo GCC 6 cross-compiler already ship both headers,
 * so we just forward to them in that case.
 */

#if defined(__GNUC__) && (__GNUC__ < 3)

/* --- stdint.h shim ------------------------------------------------- */
/* /gg/os-include/machine/types.h already provides the signed variants  */
/* (int8_t, int16_t, int32_t).  Only the unsigned ones are missing.     */
/* Guard macros follow the BSD/Amiga convention (_UINT8_T etc.) so that */
/* any header that already defined the type skips the redefinition.     */
#ifndef _UINT8_T
#define _UINT8_T
typedef unsigned char  uint8_t;
#endif

#ifndef _UINT16_T
#define _UINT16_T
typedef unsigned short uint16_t;
#endif

#ifndef _UINT32_T
#define _UINT32_T
typedef unsigned long  uint32_t;
#endif

/* --- size_t limits shim -------------------------------------------- */
#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

/* --- stdbool.h shim ------------------------------------------------ */
#ifndef _BOOL
#define _BOOL
typedef int bool;
#endif
#ifndef true
#define true  1
#endif
#ifndef false
#define false 0
#endif

#else

#include <stdint.h>
#include <stdbool.h>

#endif /* GCC version */

#endif /* GIF_COMPAT_H */
