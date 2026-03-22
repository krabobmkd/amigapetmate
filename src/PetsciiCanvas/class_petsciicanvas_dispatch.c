/*
 * PetsciiCanvas - BOOPSI gadget class for the PETSCII editing canvas.
 * Main method dispatcher.
 */

#include <proto/intuition.h>
#include <intuition/gadgetclass.h>
#include "petscii_canvas_private.h"

ULONG ASM SAVEDS PetsciiCanvas_Dispatch(
    REG(a0, Class  *cl),
    REG(a2, Object *o),
    REG(a1, Msg     msg))
{
    switch (msg->MethodID)
    {
        case OM_NEW:
            return PetsciiCanvas_OnNew(cl, o, (struct opSet *)msg);

        case OM_DISPOSE:
            return PetsciiCanvas_OnDispose(cl, o, msg);

        case OM_SET:    /* fall-through: same struct opSet */
        case OM_UPDATE:
        {
            /* Let gadgetclass handle GA_* and unknown tags first */
            int result = DoSuperMethodA(cl, o, (APTR)msg);
            return result | PetsciiCanvas_OnSet(cl, o, (struct opSet *)msg);
        }
        case OM_GET:
            return PetsciiCanvas_OnGet(cl, o, (struct opGet *)msg);

        case GM_DOMAIN:
            return PetsciiCanvas_OnDomain(cl, o, (struct gpDomain *)msg);

        case GM_RENDER:
            return PetsciiCanvas_OnRender(cl, o, (struct gpRender *)msg);

        case GM_LAYOUT:
            return PetsciiCanvas_OnLayout(cl, o, (struct gpLayout *)msg);

        case GM_HITTEST:
            return PetsciiCanvas_OnHitTest(cl, o, (struct gpHitTest *)msg);

        case GM_GOACTIVE:
            return PetsciiCanvas_OnGoActive(cl, o, (struct gpInput *)msg);

        case GM_HANDLEINPUT:
            return PetsciiCanvas_OnInput(cl, o, (struct gpInput *)msg);

        case GM_GOINACTIVE:
            return PetsciiCanvas_OnGoInactive(cl, o, (struct gpGoInactive *)msg);

        default:
            return DoSuperMethodA(cl, o, (APTR)msg);
    }
}
