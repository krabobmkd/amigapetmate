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
#include <proto/layers.h>
#include "screen_carousel_private.h"


#include <proto/cybergraphics.h>
#include <cybergraphics/cybergraphics.h>

extern struct Library *CyberGfxBase;

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

    inst->renderType = RENDT_WRITECHUNKYPIXEL8;

    if(CyberGfxBase &&
        msg->gpl_GInfo->gi_Screen &&
       msg->gpl_GInfo->gi_Screen->RastPort.BitMap &&
       (GetCyberMapAttr(msg->gpl_GInfo->gi_Screen->RastPort.BitMap, CYBRMATTR_ISCYBERGFX) != 0) &&
       (GetCyberMapAttr(msg->gpl_GInfo->gi_Screen->RastPort.BitMap, CYBRMATTR_DEPTH) > 8)
        )
    {
        /* will be ok drawing 16bit with Cybergraphics */
        inst->renderType = RENDT_CGXRGBCLUT;
    }



    return 1;
}

void ScreenCarousel_RenderOne(ScreenCarouselData *inst, Object *o,struct RastPort    *rp , ULONG idx)
{
    WORD                slotLeft, slotRight, thumbX, thumbY;
    struct Region      *oldClipRegion = NULL;
    struct Gadget      *g    = G(o);
    WORD                gw   = g->Width;
    WORD                gx   = g->LeftEdge;
    WORD                gy   = g->TopEdge;
    oldClipRegion = InstallClipRegion(rp->Layer, inst->clipRegion);


       slotLeft  = ScreenCarousel_IndexToX(inst, idx);  /* gadget-relative */
        slotRight = slotLeft + (WORD)ITEM_W - 1;

    thumbY = (WORD)ITEM_INDENT;
        /* Skip slots entirely to the left of the visible area */
        if (slotLeft + (WORD)ITEM_W <= 0) return;
        /* Stop when we've gone past the right edge */
        if (slotLeft >= gw) return;

        /* Lazy render */
        // if (!inst->minis[idx] || !inst->minis[idx]->valid)
        //     ScreenCarousel_EnsureMini(inst, i);

        thumbX = slotLeft + (WORD)ITEM_PAD;

        if (!inst->minis[idx] || !inst->minis[idx]->valid) return;

        /* Blit the 42x27 miniature */
        if(inst->renderType == RENDT_WRITECHUNKYPIXEL8)
            PetsciiScreenMini_Blit(inst->minis[idx], rp,
                                gx + thumbX, gy + thumbY);
        else if(inst->renderType == RENDT_CGXRGBCLUT)
            PetsciiScreenMini_BlitRGB(inst->minis[idx], rp,
                                gx + thumbX, gy + thumbY);
    /* important to pass NULL if oldClipRegion was NULL.*/
    InstallClipRegion(rp->Layer, oldClipRegion);
}
/* ------------------------------------------------------------------ */
/* GM_RENDER                                                            */
/* ------------------------------------------------------------------ */

ULONG ScreenCarousel_OnRender(Class *cl, Object *o, struct gpRender *msg)
{
    struct Region      *oldClipRegion = NULL;
    ScreenCarouselData *inst = (ScreenCarouselData *)INST_DATA(cl, o);
    struct Gadget      *g    = G(o);
    struct RastPort    *rp   = msg->gpr_RPort;
    WORD                gx   = g->LeftEdge;
    WORD                gy   = g->TopEdge;
    WORD                gw   = g->Width;
    WORD                gh   = g->Height;
    WORD                thumbAreaH; /* gadget height minus scroller strip   */
    ULONG               i;
    WORD                slotLeft, slotRight, thumbX, thumbY;
    WORD                midY1, midY2;
    WORD                firstSlotLeft, lastSlotRight;
    WORD                px0, px1;
    WORD                totalW, maxOffset;

    if (!rp || !inst->project) return 0;

    /* BackFill hook must be present for EraseRect to work correctly on OS3 */
    if (rp->Layer && rp->Layer->BackFill == NULL)
    {
        struct Hook *BackFill = NULL;
        GetAttr(GA_BackFill, o, &BackFill);
        if (BackFill)
            InstallLayerHook(rp->Layer, BackFill);
    }

    oldClipRegion = InstallClipRegion(rp->Layer, inst->clipRegion);

    /* Thumbnail area occupies the top part; below it a 2-px gap, then scroller */
    thumbAreaH = gh - (WORD)SCROLLER_H - (WORD)SCROLLER_GAP;

    thumbY = (WORD)ITEM_INDENT;
    midY1  = gy + thumbY;
    midY2  = gy + thumbY + (WORD)SCREENMINI_H - 1;

    /* Clamp scrollOffset */
    totalW    = (WORD)((ULONG)inst->miniCount * (ULONG)ITEM_W);
    maxOffset = (totalW > gw) ? (WORD)(totalW - gw) : 0;
    if (inst->scrollOffset < 0)         inst->scrollOffset = 0;
    if (inst->scrollOffset > maxOffset) inst->scrollOffset = maxOffset;

    /* Erase top strip (above thumbnails) */
    if (thumbY > 0)
        EraseRect(rp, (LONG)gx, (LONG)gy,
                      (LONG)(gx + gw - 1), (LONG)(midY1 - 1));

    /* Erase bottom strip of thumbnail area (below thumbnails, above scroller) */
    if (thumbY + (WORD)SCREENMINI_H < thumbAreaH)
        EraseRect(rp, (LONG)gx, (LONG)(midY2 + 1),
                      (LONG)(gx + gw - 1), (LONG)(gy + thumbAreaH - 1));

    firstSlotLeft = gw;   /* sentinel: no visible slot seen yet */
    lastSlotRight = -1;

    /* Draw each thumbnail that is (partially) visible */
    for (i = 0; i < inst->miniCount; i++) {
        slotLeft  = ScreenCarousel_IndexToX(inst, i);  /* gadget-relative */
        slotRight = slotLeft + (WORD)ITEM_W - 1;

        /* Skip slots entirely to the left of the visible area */
        if (slotLeft + (WORD)ITEM_W <= 0) continue;
        /* Stop when we've gone past the right edge */
        if (slotLeft >= gw) break;

        /* Lazy render */
        if (!inst->minis[i] || !inst->minis[i]->valid)
            ScreenCarousel_EnsureMini(inst, i);

        thumbX = slotLeft + (WORD)ITEM_PAD;

        /* Track visible slot extent for outer gap erasing */
        if (slotLeft < firstSlotLeft) firstSlotLeft = slotLeft;
        if (slotRight > lastSlotRight) lastSlotRight = slotRight;

        /* Erase left pad strip of this slot (in the thumbnail row band) */
        px0 = (slotLeft < 0) ? 0 : slotLeft;
        px1 = thumbX - 1;
        if (px0 <= px1)
            EraseRect(rp, (LONG)(gx + px0), (LONG)midY1,
                          (LONG)(gx + px1), (LONG)midY2);

        /* Erase right pad strip of this slot (in the thumbnail row band) */
        px0 = thumbX + (WORD)SCREENMINI_W;
        px1 = (slotRight >= gw) ? gw - 1 : slotRight;
        if (px0 < gw && px0 <= px1)
            EraseRect(rp, (LONG)(gx + px0), (LONG)midY1,
                          (LONG)(gx + px1), (LONG)midY2);

        if (!inst->minis[i] || !inst->minis[i]->valid) continue;

        /* Blit the 42x27 miniature */
        if(inst->renderType == RENDT_WRITECHUNKYPIXEL8)
            PetsciiScreenMini_Blit(inst->minis[i], rp,
                                gx + thumbX, gy + thumbY);
        else if(inst->renderType == RENDT_CGXRGBCLUT)
             PetsciiScreenMini_BlitRGB(inst->minis[i], rp,
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

    /* Erase horizontal outer gaps in the thumbnail row band */
    if (lastSlotRight < 0) {
        /* No visible slots at all: erase entire middle band */
        EraseRect(rp, (LONG)gx, (LONG)midY1,
                      (LONG)(gx + gw - 1), (LONG)midY2);
    } else {
        /* Left outer gap: before first visible slot */
        if (firstSlotLeft > 0)
            EraseRect(rp, (LONG)gx, (LONG)midY1,
                          (LONG)(gx + firstSlotLeft - 1), (LONG)midY2);
        /* Right outer gap: after last visible slot */
        if (lastSlotRight < gw - 1)
            EraseRect(rp, (LONG)(gx + lastSlotRight + 1), (LONG)midY1,
                          (LONG)(gx + gw - 1), (LONG)midY2);
    }

    /* ------------------------------------------------------------------ */
    /* Gap between thumbnail area and scroller bar                        */
    /* ------------------------------------------------------------------ */
    EraseRect(rp, (LONG)gx, (LONG)(gy + thumbAreaH),
                  (LONG)(gx + gw - 1), (LONG)(gy + thumbAreaH + (WORD)SCROLLER_GAP - 1));

    /* ------------------------------------------------------------------ */
    /* Horizontal scroller bar                                              */
    /* ------------------------------------------------------------------ */
    {
        WORD scrollY = gy + thumbAreaH + (WORD)SCROLLER_GAP;  /* top row of scroller strip */
        WORD scrollY2 = gy + gh - 1;    /* bottom row of scroller strip */

        if (maxOffset == 0) {
            /* All slots fit: invisible bar */
            EraseRect(rp, (LONG)gx, (LONG)scrollY,
                          (LONG)(gx + gw - 1), (LONG)scrollY2);
        } else {
            /* Compute thumb width and position */
            WORD barW = (WORD)((LONG)gw * (LONG)gw / (LONG)totalW);
            WORD barX;
            if (barW < 4)  barW = 4;
            if (barW > gw) barW = gw;
            barX = (WORD)((LONG)inst->scrollOffset * (LONG)(gw - barW)
                          / (LONG)maxOffset);

            /* Left empty area */
            if (barX > 0) {
                SetAPen(rp, 0);
                RectFill(rp, (LONG)gx, (LONG)scrollY,
                             (LONG)(gx + barX - 1), (LONG)scrollY2);
            }
            /* Thumb */
            SetAPen(rp, 1);
            RectFill(rp, (LONG)(gx + barX), (LONG)scrollY,
                         (LONG)(gx + barX + barW - 1), (LONG)scrollY2);
            /* Right empty area */
            if (barX + barW < gw) {
                SetAPen(rp, 0);
                RectFill(rp, (LONG)(gx + barX + barW), (LONG)scrollY,
                             (LONG)(gx + gw - 1), (LONG)scrollY2);
            }
        }
    }

    /* important to pass NULL if oldClipRegion was NULL.*/
    InstallClipRegion(rp->Layer, oldClipRegion);
    return 0;
}
