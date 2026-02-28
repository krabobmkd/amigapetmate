/*
 * CharSelector - BOOPSI gadget class dispatcher.
 */

#include <proto/intuition.h>
#include <intuition/gadgetclass.h>
#include "char_selector_private.h"

ULONG ASM SAVEDS CharSelector_Dispatch(
    REG(a0, Class  *cl),
    REG(a2, Object *o),
    REG(a1, Msg     msg))
{
    switch (msg->MethodID)
    {
        case OM_NEW:          return CharSelector_OnNew(cl, o, (struct opSet *)msg);
        case OM_DISPOSE:      return CharSelector_OnDispose(cl, o, msg);
        case OM_SET:
        case OM_UPDATE:
        {
            int result = DoSuperMethodA(cl, o, (APTR)msg);
            return result | CharSelector_OnSet(cl, o, (struct opSet *)msg);
        }
        case OM_GET:          return CharSelector_OnGet(cl, o, (struct opGet *)msg);

        case GM_DOMAIN:       return CharSelector_OnDomain(cl, o, (struct gpDomain *)msg);
        case GM_RENDER:       return CharSelector_OnRender(cl, o, (struct gpRender *)msg);
        case GM_LAYOUT:       return CharSelector_OnLayout(cl, o, (struct gpLayout *)msg);
        case GM_HITTEST:      return CharSelector_OnHitTest(cl, o, (struct gpHitTest *)msg);
        case GM_GOACTIVE:     return CharSelector_OnGoActive(cl, o, (struct gpInput *)msg);
        case GM_HANDLEINPUT:  return CharSelector_OnHandleInput(cl, o, (struct gpInput *)msg);
        case GM_GOINACTIVE:   return CharSelector_OnGoInactive(cl, o, (struct gpGoInactive *)msg);
        default:              return DoSuperMethodA(cl, o, (APTR)msg);
    }
}
