/*
 * LayoutWithPopup - custom BOOPSI layout class inheriting from layout.gadget.
 * EXPERIMENTAL
 * Supports one popup child that can be shown/hidden at arbitrary coordinates.
 * The popup is registered in the superclass child list with zero-height
 * constraints so normal layout flow allocates it no space.  GM_LAYOUT then
 * overrides the popup's Gadget fields directly.
 */

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/graphics.h>
#include <intuition/gadgetclass.h>
#include <gadgets/layout.h>
#include <proto/layout.h>

#include "compilers.h"
#include "class_layoutwithpopup.h"
#include "bdbprintf.h"

/* Convenience cast: Object * -> struct Gadget * */
#ifndef G
#define G(o) ((struct Gadget *)(o))
#endif

/* ------------------------------------------------------------------ */
/* Per-instance data                                                    */
/* ------------------------------------------------------------------ */

typedef struct LayoutWithPopupData {
    Object *children[LayoutWithPopup_MAXCHILDREN]; /* regular children (not popup) */
    UWORD   childCount;

    Object *popup;          /* popup child (also in super's list, 0-height)  */
    int     popup_visible;  /* TRUE = popup is currently shown               */
    WORD    popup_x;        /* popup left offset relative to layout LeftEdge */
    WORD    popup_y;        /* popup top  offset relative to layout TopEdge  */
} LayoutWithPopupData;

/* Forward declaration of dispatcher */
ULONG ASM SAVEDS LayoutWithPopup_Dispatch(
    REG(a0, Class  *cl),
    REG(a2, Object *o),
    REG(a1, Msg     msg));

/* ------------------------------------------------------------------ */
/* Popup show/hide helpers                                             */
/* ------------------------------------------------------------------ */

static void refresh(Object *o, LayoutWithPopupData *inst,
                      struct GadgetInfo *ginfo)
{
    struct gpLayout layoutMsg;
    struct gpRender renderMsg;
    struct RastPort *rp;
    UWORD i;
    /* Recursively lay out popup's own children */
    if(inst->popup)
    {
        if (inst->popup_visible) {
            G(inst->popup)->LeftEdge = G(o)->LeftEdge + inst->popup_x;
            G(inst->popup)->TopEdge  = G(o)->TopEdge  + inst->popup_y;
            G(inst->popup)->Width    = 128+16;
            G(inst->popup)->Height   = 64;

        } else {
            G(inst->popup)->LeftEdge = 16384;
            G(inst->popup)->TopEdge  = 16384;
            G(inst->popup)->Width    = 0;
            G(inst->popup)->Height   = 0;
        }

        layoutMsg.MethodID    = GM_LAYOUT;
        layoutMsg.gpl_GInfo   = ginfo;
        layoutMsg.gpl_Initial = FALSE;
        DoMethodA(inst->popup, (Msg)&layoutMsg);
    }

   //  inst->children[inst->childCount++]

    /* Render all */
    // if (ginfo) {
    //     rp = ObtainGIRPort(ginfo);
    //     if (rp) {
    //        renderMsg.MethodID   = GM_RENDER;
    //         renderMsg.gpr_GInfo  = ginfo;
    //         renderMsg.gpr_RPort  = rp;
    //         renderMsg.gpr_Redraw = GREDRAW_REDRAW;
    //         for( i=0 ; i<inst->childCount ; i++ )
    //         {
    //             if(inst->children[i])
    //              DoMethodA(inst->children[i], (Msg)&renderMsg);
    //         }
    //         ReleaseGIRPort(rp);
    //     }
    // }
    if (ginfo) {
        rp = ObtainGIRPort(ginfo);
        if (rp) {
            renderMsg.MethodID   = GM_RENDER;
            renderMsg.gpr_GInfo  = ginfo;
            renderMsg.gpr_RPort  = rp;
            renderMsg.gpr_Redraw = GREDRAW_REDRAW;
            DoMethodA(o, (Msg)&renderMsg);
            ReleaseGIRPort(rp);
        }
    }

}

static void popupShow(Object *o, LayoutWithPopupData *inst,
                      struct GadgetInfo *ginfo)
{
    inst->popup_visible = TRUE;
    refresh(o,inst,ginfo);
}

static void popupHide(Object *o, LayoutWithPopupData *inst,
                      struct GadgetInfo *ginfo)
{
    inst->popup_visible = FALSE;
    refresh(o,inst,ginfo);
}

/* ------------------------------------------------------------------ */
/* OM_NEW                                                               */
/* ------------------------------------------------------------------ */

static ULONG LayoutWithPopup_OnNew(Class *cl, Object *o, struct opSet *msg)
{
    LayoutWithPopupData *inst;
    Object              *newObj;
    Object              *popupGad;
    struct TagItem      *state;
    struct TagItem      *tag;
    struct TagItem       addPopupTags[5];
    struct opSet         addMsg;

    /* Pre-scan tag list: find the popup gadget (not forwarded to super) */
    popupGad = NULL;
    state = msg->ops_AttrList;
    while ((tag = NextTagItem(&state)) != NULL) {
        if (tag->ti_Tag == LAYOUTWP_POPUPGADGET)
            popupGad = (Object *)tag->ti_Data;
    }

    /* Let layout.gadget create the object and process LAYOUT_AddChild tags */
    newObj = (Object *)DoSuperMethodA(cl, o, (APTR)msg);
    if (!newObj) return 0;

    inst = (LayoutWithPopupData *)INST_DATA(cl, newObj);
    inst->childCount    = 0;
    inst->popup         = popupGad;
    inst->popup_visible = FALSE;
    inst->popup_x       = 0;
    inst->popup_y       = 0;

    /* Collect regular children (excluding popup) for our own records */
    state = msg->ops_AttrList;
    while ((tag = NextTagItem(&state)) != NULL) {
        if (tag->ti_Tag == LAYOUT_AddChild &&
            (Object *)tag->ti_Data != popupGad &&
            inst->childCount < LayoutWithPopup_MAXCHILDREN)
        {
            inst->children[inst->childCount++] = (Object *)tag->ti_Data;
        }
    }

    return (ULONG)newObj;
}

/* ------------------------------------------------------------------ */
/* OM_DISPOSE                                                           */
/* ------------------------------------------------------------------ */

static ULONG LayoutWithPopup_OnDispose(Class *cl, Object *o, Msg msg)
{
    /* All children (including popup) belong to layout.gadget and are
     * disposed by the superclass chain.  Nothing extra to free.       */
    return DoSuperMethodA(cl, o, (APTR)msg);
}

/* ------------------------------------------------------------------ */
/* GM_LAYOUT                                                            */
/* ------------------------------------------------------------------ */

static ULONG LayoutWithPopup_OnLayout(Class *cl, Object *o,
                                      struct gpLayout *msg)
{
    LayoutWithPopupData *inst;
    ULONG                result;

    inst = (LayoutWithPopupData *)INST_DATA(cl, o);

    /* Normal vertical layout via super (popup gets 0 height from constraints) */
    result = DoSuperMethodA(cl, o, (APTR)msg);

    /* Override popup position after super has positioned everything else */
    if (inst->popup) {
        if (inst->popup_visible) {
            G(inst->popup)->LeftEdge = G(o)->LeftEdge + inst->popup_x;
            G(inst->popup)->TopEdge  = G(o)->TopEdge  + inst->popup_y;
            G(inst->popup)->Width    = 128+16;
            G(inst->popup)->Height   = 64;

        } else {
            G(inst->popup)->LeftEdge = 16384;
            G(inst->popup)->TopEdge  = 16384;
            G(inst->popup)->Width    = 0;
            G(inst->popup)->Height   = 0;
        }
        /* Recursively lay out popup internals */
        DoMethodA(inst->popup, (Msg)msg);
    }
  
    return result;
}

/* ------------------------------------------------------------------ */
/* OM_SET / OM_UPDATE                                                   */
/* ------------------------------------------------------------------ */

static ULONG LayoutWithPopup_OnSet(Class *cl, Object *o, struct opSet *msg)
{
    LayoutWithPopupData *inst;
    struct TagItem      *state;
    struct TagItem      *tag;

    inst  = (LayoutWithPopupData *)INST_DATA(cl, o);
    state = msg->ops_AttrList;

    while ((tag = NextTagItem(&state)) != NULL) {
        switch (tag->ti_Tag) {
            case LAYOUTWP_POPUPGADGET:
                inst->popup = (Object *)tag->ti_Data;
                break;

            case LAYOUTWP_POPUPX:
                inst->popup_x = (WORD)tag->ti_Data;
                break;

            case LAYOUTWP_POPUPY:
                inst->popup_y = (WORD)tag->ti_Data;
                break;

            case LAYOUTWP_POPUPVISIBLE:
            //bdbprintf("Set visible %08x %08x %d\n",(int)inst->popup,(int) msg->ops_GInfo,tag->ti_Data);
                if(inst->popup_visible && ((int)tag->ti_Data != 0))
                {
                    /* may reopen elsewhere,
                     * if was already visible, need repair */
                    popupHide(o, inst, msg->ops_GInfo);
                }
                inst->popup_visible = (int)tag->ti_Data;
                if (inst->popup && msg->ops_GInfo) {
                    if (inst->popup_visible)
                        popupShow(o, inst, msg->ops_GInfo);
                    else
                        popupHide(o, inst, msg->ops_GInfo);
                }
                break;

            default:
                break;
        }
    }

    return DoSuperMethodA(cl, o, (APTR)msg);
}

/* ------------------------------------------------------------------ */
/* OM_GET                                                               */
/* ------------------------------------------------------------------ */

static ULONG LayoutWithPopup_OnGet(Class *cl, Object *o, struct opGet *msg)
{
    LayoutWithPopupData *inst;
    inst = (LayoutWithPopupData *)INST_DATA(cl, o);

    switch (msg->opg_AttrID) {
        case LAYOUTWP_POPUPGADGET:
            *msg->opg_Storage = (ULONG)inst->popup;
            return TRUE;

        case LAYOUTWP_POPUPX:
            *msg->opg_Storage = (ULONG)(LONG)inst->popup_x;
            return TRUE;

        case LAYOUTWP_POPUPY:
            *msg->opg_Storage = (ULONG)(LONG)inst->popup_y;
            return TRUE;

        case LAYOUTWP_POPUPVISIBLE:
            *msg->opg_Storage = (ULONG)inst->popup_visible;
            return TRUE;

        default:
            return DoSuperMethodA(cl, o, (APTR)msg);
    }
}

/* ------------------------------------------------------------------ */
/* Dispatcher                                                           */
/* ------------------------------------------------------------------ */

ULONG ASM SAVEDS LayoutWithPopup_Dispatch(
    REG(a0, Class  *cl),
    REG(a2, Object *o),
    REG(a1, Msg     msg))
{
    switch (msg->MethodID) {
        case OM_NEW:
            return LayoutWithPopup_OnNew(cl, o, (struct opSet *)msg);

        case OM_DISPOSE:
            return LayoutWithPopup_OnDispose(cl, o, msg);

        case GM_LAYOUT:
            return LayoutWithPopup_OnLayout(cl, o, (struct gpLayout *)msg);

        case OM_SET:
        case OM_UPDATE:
            return LayoutWithPopup_OnSet(cl, o, (struct opSet *)msg);

        case OM_GET:
            return LayoutWithPopup_OnGet(cl, o, (struct opGet *)msg);

        default:
            return DoSuperMethodA(cl, o, (APTR)msg);
    }
}

/* ------------------------------------------------------------------ */
/* Class init / exit                                                    */
/* ------------------------------------------------------------------ */

Class *LayoutWithPopupClass = NULL;

int LayoutWithPopup_Init(void)
{
    LayoutWithPopupClass = MakeClass(
        NULL,              /* private class — no public name  */
        NULL,              /* superclass name (use pointer)   */
        LAYOUT_GetClass(), /* superclass by pointer           */
        sizeof(LayoutWithPopupData),
        0                  /* flags                           */
    );

    if (!LayoutWithPopupClass) return 0;

    LayoutWithPopupClass->cl_Dispatcher.h_Entry    = (HOOKFUNC)LayoutWithPopup_Dispatch;
    LayoutWithPopupClass->cl_Dispatcher.h_SubEntry = NULL;
    LayoutWithPopupClass->cl_Dispatcher.h_Data     = NULL;

    return 1;
}

void LayoutWithPopup_Exit(void)
{
    if (LayoutWithPopupClass) {
        FreeClass(LayoutWithPopupClass);
        LayoutWithPopupClass = NULL;
    }
}
