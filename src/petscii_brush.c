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
