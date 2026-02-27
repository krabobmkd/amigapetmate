#ifndef PETSCII_PALETTE_H
#define PETSCII_PALETTE_H

#include <exec/types.h>
#include "petscii_types.h"

/*
 * C64 color palettes.
 * Each color stored as 0x00RRGGBB.
 * Four palette variants from the Petmate editor.
 */

extern const ULONG c64Palettes[PALETTE_COUNT][C64_COLOR_COUNT];
extern const char *c64PaletteNames[PALETTE_COUNT];

#endif /* PETSCII_PALETTE_H */
