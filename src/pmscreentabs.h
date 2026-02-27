#ifndef PMSCREENTABS_H
#define PMSCREENTABS_H

/*
 * PmScreenTabs - Horizontal screen-tab bar for Petmate Amiga.
 *
 * Creates up to SCREENTABS_MAX button gadgets in a HLayout, one per
 * screen.  Tabs beyond the current screen count are disabled.
 * The selected tab reflects the active screen.
 *
 * The layout object (tabs.layout) is placed at the top of the main
 * window layout with CHILD_WeightedHeight=0.
 */

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/classusr.h>

/* Maximum number of simultaneous tab buttons */
#define SCREENTABS_MAX 8

typedef struct PmScreenTabs {
    Object *layout;
    Object *tabs[SCREENTABS_MAX];
} PmScreenTabs;

/* Build the tab layout.
 * screenCount: how many screens are in the project (1..SCREENTABS_MAX).
 * Returns 1 on success, 0 on failure. */
int  PmScreenTabs_Create(PmScreenTabs *st, UWORD screenCount,
                          struct DrawInfo *di);

/* Refresh enabled/disabled state and selected highlight.
 * Call after any screen-count or current-screen change.
 * win may be NULL if the window is not yet open. */
void PmScreenTabs_Update(PmScreenTabs *st, UWORD screenCount,
                          UWORD currentScreen, struct Window *win);

#endif /* PMSCREENTABS_H */
