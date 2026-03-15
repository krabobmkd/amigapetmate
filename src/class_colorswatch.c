/*
 * ColorSwatch - BOOPSI gadget class.
 *
 * Draws a plain filled rectangle in a single C64 colour (from a PetsciiStyle
 * palette).  A click fires CSW_Clicked via OM_NOTIFY so the app can open
 * a colour-picker popup positioned below this gadget.
 *
 * All methods are in this single file.
 */

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/utility.h>
#include <clib/alib_protos.h>
#include <intuition/gadgetclass.h>

#include "compilers.h"
#include "class_colorswatch.h"
#include "petscii_style.h"

#include <proto/cybergraphics.h>
#include <cybergraphics/cybergraphics.h>

extern struct Library *CyberGfxBase;


#include "bdbprintf.h"
/* Convenience cast: Object * -> struct Gadget * */
#ifndef G
#define G(o) ((struct Gadget *)(o))
#endif



/* ------------------------------------------------------------------ */
/* Per-instance data                                                    */
/* ------------------------------------------------------------------ */

typedef struct ColorSwatchData {
    PetsciiStyle *style;       /* colour palette (not owned) */
    UBYTE         colorIndex;  /* 0..15 – index into style   */
    UBYTE       renderType;
} ColorSwatchData;

/* ------------------------------------------------------------------ */
/* Forward declaration of dispatcher                                    */
/* ------------------------------------------------------------------ */

ULONG ASM SAVEDS ColorSwatch_Dispatch(
    REG(a0, Class  *cl),
    REG(a2, Object *o),
    REG(a1, Msg     msg));

/* ------------------------------------------------------------------ */
/* OM_NOTIFY helper: send CSW_Clicked to ICA_TARGET                    */
/* ------------------------------------------------------------------ */

static void notifyClicked(Class *cl, Object *o, struct GadgetInfo *ginfo,
                          UBYTE colorIndex)
{
    struct opUpdate notifyMsg;
    ULONG           tags[5];

    tags[0] = GA_ID;
    tags[1] = 0;
    GetAttr(GA_ID, o, &tags[1]);
    if (tags[1] == 0) return;

    tags[2] = CSW_Clicked;
    tags[3] = (ULONG)colorIndex;
    tags[4] = TAG_DONE;

    notifyMsg.MethodID      = OM_NOTIFY;
    notifyMsg.opu_AttrList  = (struct TagItem *)tags;
    notifyMsg.opu_GInfo     = ginfo;
    notifyMsg.opu_Flags     = 0;

    DoSuperMethodA(cl, o, (Msg)&notifyMsg);
}

/* ------------------------------------------------------------------ */
/* OM_NEW                                                               */
/* ------------------------------------------------------------------ */

static ULONG ColorSwatch_OnNew(Class *cl, Object *o, struct opSet *msg)
{
    ColorSwatchData *inst;
    PetsciiStyle    *style      = NULL;
    UBYTE            colorIndex = 0;
    Object          *newObj;
    struct TagItem  *tag;

    tag = FindTagItem(CSW_Style, msg->ops_AttrList);
    if (tag) style = (PetsciiStyle *)tag->ti_Data;

    tag = FindTagItem(CSW_ColorIndex, msg->ops_AttrList);
    if (tag) colorIndex = (UBYTE)tag->ti_Data;

    if (!style) return 0;

    newObj = (Object *)DoSuperMethodA(cl, o, (APTR)msg);
    if (!newObj) return 0;

    inst             = (ColorSwatchData *)INST_DATA(cl, newObj);
    inst->style      = style;
    inst->colorIndex = colorIndex;
    inst->renderType = 0;

    return (ULONG)newObj;
}

/* ------------------------------------------------------------------ */
/* OM_DISPOSE                                                           */
/* ------------------------------------------------------------------ */

static ULONG ColorSwatch_OnDispose(Class *cl, Object *o, Msg msg)
{
    return DoSuperMethodA(cl, o, (APTR)msg);
}

/* ------------------------------------------------------------------ */
/* OM_SET / OM_UPDATE                                                   */
/* ------------------------------------------------------------------ */

static ULONG ColorSwatch_OnSet(Class *cl, Object *o, struct opSet *msg)
{
    ColorSwatchData *inst;
    struct TagItem  *state;
    struct TagItem  *tag;
    ULONG            result;
    int redraw = 0;

    inst   = (ColorSwatchData *)INST_DATA(cl, o);
    result = DoSuperMethodA(cl, o, (APTR)msg);

    state = msg->ops_AttrList;
    while ((tag = NextTagItem(&state)) != NULL) {
        switch (tag->ti_Tag) {
            case CSW_Style:
                inst->style = (PetsciiStyle *)tag->ti_Data;
                break;
            case CSW_ColorIndex:
            if(inst->colorIndex != (UBYTE)tag->ti_Data)
            {
                inst->colorIndex = (UBYTE)tag->ti_Data;
                redraw = 1;
            }

                break;
            default:
                break;
        }
    }
    if(redraw && msg->ops_GInfo )
    {
        struct RastPort *rp;

        rp = ObtainGIRPort(msg->ops_GInfo);
        if (rp)
        {
            struct gpRender  renderMsg;
            renderMsg.MethodID   = GM_RENDER;
            renderMsg.gpr_GInfo  = msg->ops_GInfo;
            renderMsg.gpr_RPort  = rp;
            renderMsg.gpr_Redraw = GREDRAW_UPDATE;

            DoMethodA(o, (Msg)(APTR)&renderMsg);

            ReleaseGIRPort(rp);
        }
    }
    return result;
}

/* ------------------------------------------------------------------ */
/* OM_GET                                                               */
/* ------------------------------------------------------------------ */

static ULONG ColorSwatch_OnGet(Class *cl, Object *o, struct opGet *msg)
{
    ColorSwatchData *inst = (ColorSwatchData *)INST_DATA(cl, o);

    switch (msg->opg_AttrID) {
        case CSW_ColorIndex:
            *msg->opg_Storage = (ULONG)inst->colorIndex;
            return TRUE;
        case CSW_Style:
            *msg->opg_Storage = (ULONG)inst->style;
            return TRUE;
        default:
            return DoSuperMethodA(cl, o, (APTR)msg);
    }
}

static ULONG ColorSwatch_OnDomain(Class *cl, Object *o, struct gpDomain *msg)
{
   struct IBox *domain = &msg->gpd_Domain;

    // Set default dimensions
    domain->Left = 0;
    domain->Top = 0;
    domain->Width = 24;  // Nominal width
    domain->Height = 16;  // Nominal height

    // Adjust based on gpd_Which
    switch (msg->gpd_Which) {
        case GDOMAIN_MINIMUM:
            domain->Width = 24;
            domain->Height = 16;
            break;
        case GDOMAIN_MAXIMUM:
            domain->Width = 128;
            domain->Height = 96;
            break;
        case GDOMAIN_NOMINAL:
        default:
            // Use default values
            break;
    }

    return 1;
}

/* ------------------------------------------------------------------ */
/* GM_RENDER                                                            */
/* ------------------------------------------------------------------ */

static ULONG ColorSwatch_OnRender(Class *cl, Object *o, struct gpRender *msg)
{
    ColorSwatchData *inst;
    struct RastPort *rp;
    WORD             left;
    WORD             top;
    WORD             w;
    WORD             h;

    (void)cl;

    inst = (ColorSwatchData *)INST_DATA(cl, o);
    if (!inst->style) return 0;

    rp   = msg->gpr_RPort;
    left = G(o)->LeftEdge;
    top  = G(o)->TopEdge;
    w    = G(o)->Width;
    h    = G(o)->Height;

    if (w <= 0 || h <= 0) return 0;


    if(CyberGfxBase &&
        msg->gpr_GInfo->gi_Screen &&
       msg->gpr_GInfo->gi_Screen->RastPort.BitMap &&
       (GetCyberMapAttr(msg->gpr_GInfo->gi_Screen->RastPort.BitMap, CYBRMATTR_ISCYBERGFX) != 0) &&
       (GetCyberMapAttr(msg->gpr_GInfo->gi_Screen->RastPort.BitMap, CYBRMATTR_DEPTH) > 8)
        )
    {
        FillPixelArray(rp,(UWORD)left,(UWORD)top,
                            (UWORD)w,(UWORD)h,
                            inst->style->paletteARGB[inst->colorIndex]
                            );
    } else
    {
        SetAPen(rp, (LONG)PetsciiStyle_Pen(inst->style, inst->colorIndex));
        RectFill(rp, (LONG)left, (LONG)top,
                     (LONG)(left + w - 1), (LONG)(top + h - 1));
    }

    SetAPen(rp, (LONG)PetsciiStyle_Pen(inst->style, 0));
    Move(rp, (LONG)left,(LONG)top);
    Draw(rp, (LONG)left + w - 1,(LONG)top);
    Draw(rp, (LONG)left + w - 1, (LONG)top + h-1);
    Draw(rp, (LONG)left , (LONG)top + h-1);
    Draw(rp, (LONG)left , (LONG)top);
    return 0;
}

/* ------------------------------------------------------------------ */
/* GM_HITTEST                                                           */
/* ------------------------------------------------------------------ */

static ULONG ColorSwatch_OnHitTest(Class *cl, Object *o, struct gpHitTest *msg)
{
    (void)cl; (void)o; (void)msg;
    return GMR_GADGETHIT;
}

/* ------------------------------------------------------------------ */
/* GM_GOACTIVE: fire CSW_Clicked and close immediately                 */
/* ------------------------------------------------------------------ */

static ULONG ColorSwatch_OnGoActive(Class *cl, Object *o, struct gpInput *msg)
{
    ColorSwatchData *inst;
    inst = (ColorSwatchData *)INST_DATA(cl, o);

    //bdbprintf("ColorSwatch_OnGoActive\n");

    notifyClicked(cl, o, msg->gpi_GInfo, inst->colorIndex);


    *msg->gpi_Termination = 0;
    return /*GMR_VERIFY |*/ GMR_NOREUSE;
}

/* ------------------------------------------------------------------ */
/* GM_HANDLEINPUT: not normally reached                                */
/* ------------------------------------------------------------------ */

static ULONG ColorSwatch_OnHandleInput(Class *cl, Object *o, struct gpInput *msg)
{
    (void)cl; (void)o;
    *msg->gpi_Termination = 0;
    return GMR_NOREUSE;
}

/* ------------------------------------------------------------------ */
/* GM_GOINACTIVE                                                        */
/* ------------------------------------------------------------------ */

static ULONG ColorSwatch_OnGoInactive(Class *cl, Object *o,
                                      struct gpGoInactive *msg)
{
    (void)cl; (void)o; (void)msg;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Dispatcher                                                           */
/* ------------------------------------------------------------------ */

ULONG ASM SAVEDS ColorSwatch_Dispatch(
    REG(a0, Class  *cl),
    REG(a2, Object *o),
    REG(a1, Msg     msg))
{
    switch (msg->MethodID) {
        case OM_NEW:         return ColorSwatch_OnNew(cl, o, (struct opSet *)msg);
        case OM_DISPOSE:     return ColorSwatch_OnDispose(cl, o, msg);
        case OM_SET:
        case OM_UPDATE:      return ColorSwatch_OnSet(cl, o, (struct opSet *)msg);
        case OM_GET:         return ColorSwatch_OnGet(cl, o, (struct opGet *)msg);
        case GM_DOMAIN:      return ColorSwatch_OnDomain(cl, o, (struct gpDomain *)msg);
        case GM_RENDER:      return ColorSwatch_OnRender(cl, o, (struct gpRender *)msg);
        case GM_HITTEST:     return ColorSwatch_OnHitTest(cl, o, (struct gpHitTest *)msg);
        case GM_GOACTIVE:    return ColorSwatch_OnGoActive(cl, o, (struct gpInput *)msg);
        case GM_HANDLEINPUT: return ColorSwatch_OnHandleInput(cl, o, (struct gpInput *)msg);
        case GM_GOINACTIVE:  return ColorSwatch_OnGoInactive(cl, o, (struct gpGoInactive *)msg);
        default:             return DoSuperMethodA(cl, o, (APTR)msg);
    }
}

/* ------------------------------------------------------------------ */
/* Class init / exit                                                    */
/* ------------------------------------------------------------------ */

Class *ColorSwatchClass = NULL;

int ColorSwatch_Init(void)
{
    if (ColorSwatchClass) return 1;

    ColorSwatchClass = MakeClass(NULL, "gadgetclass", NULL,
                                 sizeof(ColorSwatchData), 0);
    if (!ColorSwatchClass) return 0;

    ColorSwatchClass->cl_Dispatcher.h_Entry    = (HOOKFUNC)ColorSwatch_Dispatch;
    ColorSwatchClass->cl_Dispatcher.h_SubEntry = NULL;
    ColorSwatchClass->cl_Dispatcher.h_Data     = NULL;

    return 1;
}

void ColorSwatch_Exit(void)
{
    if (ColorSwatchClass) {
        FreeClass(ColorSwatchClass);
        ColorSwatchClass = NULL;
    }
}
