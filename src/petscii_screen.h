#ifndef PETSCII_SCREEN_H
#define PETSCII_SCREEN_H

#include <exec/types.h>
#include "petscii_types.h"

 struct PetsciiUndoBuffer;

/*
 * PetsciiScreen: one framebuffer of character cells.
 * Flat array storage: framebuf[row * width + col]
 */
typedef struct PetsciiScreen {
    PetsciiPixel *framebuf;     /* flat pixel array, allocated */
    UWORD width;                /* columns (default 40) */
    UWORD height;               /* rows (default 25) */
    UBYTE backgroundColor;      /* 0-15, screen background */
    UBYTE borderColor;          /* 0-15, border color */
    UBYTE charset;              /* PETSCII_CHARSET_UPPER or _LOWER */
    struct PetsciiUndoBuffer *undoBuf; /* NULL, inited at first edit  for managed screens. */
    char name[PETSCII_NAME_LEN];
} PetsciiScreen;

/* Create a new screen with default dimensions and colors */
PetsciiScreen *PetsciiScreen_Create(UWORD width, UWORD height);

/* Create a new screen with C64 defaults (40x25, blue bg, light blue border) */
PetsciiScreen *PetsciiScreen_CreateDefault(void);

/* Deep clone a screen */
PetsciiScreen *PetsciiScreen_Clone(const PetsciiScreen *src);

/* Destroy a screen and free its memory */
void PetsciiScreen_Destroy(PetsciiScreen *scr);

/* Clear all cells to space (code 32) with given color */
void PetsciiScreen_Clear(PetsciiScreen *scr, UBYTE color);

/* Set a single cell's character and color */
void PetsciiScreen_SetPixel(PetsciiScreen *scr, UWORD col, UWORD row,
                             UBYTE code, UBYTE color);

/* Get a single cell's data */
PetsciiPixel PetsciiScreen_GetPixel(const PetsciiScreen *scr,
                                     UWORD col, UWORD row);

/* Set only the character code, keep existing color */
void PetsciiScreen_SetCode(PetsciiScreen *scr, UWORD col, UWORD row,
                            UBYTE code);

/* Set only the color, keep existing character */
void PetsciiScreen_SetColor(PetsciiScreen *scr, UWORD col, UWORD row,
                             UBYTE color);

/* Copy the pixel data from src to dst (must be same dimensions) */
void PetsciiScreen_CopyData(PetsciiScreen *dst, const PetsciiScreen *src);

/* Shift screen contents in a direction (wraps around) */
void PetsciiScreen_ShiftLeft(PetsciiScreen *scr);
void PetsciiScreen_ShiftRight(PetsciiScreen *scr);
void PetsciiScreen_ShiftUp(PetsciiScreen *scr);
void PetsciiScreen_ShiftDown(PetsciiScreen *scr);

/* Get total cell count */
#define PetsciiScreen_CellCount(scr) ((ULONG)(scr)->width * (ULONG)(scr)->height)

/* Get pixel pointer for direct access (bounds not checked) */
#define PetsciiScreen_PixelAt(scr, col, row) \
    (&(scr)->framebuf[(row) * (scr)->width + (col)])

#endif /* PETSCII_SCREEN_H */
