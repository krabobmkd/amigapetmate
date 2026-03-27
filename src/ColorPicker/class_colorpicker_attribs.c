/*
 * ColorPicker - BOOPSI gadget class.
 * Attribute handlers: OM_NEW, OM_DISPOSE, OM_SET, OM_GET.
 */

#include <proto/intuition.h>
#include <proto/utility.h>
#include "color_picker_private.h"

/* keypad to color palette */
static UBYTE colorsAgainsNumPadsKeys[16]={
0x5a,0x5b,0x5c,0x5d,
0x3d,0x3e,0x3f,0x4a,
0x2d,0x2e,0x2f,0x5e,
0x1d,0x1e,0x1f,0x43
};
ULONG ColorPicker_NotifyAttribChange(Class *cl,Object *Gad, struct GadgetInfo *GInfo);
/* ------------------------------------------------------------------ */
/* OM_NEW                                                               */
/* ------------------------------------------------------------------ */

ULONG ColorPicker_OnNew(Class *cl, Object *o, struct opSet *msg)
{
    ColorPickerData *inst;
    PetsciiStyle    *style         = NULL;
    UBYTE            selectedColor = 0;
    Object          *newObj;
    struct TagItem  *tag;

    tag = FindTagItem(CPA_Style, msg->ops_AttrList);
    if (tag) style = (PetsciiStyle *)tag->ti_Data;

    tag = FindTagItem(CPA_SelectedColor, msg->ops_AttrList);
    if (tag) selectedColor = (UBYTE)tag->ti_Data;

    if (!style) return 0;

    newObj = (Object *)DoSuperMethodA(cl, o, (APTR)msg);
    if (!newObj) return 0;

    inst                = (ColorPickerData *)INST_DATA(cl, newObj);
    inst->style         = style;
    inst->selectedColor = selectedColor;
    inst->ColorsPerWidth = 8;

    return (ULONG)newObj;
}

/* ------------------------------------------------------------------ */
/* OM_DISPOSE                                                           */
/* ------------------------------------------------------------------ */

ULONG ColorPicker_OnDispose(Class *cl, Object *o, Msg msg)
{
    return DoSuperMethodA(cl, o, (APTR)msg);
}

/* ------------------------------------------------------------------ */
/* OM_SET / OM_UPDATE                                                   */
/* ------------------------------------------------------------------ */

ULONG ColorPicker_OnSet(Class *cl, Object *o, struct opSet *msg)
{
    ColorPickerData *inst;
    struct TagItem  *state;
    struct TagItem  *tag;
    ULONG            result;
    int redraw = 0;
    int notif=0;
    inst   = (ColorPickerData *)INST_DATA(cl, o);
    result = DoSuperMethodA(cl, o, (APTR)msg);

    state = msg->ops_AttrList;
    while ((tag = NextTagItem(&state)) != NULL) {
        switch (tag->ti_Tag) {
            case CPA_Style:
                inst->style = (PetsciiStyle *)tag->ti_Data;
                result = 1;
                break;
            case CPA_SelectedColor:
                inst->selectedColor = (UBYTE)tag->ti_Data;
                result = 1;
                break;
            case CPA_ColorRole:
                inst->ColorRole = (UWORD)tag->ti_Data;
                result = 1;
            break;
            case CPA_NumPadKeys:
            {
                ULONG c = tag->ti_Data,i;
                for(i=0;i<16;i++)
                if(c == colorsAgainsNumPadsKeys[i])
                {
                    if((UBYTE)i != inst->selectedColor)
                    {
                        inst->selectedColor = i;
                        redraw = 1;
                        notif = 1;
                        result = 1;
                    }
                    break;
                }
            }
            break;
            default:
                break;
        }
    }
    if(redraw && msg->ops_GInfo)
    {
        struct RastPort *rp = ObtainGIRPort(msg->ops_GInfo);
        if (rp)
        {
            struct gpRender  renderMsg;
            renderMsg.MethodID   = GM_RENDER;
            renderMsg.gpr_GInfo  = msg->ops_GInfo;
            renderMsg.gpr_RPort  = rp;
            renderMsg.gpr_Redraw = GREDRAW_UPDATE;

            DoMethodA(o, (Msg)&renderMsg);

            ReleaseGIRPort(rp);
        }
    }
    if(notif)
    {
        ColorPicker_NotifyAttribChange(cl,o,msg->ops_GInfo);
    }
    return result;
}

/* ------------------------------------------------------------------ */
/* OM_GET                                                               */
/* ------------------------------------------------------------------ */

ULONG ColorPicker_OnGet(Class *cl, Object *o, struct opGet *msg)
{
    ColorPickerData *inst = (ColorPickerData *)INST_DATA(cl, o);

    switch (msg->opg_AttrID) {
        case CPA_SelectedColor:
            *msg->opg_Storage = (ULONG)inst->selectedColor;
            return TRUE;
          case CPA_ColorRole:
               *msg->opg_Storage = (ULONG)inst->ColorRole;
            break;
        default:
            return DoSuperMethodA(cl, o, (APTR)msg);
    }
}
