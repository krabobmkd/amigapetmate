#ifndef PETSCII_ASCII_H
#define PETSCII_ASCII_H

#include <exec/types.h>

/*
 * petscii_ascii.h - ASCII [0,127] to C64 screen code translation.
 *
 * Screen codes are the indices used by the C64 character ROM:
 *   0        = '@'
 *   1-26     = 'A'-'Z'  (upper charset) or 'a'-'z' (lower charset)
 *   27-31    = [ \ ] ^ _
 *   32-63    = same as ASCII 32-63 (space through '?')
 *   64-90    = (lower charset only) '@'-'Z' uppercase glyphs
 *
 * Returns -1 for characters with no equivalent in the charset.
 */

/*
 * PetsciiAscii_ToUpperScreenCode
 *
 * Translates an ASCII character (0-127) to a C64 *upper* charset screen code.
 * Both 'A'-'Z' (65-90) and 'a'-'z' (97-122) map to screen codes 1-26
 * because the upper charset contains only uppercase glyphs.
 *
 * Returns -1 for unmapped characters (controls, backtick, {|}~, etc.).
 */
int PetsciiAscii_ToUpperScreenCode(UBYTE ascii);

/*
 * PetsciiAscii_ToLowerScreenCode
 *
 * Translates an ASCII character (0-127) to a C64 *lower* charset screen code.
 * 'a'-'z' (97-122) -> screen codes 1-26   (lowercase glyphs)
 * 'A'-'Z' (65-90)  -> screen codes 65-90  (uppercase glyphs in lower charset)
 *
 * Returns -1 for unmapped characters (controls, backtick, {|}~, etc.).
 */
int PetsciiAscii_ToLowerScreenCode(UBYTE ascii);

#endif /* PETSCII_ASCII_H */
