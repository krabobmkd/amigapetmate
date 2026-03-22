#ifndef SCREEN_CAROUSEL_PRIVATE_H
#define SCREEN_CAROUSEL_PRIVATE_H

/*
 * ScreenCarousel - private header.
 * Shared only among class_screencarousel_*.c files.
 * Do NOT include from outside ScreenCarousel/.
 */

#include <exec/types.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>
#include <intuition/intuition.h>
#include <graphics/rastport.h>

#include "compilers.h"
#include "screen_carousel.h"
#include "petscii_project.h"
#include "petscii_style.h"
#include "petscii_screenmini.h"

/* Convenience cast */
#ifndef G
#define G(o) ((struct Gadget *)(o))
#endif


#define RENDT_WRITECHUNKYPIXEL8 0
#define RENDT_CGXRGBCLUT 1
#define RENDT_OCS 2


/*
 * Layout constants (horizontal strip, left to right)
 * Each thumbnail slot is SCREENMINI_W pixels wide + ITEM_PAD on each side,
 * giving ITEM_W per slot.
 * ITEM_INDENT: vertical offset from gadget top edge to thumbnail top edge.
 */
#define SEL_BORDER   1   /* highlight border thickness (pixels)          */
#define ITEM_PAD     2   /* pixels of padding left/right/top/bottom      */
#define ITEM_W       (SCREENMINI_W + ITEM_PAD * 2)   /* 42 + 4 = 46     */
#define ITEM_INDENT  (ITEM_PAD + SEL_BORDER)          /* top margin       */
#define SCROLLER_H   4   /* height in pixels of the horizontal scroller  */
#define SCROLLER_GAP 2   /* blank pixels between thumbnail area and scroller */

/* Per-instance data stored inside the BOOPSI object */
typedef struct ScreenCarouselData {
    PetsciiProject    *project;       /* not owned */
    PetsciiStyle      *style;         /* not owned */
    ULONG              currentScreen; /* index of highlighted screen */
    ULONG              signalScreen;  /* index of last clicked screen (OM_GET) */
    WORD               scrollOffset;  /* horizontal scroll in pixels (>= 0) */
    WORD                d;
    PetsciiScreenMini *minis[PETSCII_MAX_SCREENS]; /* owned; NULL = not yet created */
    ULONG              miniCount;     /* mirrors project->screenCount */

    /* allow cliping draw commands */
    struct Region *clipRegion;

    /* Horizontal scroller drag state */
    WORD  scrollDragActive;     /* 1 while user is dragging the scroller bar */
    WORD  scrollDragStartX;     /* gadget-relative X at drag start           */
    WORD  scrollDragStartOffset;/* scrollOffset value at drag start          */

    WORD renderType;
} ScreenCarouselData;

/* ------------------------------------------------------------------ */
/* Method handlers — each implemented in a separate .c file            */
/* ------------------------------------------------------------------ */

ULONG ScreenCarousel_OnNew      (Class *cl, Object *o, struct opSet      *msg);
ULONG ScreenCarousel_OnDispose  (Class *cl, Object *o, Msg                msg);
ULONG ScreenCarousel_OnSet      (Class *cl, Object *o, struct opSet      *msg);
ULONG ScreenCarousel_OnGet      (Class *cl, Object *o, struct opGet      *msg);
ULONG ScreenCarousel_OnRender   (Class *cl, Object *o, struct gpRender   *msg);
ULONG ScreenCarousel_OnLayout   (Class *cl, Object *o, struct gpLayout   *msg);
ULONG ScreenCarousel_OnHitTest  (Class *cl, Object *o, struct gpHitTest  *msg);
ULONG ScreenCarousel_OnGoActive (Class *cl, Object *o, struct gpInput    *msg);
ULONG ScreenCarousel_OnInput    (Class *cl, Object *o, struct gpInput    *msg);

/* Main dispatcher */
ULONG ASM SAVEDS ScreenCarousel_Dispatch(
    REG(a0, Class  *cl),
    REG(a2, Object *o),
    REG(a1, Msg     msg));

/* ------------------------------------------------------------------ */
/* Shared helpers                                                        */
/* ------------------------------------------------------------------ */

/*
 * Convert a gadget-relative X coordinate to a screen index.
 * Returns -1 if the coordinate falls outside any thumbnail slot or
 * outside the valid range of screens.
 */
static LONG ScreenCarousel_XToIndex(ScreenCarouselData *inst, WORD gadgetX)
{
    LONG slotX;
    LONG idx;

    slotX = (LONG)gadgetX + (LONG)inst->scrollOffset;
    if (slotX < 0) return -1;

    idx = slotX / ITEM_W;
    if ((ULONG)idx >= inst->miniCount) return -1;

    return idx;
}

/*
 * X pixel coordinate (gadget-relative) of the left edge of thumbnail slot idx.
 */
static WORD ScreenCarousel_IndexToX(ScreenCarouselData *inst, ULONG idx)
{
    return (WORD)((LONG)(idx * (ULONG)ITEM_W) - (LONG)inst->scrollOffset);
}

/*
 * Ensure a miniature exists and is rendered for the given screen index.
 * Creates the PetsciiScreenMini if it doesn't exist yet.
 * Returns 0 on allocation failure.
 */
static int ScreenCarousel_EnsureMini(ScreenCarouselData *inst, ULONG idx)
{
    PetsciiScreen *scr;

    if (idx >= PETSCII_MAX_SCREENS || !inst->project) return 0;
    scr = inst->project->screens[idx];
    if (!scr) return 0;

    if (!inst->minis[idx]) {
        inst->minis[idx] = PetsciiScreenMini_Create();
        if (!inst->minis[idx]) return 0;
    }

    PetsciiScreenMini_Render(inst->minis[idx], scr, inst->style);
    return 1;
}

#endif /* SCREEN_CAROUSEL_PRIVATE_H */
