#ifndef PETSCII_BRUSH_H
#define PETSCII_BRUSH_H

/*
 * PetsciiBrush - a rectangular array of character cells for the brush tool.
 *
 * Cells are stored row-major: cells[row * w + col].
 * The brush is always owned by whoever created it; the canvas owns its
 * internal brush (created by lasso capture) and frees it in OM_DISPOSE
 * or when a new lasso replaces it.
 */

#include <exec/types.h>
#include "petscii_types.h"
#include "petscii_screen.h"

typedef struct PetsciiBrush {
    PetsciiPixel *cells;   /* owned, row-major grid */
    UWORD         w;        /* width  in chars */
    UWORD         h;        /* height in chars */
} PetsciiBrush;

/* Allocate an empty (zeroed) brush of given dimensions.
 * Returns NULL on failure or zero dimensions. */
PetsciiBrush *PetsciiBrush_Create(UWORD w, UWORD h);

/* Free the brush and its cell data. Safe to call with NULL. */
void PetsciiBrush_Destroy(PetsciiBrush *b);

/* Capture a rectangle from a screen into a new brush.
 * col/row is the top-left corner; w/h is the requested size.
 * Clips silently to screen bounds.
 * Returns NULL on OOM or zero-size after clipping. */
PetsciiBrush *PetsciiBrush_CaptureFromScreen(
    const PetsciiScreen *scr,
    UWORD col, UWORD row, UWORD w, UWORD h);

#endif /* PETSCII_BRUSH_H */
