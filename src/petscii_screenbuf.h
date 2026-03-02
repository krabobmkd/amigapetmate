#ifndef PETSCII_SCREENBUF_H
#define PETSCII_SCREENBUF_H

/*
 * PetsciiScreenBuf - Flat chunky-pixel render buffer for a PETSCII screen.
 *
 * Holds one Amiga pen index per pixel for the full PETSCII screen at
 * native character resolution (pixW = width*8, pixH = height*8).
 * Layout: row-major, no scramble. Row y starts at y*pixW.
 * This is exactly what WriteChunkyPixels() expects.
 *
 * Used by:
 *  - PetsciiCanvas gadget (1:1 or zoomed blit)
 *  - CharSelector gadget (its own screenbuf for the glyph grid)
 *  - Screen tab miniatures (BlitScaled to small target)
 */

#include <exec/types.h>
#include "petscii_screen.h"
#include "petscii_style.h"

/* Forward declaration */
struct RastPort;

typedef struct PetsciiScreenBuf {
    UBYTE *chunky;  /* flat [pixH * pixW] pen indices, row-major */
    ULONG  pixW;    /* screen->width  * 8  (e.g. 320 for 40-wide) */
    ULONG  pixH;    /* screen->height * 8  (e.g. 200 for 25-tall) */
    UBYTE  valid;   /* 0 = needs full rebuild */
} PetsciiScreenBuf;

/* Allocate a render buffer for a screen of given character dimensions.
 * screenW/screenH are character counts (e.g. 40, 25).
 * Returns NULL on allocation failure. */
PetsciiScreenBuf *PetsciiScreenBuf_Create(UWORD screenW, UWORD screenH);

/* Free the buffer and all allocated memory. */
void PetsciiScreenBuf_Destroy(PetsciiScreenBuf *buf);

/* Rebuild the entire buffer from a screen and style.
 * Call after palette change, charset change, or full-screen edit. */
void PetsciiScreenBuf_RebuildFull(PetsciiScreenBuf *buf,
                                   const PetsciiScreen *scr,
                                   const PetsciiStyle *style);

/* Rebuild a single 8x8 character cell in the buffer.
 * Call after each draw stroke for incremental updates. */
void PetsciiScreenBuf_UpdateCell(PetsciiScreenBuf *buf,
                                  const PetsciiScreen *scr,
                                  const PetsciiStyle *style,
                                  UWORD col, UWORD row);

/* Blit at 1:1 scale — single WriteChunkyPixels() call.
 * destX/destY: top-left destination in the RastPort. */
void PetsciiScreenBuf_BlitNative(PetsciiScreenBuf *buf,
                                  struct RastPort *rp,
                                  WORD destX, WORD destY);

/* Blit with nearest-neighbor scaling to destW x destH.
 * Uses 16:16 fixed-point arithmetic (one add+shift per pixel, no per-pixel
 * multiply or divide).  No allocation is performed inside this function.
 * tmpBuf must be a caller-owned buffer of at least destW * destH bytes.
 * Callers should cache the buffer and reallocate only when dimensions change
 * (see PetsciiCanvasData.scaledBuf).
 * Handles both zoom-in (larger) and zoom-out (miniature thumbnails). */
void PetsciiScreenBuf_BlitScaled(PetsciiScreenBuf *buf,
                                  struct RastPort *rp,
                                  WORD destX, WORD destY,
                                  UWORD destW, UWORD destH,
                                  UBYTE *tmpBuf);

/* Scale any chunky pixel buffer using nearest-neighbor 16:16 fixed-point.
 * src: srcW x srcH bytes (row-major).  dst: dstW x dstH bytes (caller-alloc).
 * Does NOT call WriteChunkyPixels; only fills dst.
 * Used by BlitScaled internally and by the hover overlay renderer. */
void PetsciiChunky_Scale(const UBYTE *src, ULONG srcW, ULONG srcH,
                          UBYTE       *dst, ULONG dstW, ULONG dstH);

/* Render brushW x brushH character cells into a native (unscaled) chunky buffer.
 * dst must be at least brushW*8 * brushH*8 bytes; pixW = brushW * 8 (row stride).
 * cells: flat [row * brushW + col] array of PetsciiPixel.
 * bgColor: screen background C64 colour index (0-15). */
void PetsciiScreenBuf_RenderCells(UBYTE              *dst,
                                   ULONG               pixW,
                                   const PetsciiPixel *cells,
                                   UWORD               brushW,
                                   UWORD               brushH,
                                   UBYTE               bgColor,
                                   UBYTE               charset,
                                   const PetsciiStyle *style);

#endif /* PETSCII_SCREENBUF_H */
