/*
 * petscii_ascii.c - ASCII [0,127] to C64 screen code translation.
 *
 * Uses two 128-entry static lookup tables — one per charset.
 * Table type is signed char: valid screen codes 0-90 all fit,
 * and -1 is the sentinel for "no equivalent in charset".
 *
 * Table layout (indices match ASCII code points):
 *
 *   [0x00-0x1F]  Control chars    -> -1  (no mapping)
 *   [0x20]       ' ' (space)      -> 32  (valid)
 *   [0x21-0x3F]  !"#...?          -> direct (ASCII == screen code)
 *   [0x40]       '@'              -> 0
 *   [0x41-0x5A]  'A'-'Z'         -> upper: 1-26  / lower: 65-90
 *   [0x5B-0x5F]  [ \ ] ^ _       -> 27-31 (same in both charsets)
 *   [0x60]       '`'              -> -1  (no mapping)
 *   [0x61-0x7A]  'a'-'z'         -> 1-26 (both charsets)
 *   [0x7B-0x7F]  { | } ~ DEL     -> -1  (no mapping)
 */

#include "petscii_ascii.h"

/* ------------------------------------------------------------------ */
/* Upper charset lookup table                                          */
/* ------------------------------------------------------------------ */
/* 'A'-'Z' and 'a'-'z' both map to screen codes 1-26 (uppercase only  */
/* in the upper charset). -1 = no equivalent.                         */

static const signed char asciiToUpper[128] = {
    /* 0x00-0x07 */ -1, -1, -1, -1, -1, -1, -1, -1,
    /* 0x08-0x0F */ -1, -1, -1, -1, -1, -1, -1, -1,
    /* 0x10-0x17 */ -1, -1, -1, -1, -1, -1, -1, -1,
    /* 0x18-0x1F */ -1, -1, -1, -1, -1, -1, -1, -1,
    /* 0x20-0x27 */ 32, 33, 34, 35, 36, 37, 38, 39,  /* ' ' ! " # $ % & ' */
    /* 0x28-0x2F */ 40, 41, 42, 43, 44, 45, 46, 47,  /* ( ) * + , - . /   */
    /* 0x30-0x37 */ 48, 49, 50, 51, 52, 53, 54, 55,  /* 0 1 2 3 4 5 6 7   */
    /* 0x38-0x3F */ 56, 57, 58, 59, 60, 61, 62, 63,  /* 8 9 : ; < = > ?   */
    /* 0x40-0x47 */  0,  1,  2,  3,  4,  5,  6,  7,  /* @ A B C D E F G   */
    /* 0x48-0x4F */  8,  9, 10, 11, 12, 13, 14, 15,  /* H I J K L M N O   */
    /* 0x50-0x57 */ 16, 17, 18, 19, 20, 21, 22, 23,  /* P Q R S T U V W   */
    /* 0x58-0x5F */ 24, 25, 26, 27, 28, 29, 30, 31,  /* X Y Z [ \ ] ^ _   */
    /* 0x60-0x67 */ -1,  1,  2,  3,  4,  5,  6,  7,  /* ` a b c d e f g   */
    /* 0x68-0x6F */  8,  9, 10, 11, 12, 13, 14, 15,  /* h i j k l m n o   */
    /* 0x70-0x77 */ 16, 17, 18, 19, 20, 21, 22, 23,  /* p q r s t u v w   */
    /* 0x78-0x7F */ 24, 25, 26, -1, -1, -1, -1, -1   /* x y z { | } ~ DEL */
};

/* ------------------------------------------------------------------ */
/* Lower charset lookup table                                          */
/* ------------------------------------------------------------------ */
/* 'a'-'z' -> screen codes 1-26  (lowercase glyphs).                  */
/* 'A'-'Z' -> screen codes 65-90 (uppercase glyphs in lower charset). */
/* -1 = no equivalent.                                                 */

static const signed char asciiToLower[128] = {
    /* 0x00-0x07 */ -1, -1, -1, -1, -1, -1, -1, -1,
    /* 0x08-0x0F */ -1, -1, -1, -1, -1, -1, -1, -1,
    /* 0x10-0x17 */ -1, -1, -1, -1, -1, -1, -1, -1,
    /* 0x18-0x1F */ -1, -1, -1, -1, -1, -1, -1, -1,
    /* 0x20-0x27 */ 32, 33, 34, 35, 36, 37, 38, 39,  /* ' ' ! " # $ % & ' */
    /* 0x28-0x2F */ 40, 41, 42, 43, 44, 45, 46, 47,  /* ( ) * + , - . /   */
    /* 0x30-0x37 */ 48, 49, 50, 51, 52, 53, 54, 55,  /* 0 1 2 3 4 5 6 7   */
    /* 0x38-0x3F */ 56, 57, 58, 59, 60, 61, 62, 63,  /* 8 9 : ; < = > ?   */
    /* 0x40-0x47 */  0, 65, 66, 67, 68, 69, 70, 71,  /* @ A B C D E F G   */
    /* 0x48-0x4F */ 72, 73, 74, 75, 76, 77, 78, 79,  /* H I J K L M N O   */
    /* 0x50-0x57 */ 80, 81, 82, 83, 84, 85, 86, 87,  /* P Q R S T U V W   */
    /* 0x58-0x5F */ 88, 89, 90, 27, 28, 29, 30, 31,  /* X Y Z [ \ ] ^ _   */
    /* 0x60-0x67 */ -1,  1,  2,  3,  4,  5,  6,  7,  /* ` a b c d e f g   */
    /* 0x68-0x6F */  8,  9, 10, 11, 12, 13, 14, 15,  /* h i j k l m n o   */
    /* 0x70-0x77 */ 16, 17, 18, 19, 20, 21, 22, 23,  /* p q r s t u v w   */
    /* 0x78-0x7F */ 24, 25, 26, -1, -1, -1, -1, -1   /* x y z { | } ~ DEL */
};

/* ------------------------------------------------------------------ */
/* Public functions                                                    */
/* ------------------------------------------------------------------ */

int PetsciiAscii_ToUpperScreenCode(UBYTE ascii)
{
    if (ascii > 127) return -1;
    return (int)asciiToUpper[ascii];
}

int PetsciiAscii_ToLowerScreenCode(UBYTE ascii)
{
    if (ascii > 127) return -1;
    return (int)asciiToLower[ascii];
}
