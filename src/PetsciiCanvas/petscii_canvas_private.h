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
#include "petscii_brush.h"

/* Convenience cast: Object * -> struct Gadget * */
#ifndef G
#define G(o) ((struct Gadget *)(o))
#endif

/*
enum for render type,  renderType
*/
#define RENDT_WRITECHUNKYPIXEL8 0
#define RENDT_CGXRGBCLUT 1
#define RENDT_OCS 2


/* Per-instance data stored inside the BOOPSI object */
typedef struct PetsciiCanvasData {
    PetsciiScreen    *screen;     /* screen being displayed (not owned) */
    PetsciiStyle     *style;      /* colour palette (not owned)         */
    PetsciiScreenBuf *screenbuf;  /* render buffer (owned)              */
    UWORD             zoomLevel;  /* 1/2/4/8 - informational            */
    BOOL              showGrid;   /* TRUE: draw character-grid overlay  */

    /* allow cliping draw commands */
    struct Region *clipRegion;

    /* Cached scaling buffer: pre-allocated to match the content rect and
     * reallocated only when the content size changes (GM_LAYOUT).
     * Owned by this instance; freed in OM_DISPOSE.                    */
    UBYTE            *scaledBuf;  /* contentW * contentH bytes, or NULL */
    UWORD             scaledW;    /* current buffer width  in pixels    */
    UWORD             scaledH;    /* current buffer height in pixels    */

    /* Aspect-ratio preservation */
    BOOL              keepRatio;  /* TRUE = preserve screenbuf pixel ratio */

    /* trick, kept here so it pads 4bytes */
    BOOL              refreshExtraMarge;

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
    UBYTE             fgColor;       /* 0-15 foreground color, to draw with */
    UBYTE             pad0;          /* alignment */

    /* Cursor / hover cell (-1 = not shown); updated by input handler   */
    WORD              cursorCol;
    WORD              cursorRow;

    /* Per-stroke drag state for Bresenham gap-fill                      */
    BOOL              isDrawing;
    WORD              lastPaintCol;
    WORD              lastPaintRow;

    /* Brush size in character cells (1x1 = single char, Phase 6 default).
     * Phase 10 will grow these when the user selects a multi-char brush. */
    WORD              brushW;
    WORD              brushH;

    /* hotspot of the hover handle in the brush, in char. */
    WORD              brushHotx;
    WORD              brushHoty;

    /* Previously rendered hover overlay position (char-cell coords).
     * Used by the partial render path to repair the old overlay region.
     * -1 = no overlay was drawn yet.                                    */
    WORD              prevHoverCol;
    WORD              prevHoverRow;
    WORD              prevBrushW;
    WORD              prevBrushH;

    /* TRUE when screenbuf cells changed (paintCell / OM_SET) but
     * scaledBuf has not yet been refreshed.  Forces a full blit in the
     * next OnRender call.  Cleared after BlitScaled.                   */
    BOOL              scaledBufDirty;

    /* Pre-allocated buffer for hover overlay rendering (scaled pixels).
     * Sized to contentW * contentH bytes — same as scaledBuf.
     * scaledBuf holds the clean screenbuf pixels (repair source).
     * overlayBuf is the temporary dest for scaling the brush preview.  */
    UBYTE            *overlayBuf;

    /* Brush buffer (owned by canvas; replaced on each lasso capture).
     * NULL means 1x1 single-char mode (brushW/H == 1).                */
    PetsciiBrush *brush;

    /* Lasso selection state (TOOL_BRUSH, select sub-mode).
     * isLassoing = TRUE while the user is drag-selecting a rectangle.
     * lassoEnd* tracks the live mouse position during the drag.        */
    BOOL  isLassoing;
    WORD  lassoStartCol;
    WORD  lassoStartRow;
    WORD  lassoEndCol;
    WORD  lassoEndRow;

    /* Pre-allocated native-resolution buffer for brush hover preview.
     * Size: nativeBrushBufSize bytes = brushW*8 * brushH*8.
     * Re-allocated whenever the brush changes size.                    */
    UBYTE *nativeBrushBuf;
    ULONG  nativeBrushBufSize;

    /* Text tool cursor: character cell where next typed char is written.
     * Initialized to (0,0). Updated by mouse click or cursor advance.
     * Always valid (>= 0) in TOOL_TEXT mode.                          */
    WORD  textCursorCol;
    WORD  textCursorRow;

    /* wheck if */
    WORD renderType;
} PetsciiCanvasData;

/* keymap.library base defined in class_petsciicanvas_lib.c */
extern struct Library *KeymapBase;

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

/* Shared notification helper (class_petsciicanvas_handleinput.c).
 * Sends an OM_NOTIFY for the given attribute up the BOOPSI chain.
 * GInfo may be NULL when called from OM_SET with no visual context. */
ULONG PetsciiCanvas_NotifyAttribChange(Class *cl, Object *Gad,
                                       struct GadgetInfo *GInfo,
                                       ULONG attrib, ULONG value);

/* Main dispatcher (class_petsciicanvas_dispatch.c) */
ULONG ASM SAVEDS PetsciiCanvas_Dispatch(
    REG(a0, Class *cl),
    REG(a2, Object *o),
    REG(a1, Msg msg));

#endif /* PETSCII_CANVAS_PRIVATE_H */
