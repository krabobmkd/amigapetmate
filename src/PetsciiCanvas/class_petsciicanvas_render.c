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

/* ------------------------------------------------------------------ */
/* Static: ensure inst->scaledBuf is (w x h) bytes.                   */
/* ------------------------------------------------------------------ */

static BOOL ensureScaledBuf(PetsciiCanvasData *inst, UWORD w, UWORD h)
{
    ULONG needed;

    if (inst->scaledW == w && inst->scaledH == h && inst->scaledBuf)
        return TRUE;

    if (inst->scaledBuf) {
        FreeVec(inst->scaledBuf);
        inst->scaledBuf = NULL;
        inst->scaledW   = 0;
        inst->scaledH   = 0;
    }

    if (w == 0 || h == 0)
        return FALSE;

    needed          = (ULONG)w * (ULONG)h;
    inst->scaledBuf = (UBYTE *)AllocVec(needed, MEMF_ANY);
    if (!inst->scaledBuf)
        return FALSE;

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
                     WORD cellW, WORD cellH)
{
    UBYTE savedMode;
    WORD  x;
    WORD  y;

    savedMode = rp->DrawMode;
    SetDrMd(rp, COMPLEMENT);
    SetAPen(rp, 1);

    for (x = left + cellW; x < left + width; x = (WORD)(x + cellW)) {
        Move(rp, (LONG)x, (LONG)top);
        Draw(rp, (LONG)x, (LONG)(top + height - 1));
    }

    for (y = top + cellH; y < top + height; y = (WORD)(y + cellH)) {
        Move(rp, (LONG)left, (LONG)y);
        Draw(rp, (LONG)(left + width - 1), (LONG)y);
    }

    SetDrMd(rp, (LONG)savedMode);
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

    return DoSuperMethodA(cl, o, (APTR)msg);
}

/* ------------------------------------------------------------------ */
/* GM_RENDER                                                            */
/* ------------------------------------------------------------------ */

ULONG PetsciiCanvas_OnRender(Class *cl, Object *o, struct gpRender *msg)
{
    PetsciiCanvasData *inst;
    struct RastPort   *rp;
    WORD               left;
    WORD               top;
    WORD               width;
    WORD               height;
    WORD               absX;   /* absolute X of content rect top-left */
    WORD               absY;
    WORD               cellW;
    WORD               cellH;

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

    /* Recompute content rect.
     * contentX/Y/W/H = inner character area (border excluded).
     * After this call, cellW/H from contentW/scrW gives the per-cell size. */
    updateContentRect(inst, width, height);

    /* Per-cell pixel size (used for border, grid, cursor) */
    cellW = (inst->screen->width  > 0) ?
            (WORD)(inst->contentW / (WORD)inst->screen->width)  : 0;
    cellH = (inst->screen->height > 0) ?
            (WORD)(inst->contentH / (WORD)inst->screen->height) : 0;

    /* Outer frame = inner content rect expanded by 1 cell on every side.
     * This covers both the character grid and the C64 border margin.    */
    {
        WORD outerX = (WORD)(inst->contentX - cellW);
        WORD outerY = (WORD)(inst->contentY - cellH);
        WORD outerW = (WORD)(inst->contentW + 2 * cellW);
        WORD outerH = (WORD)(inst->contentH + 2 * cellH);

        /* Erase letterbox/pillarbox strips OUTSIDE the outer frame */
        eraseMargins(rp, left, top, width, height,
                     outerX, outerY, outerW, outerH);

        /* Fill the four C64 border strips with the border pen.
         * PetsciiStyle_Pen maps C64 color index -> Amiga screen pen.  */
        if (cellW >= 1 && cellH >= 1) {
            WORD absOX = (WORD)(left + outerX);
            WORD absOY = (WORD)(top  + outerY);
            WORD pen   = PetsciiStyle_Pen(inst->style,
                                          inst->screen->borderColor);

            SetAPen(rp, (LONG)pen);

            absX = (WORD)(left + inst->contentX);
            absY = (WORD)(top  + inst->contentY);

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

            /* Left border strip (between top and bottom strips) */
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

    absX = (WORD)(left + inst->contentX);
    absY = (WORD)(top  + inst->contentY);

    /* Lazily rebuild the chunky buffer when marked invalid */
    if (!inst->screenbuf->valid)
        PetsciiScreenBuf_RebuildFull(inst->screenbuf, inst->screen, inst->style);

    /* Blit character grid: 1:1 native or scaled to fit inner content rect */
    if ((ULONG)inst->contentW == inst->screenbuf->pixW &&
        (ULONG)inst->contentH == inst->screenbuf->pixH) {
        PetsciiScreenBuf_BlitNative(inst->screenbuf, rp, absX, absY);
    } else {
        if (ensureScaledBuf(inst, (UWORD)inst->contentW, (UWORD)inst->contentH)) {
            PetsciiScreenBuf_BlitScaled(inst->screenbuf, rp,
                                        absX, absY,
                                        (UWORD)inst->contentW,
                                        (UWORD)inst->contentH,
                                        inst->scaledBuf);
        }
    }

    /* Character grid overlay (inside inner content rect only) */
    if (inst->showGrid && cellW >= 2 && cellH >= 2) {
        drawGrid(rp, absX, absY,
                 inst->contentW, inst->contentH, cellW, cellH);
    }

    /* Cursor highlight: complement box drawn around current cell */
    if (inst->cursorCol >= 0 && inst->cursorRow >= 0 && cellW >= 1 && cellH >= 1) {
        WORD  cx2  = (WORD)(absX + inst->cursorCol * cellW);
        WORD  cy2  = (WORD)(absY + inst->cursorRow * cellH);
        UBYTE saved = rp->DrawMode;
        SetDrMd(rp, COMPLEMENT);
        SetAPen(rp, 1);
        Move(rp, (LONG)cx2,             (LONG)cy2);
        Draw(rp, (LONG)(cx2 + cellW-1), (LONG)cy2);
        Draw(rp, (LONG)(cx2 + cellW-1), (LONG)(cy2 + cellH-1));
        Draw(rp, (LONG)cx2,             (LONG)(cy2 + cellH-1));
        Draw(rp, (LONG)cx2,             (LONG)cy2);
        SetDrMd(rp, (LONG)saved);
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/* GM_HITTEST - always hit (the gadget fills its entire bounds)        */
/* ------------------------------------------------------------------ */

ULONG PetsciiCanvas_OnHitTest(Class *cl, Object *o, struct gpHitTest *msg)
{
    (void)cl; (void)o; (void)msg;
    return GMR_GADGETHIT;
}
