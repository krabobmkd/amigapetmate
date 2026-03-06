/*
 * PetsciiBrush - brush buffer for the PETSCII canvas brush tool.
 *
 * A brush is a rectangular block of PetsciiPixel cells captured from the
 * canvas by a lasso selection.  It is stamped onto the screen each time
 * the user clicks or drags in brush-paint mode.
 */

#include "petscii_brush.h"
#include <proto/exec.h>

/* ------------------------------------------------------------------ */

PetsciiBrush *PetsciiBrush_Create(UWORD w, UWORD h)
{
    PetsciiBrush *b;
    ULONG         count;

    if (w == 0 || h == 0) return NULL;

    count = (ULONG)w * (ULONG)h;

    b = (PetsciiBrush *)AllocVec(sizeof(PetsciiBrush), MEMF_CLEAR);
    if (!b) return NULL;

    b->cells = (PetsciiPixel *)AllocVec(count * sizeof(PetsciiPixel), MEMF_CLEAR);
    if (!b->cells) {
        FreeVec(b);
        return NULL;
    }

    b->w = w;
    b->h = h;
    return b;
}

/* ------------------------------------------------------------------ */

void PetsciiBrush_Destroy(PetsciiBrush *b)
{
    if (!b) return;
    if (b->cells) {
        FreeVec(b->cells);
        b->cells = NULL;
    }
    FreeVec(b);
}

/* ------------------------------------------------------------------ */

PetsciiBrush *PetsciiBrush_CaptureFromScreen(
    const PetsciiScreen *scr,
    UWORD col, UWORD row, UWORD w, UWORD h)
{
    PetsciiBrush *b;
    UWORD         r;
    UWORD         c;

    if (!scr || !scr->framebuf) return NULL;
    if (col >= scr->width || row >= scr->height) return NULL;

    /* Clip to screen bounds */
    if ((ULONG)col + (ULONG)w > (ULONG)scr->width)
        w = (UWORD)(scr->width  - col);
    if ((ULONG)row + (ULONG)h > (ULONG)scr->height)
        h = (UWORD)(scr->height - row);

    if (w == 0 || h == 0) return NULL;

    b = PetsciiBrush_Create(w, h);
    if (!b) return NULL;

    for (r = 0; r < h; r++) {
        for (c = 0; c < w; c++) {
            b->cells[(ULONG)r * w + c] =
                scr->framebuf[(ULONG)(row + r) * scr->width + (col + c)];
        }
    }

    return b;
}

/* ------------------------------------------------------------------ */

PetsciiBrush *PetsciiBrush_Transform(const PetsciiBrush *src,
                                      int                 transform,
                                      const UBYTE        *charTable)
{
    PetsciiBrush *dst;
    UWORD         dstW;
    UWORD         dstH;
    UWORD         r;
    UWORD         c;

    if (!src || !src->cells) return NULL;

    /* Rotations swap width and height; flips and 180 keep them the same */
    if (transform == 2 || transform == 4) { /* ROT90CW / ROT90CCW */
        dstW = src->h;
        dstH = src->w;
    } else {
        dstW = src->w;
        dstH = src->h;
    }

    dst = PetsciiBrush_Create(dstW, dstH);
    if (!dst) return NULL;

    for (r = 0; r < src->h; r++) {
        for (c = 0; c < src->w; c++) {
            PetsciiPixel pix;
            ULONG        dstIdx;

            pix = src->cells[(ULONG)r * src->w + c];

            switch (transform) {
                case 0: /* BRUSH_TRANSFORM_FLIP_X: mirror columns */
                    dstIdx = (ULONG)r * dstW + (dstW - 1 - c);
                    break;

                case 1: /* BRUSH_TRANSFORM_FLIP_Y: mirror rows */
                    dstIdx = (ULONG)(src->h - 1 - r) * dstW + c;
                    break;

                case 2: /* BRUSH_TRANSFORM_ROT90CW: dst[c][h-1-r] = src[r][c]
                         * dst.w=src.h, dst.h=src.w */
                    dstIdx = (ULONG)c * dstW + (src->h - 1 - r);
                    break;

                case 3: /* BRUSH_TRANSFORM_ROT180 */
                    dstIdx = (ULONG)(src->h - 1 - r) * dstW + (dstW - 1 - c);
                    break;

                case 4: /* BRUSH_TRANSFORM_ROT90CCW: dst[w-1-c][r] = src[r][c]
                         * dst.w=src.h, dst.h=src.w */
                    dstIdx = (ULONG)(src->w - 1 - c) * dstW + r;
                    break;

                default:
                    dstIdx = (ULONG)r * dstW + c;
                    break;
            }

            /* Apply character-code remapping if a table was provided */
            if (charTable)
                pix.code = charTable[pix.code];

            dst->cells[dstIdx] = pix;
        }
    }

    return dst;
}
