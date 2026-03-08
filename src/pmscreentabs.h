#ifndef PMSCREENTABS_H
#define PMSCREENTABS_H

/*
 * PmScreenTabs - Horizontal screen-tab bar for Petmate Amiga.
 *
 * Shows SCREENTABS_VISIBLE numbered buttons for screens, with "<" and ">"
 * scroll buttons at each end to shift the visible window when there are
 * more than SCREENTABS_VISIBLE screens.
 *
 * tabOffset: index of the first visible screen (0-based).
 * Tabs show screens [tabOffset .. tabOffset+SCREENTABS_VISIBLE-1].
 * "<" and ">" buttons enabled only when there are more than
 * SCREENTABS_VISIBLE screens and there is room to scroll.
 */

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/classusr.h>

/* Number of simultaneously visible tab buttons */
#define SCREENTABS_VISIBLE 8

typedef struct PmScreenTabs {
    Object *layout;
    Object *tabs[SCREENTABS_VISIBLE];
    Object *prevBtn;                            /* "<" scroll button */
    Object *nextBtn;                            /* ">" scroll button */
    UWORD   tabOffset;                          /* first visible screen index */
    char    tabLabelBufs[SCREENTABS_VISIBLE][4];/* "1".."32" label storage */
} PmScreenTabs;

/* Build the tab layout with SCREENTABS_VISIBLE tabs plus "<" and ">" buttons.
 * Returns 1 on success, 0 on failure. */
int  PmScreenTabs_Create(PmScreenTabs *st, UWORD screenCount);

/* Refresh all button labels, enabled/disabled and selected states.
 * Does NOT auto-adjust tabOffset; call before this if you want scrolling.
 * win may be NULL if the window is not yet open. */
void PmScreenTabs_Update(PmScreenTabs *st, UWORD screenCount,
                          UWORD currentScreen, struct Window *win, int alsoCurrent);

#endif /* PMSCREENTABS_H */
