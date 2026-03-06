/*
 * PmToolbar - Left-side vertical toolbar for Petmate Amiga.
 * Creates tool-selection and action buttons in a ReAction VLayout.
 */

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/layout.h>
#include <proto/button.h>
#include <gadgets/layout.h>
#include <gadgets/button.h>
#include <clib/alib_protos.h>

#include <intuition/icclass.h>

#include "boopsimessage.h"
#include "pmtoolbar.h"
#include "pmlocale.h"
#include "gadgetid.h"
#include "petscii_types.h"
#include <stdio.h>

/* Library bases declared in petmate.c */
extern struct Library *LayoutBase;
extern struct Library *ButtonBase;

/* ------------------------------------------------------------------ */
/* Static helper: create one labeled button                            */
/* ------------------------------------------------------------------ */

static Object *makeBtnSwitch( ULONG gadID,
                       const char *label, BOOL selected)
{
    return (Object *)NewObject(BUTTON_GetClass(), NULL,
        GA_ID,       gadID,
        BUTTON_PushButton, TRUE, /*toggle*/
        ICA_TARGET,TargetInstance,
        GA_Selected, (ULONG)selected,
        GA_Text,     (ULONG)label,
        TAG_END);
}
/* thise buttons send gadgetup events */
static Object *makeBtnActionUp( ULONG gadID,
                       const char *label)
{
    return (Object *)NewObject(BUTTON_GetClass(), NULL,
        GA_ID,       gadID,
        GA_RelVerify, TRUE, /* needed for gadgetUp event */
        ICA_TARGET,TargetInstance,
        GA_Text,     (ULONG)label,
        TAG_END);
}

/* ------------------------------------------------------------------ */
/* PmToolbar_Create                                                    */
/* ------------------------------------------------------------------ */

int PmToolbar_Create(PmToolbar *tb)
{
    Object *toolGroup;
    Object *actionGroup;
    int     i;

    for (i = 0; i < TOOLBAR_TOOL_COUNT; i++)
        tb->toolBtns[i] = NULL;
    tb->undoBtn  = NULL;
    tb->redoBtn  = NULL;
    tb->clearBtn = NULL;
    tb->layout   = NULL;

    /* Tool buttons (TOOL_DRAW is initially selected) */
    tb->toolBtns[TOOL_DRAW]     = makeBtnSwitch( GAD_TOOL_DRAW,
                                           LOC(MSG_TOOL_DRAW),     TRUE);
    tb->toolBtns[TOOL_COLORIZE] = makeBtnSwitch( GAD_TOOL_COLORIZE,
                                           LOC(MSG_TOOL_COLORIZE), FALSE);
    tb->toolBtns[TOOL_CHARDRAW] = makeBtnSwitch( GAD_TOOL_CHARDRAW,
                                           LOC(MSG_TOOL_CHARDRAW), FALSE);
    tb->toolBtns[TOOL_BRUSH]    = makeBtnSwitch( GAD_TOOL_BRUSH,
                                           LOC(MSG_TOOL_BRUSH),    FALSE);
    tb->toolBtns[TOOL_TEXT]     = makeBtnSwitch( GAD_TOOL_TEXT,
                                           LOC(MSG_TOOL_TEXT),     FALSE);

    /* Action buttons */
    tb->undoBtn  = makeBtnActionUp( GAD_TOOL_UNDO,  LOC(MSG_EDIT_UNDO));
    tb->redoBtn  = makeBtnActionUp( GAD_TOOL_REDO,  LOC(MSG_EDIT_REDO));
    tb->clearBtn = makeBtnActionUp( GAD_TOOL_CLEAR, LOC(MSG_BTN_CLEAR));

    /* Verify all objects were created */
    for (i = 0; i < TOOLBAR_TOOL_COUNT; i++) {
        if (!tb->toolBtns[i]) return 0;
    }
    if (!tb->undoBtn || !tb->redoBtn || !tb->clearBtn) return 0;

    /* Tool group VLayout */
    toolGroup = (Object *)NewObject(LAYOUT_GetClass(), NULL,

        LAYOUT_Orientation,  /*LAYOUT_ORIENT_VERT*/LAYOUT_ORIENT_HORIZ,
        LAYOUT_InnerSpacing, 1,

        LAYOUT_AddChild, (ULONG)tb->toolBtns[TOOL_DRAW],
         //   CHILD_WeightedHeight, 0,
        LAYOUT_AddChild, (ULONG)tb->toolBtns[TOOL_COLORIZE],
         //   CHILD_WeightedHeight, 0,
        LAYOUT_AddChild, (ULONG)tb->toolBtns[TOOL_CHARDRAW],
         //   CHILD_WeightedHeight, 0,
        LAYOUT_AddChild, (ULONG)tb->toolBtns[TOOL_BRUSH],
         //   CHILD_WeightedHeight, 0,
        LAYOUT_AddChild, (ULONG)tb->toolBtns[TOOL_TEXT],
         //   CHILD_WeightedHeight, 0,
        TAG_END);

    if (!toolGroup) return 0;

    /* Action group VLayout */
    tb->layoutUndoRedo = (Object *)NewObject(LAYOUT_GetClass(), NULL,

        LAYOUT_Orientation,  LAYOUT_ORIENT_HORIZ,
        LAYOUT_InnerSpacing, 1,

        LAYOUT_AddChild, (ULONG)tb->undoBtn,
       //     CHILD_WeightedHeight, 0,
        LAYOUT_AddChild, (ULONG)tb->redoBtn,
       //     CHILD_WeightedHeight, 0,
        LAYOUT_AddChild, (ULONG)tb->clearBtn,
       //     CHILD_WeightedHeight, 0,
        TAG_END);

    if (!tb->layoutUndoRedo) return 0;

    /* Outer VLayout: tool group then action group, filler eats remaining */
    // tb->layout = (Object *)NewObject(LAYOUT_GetClass(), NULL,

    //     LAYOUT_Orientation,  LAYOUT_ORIENT_VERT,
    //     LAYOUT_InnerSpacing, 2,

    //     LAYOUT_AddChild, (ULONG)toolGroup,
    //         CHILD_WeightedHeight, 1,

    //     /*moved
    //     LAYOUT_AddChild, (ULONG)actionGroup,
    //         CHILD_WeightedHeight, 1,
    //     */
    //     TAG_END);
    tb->layout =toolGroup;

    return (tb->layout != NULL) ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/* PmToolbar_SetActiveTool                                             */
/* Clear GA_Selected on all tool buttons then set it on the active one */
/* ------------------------------------------------------------------ */

void PmToolbar_SetActiveTool(PmToolbar *tb, UBYTE tool, struct Window *win)
{
    int i;
    for (i = 0; i < TOOLBAR_TOOL_COUNT; i++) {
        if (!tb->toolBtns[i]) continue;
        ULONG doselect = (i == (int)tool);
        ULONG cstate=0;
        GetAttr(GA_Selected,tb->toolBtns[i],&cstate);

        if(doselect != cstate) /* important test, or would go infinite recursion */
            if (win)
                SetGadgetAttrs((struct Gadget *)tb->toolBtns[i], win, NULL,
                               GA_Selected, doselect,
                               TAG_END);
            else
                SetAttrs(tb->toolBtns[i],
                         GA_Selected,doselect,
                         TAG_END);
    }
}
/* just so the selected tool doesnt toggle up when reclicked. */
void PmToolbar_ForceDownTrick(PmToolbar *tb, UBYTE tool, struct Window *win)
{
    if(tool>=TOOLBAR_TOOL_COUNT) return;
    if (!tb->toolBtns[tool]) return;

    if (win)
        SetGadgetAttrs((struct Gadget *)tb->toolBtns[tool], win, NULL,
                       GA_Selected, TRUE,
                       TAG_END);
    else
        SetAttrs(tb->toolBtns[tool],
                 GA_Selected,TRUE,
                 TAG_END);
}
