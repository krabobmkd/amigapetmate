/*
 * ColorPicker - BOOPSI gadget class.
 * Attribute handlers: OM_NEW, OM_DISPOSE, OM_SET, OM_GET.
 */

#include <proto/intuition.h>
#include <proto/utility.h>
#include "color_picker_private.h"

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

    inst   = (ColorPickerData *)INST_DATA(cl, o);
    result = DoSuperMethodA(cl, o, (APTR)msg);

    state = msg->ops_AttrList;
    while ((tag = NextTagItem(&state)) != NULL) {
        switch (tag->ti_Tag) {
            case CPA_Style:
                inst->style = (PetsciiStyle *)tag->ti_Data;
                break;
            case CPA_SelectedColor:
                inst->selectedColor = (UBYTE)tag->ti_Data;
                break;
            case CPA_ColorRole:
                inst->ColorRole = (UWORD)tag->ti_Data;
            break;
            default:
                break;
        }
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
