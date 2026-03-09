/*
 * ScreenCarousel - BOOPSI gadget class for the screen thumbnail strip.
 * Input / hit-test handlers.
 *
 * GM_HITTEST  : report GMR_GADGETHIT if the pointer is inside the gadget.
 * GM_GOACTIVE : on left-button press, determine which thumbnail was clicked.
 *               Store the index in inst->signalScreen.
 *               Return GMR_NOREUSE | GMR_VERIFY so Intuition generates a
 *               IDCMP_GADGETUP event.  The host reads SCA_SignalScreen via
 *               GetAttr() after receiving that event.
 *
 * Scroll:
 *   Mouse-wheel (IECODE_UP_PREFIX / IECODE_DOWN_PREFIX in RAWMOUSE) is NOT
 *   handled here as it comes in via IDCMP_RAWKEY rather than gadget input.
 *   The host should call:
 *     SetAttrs(gadget, SCA_ScrollDelta, +/-ITEM_H, TAG_END);
 *   or directly manipulate scrollOffset via an attribute (see SCA_ScrollDelta
 *   in screen_carousel_private.h if added).  For now scrollOffset is set to 0
 *   and no in-gadget scroll is implemented; a scroll bar or external control
 *   can be added later.
 */

#include <proto/intuition.h>
#include <proto/graphics.h>
#include <devices/inputevent.h>
#include <intuition/gadgetclass.h>
#include "screen_carousel_private.h"

/* ------------------------------------------------------------------ */
/* GM_HITTEST                                                           */
/* ------------------------------------------------------------------ */

ULONG ScreenCarousel_OnHitTest(Class *cl, Object *o, struct gpHitTest *msg)
{
    /* The gadget occupies a rectangular area managed by gadgetclass.
     * gadgetclass has already verified that the pointer is within
     * LeftEdge/TopEdge/Width/Height, so any point that reaches us is a hit. */
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
    LONG                clickedIdx;
    WORD                relX;

    /* Only act on left mouse button presses */
    if (!ie || (ie->ie_Code & ~IECODE_UP_PREFIX) != IECODE_LBUTTON)
        return GMR_NOREUSE;

    /* Convert absolute mouse position to gadget-relative X */
    relX = (WORD)(msg->gpi_Mouse.X);  /* already gadget-relative in BOOPSI */
    (void)g;

    clickedIdx = ScreenCarousel_XToIndex(inst, relX);
    if (clickedIdx < 0)
        return GMR_NOREUSE;

    /* Record the clicked screen and request a GADGETUP notification */
    inst->signalScreen = (ULONG)clickedIdx;

    /* Return GMR_NOREUSE | GMR_VERIFY:
     *   GMR_VERIFY  -> Intuition sends IDCMP_GADGETUP to the window.
     *   GMR_NOREUSE -> release the input event (don't pass it further).  */
    return GMR_NOREUSE | GMR_VERIFY;
}
