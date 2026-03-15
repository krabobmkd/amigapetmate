#ifndef PETSCII_STYLE_H
#define PETSCII_STYLE_H

/*
 * PetsciiStyle - C64 color pen management for Amiga screen.
 *
 * Maps the 16 C64 color indices to Amiga screen pens.
 * RGB values are loaded from a palette at Init time.
 * Actual pens are allocated via ObtainBestPenA during Apply.
 * Release pens before unlocking the screen.
 *
 * Pattern adapted from aukboopsi/aukstylesheet.c.
 */

#include <exec/types.h>
#include "petscii_types.h"

/* Forward declaration - full definition in <intuition/screens.h> */
struct Screen;

/* ManagedColor - tracks an Amiga pen obtained for a specific RGB color */
typedef struct {
    ULONG rgbcolor;    /* 0x00RRGGBB format */
    WORD  pen;         /* Amiga pen index (-1 = invalid) */
    UBYTE  allocated;   /* 1 if obtained via ObtainBestPenA, 0 if FindColor fallback */
    UBYTE bmpen; /* pen used either by WriteChunkyPixels() or CGX WriteLUTPixels() */
} ManagedColor;

/* PetsciiStyle - holds the 16 C64 color pens for a given palette */
typedef struct PetsciiStyle {
    ManagedColor c64pens[C64_COLOR_COUNT]; /* One pen per C64 color index */
    struct Screen *screen;                  /* Screen pens were obtained from -> dangerous to retain screen, it can switch */
    int     paletteId; /* managed preference, as enum in palette.*/

     /* the graphics LoadRGB32() format, used for fullscreen 16c mode */
    ULONG paletteRGB32[2+(C64_COLOR_COUNT*3)];
    /* the optional one for CGX WriteLUTPixelArray */
    ULONG paletteARGB[C64_COLOR_COUNT];
} PetsciiStyle;

/* Initialize style from palette ID - fills rgbcolor fields, no pens yet.
 * paletteID: one of PALETTE_PETMATE, PALETTE_COLODORE, etc. */
void PetsciiStyle_Init(PetsciiStyle *style, UBYTE paletteID);

/* Obtain Amiga screen pens for all 16 C64 colors.
 * Call after locking the screen.
 * Returns 1 on success, 0 if style or scr is NULL. */
int PetsciiStyle_Apply(PetsciiStyle *style, struct Screen *scr);

/* Release all obtained pens back to the screen colormap.
 * Call before unlocking the screen. */
void PetsciiStyle_Release(PetsciiStyle *style);

/* Get pen number for a C64 color index (0-15) */
#define PetsciiStyle_Pen(style, c64color) \
    ((style)->c64pens[(c64color) & 0x0F].pen)

#define PetsciiStyle_BmPen(style, c64color) \
    ((style)->c64pens[(c64color) & 0x0F].bmpen)

#endif /* PETSCII_STYLE_H */
