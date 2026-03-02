/*
 * CharSelector - input handling: GM_GOACTIVE, GM_HANDLEINPUT, GM_GOINACTIVE.
 *
 * Left-button click or drag updates selectedChar; a self-render is
 * triggered so the COMPLEMENT highlight moves immediately.
 * Button release generates GMR_VERIFY|GMR_NOREUSE → Intuition posts
 * IDCMP_GADGETUP to the application.
 *
 * Mouse-to-character mapping honours CHSA_KeepRatio: when keepRatio is
 * TRUE the content rect (inst->contentX/Y/W/H) is smaller than the full
 * gadget bounds.  Mouse coordinates are clamped to the content rect before
 * the cell index is computed, so clicking in a margin selects the nearest
 * border character.
 */

#include <proto/graphics.h>
#include <proto/intuition.h>
#include <devices/inputevent.h>
#include <clib/alib_protos.h>
#include "char_selector_private.h"

/* ------------------------------------------------------------------ */
/* Static helpers                                                       */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* Notify ICA_TARGET with one attribute change (same pattern as        */
/* ColorPicker_NotifyAttribChange).                                    */
/* ------------------------------------------------------------------ */

static ULONG CharSelector_NotifyAttribChange(Class *cl, Object *o,
                                             struct GadgetInfo *gi,
                                             ULONG attrib, ULONG value)
{
    struct opUpdate notifymsg;
    ULONG messageTags[] = {
        GA_ID, 0,
        0,     0,
        TAG_DONE
    };

    GetAttr(GA_ID, o, &messageTags[1]);
    if (messageTags[1] == 0) return 0; /* must have valid GA_ID */

    messageTags[2] = attrib;
    messageTags[3] = value;

    notifymsg.MethodID      = OM_NOTIFY;
    notifymsg.opu_AttrList  = (struct TagItem *)&messageTags[0];
    notifymsg.opu_GInfo     = gi;
    notifymsg.opu_Flags     = 0;

    return DoSuperMethodA(cl, (APTR)o, (Msg)&notifymsg);
}

/*
 * Convert gadget-relative mouse coordinates to a character code 0..255.
 * gpi_Mouse.X/Y are relative to the gadget's LeftEdge/TopEdge origin.
 * The content rect (inst->contentX/Y/W/H) is applied so the mapping is
 * correct regardless of whether keepRatio is TRUE or FALSE.
 */
static UBYTE mouseToChar(CharSelectorData *inst, WORD relX, WORD relY)
{
    WORD  cW = inst->contentW;
    WORD  cH = inst->contentH;
    WORD  cx;
    WORD  cy;
    UBYTE col;
    UBYTE row;

    if (cW <= 0 || cH <= 0) return 0;

    /* Translate from gadget-relative to content-rect-relative */
    cx = relX - inst->contentX;
    cy = relY - inst->contentY;

    /* Clamp to content area (mouse in margin → nearest border char) */
    if (cx < 0)   cx = 0;
    if (cy < 0)   cy = 0;
    if (cx >= cW) cx = (WORD)(cW - 1);
    if (cy >= cH) cy = (WORD)(cH - 1);

    col = (UBYTE)((ULONG)cx * CHARSELECTOR_COLS / (ULONG)cW);
    row = (UBYTE)((ULONG)cy * CHARSELECTOR_ROWS / (ULONG)cH);

    if (col >= CHARSELECTOR_COLS) col = CHARSELECTOR_COLS - 1;
    if (row >= CHARSELECTOR_ROWS) row = CHARSELECTOR_ROWS - 1;

    /* Return the screencode displayed at this grid position */
    {
        const int *order;
        order = (inst->charset == PETSCII_CHARSET_LOWER)
                ? petmate_char_order_lower
                : petmate_char_order_upper;
        return (UBYTE)order[(ULONG)row * CHARSELECTOR_COLS + col];
    }
}

/*
 * Trigger a GM_RENDER on ourselves using ObtainGIRPort so the
 * COMPLEMENT highlight updates without waiting for the next refresh.
 */
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
/* GM_GOACTIVE                                                          */
/* ------------------------------------------------------------------ */

ULONG CharSelector_OnGoActive(Class *cl, Object *o, struct gpInput *msg)
{
    CharSelectorData *inst;
    UBYTE             newChar;

    inst    = (CharSelectorData *)INST_DATA(cl, o);
    newChar = mouseToChar(inst, msg->gpi_Mouse.X, msg->gpi_Mouse.Y);

    if (newChar != inst->selectedChar) {
        inst->selectedChar = newChar;
        renderSelf(cl, o, msg->gpi_GInfo);
    }

    return GMR_MEACTIVE;  /* stay active: track drag */
}

/* ------------------------------------------------------------------ */
/* GM_HANDLEINPUT                                                       */
/* ------------------------------------------------------------------ */

ULONG CharSelector_OnHandleInput(Class *cl, Object *o, struct gpInput *msg)
{
    CharSelectorData  *inst;
    struct InputEvent *ie;
    UBYTE              newChar;

    inst = (CharSelectorData *)INST_DATA(cl, o);
    ie   = msg->gpi_IEvent;

    /* Left button release: notify ICA_TARGET, deactivate, post GADGETUP */
    if (ie->ie_Class == IECLASS_RAWMOUSE &&
        ie->ie_Code  == (IECODE_LBUTTON | IECODE_UP_PREFIX)) {
        CharSelector_NotifyAttribChange(cl, o, msg->gpi_GInfo,
                                        CHSA_SelectedChar,
                                        (ULONG)inst->selectedChar);
        *msg->gpi_Termination = 0;
        return GMR_VERIFY | GMR_NOREUSE;
    }

    /* Mouse move while button held: update selection */
    if (ie->ie_Class == IECLASS_RAWMOUSE) {
        newChar = mouseToChar(inst, msg->gpi_Mouse.X, msg->gpi_Mouse.Y);
        if (newChar != inst->selectedChar) {
            inst->selectedChar = newChar;
            renderSelf(cl, o, msg->gpi_GInfo);
        }
    }

    return GMR_MEACTIVE;
}

/* ------------------------------------------------------------------ */
/* GM_GOINACTIVE                                                        */
/* ------------------------------------------------------------------ */

ULONG CharSelector_OnGoInactive(Class *cl, Object *o, struct gpGoInactive *msg)
{
    (void)cl; (void)o; (void)msg;
    return 0;
}
