/*
 * petscii_export_gif.c - Export PETSCII screen as a GIF image via giflib.
 *
 * Uses the giflib 4.2 encoding API:
 *   EGifOpenFileName  -> GifFileType *
 *   EGifPutScreenDesc -> global screen descriptor + colour map
 *   EGifPutImageDesc  -> image descriptor (no local colour map)
 *   EGifPutLine       -> one row of pixel indices at a time
 *   EGifCloseFile     -> flush and close
 *
 * Pixel values are C64 colour indices 0-15, matching the global colour
 * map built from style->c64pens[].rgbcolor.  No Amiga pen numbers are
 * involved.
 *
 * GIF requires the colour map size to be a power of two; 16 colours
 * (2^4) fits naturally.
 *
 * C89 compatible (variable declarations at the top of each block).
 */

#include "petscii_export_gif.h"
#include "petscii_export.h"
#include "petscii_charset.h"
#include "petscii_types.h"

#include <stdio.h>
#include <string.h>
#include <proto/exec.h>

/* giflib public header (installed via target_include_directories in CMake) */
#include "gif_lib.h"

/* ------------------------------------------------------------------ */
/* Build a flat chunky buffer: 1 byte = C64 colour index (0-15).      */
/* Optional 8-pixel border filled with scr->borderColor.              */
/* Caller must FreeVec() the returned buffer.                         */
/* ------------------------------------------------------------------ */

static UBYTE *buildChunky(const PetsciiScreen *scr, BOOL withBorder,
                           ULONG *outW, ULONG *outH)
{
    ULONG        imgW, imgH, bofs;
    UBYTE       *chunky;
    ULONG        cx, cy, py, px, idx, n;
    const UBYTE *glyph;
    UBYTE        bits, fgIdx, bgIdx, borderIdx;
    PetsciiPixel pix;

    imgW = (ULONG)scr->width  * 8UL;
    imgH = (ULONG)scr->height * 8UL;
    bofs = 0;

    if (withBorder) { imgW += 16UL; imgH += 16UL; bofs = 8UL; }

    chunky = (UBYTE *)AllocVec(imgW * imgH, MEMF_ANY);
    if (!chunky) return NULL;

    borderIdx = scr->borderColor & 0x0F;
    for (n = 0; n < imgW * imgH; n++)
        chunky[n] = borderIdx;

    bgIdx = scr->backgroundColor & 0x0F;
    for (cy = 0; cy < (ULONG)scr->height; cy++) {
        for (cx = 0; cx < (ULONG)scr->width; cx++) {
            pix   = scr->framebuf[cy * scr->width + cx];
            fgIdx = pix.color & 0x0F;
            glyph = PetsciiCharset_GetGlyph(scr->charset, pix.code);
            for (py = 0; py < 8UL; py++) {
                bits = glyph[py];
                for (px = 0; px < 8UL; px++) {
                    idx = (bofs + cy * 8UL + py) * imgW
                        + (bofs + cx * 8UL + px);
                    chunky[idx] = (bits & 0x80) ? fgIdx : bgIdx;
                    bits = (UBYTE)(bits << 1);
                }
            }
        }
    }

    *outW = imgW;
    *outH = imgH;
    return chunky;
}

/* ------------------------------------------------------------------ */
/* Public                                                              */
/* ------------------------------------------------------------------ */

int PetsciiExport_SaveGIF(const PetsciiScreen *scr,
                          const PetsciiStyle  *style,
                          const char          *path,
                          BOOL                 withBorder)
{
    UBYTE          *chunky;
    ULONG           imgW, imgH;
    GifFileType    *gif;
    ColorMapObject *cmap;
    GifColorType    colors[C64_COLOR_COUNT];
    int             i;
    ULONG           row;
    int             ok = PETSCII_EXPORT_EWRITE;

    if (!scr || !style || !path) return PETSCII_EXPORT_EOPEN;

    /* --- Build C64-indexed chunky pixel buffer -------------------------- */
    chunky = buildChunky(scr, withBorder, &imgW, &imgH);
    if (!chunky) return PETSCII_EXPORT_ENOMEM;

    /* --- Build 16-entry GIF colour map from style ----------------------- */
    for (i = 0; i < C64_COLOR_COUNT; i++) {
        ULONG rgb = style->c64pens[i].rgbcolor;
        colors[i].Red   = (GifByteType)((rgb >> 16) & 0xFF);
        colors[i].Green = (GifByteType)((rgb >>  8) & 0xFF);
        colors[i].Blue  = (GifByteType)( rgb        & 0xFF);
    }

    /* MakeMapObject allocates and fills a ColorMapObject (2^4 = 16 entries) */
    cmap = MakeMapObject(C64_COLOR_COUNT, colors);
    if (!cmap) {
        FreeVec(chunky);
        return PETSCII_EXPORT_ENOMEM;
    }

    /* --- Open GIF output file ------------------------------------------- */
    gif = EGifOpenFileName((char *)path, false);
    if (!gif) {
        FreeMapObject(cmap);
        FreeVec(chunky);
        return PETSCII_EXPORT_EOPEN;
    }

    /* --- Screen descriptor: full image size, 16 colours, bg = border ---- */
    /* ColorRes: bits per primary colour - 1.  We pass 4 (15-bit palette)   */
    /* Background: C64 border colour index as the GIF background colour.    */
    if (EGifPutScreenDesc(gif,
                          (int)imgW, (int)imgH,
                          4,                           /* colour resolution */
                          (int)(scr->borderColor & 0x0F),
                          cmap) == GIF_ERROR) {
        EGifCloseFile(gif);
        FreeMapObject(cmap);
        FreeVec(chunky);
        return PETSCII_EXPORT_EWRITE;
    }

    /* --- Image descriptor: single image, top-left, no local colour map -- */
    if (EGifPutImageDesc(gif,
                         0, 0,
                         (int)imgW, (int)imgH,
                         false,  /* not interlaced */
                         NULL)   /* use global colour map */
            == GIF_ERROR) {
        EGifCloseFile(gif);
        FreeMapObject(cmap);
        FreeVec(chunky);
        return PETSCII_EXPORT_EWRITE;
    }

    /* --- Write pixel data row by row ------------------------------------ */
    ok = PETSCII_EXPORT_OK;
    for (row = 0; row < imgH; row++) {
        if (EGifPutLine(gif,
                        (GifPixelType *)(chunky + row * imgW),
                        (int)imgW) == GIF_ERROR) {
            ok = PETSCII_EXPORT_EWRITE;
            break;
        }
    }

    EGifCloseFile(gif);
    FreeMapObject(cmap);
    FreeVec(chunky);
    return ok;
}
