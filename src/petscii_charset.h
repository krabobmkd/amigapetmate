#ifndef PETSCII_CHARSET_H
#define PETSCII_CHARSET_H

#include <exec/types.h>
#include "petscii_types.h"
#include "compilers.h"
/*
 * C64 ROM charset data.
 * Two charsets of 256 characters, each character is 8 bytes (8x8 pixels).
 * Each byte represents one row, MSB = leftmost pixel.
 *
 * Charset 0 (upper): Uppercase letters + graphics symbols
 * Charset 1 (lower): Lowercase letters + uppercase letters
 */

extern const UBYTE c64CharsetUpper[PETSCII_CHARSET_SIZE];
extern const UBYTE c64CharsetLower[PETSCII_CHARSET_SIZE];

/*
 * Character display order for the 16x16 selector grid.
 * Maps grid position (0-255) to screen code (0-255).
 * This matches the Petmate character selector layout.
 */
extern const UBYTE c64CharOrder[PETSCII_CHAR_COUNT];

/*
 * Reverse lookup: screen code -> grid position.
 */
extern const UBYTE c64CharOrderReverse[PETSCII_CHAR_COUNT];

/*
 * Get pointer to the 8-byte glyph data for a character.
 * charset: PETSCII_CHARSET_UPPER or PETSCII_CHARSET_LOWER
 * code: screen code 0-255
 */
//const UBYTE *PetsciiCharset_GetGlyph(UBYTE charset, UBYTE code);
INLINE const UBYTE *PetsciiCharset_GetGlyph(UBYTE charset, UBYTE code)
{
    if (charset == PETSCII_CHARSET_LOWER)
        return &c64CharsetLower[(UWORD)code <<3];
    return &c64CharsetUpper[(UWORD)code <<3];
}


#endif /* PETSCII_CHARSET_H */
