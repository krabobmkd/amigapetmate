#include "petscii_screenbuf.h"
#include "petscii_charset.h"

#include <proto/exec.h>
#include <proto/graphics.h>

/* jsust to get the palette */
#include "petmate.h"
#include "petscii_style.h"

#include <proto/cybergraphics.h>
#include <cybergraphics/cybergraphics.h>

extern struct Library *CyberGfxBase;

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

/* renderCell() has been inlined into each call site for 68000 performance:
 * - bgPen hoisted out of loops (constant per full render)
 * - rowStride = pixW*8 computed once
 * - all inner offsets use running additions, no per-pixel multiply
 */

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
    UBYTE              *chunky;
    ULONG               pixW;
    ULONG               rowStride;        /* pixW * 8: chunky advance per char row */
    UBYTE               bgPen;            /* constant for whole screen */
    ULONG               rowBase;          /* chunky offset to top of current char row */
    ULONG               cellBase;         /* chunky offset to top-left of current cell */
    ULONG               lineBase;         /* chunky offset to current pixel row */
    const PetsciiPixel *cellRow;          /* pointer to first cell of current char row */
    const PetsciiPixel *cell;
    const UBYTE        *glyph;
    UBYTE               fgPen;
    UBYTE               bits;
    UWORD               row, col, py, px;

    if (!buf || !scr || !style || !buf->chunky) return;

    chunky    = buf->chunky;
    pixW      = buf->pixW;
    rowStride = pixW * 8;
    bgPen     = (UBYTE)PetsciiStyle_BmPen(style, scr->backgroundColor);

    rowBase = 0;
    cellRow = scr->framebuf;
    for (row = 0; row < scr->height; row++) {
        cellBase = rowBase;
        for (col = 0; col < scr->width; col++) {
            cell     = cellRow + col;
            glyph    = PetsciiCharset_GetGlyph(scr->charset, cell->code);
            fgPen    = (UBYTE)PetsciiStyle_BmPen(style, cell->color);
            lineBase = cellBase;
            for (py = 0; py < 8; py++) {
                bits = glyph[py];
                for (px = 0; px < 8; px++) {
                    chunky[lineBase + px] = (bits & 0x80) ? fgPen : bgPen;
                    bits <<= 1;
                }
                lineBase += pixW;
            }
            cellBase += 8;
        }
        rowBase += rowStride;
        cellRow += scr->width;
    }

    buf->valid = 1;
    buf->stylesync = style->updateId;
}

void PetsciiScreenBuf_UpdateCell(PetsciiScreenBuf *buf,
                                  const PetsciiScreen *scr,
                                  const PetsciiStyle *style,
                                  UWORD col, UWORD row)
{
    const PetsciiPixel *cell;
    const UBYTE        *glyph;
    UBYTE               fgPen, bgPen;
    ULONG               lineBase;
    UBYTE               bits;
    UWORD               py, px;

    if (!buf || !scr || !style || !buf->chunky) return;
    if (col >= scr->width || row >= scr->height) return;

    cell     = PetsciiScreen_PixelAt(scr, col, row);
    glyph    = PetsciiCharset_GetGlyph(scr->charset, cell->code);
    fgPen    = (UBYTE)PetsciiStyle_BmPen(style, cell->color);
    bgPen    = (UBYTE)PetsciiStyle_BmPen(style, scr->backgroundColor);
    lineBase = ((ULONG)row << 3) * buf->pixW + ((ULONG)col << 3);

    for (py = 0; py < 8; py++) {
        bits = glyph[py];
        for (px = 0; px < 8; px++) {
            buf->chunky[lineBase + px] = (bits & 0x80) ? fgPen : bgPen;
            bits <<= 1;
        }
        lineBase += buf->pixW;
    }
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
void PetsciiScreenBuf_BlitNativeRGB(PetsciiScreenBuf *buf,
                                  struct RastPort *rp,
                                  WORD destX, WORD destY)
{
    if (!buf || !rp || !buf->chunky || !buf->valid) return;

    WriteLUTPixelArray(buf->chunky,0,0, // srcxy
                        buf->pixW, // buf->pixW, // bytes per row
                        rp,
                        (APTR)&app->style.paletteARGB[0],
                        destX,destY,
                        buf->pixW,buf->pixH,
                        CTABFMT_XRGB8
                        );
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
    UBYTE               bgPen;
    ULONG               rowStride;
    ULONG               rowBase;
    ULONG               cellBase;
    ULONG               lineBase;
    const PetsciiPixel *cellRow;
    const PetsciiPixel *cell;
    const UBYTE        *glyph;
    UBYTE               fgPen;
    UBYTE               bits;
    UWORD               row, col, py, px;

    if (!dst || !cells || !style || brushW == 0 || brushH == 0) return;

    bgPen     = (UBYTE)PetsciiStyle_BmPen(style, bgColor);
    rowStride = pixW * 8;

    rowBase = 0;
    cellRow = cells;
    for (row = 0; row < brushH; row++) {
        cellBase = rowBase;
        for (col = 0; col < brushW; col++) {
            cell     = cellRow + col;
            glyph    = PetsciiCharset_GetGlyph(charset, cell->code);
            fgPen    = (UBYTE)PetsciiStyle_BmPen(style, cell->color);
            lineBase = cellBase;
            for (py = 0; py < 8; py++) {
                bits = glyph[py];
                for (px = 0; px < 8; px++) {
                    dst[lineBase + px] = (bits & 0x80) ? fgPen : bgPen;
                    bits <<= 1;
                }
                lineBase += pixW;
            }
            cellBase += 8;
        }
        rowBase += rowStride;
        cellRow += brushW;
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


/* experimental unfinished, do not touch.  */
void PetsciiScreenBuf_BlitScaledRGB16(PetsciiScreenBuf *buf,
                                  struct RastPort *rp,
                                  WORD destX, WORD destY,
                                  UWORD destW, UWORD destH,
                                  UBYTE *tmpBuf)
{
    if(!CyberGfxBase) return;
    if (!buf || !rp || !buf->chunky || !buf->valid || !tmpBuf) return;
    if (destW == 0 || destH == 0) return;

    PetsciiChunky_Scale(buf->chunky, buf->pixW, buf->pixH,
                         tmpBuf, (ULONG)destW, (ULONG)destH);

    WriteLUTPixelArray(tmpBuf,0,0, // srcxy
                        destW, // buf->pixW, // bytes per row
                        rp,
                        (APTR)&app->style.paletteARGB[0],
                        destX,destY,
                        destW,destH,
                        CTABFMT_XRGB8
                        );

/*
ULONG        WriteLUTPixelArray(APTR, UWORD, UWORD, UWORD, struct RastPort *, APTR,
			        UWORD, UWORD, UWORD, UWORD, UBYTE);
*/
/*
APTR	srcRect - pointer to an array of pixels from which to fetch the
	          CLUT data. Pixels are specified in bytes, 8bits/pixel.
UWORD	(SrcX,SrcY) - starting point in the source rectangle
UWORD	SrcMod - The number of bytes per row in the source rectangle.
 struct RastPort *	RastPort -  pointer to a RastPort structure
APTR	CTable - pointer to the color table using the format specified
		with CTabFormat
UWORD	(DestX,DestY) - starting point in the RastPort
UWORD	(SizeX,SizeY) - size of the rectangle that should be transfered
UBYTE	CTabFormat - color table format in the source rectangle
		Currently supported formats are:
		 CTABFMT_XRGB8 - CTable is a pointer to a ULONG table
		 	which contains 256 entries. Each entry specifies the
			rgb colour value for the related index. The format
			is XXRRGGBB.

	count = WriteLUTPixelArray(srcRect,SrcX ,SrcY ,SrcMod,RastPort,
	D0		  	      A0   D0:16 D1:16 D2:16     A1
			CTable,DestX,DestY,SizeX,SizeY,CTabFormat)
			  A2   D3:16 D4:16 D5:16 D6:16      D7

	count = WriteLUTPixelArray(srcRect,SrcX ,SrcY ,SrcMod,RastPort,
	D0		  	      A0   D0:16 D1:16 D2:16     A1
			CTable,DestX,DestY,SizeX,SizeY,CTabFormat)
			*/

    // WriteChunkyPixels(rp,
    //                   (ULONG)(WORD)destX,
    //                   (ULONG)(WORD)destY,
    //                   (ULONG)(WORD)destX + (ULONG)destW - 1,
    //                   (ULONG)(WORD)destY + (ULONG)destH - 1,
    //                   tmpBuf,
    //                   (LONG)destW);
}
