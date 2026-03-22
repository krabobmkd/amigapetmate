/*
 * PetsciiScreenMini - 42x27 miniature thumbnail renderer.
 *
 * One pixel per character cell, 1-pixel border frame using border color.
 * Char color vs background color chosen via Minify density tables.
 */

#include "petscii_screenmini.h"
#include "petscii_chartransform.h"
#include "petscii_style.h"
#include "petscii_types.h"

#include <proto/exec.h>
#include <proto/graphics.h>

#include "petmate.h"
#include <proto/cybergraphics.h>
#include <cybergraphics/cybergraphics.h>

extern struct Library *CyberGfxBase;



/* ------------------------------------------------------------------ */
/* Create / Destroy                                                     */
/* ------------------------------------------------------------------ */

PetsciiScreenMini *PetsciiScreenMini_Create(void)
{
    return (PetsciiScreenMini *)AllocVec(sizeof(PetsciiScreenMini),
                                         MEMF_ANY | MEMF_CLEAR);
}

void PetsciiScreenMini_Destroy(PetsciiScreenMini *mini)
{
    if (mini)
        FreeVec(mini);
}

/* ------------------------------------------------------------------ */
/* Render                                                               */
/* ------------------------------------------------------------------ */

void PetsciiScreenMini_Render(PetsciiScreenMini   *mini,
                               const PetsciiScreen *scr,
                               const PetsciiStyle  *style)
{
    const UBYTE      *minifyTable;
    const PetsciiPixel *cell;
    UBYTE             borderPen, bgPen;
    ULONG             col, row, i;
    ULONG             rowBase;

    if (!mini || !scr || !style) return;

    minifyTable = (scr->charset == PETSCII_CHARSET_LOWER)
                  ? petsciiLowerMinify
                  : petsciiUpperMinify;

    borderPen = (UBYTE)PetsciiStyle_BmPen(style, scr->borderColor);
    bgPen     = (UBYTE)PetsciiStyle_BmPen(style, scr->backgroundColor);

    /* --- Top border row (row 0) ---------------------------------------- */
    for (i = 0; i < SCREENMINI_W; i++)
        mini->pixels[i] = borderPen;

    /* --- Bottom border row (row SCREENMINI_H-1) ------------------------- */
    rowBase = (ULONG)(SCREENMINI_H - 1) * SCREENMINI_W;
    for (i = 0; i < SCREENMINI_W; i++)
        mini->pixels[rowBase + i] = borderPen;

    /* --- Character rows ------------------------------------------------- */
    for (row = 0; row < (ULONG)scr->height; row++) {
        rowBase = (row + 1) * SCREENMINI_W;

        /* Left border pixel */
        mini->pixels[rowBase] = borderPen;

        /* One pixel per character cell */
        for (col = 0; col < (ULONG)scr->width; col++) {
            UBYTE pen;
            cell = &scr->framebuf[row * (ULONG)scr->width + col];

            if (minifyTable[cell->code])
                pen = (UBYTE)PetsciiStyle_BmPen(style, cell->color);
            else
                pen = bgPen;

            mini->pixels[rowBase + col + 1] = pen;
        }

        /* Right border pixel */
        mini->pixels[rowBase + (ULONG)scr->width + 1] = borderPen;
    }

    mini->valid = 1;
}

/* ------------------------------------------------------------------ */
/* Blit                                                                 */
/* ------------------------------------------------------------------ */

void PetsciiScreenMini_Blit(const PetsciiScreenMini *mini,
                             struct RastPort         *rp,
                             WORD                     destX,
                             WORD                     destY)
{
    if (!mini || !rp || !mini->valid) return;

    WriteChunkyPixels(rp,
                      (LONG)destX,
                      (LONG)destY,
                      (LONG)(destX + SCREENMINI_W - 1),
                      (LONG)(destY + SCREENMINI_H - 1),
                      (UBYTE *)mini->pixels,
                      SCREENMINI_W);
}

void PetsciiScreenMini_BlitRGB(const PetsciiScreenMini *mini,
                             struct RastPort         *rp,
                             WORD                     destX,
                             WORD                     destY)
{
    if (!mini || !rp || !mini->valid) return;

    WriteLUTPixelArray(mini->pixels,0,0, // srcxy
                        SCREENMINI_W,  // bytes per row
                        rp,
                        (APTR)&app->style.paletteARGB[0],
                        destX,destY,
                        SCREENMINI_W,SCREENMINI_H,
                        CTABFMT_XRGB8
                        );
}
