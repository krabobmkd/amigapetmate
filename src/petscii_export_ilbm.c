/*
 * petscii_export_ilbm.c - Export PETSCII screen as IFF ILBM image.
 *
 * IFF ILBM structure written:
 *
 *   FORM <size> ILBM
 *     BMHD <20>   BitMapHeader (w, h, depth=4, no mask, no compression)
 *     CMAP <48>   ColorMap: 16 x (R,G,B) from style->c64pens[].rgbcolor
 *     BODY <n>    Interleaved bitplanes, row-major, no compression
 *
 * All IFF fields are big-endian.  Row stride is word-aligned:
 *   rowBytes = ((width + 15) / 16) * 2
 * BODY size = imgH * 4_planes * rowBytes
 *
 * Pixel values in the chunky render buffer are C64 colour indices 0-15,
 * NOT Amiga screen pen numbers.  This keeps the CMAP and pixel data
 * in sync without any pen-number reverse mapping.
 *
 * C89 compatible.  Uses stdio + exec AllocVec/FreeVec.
 */

#include "petscii_export_ilbm.h"
#include "petscii_export.h"
#include "petscii_charset.h"
#include "petscii_types.h"

#include <stdio.h>
#include <string.h>
#include <proto/exec.h>

/* ------------------------------------------------------------------ */
/* Big-endian write helpers                                             */
/* ------------------------------------------------------------------ */

static void writeU32(FILE *fp, ULONG v)
{
    UBYTE b[4];
    b[0] = (UBYTE)(v >> 24);
    b[1] = (UBYTE)(v >> 16);
    b[2] = (UBYTE)(v >>  8);
    b[3] = (UBYTE)(v);
    fwrite(b, 1, 4, fp);
}

static void writeU16(FILE *fp, UWORD v)
{
    UBYTE b[2];
    b[0] = (UBYTE)(v >> 8);
    b[1] = (UBYTE)(v);
    fwrite(b, 1, 2, fp);
}

/* ------------------------------------------------------------------ */
/* Build a flat chunky buffer: 1 byte = C64 colour index (0-15)        */
/* Optional 8-pixel border filled with scr->borderColor.               */
/* Caller must FreeVec() the returned buffer.                          */
/* ------------------------------------------------------------------ */

static UBYTE *buildChunky(const PetsciiScreen *scr, BOOL withBorder,
                           ULONG *outW, ULONG *outH)
{
    ULONG        imgW, imgH, bofs;
    UBYTE       *chunky;
    ULONG        cx, cy, py, px, idx;
    const UBYTE *glyph;
    UBYTE        bits, fgIdx, bgIdx, borderIdx;

    imgW = (ULONG)scr->width  * 8UL;
    imgH = (ULONG)scr->height * 8UL;
    bofs = 0;

    if (withBorder) { imgW += 16UL; imgH += 16UL; bofs = 8UL; }

    chunky = (UBYTE *)AllocVec(imgW * imgH, MEMF_ANY);
    if (!chunky) return NULL;

    /* Fill entire buffer with border colour first (covers border strip) */
    borderIdx = scr->borderColor & 0x0F;
    {
        ULONG n;
        for (n = 0; n < imgW * imgH; n++)
            chunky[n] = borderIdx;
    }

    /* Render each character cell with direct C64 colour indices */
    bgIdx = scr->backgroundColor & 0x0F;
    for (cy = 0; cy < (ULONG)scr->height; cy++) {
        for (cx = 0; cx < (ULONG)scr->width; cx++) {
            PetsciiPixel pix = scr->framebuf[cy * scr->width + cx];
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
/* Write interleaved BODY data: for each row, for each plane, rowBytes */
/* Returns total bytes written (== bodySize).                          */
/* ------------------------------------------------------------------ */

static ULONG writeBody(FILE *fp, const UBYTE *chunky,
                        ULONG imgW, ULONG imgH, ULONG rowBytes)
{
    ULONG  y, plane, x, b;
    UBYTE  byte;
    ULONG  written = 0;

    for (y = 0; y < imgH; y++) {
        for (plane = 0; plane < 4UL; plane++) {
            ULONG pmask = (ULONG)(1UL << plane);
            for (x = 0; x < rowBytes * 8UL; x += 8UL) {
                byte = 0;
                for (b = 0; b < 8UL; b++) {
                    if ((x + b) < imgW &&
                        (chunky[y * imgW + x + b] & pmask))
                        byte |= (UBYTE)(0x80U >> b);
                }
                fputc((int)byte, fp);
            }
            written += rowBytes;
        }
    }
    return written;
}

/* ------------------------------------------------------------------ */
/* Public                                                               */
/* ------------------------------------------------------------------ */

int PetsciiExport_SaveILBM(const PetsciiScreen *scr,
                            const PetsciiStyle  *style,
                            const char          *path,
                            BOOL                 withBorder)
{
    UBYTE *chunky;
    ULONG  imgW, imgH;
    ULONG  rowBytes, bodySize, formSize;
    FILE  *fp;
    int    i;

    if (!scr || !style || !path) return PETSCII_EXPORT_EOPEN;

    chunky = buildChunky(scr, withBorder, &imgW, &imgH);
    if (!chunky) return PETSCII_EXPORT_ENOMEM;

    fp = fopen(path, "wb");
    if (!fp) { FreeVec(chunky); return PETSCII_EXPORT_EOPEN; }

    rowBytes = ((imgW + 15UL) / 16UL) * 2UL;
    bodySize = imgH * 4UL * rowBytes;

    /*
     * FORM size = bytes after the 8-byte FORM header =
     *   4 (ILBM tag) + 8+20 (BMHD) + 8+48 (CMAP) + 8+bodySize (BODY)
     */
    formSize = 4UL + 28UL + 56UL + 8UL + bodySize;

    /* ---- FORM ---- */
    fwrite("FORM", 1, 4, fp);
    writeU32(fp, formSize);
    fwrite("ILBM", 1, 4, fp);

    /* ---- BMHD ---- */
    fwrite("BMHD", 1, 4, fp);
    writeU32(fp, 20UL);
    writeU16(fp, (UWORD)imgW);    /* w                   */
    writeU16(fp, (UWORD)imgH);    /* h                   */
    writeU16(fp, 0);              /* x page position     */
    writeU16(fp, 0);              /* y page position     */
    fputc(4,  fp);                /* nPlanes = 4         */
    fputc(0,  fp);                /* masking: none       */
    fputc(0,  fp);                /* compression: none   */
    fputc(0,  fp);                /* pad1                */
    writeU16(fp, 0);              /* transparentColor    */
    fputc(10, fp);                /* xAspect             */
    fputc(11, fp);                /* yAspect             */
    writeU16(fp, (UWORD)imgW);    /* pageWidth           */
    writeU16(fp, (UWORD)imgH);    /* pageHeight          */

    /* ---- CMAP ---- */
    fwrite("CMAP", 1, 4, fp);
    writeU32(fp, 48UL);           /* 16 colours x 3 bytes */
    for (i = 0; i < 16; i++) {
        ULONG rgb = style->c64pens[i].rgbcolor;
        fputc((int)((rgb >> 16) & 0xFF), fp); /* R */
        fputc((int)((rgb >>  8) & 0xFF), fp); /* G */
        fputc((int)( rgb        & 0xFF), fp); /* B */
    }

    /* ---- BODY ---- */
    fwrite("BODY", 1, 4, fp);
    writeU32(fp, bodySize);
    writeBody(fp, chunky, imgW, imgH, rowBytes);

    fclose(fp);
    FreeVec(chunky);
    return PETSCII_EXPORT_OK;
}
