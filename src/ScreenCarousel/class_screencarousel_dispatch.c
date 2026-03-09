/*
 * ScreenCarousel - BOOPSI gadget class for the screen thumbnail strip.
 * Main method dispatcher.
 */

#include <proto/intuition.h>
#include <intuition/gadgetclass.h>
#include "screen_carousel_private.h"

ULONG ASM SAVEDS ScreenCarousel_Dispatch(
    REG(a0, Class  *cl),
    REG(a2, Object *o),
    REG(a1, Msg     msg))
{

    switch (msg->MethodID)
    {
        case OM_NEW:
            return ScreenCarousel_OnNew(cl, o, (struct opSet *)msg);

        case OM_DISPOSE:
            return ScreenCarousel_OnDispose(cl, o, msg);

        case OM_SET:
        case OM_UPDATE:
        {
            ULONG result = DoSuperMethodA(cl, o, (APTR)msg);
            return result | ScreenCarousel_OnSet(cl, o, (struct opSet *)msg);
        }

        case OM_GET:
            return ScreenCarousel_OnGet(cl, o, (struct opGet *)msg);

        case GM_RENDER:
            return ScreenCarousel_OnRender(cl, o, (struct gpRender *)msg);
        case GM_LAYOUT:
            return ScreenCarousel_OnLayout(cl, o, (struct gpLayout *)msg);
        case GM_HITTEST:
            return ScreenCarousel_OnHitTest(cl, o, (struct gpHitTest *)msg);

        case GM_GOACTIVE:
            return ScreenCarousel_OnGoActive(cl, o, (struct gpInput *)msg);

        default:
            return DoSuperMethodA(cl, o, (APTR)msg);
    }
}
