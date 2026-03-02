#include "petscii_screenbuf.h"
#include "petscii_charset.h"

#include <proto/exec.h>
#include <proto/graphics.h>

/*
 * PetsciiScreenBuf - flat chunky render buffer for one PETSCII screen.
 *
 * The buffer stores one Amiga pen index per pixel, row-major:
 *   pixel(col*8+px, row*8+py) is at chunky[(row*8+py)*pixW + col*8+px]
 *
 * Glyph bits: each of the 8 bytes in the glyph represents one pixel row.
 * Bit 7 (MSB) is the leftmost pixel. Set bit -> foreground pen.
 * Clear bit -> background pen (screen->backgroundColor).
 */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   Internal: render one 8x8 cell into the flat chunky buffer.
   col, row: character grid position (not pixel position).
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
static void renderCell(UBYTE *chunky, ULONG pixW,
                        UWORD col, UWORD row,
                        UBYTE charCode, UBYTE charColor, UBYTE bgColor,
                        UBYTE charset,
                        const PetsciiStyle *style)
{
    const UBYTE *glyph;
    UBYTE fgPen, bgPen;
    ULONG baseOffset;
    UWORD py, px;
    UBYTE bits;

    glyph    = PetsciiCharset_GetGlyph(charset, charCode);
    fgPen    = (UBYTE)PetsciiStyle_Pen(style, charColor);
    bgPen    = (UBYTE)PetsciiStyle_Pen(style, bgColor);

    /* Offset of the top-left pixel of this cell */
    baseOffset = (ULONG)(row * 8) * pixW + (ULONG)(col * 8);

    for (py = 0; py < 8; py++) {
        bits = glyph[py];
        for (px = 0; px < 8; px++) {
            chunky[baseOffset + (ULONG)py * pixW + px] =
                (bits & 0x80) ? fgPen : bgPen;
            bits = (UBYTE)(bits << 1);
        }
    }
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   Public API
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

PetsciiScreenBuf *PetsciiScreenBuf_Create(UWORD screenW, UWORD screenH)
{
    PetsciiScreenBuf *buf;
    ULONG pixW, pixH, size;

    pixW = (ULONG)screenW * 8;
    pixH = (ULONG)screenH * 8;
    size = pixW * pixH;

    if (size == 0) return NULL;

    buf = (PetsciiScreenBuf *)AllocVec(sizeof(PetsciiScreenBuf), MEMF_CLEAR);
    if (!buf) return NULL;

    buf->chunky = (UBYTE *)AllocVec(size, MEMF_ANY);
    if (!buf->chunky) {
        FreeVec(buf);
        return NULL;
    }

    buf->pixW  = pixW;
    buf->pixH  = pixH;
    buf->valid = 0;

    return buf;
}

void PetsciiScreenBuf_Destroy(PetsciiScreenBuf *buf)
{
    if (!buf) return;
    if (buf->chunky) {
        FreeVec(buf->chunky);
        buf->chunky = NULL;
    }
    FreeVec(buf);
}

void PetsciiScreenBuf_RebuildFull(PetsciiScreenBuf *buf,
                                   const PetsciiScreen *scr,
                                   const PetsciiStyle *style)
{
    UWORD col, row;
    const PetsciiPixel *cell;

    if (!buf || !scr || !style || !buf->chunky) return;

    for (row = 0; row < scr->height; row++) {
        for (col = 0; col < scr->width; col++) {
            cell = PetsciiScreen_PixelAt(scr, col, row);
            renderCell(buf->chunky, buf->pixW,
                       col, row,
                       cell->code, cell->color, scr->backgroundColor,
                       scr->charset,
                       style);
        }
    }

    buf->valid = 1;
}

void PetsciiScreenBuf_UpdateCell(PetsciiScreenBuf *buf,
                                  const PetsciiScreen *scr,
                                  const PetsciiStyle *style,
                                  UWORD col, UWORD row)
{
    const PetsciiPixel *cell;

    if (!buf || !scr || !style || !buf->chunky) return;
    if (col >= scr->width || row >= scr->height) return;

    cell = PetsciiScreen_PixelAt(scr, col, row);
    renderCell(buf->chunky, buf->pixW,
               col, row,
               cell->code, cell->color, scr->backgroundColor,
               scr->charset,
               style);
}

void PetsciiScreenBuf_BlitNative(PetsciiScreenBuf *buf,
                                  struct RastPort *rp,
                                  WORD destX, WORD destY)
{
    if (!buf || !rp || !buf->chunky || !buf->valid) return;

    WriteChunkyPixels(rp,
                      (ULONG)(WORD)destX,
                      (ULONG)(WORD)destY,
                      (ULONG)(WORD)destX + buf->pixW - 1,
                      (ULONG)(WORD)destY + buf->pixH - 1,
                      buf->chunky,
                      (LONG)buf->pixW);
}

void PetsciiChunky_Scale(const UBYTE *src, ULONG srcW, ULONG srcH,
                          UBYTE       *dst, ULONG dstW, ULONG dstH)
{
    /*
     * Nearest-neighbor scaling with 16:16 fixed-point arithmetic.
     * No allocation; caller owns src and dst.
     */
    ULONG        xStep;
    ULONG        yStep;
    ULONG        sy16;
    ULONG        sx16;
    ULONG        dy;
    ULONG        dx;
    const UBYTE *srcRow;
    UBYTE       *dstRow;

    if (!src || !dst || srcW == 0 || srcH == 0 || dstW == 0 || dstH == 0) return;

    xStep = (srcW << 16) / dstW;
    yStep = (srcH << 16) / dstH;

    sy16 = 0;
    for (dy = 0; dy < dstH; dy++) {
        srcRow = src + (sy16 >> 16) * srcW;
        dstRow = dst + dy           * dstW;
        sx16 = 0;
        for (dx = 0; dx < dstW; dx++) {
            dstRow[dx] = srcRow[sx16 >> 16];
            sx16 += xStep;
        }
        sy16 += yStep;
    }
}

void PetsciiScreenBuf_RenderCells(UBYTE              *dst,
                                   ULONG               pixW,
                                   const PetsciiPixel *cells,
                                   UWORD               brushW,
                                   UWORD               brushH,
                                   UBYTE               bgColor,
                                   UBYTE               charset,
                                   const PetsciiStyle *style)
{
    UWORD col;
    UWORD row;

    if (!dst || !cells || !style || brushW == 0 || brushH == 0) return;

    for (row = 0; row < brushH; row++) {
        for (col = 0; col < brushW; col++) {
            const PetsciiPixel *cell = &cells[(ULONG)row * brushW + col];
            renderCell(dst, pixW,
                       col, row,
                       cell->code, cell->color, bgColor,
                       charset, style);
        }
    }
}

void PetsciiScreenBuf_BlitScaled(PetsciiScreenBuf *buf,
                                  struct RastPort *rp,
                                  WORD destX, WORD destY,
                                  UWORD destW, UWORD destH,
                                  UBYTE *tmpBuf)
{
    if (!buf || !rp || !buf->chunky || !buf->valid || !tmpBuf) return;
    if (destW == 0 || destH == 0) return;

    PetsciiChunky_Scale(buf->chunky, buf->pixW, buf->pixH,
                         tmpBuf, (ULONG)destW, (ULONG)destH);

    WriteChunkyPixels(rp,
                      (ULONG)(WORD)destX,
                      (ULONG)(WORD)destY,
                      (ULONG)(WORD)destX + (ULONG)destW - 1,
                      (ULONG)(WORD)destY + (ULONG)destH - 1,
                      tmpBuf,
                      (LONG)destW);
}
