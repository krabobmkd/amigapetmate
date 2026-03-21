#include "petscii_style.h"
#include "petscii_palette.h"

#include <proto/exec.h>
#include <proto/graphics.h>
#include <intuition/screens.h>
#include <graphics/view.h>
#include "boopsimainwindow.h"
#include <stdio.h>

#include <proto/cybergraphics.h>
#include <cybergraphics/cybergraphics.h>

extern struct Library *CyberGfxBase;


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
    c->bmpen = (UBYTE)c->pen; /* by default same, can be overriden for CGX */
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

    for (i = 0; i < C64_COLOR_COUNT; i++) {
        style->c64pens[i].rgbcolor  = c64Palettes[paletteID][i];
        style->c64pens[i].pen       = 1;  /* pen 1 is always a safe fallback */
        style->c64pens[i].allocated = 0;
    }

}
extern int CurrentMainScreen_PreIndexed;

int PetsciiStyle_Apply(PetsciiStyle *style, struct Screen *scr)
{
    WORD isRGB;
    UWORD i;

    if (!style) return 0;

    /* if something need to update from pens, it can check this.
        It will change if screen pixel mode change, or palett change.
     */
    style->updateId++;

    /* Release any previously obtained pens first */
    PetsciiStyle_Release(style);

    style->screen = scr; /* now screen optional can be null */

    isRGB = (CyberGfxBase && scr &&
       (GetCyberMapAttr(scr->RastPort.BitMap, CYBRMATTR_ISCYBERGFX) != 0) &&
       (GetCyberMapAttr(scr->RastPort.BitMap, CYBRMATTR_DEPTH) > 8)
        );

    /*0: use screen pens,
     * 1&2: use 16c C64 order slightly scrambled,
     * 2: fullscreen that need LoadRGB32()  */
    if(CurrentMainScreen_PreIndexed)
    {
        int cmindex[C64_COLOR_COUNT];

        ULONG *ppal32 = &style->paletteRGB32[0];
        /* 16 color index of screen is almost the C64 palette */
        for (i = 0; i < C64_COLOR_COUNT; i++) {
            style->c64pens[i].allocated =0;
            style->c64pens[i].pen = i;

            cmindex[i] = i;
        }
        style->c64pens[0].pen = 1;
        style->c64pens[1].pen = 2;
        style->c64pens[2].pen = 15;
        style->c64pens[15].pen = 0;

        cmindex[0] = 15;
        cmindex[1] = 0;
        cmindex[2] = 1;
        cmindex[15] = 2;

        *ppal32++ = (C64_COLOR_COUNT<<16)|0;
        for (i = 0; i < C64_COLOR_COUNT; i++) {
            ULONG rgb = style->c64pens[cmindex[i]].rgbcolor;
            *ppal32++ = (rgb & 0x00ff0000)<<8;
            *ppal32++ = (rgb & 0x0000ff00)<<16;
            *ppal32++ = (rgb & 0x000000ff)<<24;
            style->c64pens[cmindex[i]].bmpen = i;
        }
        *ppal32++  = 0;
        /* now in prescreen init need the RGB32 version before screen opening */
        if(scr)
         LoadRGB32(&scr->ViewPort,&style->paletteRGB32[0]);

    } else
    /* both screen pens mode and cgx mode need pens */
    if(scr)
    {
        int cmindex[C64_COLOR_COUNT];
        ULONG *ppal888 = &style->paletteARGB[0];
        struct ColorMap *cm = scr->ViewPort.ColorMap;
        for (i = 0; i < C64_COLOR_COUNT; i++) {
            ObtainPenForRGB(cm, &style->c64pens[i]);
        }
    }

    /* ARGB CLUT palette used by some CyberGraphics function */
   if(isRGB)
    {
        for (i = 0; i < C64_COLOR_COUNT; i++)
        {
            style->paletteARGB[i] = style->c64pens[i].rgbcolor;
            style->c64pens[i].bmpen = i;
        }
    }
    // style->paletteARGB[0] = style->c64pens[15].rgbcolor;
    // style->paletteARGB[1] = style->c64pens[0].rgbcolor;
    // style->paletteARGB[2] = style->c64pens[1].rgbcolor;
    // style->paletteARGB[15] = style->c64pens[2].rgbcolor;


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
