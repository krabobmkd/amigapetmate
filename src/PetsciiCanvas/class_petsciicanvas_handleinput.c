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
 * Bresenham gap-fill fills in cells skipped during fast drags
 * (TOOL_DRAW / TOOL_COLORIZE / TOOL_CHARDRAW only; not for TOOL_BRUSH paste).
 *
 * TOOL_BRUSH state machine
 * ------------------------
 * Sub-state A (brush == NULL): waiting for selection.
 *   Click -> start lasso (isLassoing = TRUE).
 * Sub-state B (isLassoing): drag-selecting rectangle.
 *   Move  -> grow lasso rectangle overlay.
 *   Up    -> capture rectangle to brush, isLassoing = FALSE.
 * Sub-state C (brush != NULL): paste mode.
 *   Hover -> show brush sprite.
 *   Click -> stamp brush onto screen, keep hovering.
 * Clicking CharSelector resets to 1x1 TOOL_DRAW (handled in OnSet).
 *
 * Mouse coordinates (gpi_Mouse.X/Y) are gadget-relative.
 */

#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/exec.h>
#include <proto/keymap.h>
#include <devices/inputevent.h>
#include <clib/alib_protos.h>
#include "petscii_canvas_private.h"
#include "petscii_ascii.h"
#include <bdbprintf.h>
/* ------------------------------------------------------------------ */
/* Static helpers                                                       */
/* ------------------------------------------------------------------ */

/*
 * Convert gadget-relative mouse coords to character cell (col, row).
 * Returns -1/-1 when outside a valid content rect.
 */
static BOOL mouseToCell(PetsciiCanvasData *inst, WORD relX, WORD relY,
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
        return FALSE;
    }

    cx = (WORD)(relX - inst->contentX);
    cy = (WORD)(relY - inst->contentY);

    if (cx < 0)   cx = 0;
    if (cy < 0)   cy = 0;
    if (cx >= cW) cx = (WORD)(cW - 1);
    if (cy >= cH) cy = (WORD)(cH - 1);

    /* note div ULONG by UWORD is better for 68000 16bit. */
    *outCol = (WORD)((ULONG)cx * (ULONG)inst->screen->width  / (UWORD)cW);
    *outRow = (WORD)((ULONG)cy * (ULONG)inst->screen->height / (UWORD)cH);

    if (*outCol >= (WORD)inst->screen->width)
        *outCol = (WORD)(inst->screen->width  - 1);
    if (*outRow >= (WORD)inst->screen->height)
        *outRow = (WORD)(inst->screen->height - 1);

    return TRUE;
}
static BOOL isInsideRect(PetsciiCanvasData *inst, WORD relX, WORD relY)
{
    WORD cW = inst->contentW;
    WORD cH = inst->contentH;
    WORD cx;
    WORD cy;

    if (cW <= 0 || cH <= 0 || !inst->screen ||
        !inst->screen->width || !inst->screen->height) {
        return FALSE;
    }
    cx = (WORD)(relX - inst->contentX);
    cy = (WORD)(relY - inst->contentY);

    if ( (cx < 0) || (cy<0) || (cx >= cW) || (cy >= cH) ) return FALSE;

    return TRUE;
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
 * Stamp the current brush onto the screen starting at (col, row).
 * Clips cells that fall outside the screen.
 * watch out now col,row can be negative because of hot spot
 */
static void paintBrush(PetsciiCanvasData *inst, WORD col, WORD row)
{
    WORD         bc;
    WORD         br;
    WORD          dc;
    WORD          dr;
    PetsciiPixel *cell;

    if (!inst->brush || !inst->screen || !inst->screenbuf || !inst->style)
        return;

    for (br = 0; br < (WORD)inst->brush->h; br++) {
        for (bc = 0; bc < (WORD)inst->brush->w; bc++) {
            dc = (WORD)(col + (WORD)bc);
            dr = (WORD)(row + (WORD)br);
            if (dc < 0 || dr < 0) continue;
            if ((UWORD)dc >= inst->screen->width ||
                (UWORD)dr >= inst->screen->height) continue;

            cell = &inst->brush->cells[(ULONG)br * inst->brush->w + bc];
            PetsciiScreen_SetPixel(inst->screen, (UWORD)dc, (UWORD)dr,
                                   cell->code, cell->color);
            PetsciiScreenBuf_UpdateCell(inst->screenbuf, inst->screen,
                                        inst->style,
                                        (UWORD)dc, (UWORD)dr);
        }
    }
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
/* GM_HITTEST - always hit (the gadget fills its entire bounds)        */
/* ------------------------------------------------------------------ */

ULONG PetsciiCanvas_OnHitTest(Class *cl, Object *o, struct gpHitTest *msg)
{
    (void)cl; (void)o; (void)msg;

    return GMR_GADGETHIT;
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

    if (inst->currentTool == TOOL_TEXT) {
        /* ---- TOOL_TEXT: click sets text cursor position -------------- */
        if (isClick && col >= 0 && row >= 0) {
            inst->textCursorCol = col;
            inst->textCursorRow = row;
        }
        inst->isDrawing = FALSE;
        inst->cursorCol = col;
        inst->cursorRow = row;
        renderSelf(cl, o, msg->gpi_GInfo);
        return GMR_MEACTIVE;
    }

    if (inst->currentTool == TOOL_BRUSH) {
        /* ---- TOOL_BRUSH click ---------------------------------------- */
        if (isClick) {
            if (!inst->brush) {
                /* Sub-state A → B: start lasso selection */
                inst->isLassoing    = TRUE;
                inst->lassoStartCol = (col >= 0) ? col : 0;
                inst->lassoStartRow = (row >= 0) ? row : 0;
                inst->lassoEndCol   = inst->lassoStartCol;
                inst->lassoEndRow   = inst->lassoStartRow;
                inst->isDrawing     = FALSE;
            } else {
                /* Sub-state C: stamp brush on click */
                if (inst->undoBuf && inst->screen) {
                    PetsciiUndoBuffer_Push(inst->undoBuf, inst->screen);
                }

                if (col >= 0 && row >= 0)
                {
                  //  bdbprintf("stanmp case 1\n");
                    paintBrush(inst, col-inst->brushHotx, row-inst->brushHoty);
                }
                inst->lastPaintCol = col-inst->brushHotx;
                inst->lastPaintRow = row-inst->brushHoty;
                inst->isDrawing    = TRUE;
            }
        } else {
            /* Hover entry: just show brush preview */
            inst->isDrawing    = FALSE;
            inst->lastPaintCol = -1;
            inst->lastPaintRow = -1;
        }
    } else {
        /* ---- TOOL_DRAW / COLORIZE / CHARDRAW click ------------------- */
        if (isClick) {
            /* Take undo snapshot before the first painted cell of this stroke */
            if (inst->undoBuf && inst->screen) {
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
    BOOL               isIn;
    BOOL               brushAlreadyStamped = 0;
    /* text-mode key mapping temporaries */
    struct InputEvent  mapEvent;
    UBYTE              mapbuf[8];
    LONG               nchars;
    int                screenCode;

    inst = (PetsciiCanvasData *)INST_DATA(cl, o);
    ie   = msg->gpi_IEvent;

    /* ---------------------------------------------------------------- */
    /* RAWKEY events                                                      */
    /* In text mode: consume and handle typed characters.                */
    /* Otherwise:    pass up so window/menu handlers receive them.       */
    /* ---------------------------------------------------------------- */
    if (ie->ie_Class == IECLASS_RAWKEY) {
        if (inst->currentTool == TOOL_TEXT &&
            !(ie->ie_Code & IECODE_UP_PREFIX)) {
            /* Key-down: translate raw key to ASCII via system keymap */
            mapEvent              = *ie;
            mapEvent.ie_NextEvent = NULL;
            nchars = MapRawKey(&mapEvent, (STRPTR)mapbuf,
                               (LONG)sizeof(mapbuf), NULL);

            if (nchars > 0 && (UBYTE)mapbuf[0] <= 127 &&
                inst->screen && inst->screenbuf && inst->style) {

                if (inst->screen->charset == PETSCII_CHARSET_LOWER)
                    screenCode = PetsciiAscii_ToLowerScreenCode(mapbuf[0]);
                else
                    screenCode = PetsciiAscii_ToUpperScreenCode(mapbuf[0]);

                if (screenCode >= 0 &&
                    inst->textCursorCol >= 0 && inst->textCursorRow >= 0) {

                    if (inst->undoBuf)
                        PetsciiUndoBuffer_Push(inst->undoBuf, inst->screen);

                    PetsciiScreen_SetPixel(inst->screen,
                                           (UWORD)inst->textCursorCol,
                                           (UWORD)inst->textCursorRow,
                                           (UBYTE)screenCode,
                                           inst->fgColor);
                    PetsciiScreenBuf_UpdateCell(inst->screenbuf, inst->screen,
                                                inst->style,
                                                (UWORD)inst->textCursorCol,
                                                (UWORD)inst->textCursorRow);
                    inst->scaledBufDirty = TRUE;

                    /* Advance cursor right; wrap at end of line then screen */
                    inst->textCursorCol++;
                    if (inst->textCursorCol >= (WORD)inst->screen->width) {
                        inst->textCursorCol = 0;
                        inst->textCursorRow++;
                        if (inst->textCursorRow >= (WORD)inst->screen->height) {
                            inst->textCursorCol = 0;
                            inst->textCursorRow = 0;
                        }
                    }
                    renderSelf(cl, o, msg->gpi_GInfo);
                }
            }
        }
        /* text mode: consume (key-down handled above, key-up ignored).
         * other modes: pass key events up to window/menu handlers.    */
        //return (inst->currentTool == TOOL_TEXT) ? GMR_MEACTIVE : GMR_REUSE;
        return GMR_REUSE;
    } // rawkey end

    if (ie->ie_Class != IECLASS_RAWMOUSE)
        return GMR_MEACTIVE;

    /* ---------------------------------------------------------------- */
    /* LBUTTON UP                                                        */
    /* ---------------------------------------------------------------- */
    if (ie->ie_Code == (IECODE_LBUTTON | IECODE_UP_PREFIX)) {

        /* Text mode: stay active to keep receiving keyboard events */
        if (inst->currentTool == TOOL_TEXT)
            return GMR_MEACTIVE;

        if (inst->currentTool == TOOL_BRUSH && inst->isLassoing) {
            /* Finalize lasso: capture the selected rectangle into brush */
            WORD col1, row1, col2, row2;
            WORD bW, bH;

            col1 = (inst->lassoStartCol < inst->lassoEndCol)
                   ? inst->lassoStartCol : inst->lassoEndCol;
            row1 = (inst->lassoStartRow < inst->lassoEndRow)
                   ? inst->lassoStartRow : inst->lassoEndRow;
            col2 = (inst->lassoStartCol > inst->lassoEndCol)
                   ? inst->lassoStartCol : inst->lassoEndCol;
            row2 = (inst->lassoStartRow > inst->lassoEndRow)
                   ? inst->lassoStartRow : inst->lassoEndRow;

            bW = (WORD)(col2 - col1 + 1);
            bH = (WORD)(row2 - row1 + 1);

            /* Replace existing brush with the newly captured one */
            if (inst->brush) {
                PetsciiBrush_Destroy(inst->brush);
                inst->brush = NULL;
            }
            if (inst->screen) {
                inst->brush = PetsciiBrush_CaptureFromScreen(
                                inst->screen,
                                (UWORD)col1, (UWORD)row1,
                                (UWORD)bW,   (UWORD)bH);
            }

            if (inst->brush) {
                inst->brushW = (WORD)inst->brush->w;
                inst->brushH = (WORD)inst->brush->h;
                inst->brushHotx =  inst->brushW>>1;
                inst->brushHoty =  inst->brushH>>1;
            } else {
                /* Capture failed (OOM) — fall back to 1x1 */
                inst->brushW = 1;
                inst->brushH = 1;
                inst->brushHotx = 0;
                inst->brushHoty = 0;
            }

            inst->isLassoing   = FALSE;
            inst->isDrawing    = FALSE;
            inst->lastPaintCol = -1;
            inst->lastPaintRow = -1;

            mouseToCell(inst, msg->gpi_Mouse.X, msg->gpi_Mouse.Y,
                        &inst->cursorCol, &inst->cursorRow);

            renderSelf(cl, o, msg->gpi_GInfo);

            isIn = isInsideRect(inst, msg->gpi_Mouse.X, msg->gpi_Mouse.Y);
            return isIn ? GMR_MEACTIVE : GMR_NOREUSE;
        }

        /* Normal button release */
        inst->isDrawing    = FALSE;
        inst->lastPaintCol = -1;
        inst->lastPaintRow = -1;

        isIn = isInsideRect(inst, msg->gpi_Mouse.X, msg->gpi_Mouse.Y);
        return isIn ? GMR_MEACTIVE : GMR_NOREUSE;
    }

    /* ---------------------------------------------------------------- */
    /* LBUTTON DOWN                                                      */
    /* ---------------------------------------------------------------- */
    if (ie->ie_Code == IECODE_LBUTTON) {

        mouseToCell(inst, msg->gpi_Mouse.X, msg->gpi_Mouse.Y, &col, &row);

        /* Text mode: click repositions the text cursor */
        if (inst->currentTool == TOOL_TEXT) {
            if (col >= 0 && row >= 0) {
                inst->textCursorCol = col;
                inst->textCursorRow = row;
            }
            inst->cursorCol = col;
            inst->cursorRow = row;
            renderSelf(cl, o, msg->gpi_GInfo);
            return GMR_MEACTIVE;
        }

        if (inst->currentTool == TOOL_BRUSH) {

            if (!inst->brush && !inst->isLassoing) {
                /* Sub-state A → B: start new lasso */
                inst->isLassoing    = TRUE;
                inst->lassoStartCol = (col >= 0) ? col : 0;
                inst->lassoStartRow = (row >= 0) ? row : 0;
                inst->lassoEndCol   = inst->lassoStartCol;
                inst->lassoEndRow   = inst->lassoStartRow;
                inst->isDrawing     = FALSE;
            } else if (inst->brush && !inst->isLassoing) {
                /* Sub-state C: stamp brush */
                if (inst->undoBuf && inst->screen) {
                    PetsciiUndoBuffer_Push(inst->undoBuf, inst->screen);
                }
                if (col >= 0 && row >= 0) {
                      //              bdbprintf("stanmp case 2\n");
                    paintBrush(inst, col -inst->brushHotx, row -inst->brushHoty);
                    inst->lastPaintCol = col-inst->brushHotx;
                    inst->lastPaintRow = row-inst->brushHoty;
                    brushAlreadyStamped = 1;
                } else {
                    inst->lastPaintCol = -1;
                    inst->lastPaintRow = -1;
                }
                inst->isDrawing = TRUE;
            }
            /* If isLassoing is already TRUE, ignore (shouldn't happen) */

        } else {
            /* Normal draw tools */
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
        }

        inst->cursorCol = col;
        inst->cursorRow = row;
        renderSelf(cl, o, msg->gpi_GInfo);
        return GMR_MEACTIVE;
    }

    /* ---------------------------------------------------------------- */
    /* MOUSE MOVE (NOBUTTON)                                             */
    /* ---------------------------------------------------------------- */
    if (ie->ie_Code == IECODE_NOBUTTON) {

        /* Text mode: mouse move doesn't affect the text cursor.
         * Stay active regardless of mouse position so keyboard events
         * continue to arrive while the user is typing.               */
        if (inst->currentTool == TOOL_TEXT)
            return GMR_MEACTIVE;

        isIn = isInsideRect(inst, msg->gpi_Mouse.X, msg->gpi_Mouse.Y);

        /* Keep activation while lassoing or drawing, even outside rect */
        if (!isIn && !inst->isDrawing && !inst->isLassoing)
            return GMR_NOREUSE;

        mouseToCell(inst, msg->gpi_Mouse.X, msg->gpi_Mouse.Y, &col, &row);

        if (inst->currentTool == TOOL_BRUSH) {

            if (inst->isLassoing) {
                /* Update lasso end as mouse moves */
                if (col >= 0) inst->lassoEndCol = col;
                if (row >= 0) inst->lassoEndRow = row;

            } else if (inst->isDrawing && inst->brush && isIn) {
                /* Brush paste drag: stamp at each move event (no gap fill) */
                if (col >= 0 && row >= 0) {
                    if(brushAlreadyStamped==0)
                    {
                       // bdbprintf("stanmp case 3\n");
                        paintBrush(inst, col-inst->brushHotx, row-inst->brushHoty);
                        inst->lastPaintCol = col-inst->brushHotx;
                        inst->lastPaintRow = row-inst->brushHoty;
                    }
                } else {
                    inst->lastPaintCol = -1;
                    inst->lastPaintRow = -1;
                }
            } else if (!isIn) {
                inst->lastPaintCol = -1;
                inst->lastPaintRow = -1;
            }

        } else {
            /* Normal draw tools */
            if (inst->isDrawing) {
                if (isIn && col >= 0 && row >= 0) {
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
                } else {
                    inst->lastPaintCol = -1;
                    inst->lastPaintRow = -1;
                }
            }
        }

        /* scaledBufDirty was set by paintCell/paintBrush if drawing;
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

    /* Cancel any in-progress lasso */
    inst->isLassoing = FALSE;

    /* Hide cursor: set to -1 so drawHoverOverlay draws nothing,
     * repairHoverRegion restores the last overlay position.          */
    inst->cursorCol = -1;
    inst->cursorRow = -1;

    /* Partial render: repair the last overlay, draw nothing new */
    renderSelf(cl, o, msg->gpgi_GInfo);

    return 0;
}
