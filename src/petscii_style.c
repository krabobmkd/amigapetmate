#include "petscii_style.h"
#include "petscii_palette.h"

#include <proto/exec.h>
#include <proto/graphics.h>
#include <intuition/screens.h>
#include <graphics/view.h>

/*
 * ObtainPenForRGB - expand 8-bit 0x00RRGGBB to 32-bit and call ObtainBestPenA.
 * Falls back to FindColor if the pen allocation fails.
 * Pattern from aukboopsi/aukstylesheet.c.
 */
static void ObtainPenForRGB(struct ColorMap *cm, ManagedColor *c)
{
    ULONG r, g, b;
    LONG pen;

    if (!cm || !c) return;

    r = (c->rgbcolor >> 16) & 0xFF;
    g = (c->rgbcolor >>  8) & 0xFF;
    b =  c->rgbcolor        & 0xFF;

    /* Expand 8-bit to full 32-bit range (0xFF -> 0xFFFFFFFF) */
    r = (r << 24) | (r << 16) | (r << 8) | r;
    g = (g << 24) | (g << 16) | (g << 8) | g;
    b = (b << 24) | (b << 16) | (b << 8) | b;


    pen = ObtainBestPenA(cm, r, g, b, NULL);

    if (pen != -1) {
        c->pen = (WORD)pen;
        c->allocated = 1;
    } else {
        /* Fallback: find nearest existing pen without allocating */
        c->pen = (WORD)FindColor(cm, r, g, b, 255);
        c->allocated = 0;
    }
}

static void ReleasePenIfValid(struct ColorMap *cm, ManagedColor *c)
{
    if (!cm || !c) return;
    if (c->allocated && c->pen >= 0) {
        ReleasePen(cm, (LONG)c->pen);
    }
    c->pen = -1;
    c->allocated = 0;
}

void PetsciiStyle_Init(PetsciiStyle *style, UBYTE paletteID)
{
    UWORD i;

    if (!style) return;
    if (paletteID >= PALETTE_COUNT) paletteID = PALETTE_PETMATE;

    style->paletteId = paletteID;

    for (i = 0; i < C64_COLOR_COUNT; i++) {
        style->c64pens[i].rgbcolor  = c64Palettes[paletteID][i];
        style->c64pens[i].pen       = 1;  /* pen 1 is always a safe fallback */
        style->c64pens[i].allocated = 0;
    }
    style->screen = NULL;
}

int PetsciiStyle_Apply(PetsciiStyle *style, struct Screen *scr)
{
    struct ColorMap *cm;
    UWORD i;

    if (!style) return 0;

    /* Release any previously obtained pens first */
    PetsciiStyle_Release(style);

    style->screen = scr;
    if (!scr) return 1;

    cm = scr->ViewPort.ColorMap;
    for (i = 0; i < C64_COLOR_COUNT; i++) {
        ObtainPenForRGB(cm, &style->c64pens[i]);
    }

    return 1;
}

void PetsciiStyle_Release(PetsciiStyle *style)
{
    struct ColorMap *cm;
    UWORD i;

    if (!style || !style->screen) return;

    cm = style->screen->ViewPort.ColorMap;
    for (i = 0; i < C64_COLOR_COUNT; i++) {
        ReleasePenIfValid(cm, &style->c64pens[i]);
    }
    style->screen = NULL;
}
