/*
 * petmate_charset.h
 *
 * Character ordering tables extracted from Petmate9 (React/JS source).
 * Original source: src/utils/index.ts
 *
 * The character selector in Petmate is a 16-column x 17-row grid (272 cells).
 * The arrays below map grid cell position to PETSCII screencode:
 *
 *   screencode = charOrderXxx[row * 16 + col]
 *
 * The first 256 entries (grid rows 0-15) cover the 256 PETSCII screencodes
 * in a geometrically-sorted display order.
 * Entries 256-271 (row 16) are reserved for custom font slots and can be
 * ignored for standard PETSCII use.
 *
 * There are two orderings:
 *   UPPER - C64 uppercase / graphics charset (also used for DIRART mode)
 *   LOWER - C64 lowercase / text charset
 *
 * All other machine charsets (C128, C16, VIC-20, PET, CBM-E) reuse one of
 * these two orderings depending on their mode (upper/lower).
 *
 * Charset name strings (as used in Petmate workspace files):
 *   "upper"      - C64 uppercase/graphics,   uses UPPER ordering
 *   "lower"      - C64 lowercase/text,        uses LOWER ordering
 *   "dirart"     - C64 DIRART extended mode,  uses UPPER ordering
 *   "cbaseUpper" - CBM-E upper,               uses UPPER ordering
 *   "cbaseLower" - CBM-E lower,               uses LOWER ordering
 *   "c16Upper"   - C16/Plus4 upper,           uses UPPER ordering
 *   "c16Lower"   - C16/Plus4 lower,           uses LOWER ordering
 *   "c128Upper"  - C128 upper,                uses UPPER ordering
 *   "c128Lower"  - C128 lower,                uses LOWER ordering
 *   "vic20Upper" - VIC-20 upper,              uses UPPER ordering
 *   "vic20Lower" - VIC-20 lower,              uses LOWER ordering
 *   "petGfx"     - PET graphics,              uses UPPER ordering
 *   "petBiz"     - PET business/text,         uses LOWER ordering
 */

#ifndef PETMATE_CHARSET_H
#define PETMATE_CHARSET_H

#define PETMATE_CHARORDER_SIZE     272  /* 16 cols x 17 rows                  */
#define PETMATE_CHARORDER_PETSCII  256  /* first 256 entries = PETSCII chars   */
#define PETMATE_GRID_COLS           16
#define PETMATE_GRID_ROWS           17  /* row 16 = custom font slots 256-271  */
#define PETMATE_GRID_ROWS_PETSCII   16  /* rows 0-15 = standard PETSCII        */

/*
 * charOrderUpper[272]
 * Display ordering for the uppercase/graphics charset (and DIRART).
 * Index = grid position (row*16 + col), value = PETSCII screencode.
 */
extern const int petmate_char_order_upper[PETMATE_CHARORDER_SIZE];

/*
 * charOrderLower[272]
 * Display ordering for the lowercase/text charset.
 * Index = grid position (row*16 + col), value = PETSCII screencode.
 */
extern const int petmate_char_order_lower[PETMATE_CHARORDER_SIZE];

/*
 * dirartOrder[272]
 * Display ordering for DIRART extended mode.
 * Identical to charOrderUpper; provided separately for clarity.
 */
extern const int petmate_char_order_dirart[PETMATE_CHARORDER_SIZE];

/*
 * petmate_screencode_from_grid()
 * Returns the PETSCII screencode at grid position (col, row).
 * order must point to one of the petmate_char_order_* arrays.
 * Returns -1 if out of bounds.
 */
int petmate_screencode_from_grid(const int *order, int row, int col);

/*
 * petmate_grid_from_screencode()
 * Reverse lookup: finds grid row/col for a given screencode.
 * order must point to one of the petmate_char_order_* arrays.
 * Writes result into *out_row and *out_col.
 * Returns 1 on success, 0 if not found.
 */
int petmate_grid_from_screencode(const int *order, int screencode,
                                 int *out_row, int *out_col);

/*
 * petmate_petscii_to_screencode()
 * Converts a raw PETSCII byte (as found in a SEQ/PRG file) to a C64
 * screencode (index into the charset bitmap data).
 * This is the conversion used by Petmate's SEQ and CBASE importers.
 *
 * Reverse-video flag: if the C64 RVS-ON control code (0x12) is active,
 * add 0x80 to the returned screencode (sets the high bit).
 *
 * Returns the screencode, or -1 for control/unmapped bytes.
 *
 * Mapping (from src/utils/importers/seq2petscii.ts):
 *   PETSCII 0x20-0x3F  -> screencode = petscii
 *   PETSCII 0x40-0x5F  -> screencode = petscii - 0x40
 *   PETSCII 0x60-0x7F  -> screencode = petscii - 0x20
 *   PETSCII 0xA0-0xBF  -> screencode = petscii - 0x40
 *   PETSCII 0xC0-0xFE  -> screencode = petscii - 0x80
 *   PETSCII 0xFF       -> screencode = 94
 *   all others         -> -1 (control codes, not printable)
 */
int petmate_petscii_to_screencode(unsigned int petscii_byte);

#endif /* PETMATE_CHARSET_H */
