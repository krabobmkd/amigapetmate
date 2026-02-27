#ifndef PETSCII_CANVAS_PRIVATE_H
#define PETSCII_CANVAS_PRIVATE_H

/*
 * PetsciiCanvas - private header.
 * Shared by all class_petsciicanvas_*.c files only.
 * Do NOT include from outside PetsciiCanvas/.
 */

#include <exec/types.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>
#include <intuition/intuition.h>

#include "compilers.h"
#include "petscii_canvas.h"
#include "petscii_screen.h"
#include "petscii_style.h"
#include "petscii_screenbuf.h"
#include "petscii_undo.h"

/* Convenience cast: Object * -> struct Gadget * */
#ifndef G
#define G(o) ((struct Gadget *)(o))
#endif

/* Per-instance data stored inside the BOOPSI object */
typedef struct PetsciiCanvasData {
    PetsciiScreen    *screen;     /* screen being displayed (not owned) */
    PetsciiStyle     *style;      /* colour palette (not owned)         */
    PetsciiScreenBuf *screenbuf;  /* render buffer (owned)              */
    UWORD             zoomLevel;  /* 1/2/4/8 - informational            */
    BOOL              showGrid;   /* TRUE: draw character-grid overlay  */

    /* Cached scaling buffer: pre-allocated to match the content rect and
     * reallocated only when the content size changes (GM_LAYOUT).
     * Owned by this instance; freed in OM_DISPOSE.                    */
    UBYTE            *scaledBuf;  /* contentW * contentH bytes, or NULL */
    UWORD             scaledW;    /* current buffer width  in pixels    */
    UWORD             scaledH;    /* current buffer height in pixels    */

    /* Aspect-ratio preservation */
    BOOL              keepRatio;  /* TRUE = preserve screenbuf pixel ratio */

    /* Content sub-rectangle within the gadget bounds (gadget-relative).
     * Updated by GM_LAYOUT; safety-recomputed in GM_RENDER.
     * When keepRatio=FALSE: X=0, Y=0, W/H = full gadget size.        */
    WORD              contentX;
    WORD              contentY;
    WORD              contentW;
    WORD              contentH;

    /* Drawing tool state - set via OM_SET from the main event loop */
    UBYTE             currentTool;   /* TOOL_DRAW / TOOL_COLORIZE / ...  */
    UBYTE             selectedChar;  /* 0-255 screen code to paint        */
    UBYTE             fgColor;       /* 0-15 foreground color             */
    UBYTE             bgColor;       /* 0-15 background (informational)   */

    /* Cursor cell (-1 = not shown); updated by input handler            */
    WORD              cursorCol;
    WORD              cursorRow;

    /* Per-stroke drag state for Bresenham gap-fill                      */
    BOOL              isDrawing;
    WORD              lastPaintCol;
    WORD              lastPaintRow;

    /* Undo/redo buffer (not owned; managed by petmate.c)               */
    PetsciiUndoBuffer *undoBuf;
} PetsciiCanvasData;

/* ------------------------------------------------------------------
 * Method handlers - each implemented in a separate .c file
 * ------------------------------------------------------------------ */

ULONG PetsciiCanvas_OnNew       (Class *cl, Object *o, struct opSet       *msg);
ULONG PetsciiCanvas_OnDispose   (Class *cl, Object *o, Msg                 msg);
ULONG PetsciiCanvas_OnSet       (Class *cl, Object *o, struct opSet       *msg);
ULONG PetsciiCanvas_OnGet       (Class *cl, Object *o, struct opGet       *msg);
ULONG PetsciiCanvas_OnRender    (Class *cl, Object *o, struct gpRender    *msg);
ULONG PetsciiCanvas_OnLayout    (Class *cl, Object *o, struct gpLayout    *msg);
ULONG PetsciiCanvas_OnHitTest   (Class *cl, Object *o, struct gpHitTest   *msg);
ULONG PetsciiCanvas_OnGoActive  (Class *cl, Object *o, struct gpInput     *msg);
ULONG PetsciiCanvas_OnInput     (Class *cl, Object *o, struct gpInput     *msg);
ULONG PetsciiCanvas_OnGoInactive(Class *cl, Object *o, struct gpGoInactive *msg);

/* Main dispatcher (class_petsciicanvas_dispatch.c) */
ULONG ASM SAVEDS PetsciiCanvas_Dispatch(
    REG(a0, Class *cl),
    REG(a2, Object *o),
    REG(a1, Msg msg));

#endif /* PETSCII_CANVAS_PRIVATE_H */
