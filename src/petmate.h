#ifndef PETMATE_H
#define PETMATE_H

/*
 * Petmate Amiga - C64 PETSCII Art Editor
 * Main application header: App struct and tool state.
 */

#include <exec/types.h>
#include <exec/ports.h>
#include <intuition/classusr.h>

#include "petscii_types.h"
#include "petscii_project.h"
#include "petscii_style.h"
#include "pmaction.h"
#include "petscii_undo.h"
#include "appsettings.h"
#include "boopsimainwindow.h"
#include "pmsettingsview.h"
#include "pmtoolbar.h"
#include "pmscreentabs.h"

/* Current tool state */

typedef struct ToolState {
    UBYTE currentTool;      /* TOOL_DRAW, TOOL_COLORIZE, etc. */
    UBYTE lastDrawTool;         /* used for fallback if some tool end */
    UBYTE selectedChar;     /* 0-255 screen code */
    UBYTE fgColor;          /* 0-15 foreground color */
    UBYTE bgColor;          /* 0-15 background color */
    UBYTE bdColor;          /* 0-15 border color */
    UBYTE showGrid;         /* grid overlay toggle */
    UBYTE ww;
} ToolState;

/* Application struct */
struct App {
    Object *window_obj;
    struct MsgPort *app_port;

    BoopsiMainWindow mainwindow;

    PmSettingsView settingsView; /* Project Settings window */

    Object *mainvlayout;

    /* Status bar */
    Object *statusBarLayout;
    Object *statusBarLabel;

    /* Application state */
    PetsciiProject *project;
    ToolState toolState;

    /* Phase 2 */
    PetsciiStyle      style;       /* 16 C64 color pens */
    /*moved to BoopsiMainWindow  PmMenu            menu;  */      /* GadTools menus */
    PmActionContext   actionCtx;   /* Context passed to all actions */


    /* ... */
//    Object          *bgColorWatch;
//    Object          *borderColorWatch;
    /* Phase 4 */
    Object           *canvasGadget;        /* PetsciiCanvas BOOPSI instance  */

    /* Phase 5 */
    Object           *charSelectorGadget;  /* CharSelector BOOPSI instance   */
    Object           *currentCharLabel;

    Object           *colorPickerFgGadget; /* ColorPicker (fg) BOOPSI instance */
    //old Object           *colorPickerBgGadget; /* ColorPicker (bg) BOOPSI instance */

    Object           *colorPickerPopUp;
    Object           *colorPickerPopUpLabel;
    Object           *colorPickerPopUpLayout;

    /* Phase 7 */
    PmToolbar         toolbar;             /* left-side tool button panel    */
    PmScreenTabs      screenTabs;          /* top screen-tab button bar      */
    Object           *charsetUpperBtn;     /* charset "Upper" toggle button  */
    Object           *charsetLowerBtn;     /* charset "Lower" toggle button  */


    Object *aboutRequester; /* allocated once when asked first */

    /* Phase 8 */
    PetsciiUndoBuffer *undoBufs[PETSCII_MAX_SCREENS]; /* one per screen slot */

    /* Application-level settings (temp dir, recent files) */
    AppSettings appSettings;

    /* for some label */
    char selectedCharLabelText[32];
};

extern struct App *app;

#endif /* PETMATE_H */
