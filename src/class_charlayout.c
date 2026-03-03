/*
 * CharLayout - custom BOOPSI gadget class inheriting from layout.gadget.
 *
 * Overrides OM_NEW, OM_DISPOSE, and GM_LAYOUT.
 * All other methods chain directly to layout.gadget via DoSuperMethodA.
 *
 * Height assignment per child GA_ID in GM_LAYOUT:
 *   GAD_CHARSELECTOR   : height = available_width       (16x16 grid stays square)
 *   GAD_COLORPICKER_FG : height = available_width / 4   (8x2 grid, cells square)
 *   GAD_COLORPICKER_BG : height = available_width / 4   (same)
 *   any other child    : nominal height from GM_DOMAIN(GDOMAIN_NOMINAL)
 *
 * Children are stacked top-to-bottom with full layout width.
 * Remaining space below the last child is left empty.
 */

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <intuition/gadgetclass.h>
#include <gadgets/layout.h>
#include <proto/layout.h>

#include "compilers.h"
#include "class_charlayout.h"
#include "gadgetid.h"

/* Convenience cast: Object * -> struct Gadget * */
#ifndef G
#define G(o) ((struct Gadget *)(o))
#endif

/* ------------------------------------------------------------------ */
/* Per-instance data                                                    */
/* ------------------------------------------------------------------ */

typedef struct CharLayoutData {
    Object *children[CHARLAYOUT_MAXCHILDREN]; /* child object pointers  */
    UWORD   childCount;                        /* number of children     */
} CharLayoutData;

/* Forward declaration of dispatcher */
ULONG ASM SAVEDS CharLayout_Dispatch(
    REG(a0, Class  *cl),
    REG(a2, Object *o),
    REG(a1, Msg     msg));

/* ------------------------------------------------------------------ */
/* OM_NEW                                                               */
/* ------------------------------------------------------------------ */

static ULONG CharLayout_OnNew(Class *cl, Object *o, struct opSet *msg)
{
    CharLayoutData *inst;
    Object         *newObj;
    struct TagItem *state;
    struct TagItem *tag;

    /* Let layout.gadget create the object, add children to its list,
     * and initialise all its internal state.                          */
    newObj = (Object *)DoSuperMethodA(cl, o, (APTR)msg);
    if (!newObj) return 0;

    inst = (CharLayoutData *)INST_DATA(cl, newObj);
    inst->childCount = 0;

    /* Scan the same taglist to collect the child object pointers we
     * need in GM_LAYOUT for GA_ID lookups and recursive layout calls. */
    state = msg->ops_AttrList;
    while ((tag = NextTagItem(&state)) != NULL) {
        if (tag->ti_Tag == LAYOUT_AddChild) {
            if (inst->childCount < CHARLAYOUT_MAXCHILDREN) {
                inst->children[inst->childCount] = (Object *)tag->ti_Data;
                inst->childCount++;
            }
        }
    }

    return (ULONG)newObj;
}

/* ------------------------------------------------------------------ */
/* OM_DISPOSE                                                           */
/* ------------------------------------------------------------------ */

static ULONG CharLayout_OnDispose(Class *cl, Object *o, Msg msg)
{
    /* Our instance data holds only non-owned pointers (children belong
     * to layout.gadget which disposes them).  Nothing extra to free.  */
    return DoSuperMethodA(cl, o, (APTR)msg);
}

/* ------------------------------------------------------------------ */
/* GM_LAYOUT                                                            */
/* ------------------------------------------------------------------ */

static ULONG CharLayout_OnLayout(Class *cl, Object *o, struct gpLayout *msg)
{
    CharLayoutData *inst;
    // WORD            left;
    // WORD            top;
    WORD            availW;
    WORD            decaly=0;
    UWORD           i;
//    UWORD           h;

    inst   = (CharLayoutData *)INST_DATA(cl, o);
    // left   = G(o)->LeftEdge;
    // top    = G(o)->TopEdge;
    availW = G(o)->Width;
  //  y      = top;

    /* experimental: do a normal layout */

    DoSuperMethodA(cl, o, (APTR)msg);

    /* but then we just need to shrunk the charSelector up&down marges
        This is basically the whole point of this class
    */
    for (i = 0; i < inst->childCount; i++) {
        Object        *child = inst->children[i];
        struct Gadget *gad = (struct Gadget *)child;
        ULONG          gadID = 0;

        GetAttr(GA_ID, child, &gadID);

        if ((ULONG)gadID == (ULONG)GAD_CHARSELECTOR) {
            if(gad->Height>gad->Width)
            {
                decaly = gad->Height-gad->Width;
                gad->Height = gad->Width;
                DoMethodA((Object *)child, (Msg)msg);
            }
        } else
        if(decaly>0)
        {
            gad->TopEdge -= decaly;
            DoMethodA((Object *)child, (Msg)msg);
        }
    }
    // /*
    //     trick with 2 loops
    // */

    // /* 1rst loop set values,compute ideal value but can go too far down */
    // for (i = 0; i < inst->childCount; i++) {
    //     Object        *child = inst->children[i];
    //     ULONG          gadID = 0;
    //     WORD           childH;
    //     struct Gadget *gad;

    //     GetAttr(GA_ID, child, &gadID);

    //     if ((ULONG)gadID == (ULONG)GAD_CHARSELECTOR) {
    //         /* Square: 16x16 char grid */
    //         childH = availW;

    //     } else if ((ULONG)gadID == (ULONG)GAD_COLORPICKER_FG ||
    //                (ULONG)gadID == (ULONG)GAD_COLORPICKER_BG) {
    //         /* 8x2 color grid: width / 4 makes each cell square */
    //         childH = (availW > 0) ? (WORD)(availW / 4) : 16;

    //     } else {
    //         /* Ask the child for its nominal (preferred) height */
    //         struct gpDomain domMsg;
    //         domMsg.MethodID          = GM_DOMAIN;
    //         domMsg.gpd_GInfo         = msg->gpl_GInfo;
    //         domMsg.gpd_RPort         = msg->gpl_GInfo
    //                                    ? msg->gpl_GInfo->gi_RastPort
    //                                    : NULL;
    //         domMsg.gpd_Which         = GDOMAIN_NOMINAL;
    //         domMsg.gpd_Domain.Left   = 0;
    //         domMsg.gpd_Domain.Top    = 0;
    //         domMsg.gpd_Domain.Width  = availW;
    //         domMsg.gpd_Domain.Height = 0;
    //         DoMethodA(child, (Msg)(APTR)&domMsg);
    //         childH = (domMsg.gpd_Domain.Height > 0)
    //                  ? (WORD)domMsg.gpd_Domain.Height : 20;
    //     }

    //     if (childH < 1) childH = 1;

    //     /* Set position and size directly in the struct Gadget fields */
    //     gad           = (struct Gadget *)child;
    //     gad->LeftEdge = left;
    //     gad->TopEdge  = y;
    //     gad->Width    = availW;
    //     gad->Height   = childH;

    //     y += childH;
    // }
    // h = y-top;
    // /* */
    // if(h>G(o)->Height)
    // {
    //     UWORD hm = h-G(o)->Height;
    //     for (i = 0; i < inst->childCount; i++) {
    //         Object        *child = inst->children[i];
    //         ULONG          gadID = 0;
    //         WORD           childH;
    //         struct Gadget *gad;

    //         GetAttr(GA_ID, child, &gadID);

    //         if ((ULONG)gadID == (ULONG)GAD_CHARSELECTOR) {
    //             /* Square: 16x16 char grid */
    //             childH = availW;

    //         } else if ((ULONG)gadID == (ULONG)GAD_COLORPICKER_FG ||
    //                    (ULONG)gadID == (ULONG)GAD_COLORPICKER_BG) {
    //             /* 8x2 color grid: width / 4 makes each cell square */
    //             childH = (availW > 0) ? (WORD)(availW / 4) : 16;

    //         } else {
    //             /* Ask the child for its nominal (preferred) height */
    //             struct gpDomain domMsg;
    //             domMsg.MethodID          = GM_DOMAIN;
    //             domMsg.gpd_GInfo         = msg->gpl_GInfo;
    //             domMsg.gpd_RPort         = msg->gpl_GInfo
    //                                        ? msg->gpl_GInfo->gi_RastPort
    //                                        : NULL;
    //             domMsg.gpd_Which         = GDOMAIN_NOMINAL;
    //             domMsg.gpd_Domain.Left   = 0;
    //             domMsg.gpd_Domain.Top    = 0;
    //             domMsg.gpd_Domain.Width  = availW;
    //             domMsg.gpd_Domain.Height = 0;
    //             DoMethodA(child, (Msg)(APTR)&domMsg);
    //             childH = (domMsg.gpd_Domain.Height > 0)
    //                      ? (WORD)domMsg.gpd_Domain.Height : 20;
    //         }

    //         if (childH < 1) childH = 1;

    //         /* Set position and size directly in the struct Gadget fields */
    //         gad           = (struct Gadget *)child;
    //         gad->LeftEdge = left;
    //         gad->TopEdge  = y;
    //         gad->Width    = availW;
    //         gad->Height   = childH;

    //         y += childH;
    //     }

    // }


    // for (i = 0; i < inst->childCount; i++) {
    //     Object        *child = inst->children[i];

    //     /* Recursively lay out the child, reusing the incoming message */
    //     DoMethodA((Object *)child, (Msg)msg);
    // }

    return 0;
}

/* ------------------------------------------------------------------ */
/* Dispatcher                                                           */
/* ------------------------------------------------------------------ */

ULONG ASM SAVEDS CharLayout_Dispatch(
    REG(a0, Class  *cl),
    REG(a2, Object *o),
    REG(a1, Msg     msg))
{
    switch (msg->MethodID) {
        case OM_NEW:
            return CharLayout_OnNew(cl, o, (struct opSet *)msg);

        case OM_DISPOSE:
            return CharLayout_OnDispose(cl, o, msg);

        case GM_LAYOUT:
            return CharLayout_OnLayout(cl, o, (struct gpLayout *)msg);

        default:
            return DoSuperMethodA(cl, o, (APTR)msg);
    }
}

/* ------------------------------------------------------------------ */
/* Class init / exit                                                    */
/* ------------------------------------------------------------------ */

Class *CharLayoutClass = NULL;

int CharLayout_Init(void)
{
    CharLayoutClass = MakeClass(
        NULL,             /* private class — no public name   */
       NULL,  /* superclass name                   */
        LAYOUT_GetClass(),             /* superclass by pointer (not used)  */
        sizeof(CharLayoutData),
        0                 /* flags                             */
    );

    if (!CharLayoutClass) return 0;

    CharLayoutClass->cl_Dispatcher.h_Entry    = (HOOKFUNC)CharLayout_Dispatch;
    CharLayoutClass->cl_Dispatcher.h_SubEntry = NULL;
    CharLayoutClass->cl_Dispatcher.h_Data     = NULL;

    return 1;
}

void CharLayout_Exit(void)
{
    if (CharLayoutClass) {
        FreeClass(CharLayoutClass);
        CharLayoutClass = NULL;
    }
}
