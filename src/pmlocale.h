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
    MSG_MENU_ICONIFY,
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

    MSG_SCREEN_ADD_ERROR,
    MSG_SCREEN_D,

    /* Menu: View */
    MSG_MENU_VIEW,
    MSG_VIEW_TOGGLE_GRID,
    MSG_VIEW_CHARSET_UPPER,
    MSG_VIEW_CHARSET_LOWER,

    MSG_TOGGLE_FULLSCREEN,

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

    /* follow enum of petscii_fileio.h */
    MSG_PETSCII_FILEIO_OK,
    MSG_PETSCII_FILEIO_EOPEN,
    MSG_PETSCII_FILEIO_EREAD,
    MSG_PETSCII_FILEIO_EPARSE,
    MSG_PETSCII_FILEIO_EFORMAT,
    MSG_PETSCII_FILEIO_EALLOC,
    MSG_PETSCII_FILEIO_EWRITE,
    MSG_PETSCII_FILEIO_NEED_DT,
    MSG_PETSCII_FILEIO_WRITEOK,

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

    /* Export actions */
    MSG_EXPORT_BAS,
    MSG_EXPORT_ASM,
    MSG_EXPORT_SEQ,

    MSG_EXPORT_PRG_BAS,
    MSG_EXPORT_PRG_ASM,

    MSG_EXPORT_IFF,
    MSG_EXPORT_GIF,
    MSG_EXPORT_PNG,

    /* Import actions */
    MSG_IMPORT_IMAGE,
    MSG_IMPORT_NOMATCH,

    /* Menu: Generate */
    MSG_MENU_GENERATE,
    MSG_GENERATE_RANDOM_BRUSH,
    MSG_GENERATE_MAGIC_LINE,
    MSG_GENERATE_TRON_LINES,

    /* status bar messages */
    MSG_STATUS_DRAW,
    MSG_STATUS_COLORIZE,
    MSG_STATUS_CHARDRAW,
    MSG_STATUS_START_LASSO,
    MSG_STATUS_TEXT,

    /* Settings window - UI Background group */
    MSG_SETTINGS_UI_BG_GROUP,     /* group label: "UI Background"              */
    MSG_SETTINGS_USEONECLORBG,    /* checkbox:    "Use one color for background"*/
    MSG_SETTINGS_BGIMAGE_LABEL,   /* row label:   "Background Image:"           */
    MSG_SETTINGS_CHOOSE_BGIMAGE,  /* button:      "Choose..."                  */
    MSG_SETTINGS_BGIMAGE_TITLE,   /* ASL title:   "Choose background image"    */
    MSG_SETTINGS_BGIMAGE_NONE,    /* placeholder: "(none)"                     */
    MSG_SETTINGS_REMOVEBGIMAGE,   /* button:      "Remove"                     */

    /* Recent files submenu */
    MSG_MENU_OPEN_RECENT,             /* "Open Recent" parent item label       */

    MSG_SETTINGS_FSUSEWBMODE,
    MSG_SETTINGS_CHOOSEDOTS,
    MSG_SETTINGS_SCREENMODECL,
    MSG_SETTINGS_DESCRIPTIONCL,
    MSG_SETTINGS_FULLSCREENDISPLAYMODE,

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
