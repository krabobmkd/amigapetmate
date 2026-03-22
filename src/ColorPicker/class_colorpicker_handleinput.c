/*
 * ColorPicker - input handling: GM_GOACTIVE, GM_HANDLEINPUT, GM_GOINACTIVE.
 *
 * A single click selects the colour under the mouse and immediately
 * returns GMR_VERIFY|GMR_NOREUSE from GM_GOACTIVE.  This causes Intuition
 * to post IDCMP_GADGETUP without ever entering drag mode.
 */

#include <proto/graphics.h>
#include <proto/intuition.h>
#include <clib/alib_protos.h>
#include "color_picker_private.h"

/*
    Generic function to send event to ICA_TARGET object for any attrib/value pair.
    Very generic
    Note we can define more long message with more long taglist and precise anything in that.
    we can just define our own messages with attribs enums or any sort of enum and values to precise parameters.

*/
static ULONG ColorPicker_NotifyAttribChange(Class *cl,Object *Gad, struct GadgetInfo *GInfo)
{
    ColorPickerData *inst;
    struct opUpdate notifymsg;
    ULONG messageTags[]={
     GA_ID,0,
     CPA_SelectedColor,0,
     CPA_ColorRole,0,
     TAG_DONE
    };

    inst = (ColorPickerData *)INST_DATA(cl, Gad);

    /* build the boopsi message taglist in table messageTags and link it to opUpdate struct  */

    GetAttr(GA_ID,Gad,&messageTags[1]);
    if(messageTags[1]==0) return 0; /* there must be a valid GA_ID */

    messageTags[3] = (ULONG)inst->selectedColor;
    messageTags[5] = (ULONG)inst->ColorRole;

    notifymsg.MethodID = OM_NOTIFY;
    notifymsg.opu_AttrList = (struct TagItem *)&messageTags[0];
    notifymsg.opu_GInfo = GInfo; /* always there for gadget, in all messages */
    notifymsg.opu_Flags = 0;

    /* will be received by object registered by ICA_TARGET attrib */
    return DoSuperMethodA(cl,(APTR)Gad,(Msg)&notifymsg );
}


/* ------------------------------------------------------------------ */
/* Static helpers                                                       */
/* ------------------------------------------------------------------ */

/*
 * Convert gadget-relative mouse coordinates to a colour index 0..15.
 * gpi_Mouse.X/Y are relative to the gadget's LeftEdge/TopEdge origin.
 */
static UBYTE mouseToColor(Object *o, WORD relX, WORD relY)
{
    WORD  w;
    WORD  h;
    UBYTE col;
    UBYTE row;

    w = G(o)->Width;
    h = G(o)->Height;
    if (w <= 0 || h <= 0) return 0;

    if (relX < 0)   relX = 0;
    if (relY < 0)   relY = 0;
    if (relX >= w)  relX = (WORD)(w - 1);
    if (relY >= h)  relY = (WORD)(h - 1);

    col = (UBYTE)((ULONG)relX * COLORPICKER_COLS / (ULONG)w);
    row = (UBYTE)((ULONG)relY * COLORPICKER_ROWS / (ULONG)h);

    if (col >= COLORPICKER_COLS) col = COLORPICKER_COLS - 1;
    if (row >= COLORPICKER_ROWS) row = COLORPICKER_ROWS - 1;

    return (UBYTE)(row * COLORPICKER_COLS + col);
}

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
/* GM_GOACTIVE: select colour and post GADGETUP immediately            */
/* ------------------------------------------------------------------ */

ULONG ColorPicker_OnGoActive(Class *cl, Object *o, struct gpInput *msg)
{
    ColorPickerData *inst;
    UBYTE            newColor;

    inst     = (ColorPickerData *)INST_DATA(cl, o);
    newColor = mouseToColor(o, msg->gpi_Mouse.X, msg->gpi_Mouse.Y);

    if (newColor != inst->selectedColor) {
        inst->selectedColor = newColor;
        renderSelf(cl, o, msg->gpi_GInfo);
        /* this is how you notify ICA_TARGET object from a gadget other than the limited gadgetup. */
        ColorPicker_NotifyAttribChange(cl,o,msg->gpi_GInfo);
    }

    *msg->gpi_Termination = 0;
    return GMR_VERIFY | GMR_NOREUSE;
}

/* ------------------------------------------------------------------ */
/* GM_HANDLEINPUT: should not normally be reached                      */
/* ------------------------------------------------------------------ */

ULONG ColorPicker_OnHandleInput(Class *cl, Object *o, struct gpInput *msg)
{
    (void)cl; (void)o;
    *msg->gpi_Termination = 0;
    return GMR_NOREUSE;
}

/* ------------------------------------------------------------------ */
/* GM_GOINACTIVE                                                        */
/* ------------------------------------------------------------------ */

ULONG ColorPicker_OnGoInactive(Class *cl, Object *o, struct gpGoInactive *msg)
{
    (void)cl; (void)o; (void)msg;
    return 0;
}
