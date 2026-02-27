/*
 * PetsciiCanvas - Mouse input: GM_GOACTIVE, GM_HANDLEINPUT, GM_GOINACTIVE.
 *
 * Left-click or drag paints PETSCII cells using the current tool:
 *   TOOL_DRAW      - set both character code and foreground color
 *   TOOL_COLORIZE  - change color, keep existing character
 *   TOOL_CHARDRAW  - change character code, keep existing color
 *
 * Bresenham line interpolation fills gaps between cells on fast drags.
 *
 * GM_GOACTIVE is called on SELECTDOWN; GMR_MEACTIVE is returned to
 * stay active during drag.  GM_HANDLEINPUT is called for subsequent
 * events; SELECTUP returns GMR_VERIFY|GMR_NOREUSE, causing Intuition
 * to post IDCMP_GADGETUP.
 *
 * Mouse coordinates (gpi_Mouse.X/Y) are gadget-relative.
 * The content rect (inst->contentX/Y/W/H) handles keepRatio offsets.
 */

#include <proto/graphics.h>
#include <proto/intuition.h>
#include <devices/inputevent.h>
#include <clib/alib_protos.h>
#include "petscii_canvas_private.h"

/* ------------------------------------------------------------------ */
/* Static helpers                                                       */
/* ------------------------------------------------------------------ */

/*
 * Convert gadget-relative mouse coords to character cell (col, row).
 * Clamps to the content rect so clicks in margins select border cells.
 * Returns -1/-1 when the canvas has no valid content rect or screen.
 */
static void mouseToCell(PetsciiCanvasData *inst, WORD relX, WORD relY,
                         WORD *outCol, WORD *outRow)
{
    WORD cW = inst->contentW;
    WORD cH = inst->contentH;
    WORD cx;
    WORD cy;

    if (cW <= 0 || cH <= 0 || !inst->screen ||
        !inst->screen->width || !inst->screen->height) {
        *outCol = -1;
        *outRow = -1;
        return;
    }

    /* Translate from gadget-relative to content-rect-relative */
    cx = (WORD)(relX - inst->contentX);
    cy = (WORD)(relY - inst->contentY);

    /* Clamp to content area */
    if (cx < 0)   cx = 0;
    if (cy < 0)   cy = 0;
    if (cx >= cW) cx = (WORD)(cW - 1);
    if (cy >= cH) cy = (WORD)(cH - 1);

    *outCol = (WORD)((ULONG)cx * (ULONG)inst->screen->width  / (ULONG)cW);
    *outRow = (WORD)((ULONG)cy * (ULONG)inst->screen->height / (ULONG)cH);

    /* Safety clamp */
    if (*outCol >= (WORD)inst->screen->width)
        *outCol = (WORD)(inst->screen->width  - 1);
    if (*outRow >= (WORD)inst->screen->height)
        *outRow = (WORD)(inst->screen->height - 1);
}

/*
 * Apply the current tool to one cell and update the render buffer.
 * TOOL_BRUSH and TOOL_TEXT are no-ops here (handled in later phases).
 */
static void paintCell(PetsciiCanvasData *inst, WORD col, WORD row)
{
    if (!inst->screen || !inst->screenbuf || !inst->style) return;
    if (col < 0 || row < 0) return;
    if ((UWORD)col >= inst->screen->width ||
        (UWORD)row >= inst->screen->height) return;

    switch (inst->currentTool) {
        case TOOL_DRAW:
            PetsciiScreen_SetPixel(inst->screen, (UWORD)col, (UWORD)row,
                                   inst->selectedChar, inst->fgColor);
            break;
        case TOOL_COLORIZE:
            PetsciiScreen_SetColor(inst->screen, (UWORD)col, (UWORD)row,
                                   inst->fgColor);
            break;
        case TOOL_CHARDRAW:
            PetsciiScreen_SetCode(inst->screen, (UWORD)col, (UWORD)row,
                                  inst->selectedChar);
            break;
        default:
            return;
    }

    PetsciiScreenBuf_UpdateCell(inst->screenbuf, inst->screen, inst->style,
                                 (UWORD)col, (UWORD)row);
}

/*
 * Bresenham rasterizer: paint every cell along the line from
 * (x0,y0) to (x1,y1) inclusive, skipping the origin cell (already
 * painted in the previous event).
 *
 * We skip the first point so that GOACTIVE + a series of HANDLEINPUT
 * moves never re-paint the origin redundantly.  The first cell of
 * each drag is always painted by OnGoActive.
 */
static void paintLineFrom(PetsciiCanvasData *inst,
                           WORD x0, WORD y0, WORD x1, WORD y1)
{
    WORD dx;
    WORD dy;
    WORD sx;
    WORD sy;
    WORD err;
    WORD e2;
    BOOL first = TRUE;

    dx = x1 - x0; if (dx < 0) dx = (WORD)(-dx);
    dy = y1 - y0; if (dy < 0) dy = (WORD)(-dy);
    sx = (x0 < x1) ? 1 : -1;
    sy = (y0 < y1) ? 1 : -1;
    err = (WORD)(dx - dy);

    for (;;) {
        if (!first) {
            paintCell(inst, x0, y0);
        }
        first = FALSE;

        if (x0 == x1 && y0 == y1) break;

        e2 = (WORD)(2 * err);
        if (e2 > -dy) { err = (WORD)(err - dy); x0 = (WORD)(x0 + sx); }
        if (e2 <  dx) { err = (WORD)(err + dx); y0 = (WORD)(y0 + sy); }
    }
}

/*
 * Trigger a GM_RENDER on ourselves so the canvas updates immediately
 * after each paint operation or cursor move.
 */
static void renderSelf(Class *cl, Object *o, struct GadgetInfo *gi)
{
    struct RastPort *rp;
    struct gpRender  renderMsg;

    (void)cl;

    rp = ObtainGIRPort(gi);
    if (!rp) return;

    renderMsg.MethodID   = GM_RENDER;
    renderMsg.gpr_GInfo  = gi;
    renderMsg.gpr_RPort  = rp;
    renderMsg.gpr_Redraw = GREDRAW_UPDATE;

    DoMethodA(o, (Msg)(APTR)&renderMsg);

    ReleaseGIRPort(rp);
}

/* ------------------------------------------------------------------ */
/* GM_GOACTIVE - called on SELECTDOWN                                   */
/* ------------------------------------------------------------------ */

ULONG PetsciiCanvas_OnGoActive(Class *cl, Object *o, struct gpInput *msg)
{
    PetsciiCanvasData *inst;
    WORD               col;
    WORD               row;

    inst = (PetsciiCanvasData *)INST_DATA(cl, o);

    /* Take undo snapshot before the first cell of this stroke */
    if (inst->undoBuf && inst->screen &&
        inst->currentTool != TOOL_BRUSH &&
        inst->currentTool != TOOL_TEXT) {
        PetsciiUndoBuffer_Push(inst->undoBuf, inst->screen);
    }

    mouseToCell(inst, msg->gpi_Mouse.X, msg->gpi_Mouse.Y, &col, &row);

    if (col >= 0 && row >= 0) {
        paintCell(inst, col, row);
        inst->lastPaintCol = col;
        inst->lastPaintRow = row;
    } else {
        inst->lastPaintCol = -1;
        inst->lastPaintRow = -1;
    }

    inst->cursorCol = col;
    inst->cursorRow = row;
    inst->isDrawing = TRUE;

    renderSelf(cl, o, msg->gpi_GInfo);

    return GMR_MEACTIVE;  /* stay active: track drag */
}

/* ------------------------------------------------------------------ */
/* GM_HANDLEINPUT - called while gadget is active (button held)        */
/* ------------------------------------------------------------------ */

ULONG PetsciiCanvas_OnInput(Class *cl, Object *o, struct gpInput *msg)
{
    PetsciiCanvasData *inst;
    struct InputEvent *ie;
    WORD               col;
    WORD               row;

    inst = (PetsciiCanvasData *)INST_DATA(cl, o);
    ie   = msg->gpi_IEvent;

    /* Left button release: finish stroke, post GADGETUP */
    if (ie->ie_Class == IECLASS_RAWMOUSE &&
        ie->ie_Code  == (IECODE_LBUTTON | IECODE_UP_PREFIX)) {
        inst->isDrawing = FALSE;
        *msg->gpi_Termination = 0;
        return GMR_VERIFY | GMR_NOREUSE;
    }

    /* Mouse move while button held: Bresenham gap-fill */
    if (ie->ie_Class == IECLASS_RAWMOUSE &&
        ie->ie_Code  == IECODE_NOBUTTON) {
        mouseToCell(inst, msg->gpi_Mouse.X, msg->gpi_Mouse.Y, &col, &row);

        if (col >= 0 && row >= 0) {
            if (inst->lastPaintCol >= 0 && inst->lastPaintRow >= 0) {
                /* Paint from last position to current (skip origin) */
                paintLineFrom(inst,
                               inst->lastPaintCol, inst->lastPaintRow,
                               col, row);
            } else {
                paintCell(inst, col, row);
            }
            inst->lastPaintCol = col;
            inst->lastPaintRow = row;
        }

        inst->cursorCol = col;
        inst->cursorRow = row;
        renderSelf(cl, o, msg->gpi_GInfo);
    }

    return GMR_MEACTIVE;
}

/* ------------------------------------------------------------------ */
/* GM_GOINACTIVE - gadget deactivated (e.g. requester opened)          */
/* ------------------------------------------------------------------ */

ULONG PetsciiCanvas_OnGoInactive(Class *cl, Object *o,
                                  struct gpGoInactive *msg)
{
    PetsciiCanvasData *inst = (PetsciiCanvasData *)INST_DATA(cl, o);

    inst->isDrawing    = FALSE;
    inst->lastPaintCol = -1;
    inst->lastPaintRow = -1;
    inst->cursorCol    = -1;
    inst->cursorRow    = -1;

    /* Redraw to erase the cursor highlight */
    renderSelf(cl, o, msg->gpgi_GInfo);

    return 0;
}
