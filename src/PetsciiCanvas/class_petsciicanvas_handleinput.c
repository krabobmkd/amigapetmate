/*
 * PetsciiCanvas - Mouse input: GM_GOACTIVE, GM_HANDLEINPUT, GM_GOINACTIVE.
 *
 * Hover vs. draw distinction
 * --------------------------
 * The gadget stays continuously active (always returns GMR_MEACTIVE).
 * GM_GOACTIVE fires when the mouse enters the gadget area.  If the left
 * button is held at entry (ie_Code == IECODE_LBUTTON), drawing begins
 * immediately.  Otherwise it is pure hover: the brush preview is shown
 * but the screenbuf is not modified.
 *
 * While the gadget is active GM_HANDLEINPUT receives every input event:
 *   IECODE_NOBUTTON   - mouse move; hover-only if !isDrawing, paint if isDrawing
 *   IECODE_LBUTTON    - button pressed while hovering  -> start drawing
 *   IECODE_LBUTTON|UP - button released while drawing  -> stop drawing, keep hovering
 *
 * Render strategy
 * ---------------
 * After a cell is painted:   scaledBufDirty = TRUE  -> OnRender full path
 *   (scaledBuf must be refreshed before it can serve as repair source)
 * After a hover-only move:   scaledBufDirty = FALSE -> OnRender partial path
 *   (repair old overlay from scaledBuf, draw new overlay)
 *
 * Bresenham gap-fill fills in cells skipped during fast drags.
 *
 * Mouse coordinates (gpi_Mouse.X/Y) are gadget-relative.
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
 * Returns -1/-1 when outside a valid content rect.
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

    cx = (WORD)(relX - inst->contentX);
    cy = (WORD)(relY - inst->contentY);

    if (cx < 0)   cx = 0;
    if (cy < 0)   cy = 0;
    if (cx >= cW) cx = (WORD)(cW - 1);
    if (cy >= cH) cy = (WORD)(cH - 1);

    *outCol = (WORD)((ULONG)cx * (ULONG)inst->screen->width  / (ULONG)cW);
    *outRow = (WORD)((ULONG)cy * (ULONG)inst->screen->height / (ULONG)cH);

    if (*outCol >= (WORD)inst->screen->width)
        *outCol = (WORD)(inst->screen->width  - 1);
    if (*outRow >= (WORD)inst->screen->height)
        *outRow = (WORD)(inst->screen->height - 1);
}

/*
 * Apply the current tool to one cell, update the render buffer, and
 * mark scaledBuf dirty so the next OnRender takes the full-blit path.
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

    /* scaledBuf now lags behind screenbuf — force full blit next render */
    inst->scaledBufDirty = TRUE;
}

/*
 * Bresenham rasterizer: paint every cell along the line from
 * (x0,y0) to (x1,y1) inclusive, skipping the first point.
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
        if (!first)
            paintCell(inst, x0, y0);
        first = FALSE;

        if (x0 == x1 && y0 == y1) break;

        e2 = (WORD)(2 * err);
        if (e2 > -dy) { err = (WORD)(err - dy); x0 = (WORD)(x0 + sx); }
        if (e2 <  dx) { err = (WORD)(err + dx); y0 = (WORD)(y0 + sy); }
    }
}

/*
 * Trigger GM_RENDER on ourselves.
 * OnRender decides full vs. partial based on inst->scaledBufDirty.
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
/* GM_GOACTIVE - mouse enters gadget area                              */
/* ------------------------------------------------------------------ */

ULONG PetsciiCanvas_OnGoActive(Class *cl, Object *o, struct gpInput *msg)
{
    PetsciiCanvasData *inst;
    WORD               col;
    WORD               row;
    BOOL               isClick;

    inst = (PetsciiCanvasData *)INST_DATA(cl, o);

    mouseToCell(inst, msg->gpi_Mouse.X, msg->gpi_Mouse.Y, &col, &row);

    /* Detect actual left-button click vs. passive mouse-over */
    isClick = (msg->gpi_IEvent &&
               msg->gpi_IEvent->ie_Class == IECLASS_RAWMOUSE &&
               msg->gpi_IEvent->ie_Code  == IECODE_LBUTTON);

    if (isClick) {
        /* Take undo snapshot before the first painted cell of this stroke */
        if (inst->undoBuf && inst->screen &&
            inst->currentTool != TOOL_BRUSH &&
            inst->currentTool != TOOL_TEXT) {
            PetsciiUndoBuffer_Push(inst->undoBuf, inst->screen);
        }

        if (col >= 0 && row >= 0) {
            paintCell(inst, col, row);
            inst->lastPaintCol = col;
            inst->lastPaintRow = row;
        } else {
            inst->lastPaintCol = -1;
            inst->lastPaintRow = -1;
        }
        inst->isDrawing = TRUE;
    } else {
        /* Hover entry: no painting, scaledBufDirty stays as-is */
        inst->isDrawing    = FALSE;
        inst->lastPaintCol = -1;
        inst->lastPaintRow = -1;
    }

    inst->cursorCol = col;
    inst->cursorRow = row;

    renderSelf(cl, o, msg->gpi_GInfo);

    return GMR_MEACTIVE;  /* stay active while over the gadget */
}

/* ------------------------------------------------------------------ */
/* GM_HANDLEINPUT - called for every event while gadget is active      */
/* ------------------------------------------------------------------ */

ULONG PetsciiCanvas_OnInput(Class *cl, Object *o, struct gpInput *msg)
{
    PetsciiCanvasData *inst;
    struct InputEvent *ie;
    WORD               col;
    WORD               row;

    inst = (PetsciiCanvasData *)INST_DATA(cl, o);
    ie   = msg->gpi_IEvent;

    if (ie->ie_Class != IECLASS_RAWMOUSE)
        return GMR_MEACTIVE;

    if (ie->ie_Code == (IECODE_LBUTTON | IECODE_UP_PREFIX)) {
        /* Button released: stop drawing but stay active for hover */
        inst->isDrawing    = FALSE;
        inst->lastPaintCol = -1;
        inst->lastPaintRow = -1;
        /* No render needed: cursor stays at same position, scaledBuf is
         * already current (full path was taken on every paint).        */
        return GMR_MEACTIVE;
    }

    if (ie->ie_Code == IECODE_LBUTTON) {
        /* Button pressed while hovering: start a new draw stroke */
        mouseToCell(inst, msg->gpi_Mouse.X, msg->gpi_Mouse.Y, &col, &row);

        if (inst->undoBuf && inst->screen &&
            inst->currentTool != TOOL_BRUSH &&
            inst->currentTool != TOOL_TEXT) {
            PetsciiUndoBuffer_Push(inst->undoBuf, inst->screen);
        }

        if (col >= 0 && row >= 0) {
            paintCell(inst, col, row);
            inst->lastPaintCol = col;
            inst->lastPaintRow = row;
        } else {
            inst->lastPaintCol = -1;
            inst->lastPaintRow = -1;
        }
        inst->isDrawing = TRUE;
        inst->cursorCol = col;
        inst->cursorRow = row;

        renderSelf(cl, o, msg->gpi_GInfo);
        return GMR_MEACTIVE;
    }

    if (ie->ie_Code == IECODE_NOBUTTON) {
        /* Mouse move */
        mouseToCell(inst, msg->gpi_Mouse.X, msg->gpi_Mouse.Y, &col, &row);

        if (inst->isDrawing && col >= 0 && row >= 0) {
            /* Draw stroke: Bresenham gap-fill from last painted cell */
            if (inst->lastPaintCol >= 0 && inst->lastPaintRow >= 0) {
                paintLineFrom(inst,
                               inst->lastPaintCol, inst->lastPaintRow,
                               col, row);
            } else {
                paintCell(inst, col, row);
            }
            inst->lastPaintCol = col;
            inst->lastPaintRow = row;
        }
        /* scaledBufDirty was set by paintCell if drawing;
         * stays FALSE if hover-only -> OnRender takes partial path */

        inst->cursorCol = col;
        inst->cursorRow = row;
        renderSelf(cl, o, msg->gpi_GInfo);
    }

    return GMR_MEACTIVE;
}

/* ------------------------------------------------------------------ */
/* GM_GOINACTIVE - mouse left the gadget area                          */
/* ------------------------------------------------------------------ */

ULONG PetsciiCanvas_OnGoInactive(Class *cl, Object *o,
                                  struct gpGoInactive *msg)
{
    PetsciiCanvasData *inst = (PetsciiCanvasData *)INST_DATA(cl, o);

    inst->isDrawing    = FALSE;
    inst->lastPaintCol = -1;
    inst->lastPaintRow = -1;

    /* Hide cursor: set to -1 so drawHoverOverlay draws nothing,
     * repairHoverRegion restores the last overlay position.          */
    inst->cursorCol = -1;
    inst->cursorRow = -1;

    /* Partial render: repair the last overlay, draw nothing new */
    renderSelf(cl, o, msg->gpgi_GInfo);

    return 0;
}
