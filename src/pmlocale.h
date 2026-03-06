#ifndef PMLOCALE_H
#define PMLOCALE_H

/*
 * pmlocale - Localization support for Petmate Amiga.
 * Pattern adapted from aukboopsi/auklocale.h/.c.
 *
 * Uses locale.library catalog if available, falls back to built-in
 * English strings. LocaleBase must be declared in petmate.c.
 */

#include <exec/types.h>

/* String IDs for all localizable strings - must stay in this order */
enum {
    /* Window and general */
    MSG_WINDOW_TITLE = 0,
    MSG_ABOUT_TITLE,
    MSG_ABOUT_TEXT,

    /* Menu: Project */
    MSG_MENU_PROJECT,
    MSG_FILE_NEW,
    MSG_FILE_OPEN,
    MSG_FILE_SAVEAS,
    MSG_FILE_SAVE,
    MSG_MENU_ABOUT,
    MSG_MENU_QUIT,

    /* Menu: Edit */
    MSG_MENU_EDIT,
    MSG_EDIT_UNDO,
    MSG_EDIT_REDO,
    MSG_EDIT_COPY_SCREEN,
    MSG_EDIT_PASTE_SCREEN,
    MSG_EDIT_CLEAR_SCREEN,
    MSG_EDIT_SHIFT_LEFT,
    MSG_EDIT_SHIFT_RIGHT,
    MSG_EDIT_SHIFT_UP,
    MSG_EDIT_SHIFT_DOWN,

    /* Menu: Screen */
    MSG_MENU_SCREEN,
    MSG_SCREEN_ADD,
    MSG_SCREEN_CLONE,
    MSG_SCREEN_REMOVE,
    MSG_SCREEN_PREV,
    MSG_SCREEN_NEXT,

    /* Menu: View */
    MSG_MENU_VIEW,
    MSG_VIEW_TOGGLE_GRID,
    MSG_VIEW_CHARSET_UPPER,
    MSG_VIEW_CHARSET_LOWER,

    /* Menu: Palette */
    MSG_MENU_PALETTE,
    MSG_PALETTE_PETMATE,
    MSG_PALETTE_COLODORE,
    MSG_PALETTE_PEPTO,
    MSG_PALETTE_VICE,

    /* Menu: Brush */
    MSG_MENU_BRUSH,
    MSG_BRUSH_FLIP_X,
    MSG_BRUSH_FLIP_Y,
    MSG_BRUSH_ROT90CW,
    MSG_BRUSH_ROT180,
    MSG_BRUSH_ROT90CCW,

     /* Labels */
     MSG_LABEL_DRAWCOLOR,
     MSG_LABEL_BACKGROUNDCOLOR,
     MSG_LABEL_BORDERCOLOR,

    /* Status messages */
    MSG_STATUS_READY,
    MSG_STATUS_LOADING,
    MSG_STATUS_SAVING,
    MSG_STATUS_ERROR,

    /* Error messages */
    MSG_ERROR_OPENFILE,
    MSG_ERROR_SAVEFILE,
    MSG_ERROR_NOMEMORY,
    MSG_ERROR_INVALIDFILE,

    /* Toolbar tool button labels */
    MSG_TOOL_DRAW,
    MSG_TOOL_COLORIZE,
    MSG_TOOL_CHARDRAW,
    MSG_TOOL_BRUSH,
    MSG_TOOL_TEXT,

    /* Short button labels (used on toolbar and in right panel) */
    MSG_BTN_CLEAR,
    MSG_BTN_CHARSET_UPPER,
    MSG_BTN_CHARSET_LOWER,

    /**/
    MSG_SETTINGS,

    /* Must be last */
    MSG_COUNT
};

/* Initialize locale system.
 * catalogName: .catalog file to load, or NULL for English only.
 * version: minimum catalog version required. */
BOOL PmLocale_Init(const char *catalogName, ULONG version);

/* Close locale system - closes catalog. */
void PmLocale_Close(void);

/* Get localized string by ID. Returns built-in English if catalog not loaded. */
const char *PmLocale_GetString(ULONG stringID);

/* Convenience macro */
#define LOC(id) PmLocale_GetString(id)

#endif /* PMLOCALE_H */
