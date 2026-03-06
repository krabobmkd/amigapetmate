#include "pmmenu.h"
#include "pmaction.h"
#include "pmlocale.h"
#include "compilers.h"

#include <stdio.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <libraries/gadtools.h>

extern struct Library *GadToolsBase;
void cleanexit(const char *pmessage);

/*
 * Menu template.
 * nm_Label = NULL for items whose label comes from the action name (set by resolveMenuLabels).
 * nm_Label = NM_BARLABEL for separator bars.
 * nm_UserData = (APTR)MSG_* for NM_TITLE entries (localized title).
 * nm_UserData = (APTR)ACTION_* for NM_ITEM entries (action to execute).
 * nm_CommKey = keyboard shortcut character string, or 0 for none.
 *
 * Note: These labels start as NULL and are filled by resolveMenuLabels().
 * The template is modified in-place, so it must NOT be const.
 */
static struct NewMenu menuTemplate[] = {
    /* - - - Project - - - */
    {NM_TITLE, NULL, 0,   0, 0, (APTR)MSG_MENU_PROJECT},
        {NM_ITEM, NULL, "N", 0, 0, (APTR)ACTION_PROJECT_NEW},
        {NM_ITEM, NULL, "O", 0, 0, (APTR)ACTION_PROJECT_OPEN},
        {NM_ITEM, NULL, "S", 0, 0, (APTR)ACTION_PROJECT_SAVE},
        {NM_ITEM, NULL, "A", 0, 0, (APTR)ACTION_PROJECT_SAVEAS},
        {NM_ITEM, NM_BARLABEL, 0, 0, 0, NULL},
        {NM_ITEM, NULL, 0,   0, 0, (APTR)ACTION_PROJECT_ABOUT},
        {NM_ITEM, NM_BARLABEL, 0, 0, 0, NULL},
        {NM_ITEM, NULL, "Q", 0, 0, (APTR)ACTION_PROJECT_QUIT},

    /* - - - Edit - - - */
    {NM_TITLE, NULL, 0,   0, 0, (APTR)MSG_MENU_EDIT},
        {NM_ITEM, NULL, "Z", 0, 0, (APTR)ACTION_EDIT_UNDO},
        {NM_ITEM, NULL, "Y", 0, 0, (APTR)ACTION_EDIT_REDO},
        {NM_ITEM, NM_BARLABEL, 0, 0, 0, NULL},
        {NM_ITEM, NULL, 0, 0, 0, (APTR)ACTION_EDIT_COPY_SCREEN},
        {NM_ITEM, NULL, 0, 0, 0, (APTR)ACTION_EDIT_PASTE_SCREEN},
        {NM_ITEM, NM_BARLABEL, 0, 0, 0, NULL},
        {NM_ITEM, NULL, 0, 0, 0, (APTR)ACTION_EDIT_CLEAR_SCREEN},
        {NM_ITEM, NM_BARLABEL, 0, 0, 0, NULL},
        {NM_ITEM, NULL, 0, 0, 0, (APTR)ACTION_EDIT_SHIFT_LEFT},
        {NM_ITEM, NULL, 0, 0, 0, (APTR)ACTION_EDIT_SHIFT_RIGHT},
        {NM_ITEM, NULL, 0, 0, 0, (APTR)ACTION_EDIT_SHIFT_UP},
        {NM_ITEM, NULL, 0, 0, 0, (APTR)ACTION_EDIT_SHIFT_DOWN},

    /* - - - Screen - - - */
    {NM_TITLE, NULL, 0, 0, 0, (APTR)MSG_MENU_SCREEN},
        {NM_ITEM, NULL, 0, 0, 0, (APTR)ACTION_SCREEN_ADD},
        {NM_ITEM, NULL, 0, 0, 0, (APTR)ACTION_SCREEN_CLONE},
        {NM_ITEM, NM_BARLABEL, 0, 0, 0, NULL},
        {NM_ITEM, NULL, 0, 0, 0, (APTR)ACTION_SCREEN_REMOVE},
        {NM_ITEM, NM_BARLABEL, 0, 0, 0, NULL},
        {NM_ITEM, NULL, "[", 0, 0, (APTR)ACTION_SCREEN_PREV},
        {NM_ITEM, NULL, "]", 0, 0, (APTR)ACTION_SCREEN_NEXT},

    /* - - - View - - - */
    {NM_TITLE, NULL, 0, 0, 0, (APTR)MSG_MENU_VIEW},
        {NM_ITEM, NULL, "G", 0, 0, (APTR)ACTION_VIEW_TOGGLE_GRID},
        {NM_ITEM, NM_BARLABEL, 0, 0, 0, NULL},
        {NM_ITEM, NULL, 0, 0, 0, (APTR)ACTION_VIEW_CHARSET_UPPER},
        {NM_ITEM, NULL, 0, 0, 0, (APTR)ACTION_VIEW_CHARSET_LOWER},

    /* - - - Brush - - - */
    {NM_TITLE, NULL, 0, 0, 0, (APTR)MSG_MENU_BRUSH},
        {NM_ITEM, NULL, 0, 0, 0, (APTR)ACTION_BRUSH_FLIP_X},
        {NM_ITEM, NULL, 0, 0, 0, (APTR)ACTION_BRUSH_FLIP_Y},
        {NM_ITEM, NM_BARLABEL, 0, 0, 0, NULL},
        {NM_ITEM, NULL, 0, 0, 0, (APTR)ACTION_BRUSH_ROT90CW},
        {NM_ITEM, NULL, 0, 0, 0, (APTR)ACTION_BRUSH_ROT180},
        {NM_ITEM, NULL, 0, 0, 0, (APTR)ACTION_BRUSH_ROT90CCW},

    /* - - - Palette - - - */
    {NM_TITLE, NULL, 0, 0, 0, (APTR)MSG_MENU_PALETTE},
        {NM_ITEM, NULL, 0, 0, 0, (APTR)ACTION_PALETTE_PETMATE},
        {NM_ITEM, NULL, 0, 0, 0, (APTR)ACTION_PALETTE_COLODORE},
        {NM_ITEM, NULL, 0, 0, 0, (APTR)ACTION_PALETTE_PEPTO},
        {NM_ITEM, NULL, 0, 0, 0, (APTR)ACTION_PALETTE_VICE},

    {NM_END, NULL, 0, 0, 0, NULL}
};

/*
 * Fill in nm_Label fields from locale (titles) and action names (items).
 * Separator bars (nm_Label == NM_BARLABEL) are left unchanged.
 * Call this after PmAction_Init() so action->name pointers are valid.
 */
static void resolveMenuLabels(struct NewMenu *tmpl)
{
    int i;

    for (i = 0; tmpl[i].nm_Type != NM_END; i++) {
        /* Skip bars and items whose label is already set */
        if (tmpl[i].nm_Label == NM_BARLABEL) continue;
        if (tmpl[i].nm_Label != NULL) continue;

        if (tmpl[i].nm_Type == NM_TITLE) {
            /* UserData holds a MSG_* string ID */
            ULONG msgID = (ULONG)tmpl[i].nm_UserData;
            tmpl[i].nm_Label = (STRPTR)LOC(msgID);
        } else {
            /* UserData holds an ACTION_* ID */
            ULONG actionID = (ULONG)tmpl[i].nm_UserData;
            PmAction *action = PmAction_Get(actionID);
            if (action && action->name) {
                tmpl[i].nm_Label = (STRPTR)action->name;
            } else {
                tmpl[i].nm_Label = (STRPTR)"???";
            }
        }
    }
}

BOOL PmMenu_Create(PmMenu *pm, struct Screen *screen, struct Window *window)
{
    if (!pm || !screen || !window || !GadToolsBase) return FALSE;

    pm->visualInfo = GetVisualInfo(screen, TAG_END);
    if (!pm->visualInfo) {
        cleanexit("Failed to get visual info for menus\n");
        return FALSE;
    }

    resolveMenuLabels(menuTemplate);

    pm->menu = CreateMenus(menuTemplate, TAG_END);
    if (!pm->menu) {
        cleanexit("Failed to create menus\n");
        FreeVisualInfo(pm->visualInfo);
        pm->visualInfo = NULL;
        return FALSE;
    }

    if (!LayoutMenus(pm->menu, pm->visualInfo,
                     GTMN_NewLookMenus, TRUE,
                     TAG_END)) {
        cleanexit("Failed to layout menus\n");
        FreeMenus(pm->menu);
        FreeVisualInfo(pm->visualInfo);
        pm->menu = NULL;
        pm->visualInfo = NULL;
        return FALSE;
    }

    SetMenuStrip(window, pm->menu);
    return TRUE;
}

void PmMenu_Close(PmMenu *pm, struct Window *window)
{
    if (!pm) return;

    if (window && pm->menu) {
        ClearMenuStrip(window);
    }

    if (pm->menu) {
        FreeMenus(pm->menu);
        pm->menu = NULL;
    }

    if (pm->visualInfo) {
        FreeVisualInfo(pm->visualInfo);
        pm->visualInfo = NULL;
    }
}

LONG PmMenu_ToActionID(PmMenu *pm, UWORD menuCode)
{
    struct MenuItem *item;

    if (!pm || !pm->menu) return -1;
    if (menuCode == MENUNULL) return -1;

    item = ItemAddress(pm->menu, menuCode);
    if (!item) return -1;

    return (LONG)GTMENUITEM_USERDATA(item);
}
