/*
 * PmScreenTabs - Horizontal screen-tab bar for Petmate Amiga.
 *
 * Layout (left to right):
 *   [<]  [tab0] [tab1] ... [tab7]  [>]
 *
 * "<" and ">" are narrower and shift the visible window by 1.
 * Tab buttons are enabled for screens that exist, selected for the
 * current screen.  "<"/">" are enabled only when scrolling is possible.
 */

#include <stdio.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/layout.h>
#include <proto/button.h>
#include <gadgets/layout.h>
#include <gadgets/button.h>
#include <clib/alib_protos.h>
#include <intuition/icclass.h>

#include "boopsimessage.h"
#include "pmscreentabs.h"
#include "gadgetid.h"

/* Library bases declared in petmate.c */
extern struct Library *LayoutBase;
extern struct Library *ButtonBase;

/* Weighted width for the scroll arrows (narrower than tab buttons) */
#define SCROLL_BTN_WEIGHT  400
#define TAB_BTN_WEIGHT    1000

/* ------------------------------------------------------------------ */
/* Internal: fill label buffer for tab slot i given offset            */
/* ------------------------------------------------------------------ */
static void buildLabel(PmScreenTabs *st, int slot, UWORD offset)
{
    /* Display as 1-based screen number */
    int n = (int)offset + slot + 1;
    /* snprintf not available in all C89 environments, use sprintf */
    sprintf(st->tabLabelBufs[slot], "%d", n);
}

/* ------------------------------------------------------------------ */
/* PmScreenTabs_Create                                                  */
/* ------------------------------------------------------------------ */

int PmScreenTabs_Create(PmScreenTabs *st, UWORD screenCount)
{
    int i;
    Object *lo;

    st->layout    = NULL;
    st->prevBtn   = NULL;
    st->nextBtn   = NULL;
    st->tabOffset = 0;

    for (i = 0; i < SCREENTABS_VISIBLE; i++)
        st->tabs[i] = NULL;

    /* Build initial label buffers "1".."SCREENTABS_VISIBLE" */
    for (i = 0; i < SCREENTABS_VISIBLE; i++)
        buildLabel(st, i, 0);

    /* "<" scroll button - initially disabled (only 1 screen at startup) */
    st->prevBtn = (Object *)NewObject(BUTTON_GetClass(), NULL,
        GA_ID,       (ULONG)GAD_SCREENTAB_PREV,
        ICA_TARGET,  TargetInstance,
        GA_RELVERIFY,TRUE,
        GA_Disabled, TRUE,
        GA_Text,     (ULONG)"<",
        TAG_END);
    if (!st->prevBtn) return 0;

    /* ">" scroll button - initially disabled */
    st->nextBtn = (Object *)NewObject(BUTTON_GetClass(), NULL,
        GA_ID,       (ULONG)GAD_SCREENTAB_NEXT,
        ICA_TARGET,  TargetInstance,
        GA_RELVERIFY,TRUE,
        GA_Disabled, TRUE,
        GA_Text,     (ULONG)">",
        TAG_END);
    if (!st->nextBtn) return 0;

    /* Numbered tab buttons */
    for (i = 0; i < SCREENTABS_VISIBLE; i++) {
        BOOL enabled  = (UWORD)i < screenCount;
        BOOL selected = (i == 0);

        st->tabs[i] = (Object *)NewObject(BUTTON_GetClass(), NULL,
            GA_ID,          (ULONG)(GAD_SCREENTAB_FIRST + i),
            ICA_TARGET,     TargetInstance,
            BUTTON_PushButton, TRUE,
            GA_RELVERIFY,   TRUE,
            GA_Disabled,    (ULONG)(!enabled),
            GA_Selected,    (ULONG)selected,
            GA_Text,        (ULONG)st->tabLabelBufs[i],
            TAG_END);

        if (!st->tabs[i]) return 0;
    }

    /* HLayout: [<] [tab0..7] [>]
     * "<"/">" have SCROLL_BTN_WEIGHT; tabs have TAB_BTN_WEIGHT. */
    lo = (Object *)NewObject(LAYOUT_GetClass(), NULL,
        LAYOUT_Orientation,  LAYOUT_ORIENT_HORIZ,
        LAYOUT_InnerSpacing, 1,
        LAYOUT_BevelStyle,   BVS_SBAR_VERT,

        LAYOUT_AddChild, (ULONG)st->prevBtn,   CHILD_WeightedWidth, SCROLL_BTN_WEIGHT,
        LAYOUT_AddChild, (ULONG)st->tabs[0],   CHILD_WeightedWidth, TAB_BTN_WEIGHT,
        LAYOUT_AddChild, (ULONG)st->tabs[1],   CHILD_WeightedWidth, TAB_BTN_WEIGHT,
        LAYOUT_AddChild, (ULONG)st->tabs[2],   CHILD_WeightedWidth, TAB_BTN_WEIGHT,
        LAYOUT_AddChild, (ULONG)st->tabs[3],   CHILD_WeightedWidth, TAB_BTN_WEIGHT,
        LAYOUT_AddChild, (ULONG)st->tabs[4],   CHILD_WeightedWidth, TAB_BTN_WEIGHT,
        LAYOUT_AddChild, (ULONG)st->tabs[5],   CHILD_WeightedWidth, TAB_BTN_WEIGHT,
        LAYOUT_AddChild, (ULONG)st->tabs[6],   CHILD_WeightedWidth, TAB_BTN_WEIGHT,
        LAYOUT_AddChild, (ULONG)st->tabs[7],   CHILD_WeightedWidth, TAB_BTN_WEIGHT,
        LAYOUT_AddChild, (ULONG)st->nextBtn,   CHILD_WeightedWidth, SCROLL_BTN_WEIGHT,
        TAG_END);

    if (!lo) return 0;
    st->layout = lo;
    return 1;
}

/* ------------------------------------------------------------------ */
/* PmScreenTabs_Update                                                  */
/* ------------------------------------------------------------------ */

void PmScreenTabs_Update(PmScreenTabs *st, UWORD screenCount,
                          UWORD currentScreen, struct Window *win, int alsoCurrent)
{
    int   i;
    BOOL  prevEnabled;
    BOOL  nextEnabled;

    if (!st) return;
    (void)alsoCurrent; /* always update everything */

    /* Clamp tabOffset so we never show past the last screen */
    if (screenCount > SCREENTABS_VISIBLE) {
        UWORD maxOffset = screenCount - SCREENTABS_VISIBLE;
        if (st->tabOffset > maxOffset)
            st->tabOffset = maxOffset;
    } else {
        st->tabOffset = 0;
    }

    /* Scroll buttons: enabled only when more than SCREENTABS_VISIBLE screens
     * and there is room to go in that direction. */
    prevEnabled = (screenCount > SCREENTABS_VISIBLE) && (st->tabOffset > 0);
    nextEnabled = (screenCount > SCREENTABS_VISIBLE) &&
                  ((UWORD)(st->tabOffset + SCREENTABS_VISIBLE) < screenCount);

    /* Update tab buttons */
    for (i = 0; i < SCREENTABS_VISIBLE; i++) {
        UWORD screenIdx = st->tabOffset + (UWORD)i;
        BOOL  enabled   = screenIdx < screenCount;
        BOOL  selected  = enabled && (screenIdx == currentScreen);

        if (!st->tabs[i]) continue;

        buildLabel(st, i, st->tabOffset);

        if (win) {
            SetGadgetAttrs((struct Gadget *)st->tabs[i], win, NULL,
                GA_Text,     (ULONG)st->tabLabelBufs[i],
                GA_Disabled, (ULONG)(!enabled),
                GA_Selected, (ULONG)selected,
                TAG_END);
        } else {
            SetAttrs(st->tabs[i],
                GA_Text,     (ULONG)st->tabLabelBufs[i],
                GA_Disabled, (ULONG)(!enabled),
                GA_Selected, (ULONG)selected,
                TAG_END);
        }
    }

    /* Update scroll buttons */
    if (st->prevBtn) {
        if (win)
            SetGadgetAttrs((struct Gadget *)st->prevBtn, win, NULL,
                GA_Disabled, (ULONG)(!prevEnabled), TAG_END);
        else
            SetAttrs(st->prevBtn,
                GA_Disabled, (ULONG)(!prevEnabled), TAG_END);
    }
    if (st->nextBtn) {
        if (win)
            SetGadgetAttrs((struct Gadget *)st->nextBtn, win, NULL,
                GA_Disabled, (ULONG)(!nextEnabled), TAG_END);
        else
            SetAttrs(st->nextBtn,
                GA_Disabled, (ULONG)(!nextEnabled), TAG_END);
    }
}
