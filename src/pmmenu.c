#include "pmmenu.h"
#include "pmaction.h"
#include "pmlocale.h"
#include "appsettings.h"
#include "compilers.h"

#include <stdio.h>
#include <string.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <libraries/gadtools.h>
#include "PetsciiCanvas/petscii_canvas.h"  /* PCA_Brush, PCA_BrushRemoved */
#include "petmate.h"

extern struct Library *GadToolsBase;
void cleanexit(const char *pmessage);

/*
 * Dynamic menu template.
 *
 * The template is rebuilt from scratch before each CreateMenus() call so the
 * "Open Recent" sub-menu always reflects the current recent-file list.
 *
 * nm_Label = NULL  -> resolveMenuLabels() fills from action name or MSG_* id.
 * nm_Label = NM_BARLABEL -> separator bar, left unchanged.
 * nm_Label = any other non-NULL ptr -> already set, left unchanged.
 *
 * nm_UserData = (APTR)MSG_* for NM_TITLE entries.
 * nm_UserData = (APTR)ACTION_* for item/sub entries that dispatch an action.
 * nm_UserData = NULL for parent-only items (Export As, Import, Open Recent).
 */

#define MENU_TEMPLATE_MAX 80   /* enough for all static entries + 4 recent */

static struct NewMenu s_menuTemplate[MENU_TEMPLATE_MAX];

/* Label buffers for the dynamically generated recent-file sub-items */
static char s_openRecentLabel[32];
static char s_recentLabels[APPSETTINGS_MAX_RECENT][40];

/* -------------------------------------------------------------------------
 * addEntry - append one NewMenu entry to s_menuTemplate
 * -------------------------------------------------------------------------*/
static int s_n; /* running index into s_menuTemplate while building */

static void addEntry(UBYTE type, STRPTR label, STRPTR key,
                     UWORD flags, LONG mutex, ULONG udata)
{
    struct NewMenu *e = &s_menuTemplate[s_n++];
    e->nm_Type          = type;
    e->nm_Label         = label;
    e->nm_CommKey       = key;
    e->nm_Flags         = flags;
    e->nm_MutualExclude = mutex;
    e->nm_UserData      = (APTR)udata;
}

/* Convenience shorthands */
#define ADD(t,l,k,f,m,u)  addEntry((t),(STRPTR)(l),(STRPTR)(k),(f),(m),(ULONG)(u))
#define BAR(type)          ADD((type), NM_BARLABEL, 0, 0, 0, 0)

/* -------------------------------------------------------------------------
 * buildMenuTemplate - construct s_menuTemplate from scratch
 * Inserts "Open Recent" sub-menu when as->recentCount > 0.
 * -------------------------------------------------------------------------*/
static void buildMenuTemplate(const AppSettings *as)
{
    int r;
    const char *fp;

    s_n = 0;

    /* - - - Project - - - */
    ADD(NM_TITLE, NULL, 0,    0, 0, MSG_MENU_PROJECT);
    ADD(NM_ITEM,  NULL, "N",  0, 0, ACTION_PROJECT_NEW);
    ADD(NM_ITEM,  NULL, "O",  0, 0, ACTION_PROJECT_OPEN);
    ADD(NM_ITEM,  NULL, "S",  0, 0, ACTION_PROJECT_SAVE);
    ADD(NM_ITEM,  NULL, "A",  0, 0, ACTION_PROJECT_SAVEAS);

    /* Open Recent sub-menu - only when there are recent files */
    if (as && as->recentCount > 0) {
        strncpy(s_openRecentLabel, LOC(MSG_MENU_OPEN_RECENT),
                sizeof(s_openRecentLabel) - 1);
        s_openRecentLabel[sizeof(s_openRecentLabel) - 1] = '\0';

        ADD(NM_ITEM, s_openRecentLabel, 0, 0, 0, 0);

        for (r = 0; r < as->recentCount && r < APPSETTINGS_MAX_RECENT; r++) {
            fp = FilePart((STRPTR)as->recentFiles[r]);
            strncpy(s_recentLabels[r], fp ? fp : as->recentFiles[r],
                    sizeof(s_recentLabels[r]) - 1);
            s_recentLabels[r][sizeof(s_recentLabels[r]) - 1] = '\0';
            ADD(NM_SUB, s_recentLabels[r], 0, 0, 0,
                ACTION_OPEN_RECENT_0 + (ULONG)r);
        }
    }

    BAR(NM_ITEM);
    ADD(NM_ITEM, (STRPTR)"Export As", 0, 0, 0, 0);
        ADD(NM_SUB, NULL, 0, 0, 0, ACTION_EXPORT_IFF_ILBM);
        ADD(NM_SUB, NULL, 0, 0, 0, ACTION_EXPORT_GIF);
        BAR(NM_SUB);
        ADD(NM_SUB, NULL, 0, 0, 0, ACTION_EXPORT_BAS);
        ADD(NM_SUB, NULL, 0, 0, 0, ACTION_EXPORT_ASM);
        ADD(NM_SUB, NULL, 0, 0, 0, ACTION_EXPORT_SEQ);
    ADD(NM_ITEM, (STRPTR)"Import", 0, 0, 0, 0);
        ADD(NM_SUB, NULL, 0, 0, 0, ACTION_IMPORT_IMAGE);
    BAR(NM_ITEM);
    ADD(NM_ITEM, NULL, 0,   0, 0, ACTION_PROJECT_ICONIFY);
    ADD(NM_ITEM, NULL, 0,   0, 0, ACTION_PROJECT_ABOUT);
    BAR(NM_ITEM);
    ADD(NM_ITEM, NULL, "Q", 0, 0, ACTION_PROJECT_QUIT);

    /* - - - Edit - - - */
    ADD(NM_TITLE, NULL, 0,     0, 0, MSG_MENU_EDIT);
    ADD(NM_ITEM,  NULL, "F1",  NM_COMMANDSTRING, 0, ACTION_EDIT_DRAW1);
    ADD(NM_ITEM,  NULL, "F2",  NM_COMMANDSTRING, 0, ACTION_EDIT_DRAW2);
    ADD(NM_ITEM,  NULL, "F3",  NM_COMMANDSTRING, 0, ACTION_EDIT_DRAW3);
    ADD(NM_ITEM,  NULL, "F4",  NM_COMMANDSTRING, 0, ACTION_EDIT_DRAW4);
    ADD(NM_ITEM,  NULL, "F5",  NM_COMMANDSTRING, 0, ACTION_EDIT_DRAW5);
    BAR(NM_ITEM);
    ADD(NM_ITEM, NULL, "Z", 0, 0, ACTION_EDIT_UNDO);
    ADD(NM_ITEM, NULL, "Y", 0, 0, ACTION_EDIT_REDO);
    BAR(NM_ITEM);
    ADD(NM_ITEM, NULL, 0, 0, 0, ACTION_EDIT_COPY_SCREEN);
    ADD(NM_ITEM, NULL, 0, 0, 0, ACTION_EDIT_PASTE_SCREEN);
    BAR(NM_ITEM);
    ADD(NM_ITEM, NULL, 0, 0, 0, ACTION_EDIT_CLEAR_SCREEN);
    BAR(NM_ITEM);
    ADD(NM_ITEM, NULL, 0, 0, 0, ACTION_EDIT_SHIFT_LEFT);
    ADD(NM_ITEM, NULL, 0, 0, 0, ACTION_EDIT_SHIFT_RIGHT);
    ADD(NM_ITEM, NULL, 0, 0, 0, ACTION_EDIT_SHIFT_UP);
    ADD(NM_ITEM, NULL, 0, 0, 0, ACTION_EDIT_SHIFT_DOWN);

    /* - - - Screen - - - */
    ADD(NM_TITLE, NULL, 0, 0, 0, MSG_MENU_SCREEN);
    ADD(NM_ITEM,  NULL, 0, 0, 0, ACTION_SCREEN_ADD);
    ADD(NM_ITEM,  NULL, 0, 0, 0, ACTION_SCREEN_CLONE);
    BAR(NM_ITEM);
    ADD(NM_ITEM, NULL, 0,   0, 0, ACTION_SCREEN_REMOVE);
    BAR(NM_ITEM);
    ADD(NM_ITEM, NULL, "[", 0, 0, ACTION_SCREEN_PREV);
    ADD(NM_ITEM, NULL, "]", 0, 0, ACTION_SCREEN_NEXT);

    /* - - - View - - - */
    ADD(NM_TITLE, NULL, 0,     0, 0, MSG_MENU_VIEW);
    ADD(NM_ITEM,  NULL, "G",   0, 0, ACTION_VIEW_TOGGLE_GRID);
    BAR(NM_ITEM);
    ADD(NM_ITEM, NULL, 0, 0, 0, ACTION_VIEW_CHARSET_UPPER);
    ADD(NM_ITEM, NULL, 0, 0, 0, ACTION_VIEW_CHARSET_LOWER);
    BAR(NM_ITEM);
    ADD(NM_ITEM, NULL, "F10", NM_COMMANDSTRING, 0, ACTION_VIEW_TOGGLE_FULL_SCREEN);
    ADD(NM_ITEM, NULL, "P",   0,                0, ACTION_VIEW_OPEN_SETTINGS);

    /* - - - Generate - - - */
    ADD(NM_TITLE, NULL, 0, 0, 0, (APTR)MSG_MENU_GENERATE);
        ADD(NM_ITEM, NULL, 0, 0, 0, (APTR)ACTION_GENERATE_RANDOM_BRUSH);
        ADD(NM_ITEM, NULL, 0, 0, 0, (APTR)ACTION_GENERATE_MAGIC_LINE);
        ADD(NM_ITEM, NULL, 0, 0, 0, (APTR)ACTION_GENERATE_TRON_LINES);

    /* - - - Brush - - - */
    ADD(NM_TITLE, NULL, 0, 0, 0, MSG_MENU_BRUSH);
    ADD(NM_ITEM,  NULL, 0, 0, 0, ACTION_BRUSH_FLIP_X);
    ADD(NM_ITEM,  NULL, 0, 0, 0, ACTION_BRUSH_FLIP_Y);
    BAR(NM_ITEM);
    ADD(NM_ITEM, NULL, 0, 0, 0, ACTION_BRUSH_ROT90CW);
    ADD(NM_ITEM, NULL, 0, 0, 0, ACTION_BRUSH_ROT180);
    ADD(NM_ITEM, NULL, 0, 0, 0, ACTION_BRUSH_ROT90CCW);

    /* - - - Palette - - - */
    ADD(NM_TITLE, NULL, 0, 0, 0, MSG_MENU_PALETTE);
    ADD(NM_ITEM,  NULL, 0, CHECKIT|CHECKED, 0x0E, ACTION_PALETTE_PETMATE);
    ADD(NM_ITEM,  NULL, 0, CHECKIT,         0x0D, ACTION_PALETTE_COLODORE);
    ADD(NM_ITEM,  NULL, 0, CHECKIT,         0x0B, ACTION_PALETTE_PEPTO);
    ADD(NM_ITEM,  NULL, 0, CHECKIT,         0x07, ACTION_PALETTE_VICE);

    /* - - - End - - - */
    ADD(NM_END, NULL, 0, 0, 0, 0);
}

/*
 * Fill in nm_Label fields from locale (titles) and action names (items).
 * Separator bars and items whose label is already set are left unchanged.
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

BOOL PmMenu_Create(PmMenu *pm, struct Screen *screen, struct Window *window,
                   const AppSettings *as)
{
    if (!pm || !screen || !window || !GadToolsBase) return FALSE;

    pm->visualInfo = GetVisualInfo(screen, TAG_END);
    if (!pm->visualInfo) {
        cleanexit("Failed to get visual info for menus\n");
        return FALSE;
    }

    buildMenuTemplate(as);
    resolveMenuLabels(s_menuTemplate);

    pm->menu = CreateMenus(s_menuTemplate, TAG_END);
    if (!pm->menu) {
        cleanexit("Failed to create menus\n");
        FreeVisualInfo(pm->visualInfo);
        pm->visualInfo = NULL;
        return FALSE;
    }

    PmMenu_UpdateBrushMenu(&app->mainwindow.menu,
        CurrentMainWindow, app->canvasGadget);

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

/*
 * Rebuild the menu from the current recent-file list without reallocating
 * the visual info (screen hasn't changed).  Called after open/save actions.
 */
void PmMenu_Rebuild(PmMenu *pm, struct Screen *screen, struct Window *window,
                    const AppSettings *as)
{
    if (!pm || !window || !pm->visualInfo) return;

    ClearMenuStrip(window);

    if (pm->menu) {
        FreeMenus(pm->menu);
        pm->menu = NULL;
    }

    buildMenuTemplate(as);
    resolveMenuLabels(s_menuTemplate);

    pm->menu = CreateMenus(s_menuTemplate, TAG_END);
    if (!pm->menu) return;

    if (!LayoutMenus(pm->menu, pm->visualInfo,
                     GTMN_NewLookMenus, TRUE,
                     TAG_END)) {
        FreeMenus(pm->menu);
        pm->menu = NULL;
        return;
    }

    SetMenuStrip(window, pm->menu);
}

/*
 * Update the checkmark on the palette sub-menu to reflect paletteID.
 * paletteID 0..3 maps to ACTION_PALETTE_PETMATE..ACTION_PALETTE_VICE.
 * Must be called while the menu is attached to a window.
 */
void PmMenu_UpdatePaletteCheck(PmMenu *pm, struct Window *window, UBYTE paletteID)
{
    struct Menu     *m;
    struct MenuItem *item;
    static const ULONG paletteActions[4] = {
        ACTION_PALETTE_PETMATE,
        ACTION_PALETTE_COLODORE,
        ACTION_PALETTE_PEPTO,
        ACTION_PALETTE_VICE
    };

    if (!pm || !pm->menu || !window) return;

    ClearMenuStrip(window);

    for (m = pm->menu; m; m = m->NextMenu) {
        for (item = m->FirstItem; item; item = item->NextItem) {
            ULONG aid = (ULONG)GTMENUITEM_USERDATA(item);
            int i;
            for (i = 0; i < 4; i++) {
                if (aid == paletteActions[i]) {
                    if ((UBYTE)i == paletteID)
                        item->Flags |= CHECKED;
                    else
                        item->Flags &= ~CHECKED;
                    break;
                }
            }
        }
    }

    SetMenuStrip(window, pm->menu);
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


void PmMenu_UpdateBrushMenu(PmMenu *pm, struct Window *window, Object *canvasGadget)
{
    struct Menu     *menu;
    struct MenuItem *item;
    ULONG            brushPtr = 0;
    BOOL             hasBrush;
    UWORD            enableFlag;

    if (!pm || !pm->menu || !window || !canvasGadget) return;

    GetAttr(PCA_Brush, canvasGadget, &brushPtr);
    hasBrush    = (brushPtr != 0) ? TRUE : FALSE;
    enableFlag  = hasBrush ? ITEMENABLED : 0;

    /* Locate the Brush menu by finding the first menu whose first item
     * carries ACTION_BRUSH_FLIP_X in its UserData. */
    for (menu = pm->menu; menu; menu = menu->NextMenu) {
        item = menu->FirstItem;
        if (!item) continue;
        if (GTMENUITEM_USERDATA(item) != (APTR)ACTION_BRUSH_FLIP_X) continue;

        /* Found it update menu title and every item atomically.
         * ClearMenuStrip/ResetMenuStrip is required while modifying flags
         * on a menu already attached to a window. */
        ClearMenuStrip(window);

        if (hasBrush)
            menu->Flags |= MENUENABLED;
        else
            menu->Flags &= ~MENUENABLED;

        for (item = menu->FirstItem; item; item = item->NextItem) {
            if (item->ItemFill == (APTR)NM_BARLABEL) continue; /* separator */
            if (hasBrush)
                item->Flags |= ITEMENABLED;
            else
                item->Flags &= ~ITEMENABLED;
        }

        ResetMenuStrip(window, pm->menu);
        return;
    }
}
