/*
 * CharSelector - attribute handlers: OM_NEW, OM_DISPOSE, OM_SET, OM_GET.
 */

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/utility.h>
#include "char_selector_private.h"
#include <bdbprintf.h>
#define CBUF_SIZE  ((ULONG)CHARSELECTOR_NATIVE_W * CHARSELECTOR_NATIVE_H)

ULONG CharSelector_OnSet(Class *cl, Object *o, struct opSet *msg);
/* ------------------------------------------------------------------ */
/* OM_NEW                                                               */
/* ------------------------------------------------------------------ */
ULONG CharSelector_OnNew(Class *cl, Object *o, struct opSet *msg)
{
    CharSelectorData *inst;
    PetsciiStyle     *style    = NULL;
    Object           *newObj;
    struct TagItem   *tag;
    ULONG             mDispose;

    tag = FindTagItem(CHSA_Style, msg->ops_AttrList);
    if (tag) style = (PetsciiStyle *)tag->ti_Data;

    if (!style) return 0;

    newObj = (Object *)DoSuperMethodA(cl, o, (APTR)msg);
    if (!newObj) return 0;

    inst               = (CharSelectorData *)INST_DATA(cl, newObj);
    inst->style        = NULL;
    inst->clipRegion   =  NewRegion();
    inst->charset      = 0;//charset;
    inst->fgColor      = 1;
    inst->bgColor      = 0;
    inst->selectedChar =  0xa0; /* space reverted: full char */
    inst->cbuf         = NULL;
    inst->valid        = 0;
    inst->scaledBuf    = NULL;
    inst->scaledW      = 0;
    inst->scaledH      = 0;
    inst->keepRatio    = FALSE;
    inst->contentX     = 0;
    inst->contentY     = 0;
    inst->contentW     = 0;
    inst->contentH     = 0;

    inst->cbuf = (UBYTE *)AllocVec(CBUF_SIZE, MEMF_ANY);
    if (!inst->cbuf) {
        mDispose = OM_DISPOSE;
        DoSuperMethodA(cl, newObj, (APTR)&mDispose);

        return 0;
    }
    CharSelector_OnSet(cl, newObj,msg);
    return (ULONG)newObj;
}

/* ------------------------------------------------------------------ */
/* OM_DISPOSE                                                           */
/* ------------------------------------------------------------------ */

ULONG CharSelector_OnDispose(Class *cl, Object *o, Msg msg)
{
    CharSelectorData *inst = (CharSelectorData *)INST_DATA(cl, o);

    if (inst->cbuf)      { FreeVec(inst->cbuf);      inst->cbuf      = NULL; }
    if (inst->scaledBuf) { FreeVec(inst->scaledBuf); inst->scaledBuf = NULL; }
    if (inst->clipRegion) { DisposeRegion(inst->clipRegion); inst->clipRegion = NULL; }

    return DoSuperMethodA(cl, o, (APTR)msg);
}

/* ------------------------------------------------------------------ */
/* OM_SET / OM_UPDATE                                                   */
/* ------------------------------------------------------------------ */

ULONG CharSelector_OnSet(Class *cl, Object *o, struct opSet *msg)
{
    CharSelectorData *inst;
    struct TagItem   *state;
    struct TagItem   *tag;
    ULONG             result = 0;
    ULONG           redraw = 0;

    inst   = (CharSelectorData *)INST_DATA(cl, o);
    state  = msg->ops_AttrList;

    while ((tag = NextTagItem(&state)) != NULL)
    {
        switch (tag->ti_Tag)
        {
            case CHSA_Style:
                inst->style  = (PetsciiStyle *)tag->ti_Data;
                inst->valid  = 0;
                result = 1;
                redraw = 1;
                break;
            case CHSA_Charset:
                if(inst->charset != (UBYTE)tag->ti_Data)
                {
                    inst->charset = (UBYTE)tag->ti_Data;
                    inst->valid   = 0;
                    redraw = 1;
                }
                result = 1;
                break;
            case CHSA_FgColor:
                if(inst->fgColor != (UBYTE)tag->ti_Data)
                {
                    inst->fgColor = (UBYTE)tag->ti_Data;
                    redraw = 1;
                }
                inst->valid   = 0;
                result = 1;
                break;
            case CHSA_BgColor:
                if(inst->bgColor != (UBYTE)tag->ti_Data)
                {
                    inst->bgColor = (UBYTE)tag->ti_Data;
                    redraw = 1;
                }
                inst->valid = 0;
                result = 1;
                break;
            case CHSA_SelectedChar:
                inst->selectedChar = (UBYTE)tag->ti_Data;
                result = 1;
                redraw = 1;
                break;
            case CHSA_Dirty:
                if (tag->ti_Data)
                {
                    inst->valid = 0;
                    result = 1;
                    redraw = 1;
                }
                break;
            case CHSA_KeepRatio:
                if(inst->keepRatio == (BOOL)tag->ti_Data) continue;
                inst->keepRatio = (BOOL)tag->ti_Data;
                /* Force content rect + scaled buf to be recomputed */
                inst->contentW  = 0;
                inst->contentH  = 0;
                inst->scaledW   = 0;
                inst->scaledH   = 0;
                result = 1;
                break;
            default:
                break;
        }
    }
    /* I'm not fan of sending redraws under setAttribs,
     * because render are better send up down on the right process.
     * any process or context can originate a SetAttrs() or SetGAdgetAttrs()
     * would be better trigger an external redraw event up->down from the process.
     * Yet, sending redraw under Setttrs is the documented way,
     * Even if it's very clear that is generate bugs when done from HandleInput.
     * well let's go.... */
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

    return result;
}

/* ------------------------------------------------------------------ */
/* OM_GET                                                               */
/* ------------------------------------------------------------------ */

ULONG CharSelector_OnGet(Class *cl, Object *o, struct opGet *msg)
{
    CharSelectorData *inst = (CharSelectorData *)INST_DATA(cl, o);

    switch (msg->opg_AttrID)
    {
        case CHSA_SelectedChar:
            *msg->opg_Storage = (ULONG)inst->selectedChar;
            return TRUE;
        case CHSA_Charset:
            *msg->opg_Storage = (ULONG)inst->charset;
            return TRUE;
        case CHSA_KeepRatio:
            *msg->opg_Storage = (ULONG)inst->keepRatio;
            return TRUE;
        default:
            return DoSuperMethodA(cl, o, (APTR)msg);
    }
}
