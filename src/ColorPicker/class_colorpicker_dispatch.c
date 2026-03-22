/*
 * ColorPicker - BOOPSI gadget class dispatcher.
 */

#include <proto/intuition.h>
#include <intuition/gadgetclass.h>
#include "color_picker_private.h"

ULONG ASM SAVEDS ColorPicker_Dispatch(
    REG(a0, Class  *cl),
    REG(a2, Object *o),
    REG(a1, Msg     msg))
{
    switch (msg->MethodID)
    {
        case OM_NEW:         return ColorPicker_OnNew(cl, o, (struct opSet *)msg);
        case OM_DISPOSE:     return ColorPicker_OnDispose(cl, o, msg);
        case OM_SET:
        case OM_UPDATE:      return ColorPicker_OnSet(cl, o, (struct opSet *)msg);
        case OM_GET:         return ColorPicker_OnGet(cl, o, (struct opGet *)msg);
        case GM_RENDER:      return ColorPicker_OnRender(cl, o, (struct gpRender *)msg);
        case GM_HITTEST:     return ColorPicker_OnHitTest(cl, o, (struct gpHitTest *)msg);
        case GM_GOACTIVE:    return ColorPicker_OnGoActive(cl, o, (struct gpInput *)msg);
        case GM_HANDLEINPUT: return ColorPicker_OnHandleInput(cl, o, (struct gpInput *)msg);
        case GM_GOINACTIVE:  return ColorPicker_OnGoInactive(cl, o, (struct gpGoInactive *)msg);
        default:             return DoSuperMethodA(cl, o, (APTR)msg);
    }
}
