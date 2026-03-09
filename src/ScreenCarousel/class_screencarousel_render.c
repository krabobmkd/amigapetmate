/*
 * ScreenCarousel - BOOPSI gadget class for the screen thumbnail strip.
 * GM_RENDER: paint the horizontal thumbnail strip into the gadget area.
 *
 * Layout (gadget-relative coordinates):
 *
 *   Thumbnails are arranged left to right.  Each slot is ITEM_W (46) pixels
 *   wide: ITEM_PAD (2) | 42-px thumbnail | ITEM_PAD (2).
 *   Vertically the gadget is just tall enough to hold one thumbnail:
 *     ITEM_INDENT (= ITEM_PAD + SEL_BORDER = 3 px) above the 27-px thumbnail,
 *     and the same amount below (enforced by MinHeight in the layout).
 *
 *   The current screen's thumbnail gets a 1-pixel highlight border (pen 2)
 *   drawn around it.
 *
 *   Background (gaps between / around thumbs) is pen 0.
 *
 *   inst->scrollOffset is a pixel offset from the start of the list to the
 *   left edge of the gadget — non-negative; 0 means the first thumbnail
 *   starts at the gadget's left edge.
 */

#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include "screen_carousel_private.h"

/* ------------------------------------------------------------------ */
/* Internal helpers                                                     */
/* ------------------------------------------------------------------ */

static void drawBorder(struct RastPort *rp,
                       WORD gx, WORD gy,
                       WORD x0, WORD y0, WORD x1, WORD y1,
                       UBYTE p)
{
    SetAPen(rp, p);
    Move(rp, gx + x0, gy + y0); Draw(rp, gx + x1, gy + y0); /* top    */
    Move(rp, gx + x0, gy + y1); Draw(rp, gx + x1, gy + y1); /* bottom */
    Move(rp, gx + x0, gy + y0); Draw(rp, gx + x0, gy + y1); /* left   */
    Move(rp, gx + x1, gy + y0); Draw(rp, gx + x1, gy + y1); /* right  */
}

/* ------------------------------------------------------------------ */
/* GM_LAYOUT                                                            */
/* ------------------------------------------------------------------ */

ULONG ScreenCarousel_OnLayout(Class *cl, Object *o, struct gpLayout *msg)
{
   ScreenCarouselData *inst = (ScreenCarouselData *)INST_DATA(cl, o);


    if (inst->clipRegion)
    {
        struct Rectangle framerect;
        framerect.MinX = G(o)->LeftEdge ;
        framerect.MinY = G(o)->TopEdge;
        framerect.MaxX = G(o)->LeftEdge  + G(o)->Width -1;
        framerect.MaxY = G(o)->TopEdge  + G(o)->Height -1;

        ClearRegion(inst->clipRegion);
        OrRectRegion(inst->clipRegion, &framerect);
    }

    return 1;
}

/* ------------------------------------------------------------------ */
/* GM_RENDER                                                            */
/* ------------------------------------------------------------------ */

ULONG ScreenCarousel_OnRender(Class *cl, Object *o, struct gpRender *msg)
{
    struct Region *oldClipRegion=NULL;
    ScreenCarouselData *inst = (ScreenCarouselData *)INST_DATA(cl, o);
    struct Gadget      *g    = G(o);
    struct RastPort    *rp   = msg->gpr_RPort;
    WORD                gx   = g->LeftEdge;
    WORD                gy   = g->TopEdge;
    WORD                gw   = g->Width;
    WORD                gh   = g->Height;
    ULONG               i;
    WORD                slotLeft, thumbX, thumbY;

    if (!rp || !inst->project) return 0;

    oldClipRegion = InstallClipRegion( rp->Layer, inst->clipRegion);


    /* Fill entire gadget background with pen 0 */
    SetAPen(rp, 0);
    RectFill(rp, gx, gy, gx + gw - 1, gy + gh - 1);

    /* Vertical position of thumbnails: centred with ITEM_INDENT top margin */
    thumbY = (WORD)ITEM_INDENT;

    /* Draw each thumbnail that is (partially) visible */
    for (i = 0; i < inst->miniCount; i++) {
        slotLeft = ScreenCarousel_IndexToX(inst, i);  /* gadget-relative */

        /* Skip slots entirely to the left of the visible area */
        if (slotLeft + (WORD)ITEM_W <= 0) continue;
        /* Stop when we've gone past the right edge */
        if (slotLeft >= gw) break;

        /* Lazy render */
        if (!inst->minis[i] || !inst->minis[i]->valid)
            ScreenCarousel_EnsureMini(inst, i);

        if (!inst->minis[i] || !inst->minis[i]->valid) continue;

        thumbX = slotLeft + (WORD)ITEM_PAD;

        /* Blit the 42x27 miniature */
        PetsciiScreenMini_Blit(inst->minis[i], rp,
                                gx + thumbX, gy + thumbY);

        /* Highlight border around the current screen */
        if (i == inst->currentScreen) {
            drawBorder(rp, gx, gy,
                       thumbX - SEL_BORDER,
                       thumbY - SEL_BORDER,
                       thumbX + SCREENMINI_W - 1 + SEL_BORDER,
                       thumbY + SCREENMINI_H - 1 + SEL_BORDER,
                       2);
        }
    }
    /* important to pass NULL if oldClipRegion was NULL.*/
    InstallClipRegion( rp->Layer,oldClipRegion);
    return 0;
}
