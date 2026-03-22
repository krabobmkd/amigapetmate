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

/* Apply a geometric transformation to a brush.
 * transform: one of BRUSH_TRANSFORM_FLIP_X/Y, BRUSH_TRANSFORM_ROT90CW/CCW/180
 *            (constants defined in petscii_canvas.h via PCA_TransformBrush).
 * charTable: 256-entry table [src_code] -> dst_code for character remapping,
 *            e.g. petsciiUpperFlipX[] from petscii_chartransform.h.
 *            Pass NULL to rearrange cells without remapping character codes.
 * Returns a newly allocated PetsciiBrush (caller must destroy), or NULL. */
PetsciiBrush *PetsciiBrush_Transform(const PetsciiBrush *src,
                                      int                 transform,
                                      const UBYTE        *charTable);

#endif /* PETSCII_BRUSH_H */
