/*
 * PetsciiCanvas - BOOPSI gadget class for the PETSCII editing canvas.
 * Rendering: GM_RENDER, GM_LAYOUT, GM_HITTEST, GM_HANDLEINPUT (Phase 4 stub).
 *
 * Scaling buffer strategy
 * -----------------------
 * PetsciiScreenBuf_BlitScaled() performs no allocation; the caller must
 * supply a pre-allocated buffer (inst->scaledBuf).  The buffer is allocated
 * once when the gadget is first laid out and reallocated only when the
 * content rect size changes (GM_LAYOUT from the ReAction layout manager).
 * GM_RENDER recomputes the content rect and calls ensureScaledBuf() as a
 * safety net in case GM_LAYOUT was never received.
 *
 * Aspect-ratio preservation (PCA_KeepRatio = TRUE)
 * -------------------------------------------------
 * The native pixel dimensions of the PETSCII canvas (screenbuf->pixW x
 * screenbuf->pixH, e.g. 320x200 for a 40x25 screen, 640x200 for 80x25,
 * or any other configured screen size) define the ideal ratio.
 * When keepRatio is TRUE the content is letter/pillarboxed inside the
 * gadget bounds.  Unused margin strips are erased with EraseRect() which
 * uses the screen background colour from Intuition/prefs.
 *
 * Grid overlay
 * ------------
 * Drawn in COMPLEMENT mode so lines are always visible on any background.
 * The grid is drawn inside the content rect only (not over margins).
 * Skipped when cells would be narrower than 2 pixels.
 *
 * WriteChunkyPixels requires graphics.library V40 (Kickstart 3.1+).
 * Since Petmate Amiga targets OS 3.9 this is always available.
 */

#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <intuition/gadgetclass.h>
#include "petscii_canvas_private.h"
#include "petscii_screenbuf.h"
#include <bdbprintf.h>

/*
 * CELL_PX(n, contentDim, pixDim)
 * Project char edge index n (0..screenW or 0..screenH) to a content-relative
 * pixel coordinate.  Same formula used by drawGrid() and the cursor highlight,
 * guaranteed to align with BlitScaled's 16:16 mapping.
 */
#define CELL_PX(n, cDim, pDim) \
    ((WORD)((ULONG)(n) * 8UL * (ULONG)(cDim) / (ULONG)(pDim)))
/* ------------------------------------------------------------------ */
/* Static: ensure inst->scaledBuf is (w x h) bytes.                   */
/* ------------------------------------------------------------------ */

static BOOL ensureScaledBuf(PetsciiCanvasData *inst, UWORD w, UWORD h)
{
    ULONG needed;

    if (inst->scaledW == w && inst->scaledH == h &&
        inst->scaledBuf && inst->overlayBuf)
        return TRUE;

    if (inst->scaledBuf) {
        FreeVec(inst->scaledBuf);
        inst->scaledBuf = NULL;
    }
    if (inst->overlayBuf) {
        FreeVec(inst->overlayBuf);
        inst->overlayBuf = NULL;
    }
    inst->scaledW = 0;
    inst->scaledH = 0;

    if (w == 0 || h == 0)
        return FALSE;

    needed           = (ULONG)w * (ULONG)h;
    inst->scaledBuf  = (UBYTE *)AllocVec(needed, MEMF_ANY);
    inst->overlayBuf = (UBYTE *)AllocVec(needed, MEMF_ANY);
    if (!inst->scaledBuf || !inst->overlayBuf) {
        if (inst->scaledBuf)  { FreeVec(inst->scaledBuf);  inst->scaledBuf  = NULL; }
        if (inst->overlayBuf) { FreeVec(inst->overlayBuf); inst->overlayBuf = NULL; }
        return FALSE;
    }

    inst->scaledW = w;
    inst->scaledH = h;
    return TRUE;
}

/* ------------------------------------------------------------------ */
/* Static: compute the largest sub-rect that preserves idealW:idealH  */
/* Result is gadget-relative (outX/Y are offsets from gadget origin). */
/* ------------------------------------------------------------------ */

static void computeContentRect(
    WORD gadW, WORD gadH,
    ULONG idealW, ULONG idealH,
    WORD *outX, WORD *outY, WORD *outW, WORD *outH)
{
    ULONG fitW;
    ULONG fitH;

    if (idealW == 0 || idealH == 0 || gadW <= 0 || gadH <= 0) {
        *outX = 0; *outY = 0; *outW = gadW; *outH = gadH;
        return;
    }

    /* Try fitting content to full width */
    fitH = (ULONG)gadW * idealH / idealW;
    if (fitH <= (ULONG)gadH) {
        *outW = gadW;
        *outH = (WORD)fitH;
        *outX = 0;
        *outY = (WORD)(((ULONG)gadH - fitH) / 2);
        return;
    }

    /* Otherwise fit to full height */
    fitW = (ULONG)gadH * idealW / idealH;
    if (fitW > (ULONG)gadW) fitW = (ULONG)gadW; /* safety clamp */
    *outW = (WORD)fitW;
    *outH = gadH;
    *outX = (WORD)(((ULONG)gadW - fitW) / 2);
    *outY = 0;
}

/* ------------------------------------------------------------------ */
/* Static: erase letterbox/pillarbox margins using EraseRect           */
/* EraseRect fills with the background colour from screen prefs.       */
/* ------------------------------------------------------------------ */

static void eraseMargins(struct RastPort *rp,
                          WORD gadLeft, WORD gadTop,
                          WORD gadW, WORD gadH,
                          WORD cX, WORD cY, WORD cW, WORD cH)
{
    /* Top strip */
    if (cY > 0)
        EraseRect(rp,
                  (LONG)gadLeft,              (LONG)gadTop,
                  (LONG)(gadLeft + gadW - 1), (LONG)(gadTop + cY - 1));

    /* Bottom strip */
    if (cY + cH < gadH)
        EraseRect(rp,
                  (LONG)gadLeft,              (LONG)(gadTop + cY + cH),
                  (LONG)(gadLeft + gadW - 1), (LONG)(gadTop + gadH - 1));

    /* Left strip (between top and bottom strips) */
    if (cX > 0)
        EraseRect(rp,
                  (LONG)gadLeft,             (LONG)(gadTop + cY),
                  (LONG)(gadLeft + cX - 1),  (LONG)(gadTop + cY + cH - 1));

    /* Right strip */
    if (cX + cW < gadW)
        EraseRect(rp,
                  (LONG)(gadLeft + cX + cW),  (LONG)(gadTop + cY),
                  (LONG)(gadLeft + gadW - 1),  (LONG)(gadTop + cY + cH - 1));
}

/* ------------------------------------------------------------------ */
/* Static: update inst->content* from current gadget size + keepRatio */
/*                                                                      */
/* The C64 display has a 1-character-wide border surrounding the       */
/* character grid.  We include this border in the aspect ratio so the  */
/* total rendered area is (scrW+2) x (scrH+2) character cells.         */
/*                                                                      */
/* inst->contentX/Y/W/H always refer to the INNER character grid       */
/* (no border), so mouseToCell() and screenbuf blitting are unaffected. */
/* The outer frame (including border) is computed in GM_RENDER.        */
/* ------------------------------------------------------------------ */

static void updateContentRect(PetsciiCanvasData *inst, WORD gadW, WORD gadH)
{
    UWORD scrW;
    UWORD scrH;
    WORD  outerX;
    WORD  outerY;
    WORD  outerW;
    WORD  outerH;
    WORD  cellW;
    WORD  cellH;

    if (!inst->screen || inst->screen->width == 0 || inst->screen->height == 0) {
        inst->contentX = 0;
        inst->contentY = 0;
        inst->contentW = gadW;
        inst->contentH = gadH;
        return;
    }

    scrW = inst->screen->width;
    scrH = inst->screen->height;

    if (inst->keepRatio) {
        /* Ideal total size includes a 1-char border on every side:
         * (scrW+2)*8 wide by (scrH+2)*8 tall                    */
        computeContentRect(gadW, gadH,
                           (ULONG)(scrW + 2) * 8,
                           (ULONG)(scrH + 2) * 8,
                           &outerX, &outerY, &outerW, &outerH);
    } else {
        outerX = 0;
        outerY = 0;
        outerW = gadW;
        outerH = gadH;
    }

    /* Per-cell pixel size derived from the outer frame */
    cellW = (scrW + 2 > 0) ? (WORD)(outerW / (WORD)(scrW + 2)) : 0;
    cellH = (scrH + 2 > 0) ? (WORD)(outerH / (WORD)(scrH + 2)) : 0;

    /* Inner character rect = outer frame minus 1-cell border on each side */
    inst->contentX = (WORD)(outerX + cellW);
    inst->contentY = (WORD)(outerY + cellH);
    inst->contentW = (WORD)(outerW - 2 * cellW);
    inst->contentH = (WORD)(outerH - 2 * cellH);
}

/* ------------------------------------------------------------------ */
/* Static: draw the character grid in COMPLEMENT mode                  */
/* ------------------------------------------------------------------ */

static void drawGrid(struct RastPort *rp,
                     WORD left, WORD top,
                     WORD width, WORD height,
                     PetsciiCanvasData *inst
                     )
{
    UBYTE savedMode;
    WORD  x=0;
    WORD  y=0;

    savedMode = rp->DrawMode;
    SetDrMd(rp, COMPLEMENT);
    SetAPen(rp, 1);

    while(1) {
        WORD  xx  = (WORD)(left + ((x*8*width)/inst->screenbuf->pixW) );
        if(xx>=(left+width)) break;

        Move(rp, (LONG)xx, (LONG)top);
        Draw(rp, (LONG)xx, (LONG)(top + height - 1));

        x++;
    }

   while(1) {
        WORD  yy  = (WORD)(top + ((y*8*height)/inst->screenbuf->pixH) );
        if(yy>=(top+height)) break;

        Move(rp, (LONG)left, (LONG)yy);
        Draw(rp, (LONG)(left + width - 1), (LONG)yy);
        y++;
    }

    SetDrMd(rp, (LONG)savedMode);
}

/* ------------------------------------------------------------------ */
/* Static: draw only the grid lines that cross a pixel sub-rect        */
/* px1/py1 inclusive, px2/py2 exclusive (content-relative pixels)     */
/* ------------------------------------------------------------------ */

static void drawGridSubRect(struct RastPort *rp,
                             WORD absX, WORD absY,
                             WORD px1, WORD py1, WORD px2, WORD py2,
                             PetsciiCanvasData *inst)
{
    UBYTE savedMode;
    UWORD x;
    UWORD y;
    WORD  xx;
    WORD  yy;
    WORD  cellW2;
    WORD  cellH2;

    if (!inst->screenbuf) return;

    cellW2 = (inst->screen && inst->screen->width  > 0)
             ? (WORD)(inst->contentW / (WORD)inst->screen->width)  : 0;
    cellH2 = (inst->screen && inst->screen->height > 0)
             ? (WORD)(inst->contentH / (WORD)inst->screen->height) : 0;
    if (cellW2 < 2 || cellH2 < 2) return;

    savedMode = rp->DrawMode;
    SetDrMd(rp, COMPLEMENT);
    SetAPen(rp, 1);

    /* Vertical lines */
    for (x = 0; ; x++) {
        xx = CELL_PX(x, inst->contentW, inst->screenbuf->pixW);
        if (xx >= inst->contentW) break;
        if (xx >= px1 && xx < px2) {
            Move(rp, (LONG)(absX + xx), (LONG)(absY + py1));
            Draw(rp, (LONG)(absX + xx), (LONG)(absY + py2 - 1));
        }
    }

    /* Horizontal lines */
    for (y = 0; ; y++) {
        yy = CELL_PX(y, inst->contentH, inst->screenbuf->pixH);
        if (yy >= inst->contentH) break;
        if (yy >= py1 && yy < py2) {
            Move(rp, (LONG)(absX + px1), (LONG)(absY + yy));
            Draw(rp, (LONG)(absX + px2 - 1), (LONG)(absY + yy));
        }
    }

    SetDrMd(rp, (LONG)savedMode);
}

/* ------------------------------------------------------------------ */
/* Static: restore the canvas pixels under a brush overlay region     */
/* Repairs (col,row) + 1 expanded char in each direction.             */
/* Blits from scaledBuf (the clean screenbuf-only scale).             */
/* ------------------------------------------------------------------ */

static void repairHoverRegion(struct RastPort *rp,
                               PetsciiCanvasData *inst,
                               WORD absX, WORD absY,
                               WORD col, WORD row, WORD bW, WORD bH)
{
    WORD rcol;
    WORD rrow;
    WORD rcol2;
    WORD rrow2;
    WORD px1;
    WORD py1;
    WORD px2;
    WORD py2;
    WORD scrW;
    WORD scrH;

    if (!inst->scaledBuf || !inst->screenbuf || !inst->screen) return;

    scrW = (WORD)inst->screen->width;
    scrH = (WORD)inst->screen->height;

    /* Expand by 1 char on every side to cover selection-rect border pixels */
    rcol  = (col > 0)     ? (WORD)(col - 1)        : 0;
    rrow  = (row > 0)     ? (WORD)(row - 1)        : 0;
    rcol2 = (col + bW + 1 < scrW) ? (WORD)(col + bW + 1) : scrW;
    rrow2 = (row + bH + 1 < scrH) ? (WORD)(row + bH + 1) : scrH;

    px1 = CELL_PX(rcol,  inst->contentW, inst->screenbuf->pixW);
    py1 = CELL_PX(rrow,  inst->contentH, inst->screenbuf->pixH);
    px2 = CELL_PX(rcol2, inst->contentW, inst->screenbuf->pixW);
    py2 = CELL_PX(rrow2, inst->contentH, inst->screenbuf->pixH);

    /* Safety clamp to content area */
    if (px1 < 0)               px1 = 0;
    if (py1 < 0)               py1 = 0;
    if (px2 > inst->contentW)  px2 = inst->contentW;
    if (py2 > inst->contentH)  py2 = inst->contentH;
    if (px2 <= px1 || py2 <= py1) return;

    /* Restore clean pixels from scaledBuf */
    WriteChunkyPixels(rp,
                      (ULONG)(absX + px1),
                      (ULONG)(absY + py1),
                      (ULONG)(absX + px2 - 1),
                      (ULONG)(absY + py2 - 1),
                      inst->scaledBuf + (ULONG)py1 * (ULONG)inst->contentW + px1,
                      (LONG)inst->contentW);

    /* Re-draw grid lines within this sub-rect if grid is visible */
    if (inst->showGrid) {
        WORD cellW2 = (inst->screen->width  > 0)
                      ? (WORD)(inst->contentW / (WORD)inst->screen->width)  : 0;
        WORD cellH2 = (inst->screen->height > 0)
                      ? (WORD)(inst->contentH / (WORD)inst->screen->height) : 0;
        if (cellW2 >= 2 && cellH2 >= 2)
            drawGridSubRect(rp, absX, absY, px1, py1, px2, py2, inst);
    }
}

/* ------------------------------------------------------------------ */
/* Static: draw hover overlay — lasso rect OR brush preview + rect    */
/* ------------------------------------------------------------------ */

/*
 * Helper: compute normalized lasso bounds (char cells) from inst state.
 * Writes col1/row1 (top-left inclusive) and col2/row2 (exclusive).
 */
static void lassoNormalized(const PetsciiCanvasData *inst,
                             WORD *col1, WORD *row1,
                             WORD *col2, WORD *row2)
{
    *col1 = (inst->lassoStartCol < inst->lassoEndCol)
            ? inst->lassoStartCol : inst->lassoEndCol;
    *row1 = (inst->lassoStartRow < inst->lassoEndRow)
            ? inst->lassoStartRow : inst->lassoEndRow;
    *col2 = (inst->lassoStartCol > inst->lassoEndCol)
            ? (WORD)(inst->lassoStartCol + 1) : (WORD)(inst->lassoEndCol + 1);
    *row2 = (inst->lassoStartRow > inst->lassoEndRow)
            ? (WORD)(inst->lassoStartRow + 1) : (WORD)(inst->lassoEndRow + 1);
}

static void drawHoverOverlay(struct RastPort *rp,
                              PetsciiCanvasData *inst,
                              WORD absX, WORD absY)
{
    UBYTE savedMode;
    WORD  sx1;
    WORD  sy1;
    WORD  sx2;
    WORD  sy2;

    if (!inst->screen || !inst->style || !inst->screenbuf) return;
    if (!inst->overlayBuf)                                 return;

    /* ---- LASSO SELECTION RECTANGLE ---------------------------------- */
    if (inst->isLassoing) {
        WORD col1, row1, col2, row2;
        WORD lx1, ly1, lx2, ly2;

        if (inst->lassoStartCol < 0 || inst->lassoStartRow < 0) return;

        lassoNormalized(inst, &col1, &row1, &col2, &row2);

        lx1 = CELL_PX(col1, inst->contentW, inst->screenbuf->pixW);
        ly1 = CELL_PX(row1, inst->contentH, inst->screenbuf->pixH);
        lx2 = CELL_PX(col2, inst->contentW, inst->screenbuf->pixW);
        ly2 = CELL_PX(row2, inst->contentH, inst->screenbuf->pixH);

        /* 1px outside the char cells to match repairHoverRegion expansion */
        sx1 = (WORD)(absX + lx1 - 1);
        sy1 = (WORD)(absY + ly1 - 1);
        sx2 = (WORD)(absX + lx2);
        sy2 = (WORD)(absY + ly2);

        savedMode = rp->DrawMode;
        SetDrMd(rp, COMPLEMENT);
        SetAPen(rp, 1);
        Move(rp, (LONG)sx1, (LONG)sy1);
        Draw(rp, (LONG)sx2, (LONG)sy1);
        Draw(rp, (LONG)sx2, (LONG)sy2);
        Draw(rp, (LONG)sx1, (LONG)sy2);
        Draw(rp, (LONG)sx1, (LONG)(sy1 + 1)); /* close: skip corner double-pixel */
        SetDrMd(rp, (LONG)savedMode);
        return;
    }

    /* ---- BRUSH / CHAR HOVER PREVIEW --------------------------------- */
    {
        WORD  px1;
        WORD  py1;
        WORD  px2;
        WORD  py2;
        ULONG dstW;
        ULONG dstH;
        ULONG srcW;
        ULONG srcH;

        if (inst->cursorCol < 0 || inst->cursorRow < 0) return;

        px1 = CELL_PX(inst->cursorCol,
                       inst->contentW, inst->screenbuf->pixW);
        py1 = CELL_PX(inst->cursorRow,
                       inst->contentH, inst->screenbuf->pixH);
        px2 = CELL_PX(inst->cursorCol + inst->brushW,
                       inst->contentW, inst->screenbuf->pixW);
        py2 = CELL_PX(inst->cursorRow + inst->brushH,
                       inst->contentH, inst->screenbuf->pixH);
        /* magic, we let the clipRegion manage this a best way.
        *if (px1 < 0)              px1 = 0;
        *if (py1 < 0)              py1 = 0;
        *if (px2 > inst->contentW) px2 = inst->contentW;
        *if (py2 > inst->contentH) py2 = inst->contentH;
        */
        dstW = (ULONG)(px2 - px1);
        dstH = (ULONG)(py2 - py1);
        if (dstW == 0 || dstH == 0) return;

        srcW = (ULONG)inst->brushW * 8UL;
        srcH = (ULONG)inst->brushH * 8UL;

        if (inst->brush) {
            /* Multi-char brush: render into nativeBrushBuf */
            ULONG needed = srcW * srcH;
            if (!inst->nativeBrushBuf || needed > inst->nativeBrushBufSize) {
                if (inst->nativeBrushBuf) {
                    FreeVec(inst->nativeBrushBuf);
                    inst->nativeBrushBuf = NULL;
                }
                /* Cast away const: inst is modified only for cache management */
                ((PetsciiCanvasData *)inst)->nativeBrushBuf =
                    (UBYTE *)AllocVec(needed, MEMF_ANY);
                if (!inst->nativeBrushBuf) {
                    ((PetsciiCanvasData *)inst)->nativeBrushBufSize = 0;
                    return;
                }
                ((PetsciiCanvasData *)inst)->nativeBrushBufSize = needed;
            }
            PetsciiScreenBuf_RenderCells(inst->nativeBrushBuf, srcW,
                                          inst->brush->cells,
                                          (UWORD)inst->brushW,
                                          (UWORD)inst->brushH,
                                          inst->screen->backgroundColor,
                                          inst->screen->charset,
                                          inst->style);
            PetsciiChunky_Scale(inst->nativeBrushBuf, srcW, srcH,
                                 inst->overlayBuf, dstW, dstH);
        } else {
            /* 1x1 single char: stack buffer */
            UBYTE        nativeBuf[8 * 8];
            PetsciiPixel cell;
            cell.code  = inst->selectedChar;
            cell.color = inst->fgColor;
            PetsciiScreenBuf_RenderCells(nativeBuf, srcW, &cell,
                                          1, 1,
                                          inst->screen->backgroundColor,
                                          inst->screen->charset,
                                          inst->style);
            PetsciiChunky_Scale(nativeBuf, srcW, srcH,
                                 inst->overlayBuf, dstW, dstH);
        }

        /* Blit scaled brush preview to screen */
        WriteChunkyPixels(rp,
                          (ULONG)(absX + px1),
                          (ULONG)(absY + py1),
                          (ULONG)(absX + px2 - 1),
                          (ULONG)(absY + py2 - 1),
                          inst->overlayBuf,
                          (LONG)dstW);

        /* Selection rectangle: 1px outside brush bounds, COMPLEMENT */
        sx1 = (WORD)(absX + px1 - 1);
        sy1 = (WORD)(absY + py1 - 1);
        sx2 = (WORD)(absX + px2);
        sy2 = (WORD)(absY + py2);

        savedMode = rp->DrawMode;
        SetDrMd(rp, COMPLEMENT);
        SetAPen(rp, 1);
        Move(rp, (LONG)sx1, (LONG)sy1);
        Draw(rp, (LONG)sx2, (LONG)sy1);
        Draw(rp, (LONG)sx2, (LONG)sy2);
        Draw(rp, (LONG)sx1, (LONG)sy2);
        Draw(rp, (LONG)sx1, (LONG)(sy1 + 1)); /* close: skip corner double-pixel */
        SetDrMd(rp, (LONG)savedMode);
    }
}

ULONG PetsciiCanvas_OnDomain(Class *cl, Object *o, struct gpDomain *msg)
{
    struct IBox *domain = &msg->gpd_Domain;

    // Set default dimensions
    domain->Left = 0;
    domain->Top = 0;
    domain->Width = 42*8;  // Nominal width
    domain->Height = 27*8;  // Nominal height

 bdbprintf("CharSelector_OnDomain\n");
    // Adjust based on gpd_Which
    switch (msg->gpd_Which) {
        case GDOMAIN_MINIMUM:

            break;
        case GDOMAIN_MAXIMUM:
            domain->Width *= 6;
            domain->Height *= 6;
            break;
        case GDOMAIN_NOMINAL:
        default:
            // Use default values
            break;
    }

    return 1;
}

/* ------------------------------------------------------------------ */
/* GM_LAYOUT - pre-allocate scaling buffer to match content rect.     */
/* ------------------------------------------------------------------ */

ULONG PetsciiCanvas_OnLayout(Class *cl, Object *o, struct gpLayout *msg)
{
    PetsciiCanvasData *inst = (PetsciiCanvasData *)INST_DATA(cl, o);
    WORD w = G(o)->Width;
    WORD h = G(o)->Height;

    if (w > 0 && h > 0) {
        updateContentRect(inst, w, h);
        ensureScaledBuf(inst, (UWORD)inst->contentW, (UWORD)inst->contentH);
    }
    inst->refreshExtraMarge = 1;

    /* when a rastport is given or created rto draw on gadget,
        It is most often delivered with no clipping at layer level.
        Optionally during draw we can force installation of a clipping rect,
        that will cut any graphics.library drawing cals using RastPort.
        Here we refresh a Region geometry that is used as clipping at draw.
      */
    if (inst->clipRegion)
    {
        struct Rectangle framerect;
        framerect.MinX = G(o)->LeftEdge + inst->contentX;
        framerect.MinY = G(o)->TopEdge + inst->contentY;
        framerect.MaxX = G(o)->LeftEdge + inst->contentX + inst->contentW -1;
        framerect.MaxY = G(o)->TopEdge + inst->contentY + inst->contentH -1;

        /* note you can go wild with Or/And boolean operation on geometry.
           But a single rect (should be) enough at Gadget level.
           Note there are issues on "Virtual.class" up to OS3.2.2 with this.
           One of the reason we don't use Virtual.class.
           */
        ClearRegion(inst->clipRegion);
        OrRectRegion(inst->clipRegion, &framerect);
    }

    return DoSuperMethodA(cl, o, (APTR)msg);
}

/* ------------------------------------------------------------------ */
/* GM_RENDER                                                            */
/* ------------------------------------------------------------------ */

ULONG PetsciiCanvas_OnRender(Class *cl, Object *o, struct gpRender *msg)
{
    struct Region *oldClipRegion=NULL;
    PetsciiCanvasData *inst;
    struct RastPort   *rp;
    WORD               left;
    WORD               top;
    WORD               width;
    WORD               height;
    WORD               absX;
    WORD               absY;
    WORD               cellW;
    WORD               cellH;
    BOOL               needFull;

    (void)cl;

    inst = (PetsciiCanvasData *)INST_DATA(cl, o);

    if (!inst->screenbuf || !inst->screen || !inst->style)
        return 0;

    rp     = msg->gpr_RPort;
    left   = G(o)->LeftEdge;
    top    = G(o)->TopEdge;
    width  = G(o)->Width;
    height = G(o)->Height;

    if (width <= 0 || height <= 0)
        return 0;

    /* Always recompute content rect (cheap arithmetic) */
    updateContentRect(inst, width, height);


    absX  = (WORD)(left + inst->contentX);
    absY  = (WORD)(top  + inst->contentY);
    cellW = (inst->screen->width  > 0) ?
            (WORD)(inst->contentW / (WORD)inst->screen->width)  : 0;
    cellH = (inst->screen->height > 0) ?
            (WORD)(inst->contentH / (WORD)inst->screen->height) : 0;

    /* Decide: full canvas blit, or hover-only partial update? */
    needFull = !inst->screenbuf->valid   ||
               inst->scaledBufDirty      ||
               inst->refreshExtraMarge   ||
               inst->scaledBuf  == NULL  ||
               inst->overlayBuf == NULL  ||
               inst->contentW   == 0     ||
               inst->contentH   == 0     ||
               (inst->scaledW != (UWORD)inst->contentW) ||
               (inst->scaledH != (UWORD)inst->contentH);

    if (needFull) {
        /* ---- FULL RENDER PATH ----------------------------------------- */

        /* Outer frame = inner content expanded by 1 cell on every side */
        {
            WORD outerX = (WORD)(inst->contentX - cellW);
            WORD outerY = (WORD)(inst->contentY - cellH);
            WORD outerW = (WORD)(inst->contentW + 2 * cellW);
            WORD outerH = (WORD)(inst->contentH + 2 * cellH);

            /* Erase letterbox/pillarbox strips outside the outer frame */
            if (inst->refreshExtraMarge) {
                eraseMargins(rp, left, top, width, height,
                             outerX, outerY, outerW, outerH);
                inst->refreshExtraMarge = 0;
            }

            /* Fill C64 border strips */
            if (cellW >= 1 && cellH >= 1) {
                WORD absOX = (WORD)(left + outerX);
                WORD absOY = (WORD)(top  + outerY);
                WORD pen   = PetsciiStyle_Pen(inst->style,
                                              inst->screen->borderColor);
                SetAPen(rp, (LONG)pen);

                /* Top border strip */
                RectFill(rp,
                         (LONG)absOX,
                         (LONG)absOY,
                         (LONG)(absOX + outerW - 1),
                         (LONG)(absY - 1));
                /* Bottom border strip */
                RectFill(rp,
                         (LONG)absOX,
                         (LONG)(absY + inst->contentH),
                         (LONG)(absOX + outerW - 1),
                         (LONG)(absOY + outerH - 1));
                /* Left border strip */
                RectFill(rp,
                         (LONG)absOX,
                         (LONG)absY,
                         (LONG)(absX - 1),
                         (LONG)(absY + inst->contentH - 1));
                /* Right border strip */
                RectFill(rp,
                         (LONG)(absX + inst->contentW),
                         (LONG)absY,
                         (LONG)(absOX + outerW - 1),
                         (LONG)(absY + inst->contentH - 1));
            }
        }

        /* Rebuild screenbuf chunky if stale */
        if (!inst->screenbuf->valid)
            PetsciiScreenBuf_RebuildFull(inst->screenbuf, inst->screen, inst->style);

        oldClipRegion = InstallClipRegion( rp->Layer, inst->clipRegion);

        /* Full blit — always via BlitScaled so scaledBuf stays current.
         * At 1:1 the step is exactly 1.0 (identity copy), so correctness
         * and the repair source are both guaranteed.                      */
        if (ensureScaledBuf(inst, (UWORD)inst->contentW, (UWORD)inst->contentH)) {
            PetsciiScreenBuf_BlitScaled(inst->screenbuf, rp,
                                         absX, absY,
                                         (UWORD)inst->contentW,
                                         (UWORD)inst->contentH,
                                         inst->scaledBuf);
        }
        inst->scaledBufDirty = FALSE;

        /* Grid overlay */
        if (inst->showGrid && cellW >= 2 && cellH >= 2)
            drawGrid(rp, absX, absY, inst->contentW, inst->contentH, inst);

        /* Hover overlay (lasso rect or brush preview + selection rect) */
        drawHoverOverlay(rp, inst, absX, absY);

        /* Remember where the overlay was drawn for the next repair.
         * When lassoing, track the full lasso bounding rect.           */
        if (inst->isLassoing) {
            WORD lc1, lr1, lc2, lr2;
            lassoNormalized(inst, &lc1, &lr1, &lc2, &lr2);
            inst->prevHoverCol = lc1;
            inst->prevHoverRow = lr1;
            inst->prevBrushW   = (WORD)(lc2 - lc1);
            inst->prevBrushH   = (WORD)(lr2 - lr1);
        } else {
            inst->prevHoverCol = inst->cursorCol;
            inst->prevHoverRow = inst->cursorRow;
            inst->prevBrushW   = inst->brushW;
            inst->prevBrushH   = inst->brushH;
        }

        /* important to pass NULL if oldClipRegion was NULL.*/
        InstallClipRegion( rp->Layer,oldClipRegion);
    } else {
        /* ---- PARTIAL HOVER UPDATE PATH --------------------------------- */

        oldClipRegion = InstallClipRegion( rp->Layer, inst->clipRegion);


        /* Repair old overlay region (restores screenbuf pixels + grid) */
        if (inst->prevHoverCol >= 0) {
            repairHoverRegion(rp, inst, absX, absY,
                              inst->prevHoverCol, inst->prevHoverRow,
                              inst->prevBrushW,   inst->prevBrushH);
        }

        /* Draw new hover overlay */
        drawHoverOverlay(rp, inst, absX, absY);

        if (inst->isLassoing) {
            WORD lc1, lr1, lc2, lr2;
            lassoNormalized(inst, &lc1, &lr1, &lc2, &lr2);
            inst->prevHoverCol = lc1;
            inst->prevHoverRow = lr1;
            inst->prevBrushW   = (WORD)(lc2 - lc1);
            inst->prevBrushH   = (WORD)(lr2 - lr1);
        } else {
            inst->prevHoverCol = inst->cursorCol;
            inst->prevHoverRow = inst->cursorRow;
            inst->prevBrushW   = inst->brushW;
            inst->prevBrushH   = inst->brushH;
        }

        /* important to pass NULL if oldClipRegion was NULL.*/
        InstallClipRegion( rp->Layer,oldClipRegion);
    }


    return 0;
}
