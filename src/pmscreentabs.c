/*
 * PmScreenTabs - Horizontal screen-tab bar for Petmate Amiga.
 * Creates SCREENTABS_MAX button gadgets in a ReAction HLayout.
 * Tabs beyond the current screen count are disabled (grayed out).
 */

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/layout.h>
#include <proto/button.h>
#include <gadgets/layout.h>
#include <gadgets/button.h>
#include <clib/alib_protos.h>

#include "pmscreentabs.h"
#include "gadgetid.h"

/* Library bases declared in petmate.c */
extern struct Library *LayoutBase;
extern struct Library *ButtonBase;

/* Fixed single-character labels "1".."8" */
static const char *tabLabels[SCREENTABS_MAX] = {
    "1", "2", "3", "4", "5", "6", "7", "8"
};

/* ------------------------------------------------------------------ */
/* PmScreenTabs_Create                                                  */
/* ------------------------------------------------------------------ */

int PmScreenTabs_Create(PmScreenTabs *st, UWORD screenCount,
                         struct DrawInfo *di)
{
    int i;
    Object *lo;

    for (i = 0; i < SCREENTABS_MAX; i++)
        st->tabs[i] = NULL;
    st->layout = NULL;

    /* Create SCREENTABS_MAX button gadgets up front; disable extras */
    for (i = 0; i < SCREENTABS_MAX; i++) {
        BOOL enabled = (UWORD)i < screenCount;
        BOOL selected = (i == 0);  /* screen 0 is current at startup */

        st->tabs[i] = (Object *)NewObject(BUTTON_GetClass(), NULL,
            GA_ID,       (ULONG)(GAD_SCREENTAB_FIRST + i),
            GA_DrawInfo, (ULONG)di,
            GA_Disabled, (ULONG)(!enabled),
            GA_Selected, (ULONG)selected,
            GA_Text,     (ULONG)tabLabels[i],
            TAG_END);

        if (!st->tabs[i]) return 0;
    }

    /* Build HLayout - all tabs equal width */
    lo = (Object *)NewObject(LAYOUT_GetClass(), NULL,
        GA_DrawInfo,         (ULONG)di,
        LAYOUT_Orientation,  LAYOUT_ORIENT_HORIZ,
        LAYOUT_InnerSpacing, 1,
        LAYOUT_BevelStyle,   BVS_SBAR_VERT,

        LAYOUT_AddChild, (ULONG)st->tabs[0], CHILD_WeightedWidth, 1000,
        LAYOUT_AddChild, (ULONG)st->tabs[1], CHILD_WeightedWidth, 1000,
        LAYOUT_AddChild, (ULONG)st->tabs[2], CHILD_WeightedWidth, 1000,
        LAYOUT_AddChild, (ULONG)st->tabs[3], CHILD_WeightedWidth, 1000,
        LAYOUT_AddChild, (ULONG)st->tabs[4], CHILD_WeightedWidth, 1000,
        LAYOUT_AddChild, (ULONG)st->tabs[5], CHILD_WeightedWidth, 1000,
        LAYOUT_AddChild, (ULONG)st->tabs[6], CHILD_WeightedWidth, 1000,
        LAYOUT_AddChild, (ULONG)st->tabs[7], CHILD_WeightedWidth, 1000,
        TAG_END);

    if (!lo) return 0;
    st->layout = lo;
    return 1;
}

/* ------------------------------------------------------------------ */
/* PmScreenTabs_Update                                                  */
/* ------------------------------------------------------------------ */

void PmScreenTabs_Update(PmScreenTabs *st, UWORD screenCount,
                          UWORD currentScreen, struct Window *win)
{
    int i;

    if (!st) return;

    for (i = 0; i < SCREENTABS_MAX; i++) {
        BOOL enabled  = (UWORD)i < screenCount;
        BOOL selected = ((UWORD)i == currentScreen);

        if (!st->tabs[i]) continue;

        if (win) {
            SetGadgetAttrs((struct Gadget *)st->tabs[i], win, NULL,
                GA_Disabled, (ULONG)(!enabled),
                GA_Selected, (ULONG)selected,
                TAG_END);
        } else {
            SetAttrs(st->tabs[i],
                GA_Disabled, (ULONG)(!enabled),
                GA_Selected, (ULONG)selected,
                TAG_END);
        }
    }
}
