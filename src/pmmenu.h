#ifndef PMMENU_H
#define PMMENU_H

/*
 * pmmenu - GadTools menu creation for Petmate Amiga.
 *
 * The menu template is rebuilt dynamically before each CreateMenus() call
 * so the "Open Recent" sub-menu always reflects the current recent-file list.
 *
 * nm_UserData stores MSG_* IDs for NM_TITLE entries and ACTION_* IDs for
 * item/sub entries.  resolveMenuLabels() fills nm_Label accordingly.
 */

#include <exec/types.h>
#include <intuition/intuition.h>
#include "pmaction.h"
#include "appsettings.h"
#include <intuition/classusr.h>

/* Menu state */
typedef struct PmMenu {
    struct Menu *menu;      /* GadTools menu structure */
    APTR visualInfo;        /* Visual info for the screen */
} PmMenu;

/* Create menus and attach to window. as may be NULL (no recent files shown).
 * Returns TRUE on success. */
BOOL PmMenu_Create(PmMenu *pm, struct Screen *screen, struct Window *window,
                   const AppSettings *as);

/* Remove menus from window and free all resources. */
void PmMenu_Close(PmMenu *pm, struct Window *window);

/* Rebuild the menu (e.g. after recent-file list changes).
 * Keeps existing visualInfo; only FreeMenus/CreateMenus/LayoutMenus are redone. */
void PmMenu_Rebuild(PmMenu *pm, struct Screen *screen, struct Window *window,
                    const AppSettings *as);

/* Get action ID from a menu selection code (result & WMHI_MENUMASK).
 * Returns -1 if no valid selection. */
LONG PmMenu_ToActionID(PmMenu *pm, UWORD menuCode);

/* Update the checkmark on palette menu items to reflect the active palette.
 * paletteID 0..3 maps to PETMATE..VICE. Menu must be attached to window. */
void PmMenu_UpdatePaletteCheck(PmMenu *pm, struct Window *window, UBYTE paletteID);

/* Get action ID from a menu selection code (result & WMHI_MENUMASK).
 * Returns -1 if no valid selection. */
LONG PmMenu_ToActionID(PmMenu *pm, UWORD menuCode);

/* Enable or disable the Brush menu and all its items depending on whether
 * the canvas gadget currently holds a brush (PCA_Brush != NULL).
 * Call after PmMenu_Create() and whenever PCA_SignalStopTool or
 * PCA_BrushRemoved is received. */
void PmMenu_UpdateBrushMenu(PmMenu *pm, struct Window *window, Object *canvasGadget);

#endif /* PMMENU_H */
