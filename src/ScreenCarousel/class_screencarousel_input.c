/*
 * ScreenCarousel - BOOPSI gadget class for the screen thumbnail strip.
 * Input / hit-test handlers.
 *
 * GM_HITTEST    : always hit (gadget fills its bounds).
 * GM_GOACTIVE  : left-button press — two zones:
 *                  - thumbnail area (relY < gh - SCROLLER_H):
 *                      determine clicked screen, signal host via GADGETUP.
 *                  - scroller area  (relY >= gh - SCROLLER_H):
 *                      begin drag, return GMR_MEACTIVE to keep input.
 * GM_HANDLEINPUT: while scroller drag is active, map mouse X delta to
 *                 scrollOffset and trigger GM_RENDER; button-up ends drag.
 */

#include <proto/intuition.h>
#include <proto/graphics.h>
#include <devices/inputevent.h>
#include <intuition/gadgetclass.h>
#include <clib/alib_protos.h>
#include "screen_carousel_private.h"

/* ------------------------------------------------------------------ */
/* Static helper: trigger GM_RENDER on ourselves (same as PetsciiCanvas)*/
/* ------------------------------------------------------------------ */

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
/* Static helper: compute scroller bar width (same formula as render)  */
/* ------------------------------------------------------------------ */

static WORD scrollerBarW(WORD gw, WORD totalW)
{
    WORD barW = (WORD)((LONG)gw * (LONG)gw / (LONG)totalW);
    if (barW < 4)  barW = 4;
    if (barW > gw) barW = gw;
    return barW;
}

/* ------------------------------------------------------------------ */
/* GM_HITTEST                                                           */
/* ------------------------------------------------------------------ */

ULONG ScreenCarousel_OnHitTest(Class *cl, Object *o, struct gpHitTest *msg)
{
    (void)cl; (void)o; (void)msg;
    return GMR_GADGETHIT;
}

/* ------------------------------------------------------------------ */
/* GM_GOACTIVE                                                          */
/* ------------------------------------------------------------------ */

ULONG ScreenCarousel_OnGoActive(Class *cl, Object *o, struct gpInput *msg)
{
    ScreenCarouselData *inst = (ScreenCarouselData *)INST_DATA(cl, o);
    struct Gadget      *g    = G(o);
    struct InputEvent  *ie   = msg->gpi_IEvent;
    WORD                relX = (WORD)(msg->gpi_Mouse.X);
    WORD                relY = (WORD)(msg->gpi_Mouse.Y);
    WORD                gw   = g->Width;
    WORD                gh   = g->Height;

    (void)cl;

    /* Only act on left mouse button presses */
    if (!ie || (ie->ie_Code & ~IECODE_UP_PREFIX) != IECODE_LBUTTON)
        return GMR_NOREUSE;

    /* --- Scroller zone: bottom SCROLLER_GAP + SCROLLER_H rows --- */
    if (relY >= gh - (WORD)SCROLLER_H - (WORD)SCROLLER_GAP)
    {
        inst->scrollDragActive      = 1;
        inst->scrollDragStartX      = relX;
        inst->scrollDragStartOffset = inst->scrollOffset;
        /* Stay active so GM_HANDLEINPUT receives further events */
        return GMR_MEACTIVE;
    }

    /* --- Thumbnail zone --- */
    inst->scrollDragActive = 0;
    {
        LONG clickedIdx = ScreenCarousel_XToIndex(inst, relX);
        if (clickedIdx < 0)
            return GMR_NOREUSE;

        inst->signalScreen = (ULONG)clickedIdx;
        /* GMR_VERIFY causes Intuition to send IDCMP_GADGETUP to the window */
        return GMR_NOREUSE | GMR_VERIFY;
    }
}

/* ------------------------------------------------------------------ */
/* GM_HANDLEINPUT                                                       */
/* ------------------------------------------------------------------ */

ULONG ScreenCarousel_OnInput(Class *cl, Object *o, struct gpInput *msg)
{
    ScreenCarouselData *inst = (ScreenCarouselData *)INST_DATA(cl, o);
    struct Gadget      *g    = G(o);
    struct InputEvent  *ie   = msg->gpi_IEvent;
    WORD                relX = (WORD)(msg->gpi_Mouse.X);
    WORD                gw   = g->Width;
    WORD                totalW;
    WORD                maxOffset;
    WORD                barW;
    WORD                scrollRange;
    WORD                mouseDelta;
    WORD                newOffset;

    if (!ie) return GMR_MEACTIVE;

    /* Button released: end drag */
    if (ie->ie_Code == (IECODE_LBUTTON | IECODE_UP_PREFIX))
    {
        inst->scrollDragActive = 0;
        return GMR_NOREUSE;
    }

    if (!inst->scrollDragActive) return GMR_MEACTIVE;

    /* Compute scroll geometry */
    totalW    = (WORD)((ULONG)inst->miniCount * (ULONG)ITEM_W);
    maxOffset = (totalW > gw) ? (WORD)(totalW - gw) : 0;

    if (maxOffset > 0)
    {
        barW        = scrollerBarW(gw, totalW);
        scrollRange = (WORD)(gw - barW);
        mouseDelta  = (WORD)(relX - inst->scrollDragStartX);

        if (scrollRange > 0)
            newOffset = (WORD)((LONG)inst->scrollDragStartOffset
                               + (LONG)mouseDelta * (LONG)maxOffset
                               / (LONG)scrollRange);
        else
            newOffset = inst->scrollDragStartOffset;

        if (newOffset < 0)         newOffset = 0;
        if (newOffset > maxOffset) newOffset = maxOffset;

        if (newOffset != inst->scrollOffset)
        {
            inst->scrollOffset = newOffset;
            renderSelf(cl, o, msg->gpi_GInfo);
        }
    }

    return GMR_MEACTIVE;
}
