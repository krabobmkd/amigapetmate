#ifndef PMMENU_H
#define PMMENU_H

/*
 * pmmenu - GadTools menu creation for Petmate Amiga.
 * Pattern adapted from aukboopsi/aukmenu.h/.c.
 *
 * Menu template uses nm_UserData to store MSG_* IDs (for titles)
 * and ACTION_* IDs (for items). resolveMenuLabels() fills nm_Label
 * from the locale and action systems.
 */

#include <exec/types.h>
#include <intuition/intuition.h>
#include "pmaction.h"

/* Menu state */
typedef struct PmMenu {
    struct Menu *menu;      /* GadTools menu structure */
    APTR visualInfo;        /* Visual info for the screen */
} PmMenu;

/* Create menus and attach to window.
 * Returns TRUE on success. */
BOOL PmMenu_Create(PmMenu *pm, struct Screen *screen, struct Window *window);

/* Remove menus from window and free all resources. */
void PmMenu_Close(PmMenu *pm, struct Window *window);

/* Get action ID from a menu selection code (result & WMHI_MENUMASK).
 * Returns -1 if no valid selection. */
LONG PmMenu_ToActionID(PmMenu *pm, UWORD menuCode);

#endif /* PMMENU_H */
