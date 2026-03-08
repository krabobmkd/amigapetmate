#include "pmlocale.h"
#include "compilers.h"

#include <stdio.h>
#include <proto/locale.h>
#include <libraries/locale.h>

/*
 * Default English strings - MUST stay in the same order as the MSG_* enum.
 * C89: no designated initializers, sequential only.
 */
static const char *defaultStrings[MSG_COUNT] = {
    /* MSG_WINDOW_TITLE = 0 */
    "PetMate",
    /* MSG_ABOUT_TITLE */
    "About PetMate",
    /* MSG_ABOUT_TEXT */
    "PetMate - C64 PETSCII Art Editor\nAmiga port v0.1",

    /* MSG_MENU_PROJECT */
    "Project",
    /* MSG_FILE_NEW */
    "New",
    /* MSG_FILE_OPEN */
    "Open...",
    /* MSG_FILE_SAVEAS */
    "Save As...",
    /* MSG_FILE_SAVE */
    "Save",
    /* MSG_MENU_ABOUT */
    "About",
    /* MSG_MENU_QUIT */
    "Quit",

    /* MSG_MENU_EDIT */
    "Edit",
    /* MSG_EDIT_UNDO */
    "Undo",
    /* MSG_EDIT_REDO */
    "Redo",
    /* MSG_EDIT_COPY_SCREEN */
    "Copy Screen",
    /* MSG_EDIT_PASTE_SCREEN */
    "Paste Screen",
    /* MSG_EDIT_CLEAR_SCREEN */
    "Clear Screen",
    /* MSG_EDIT_SHIFT_LEFT */
    "Shift Left",
    /* MSG_EDIT_SHIFT_RIGHT */
    "Shift Right",
    /* MSG_EDIT_SHIFT_UP */
    "Shift Up",
    /* MSG_EDIT_SHIFT_DOWN */
    "Shift Down",

    /* MSG_MENU_SCREEN */
    "Screen",
    /* MSG_SCREEN_ADD */
    "Add Screen",
    /* MSG_SCREEN_CLONE */
    "Clone Screen",
    /* MSG_SCREEN_REMOVE */
    "Remove Screen",
    /* MSG_SCREEN_PREV */
    "Previous Screen",
    /* MSG_SCREEN_NEXT */
    "Next Screen",

    /* MSG_MENU_VIEW */
    "View",
    /* MSG_VIEW_TOGGLE_GRID */
    "Toggle Grid",
    /* MSG_VIEW_CHARSET_UPPER */
    "Uppercase Charset",
    /* MSG_VIEW_CHARSET_LOWER */
    "Lowercase Charset",

    /* MSG_TOGGLE_FULLSCREEN */
    "Toggle Full Screen",

    /* MSG_MENU_PALETTE */
    "Palette",
    /* MSG_PALETTE_PETMATE */
    "PetMate",
    /* MSG_PALETTE_COLODORE */
    "Colodore",
    /* MSG_PALETTE_PEPTO */
    "Pepto",
    /* MSG_PALETTE_VICE */
    "VICE",

    /* MSG_MENU_BRUSH */
    "Brush",
    /* MSG_BRUSH_FLIP_X */
    "Flip Horizontally",
    /* MSG_BRUSH_FLIP_Y */
    "Flip Vertically",
    /* MSG_BRUSH_ROT90CW */
    "Rotate 90\xb0 CW",
    /* MSG_BRUSH_ROT180 */
    "Rotate 180\xb0",
    /* MSG_BRUSH_ROT90CCW */
    "Rotate 90\xb0 CCW",

    "Draw Color",
    "Background Color",
    "Border Color",
     /* MSG_LABEL_DRAWCOLOR,
      MSG_LABEL_BACKGROUNDCOLOR,
      MSG_LABEL_BORDERCOLOR,*/

    /* MSG_STATUS_READY */
    "Ready",
    /* MSG_STATUS_LOADING */
    "Loading...",
    /* MSG_STATUS_SAVING */
    "Saving...",
    /* MSG_STATUS_ERROR */
    "Error",

    /* MSG_ERROR_OPENFILE */
    "Cannot open file",
    /* MSG_ERROR_SAVEFILE */
    "Cannot save file",
    /* MSG_ERROR_NOMEMORY */
    "Out of memory",
    /* MSG_ERROR_INVALIDFILE */
    "Invalid file format",

    /* follow enum in petscii_fileio.h, message for status bar */
    "Project read OK.",
    "Cannot open file.",
    "File read error.",
    "JSON parse error.",
    "Unexpected json file format.",
    "Out of memory.",
    "File write error.",

    "File written.",

    /* MSG_TOOL_DRAW */
    "Char & Color",
    /* MSG_TOOL_COLORIZE */
    "Color only",
    /* MSG_TOOL_CHARDRAW */
    "Char only",
    /* MSG_TOOL_BRUSH */
    "Brush",
    /* MSG_TOOL_TEXT */
    "Text",

    /* MSG_BTN_CLEAR */
    "Clear",
    /* MSG_BTN_CHARSET_UPPER */
    "Upper",
    /* MSG_BTN_CHARSET_LOWER */
    "Lower",

    /* MSG_SETTINGS */
    "Settings",

    /* MSG_EXPORT_BAS */
    "Export as BASIC (.bas)...",
    /* MSG_EXPORT_ASM */
    "Export as ASM (.asm)...",
    /* MSG_EXPORT_SEQ */
    "Export as SEQ (.seq)...",

    /* status bar message per tools - keep it same order as enum */
    "Draw char and Color.",
    "Colorize mode.",
    "Draw char mode.",
    "Use the lasso to detour a brush.",
    "Set cursor position and type text."
};

/* LocaleBase declared in petmate.c */
extern struct LocaleBase *LocaleBase;

static struct Catalog *catalog = NULL;

BOOL PmLocale_Init(const char *catalogName, ULONG version)
{
    if (LocaleBase) {
        if (catalogName) {
            catalog = OpenCatalog(NULL, (STRPTR)catalogName,
                                  OC_Version, version,
                                  OC_BuiltInLanguage, (ULONG)"english",
                                  TAG_DONE);
            if (!catalog) {
                printf("Warning: Could not open catalog '%s', using defaults\n",
                       catalogName);
            }
        }
    } else {
        printf("locale.library not found, using default strings\n");
    }
    return TRUE;
}

void PmLocale_Close(void)
{
    if (catalog) {
        CloseCatalog(catalog);
        catalog = NULL;
    }
}

const char *PmLocale_GetString(ULONG stringID)
{
    if (stringID >= MSG_COUNT) return "???";

    if (LocaleBase && catalog) {
        return GetCatalogStr(catalog, stringID,
                             (STRPTR)defaultStrings[stringID]);
    }
    return defaultStrings[stringID];
}
