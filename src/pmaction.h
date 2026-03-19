#ifndef PMACTION_H
#define PMACTION_H

/*
 * pmaction - Table-driven action dispatch for Petmate Amiga.
 * Pattern adapted from aukboopsi/aukaction.h/.c.
 *
 * Actions are triggered from menu picks, keyboard shortcuts, or buttons.
 * PmActionContext carries all state needed by action functions.
 */

#include <exec/types.h>
#include "petscii_project.h"

/* Forward declarations - avoid heavy header pulls into this header */
typedef struct PmAction PmAction;
typedef struct PmActionContext PmActionContext;

/* Action function signature - returns TRUE on success */
typedef BOOL (*PmActionFunc)(PmActionContext *context);

/* Action context passed to every action function.
 * Pointers are set by petmate.c before dispatching.
 * void* fields are cast to their real types inside pmaction.c. */
struct PmActionContext {
    PetsciiProject **pproject;  /* Pointer to the project pointer */
    void *toolState;            /* ToolState * - drawing state */
    void *style;                /* PetsciiStyle * - C64 color pens */
    void *clipScreen;           /* PetsciiScreen ** - clipboard for copy/paste */
    void *pmenu;                /* PmMenu * - for updating menu checkmarks */
};

/* Action definition */
struct PmAction {
    PmActionFunc func;
    ULONG        nameStringID;  /* MSG_* constant used to localize the name */
    const char  *name;          /* Cached localized name (set by PmAction_Init) */
    WORD         shortcutKey;   /* Raw Amiga keycode for RAWKEY handler, 0 = none */
    UWORD        shortcutQual;  /* IEQUALIFIER_* flags, 0 = no qualifier */
};

/* Action IDs */
enum {
    /* Project */
    ACTION_PROJECT_NEW = 0,
    ACTION_PROJECT_OPEN,
    ACTION_PROJECT_SAVE,
    ACTION_PROJECT_SAVEAS,
    ACTION_PROJECT_ICONIFY,
    ACTION_PROJECT_ABOUT,
    ACTION_PROJECT_HELP,
    ACTION_PROJECT_QUIT,

    /* Edit */
    ACTION_EDIT_DRAW1,
    ACTION_EDIT_DRAW2,
    ACTION_EDIT_DRAW3,
    ACTION_EDIT_DRAW4,
    ACTION_EDIT_DRAW5,

    ACTION_EDIT_UNDO,
    ACTION_EDIT_REDO,
    ACTION_EDIT_COPY_SCREEN,
    ACTION_EDIT_PASTE_SCREEN,
    ACTION_EDIT_CLEAR_SCREEN,
    ACTION_EDIT_SHIFT_LEFT,
    ACTION_EDIT_SHIFT_RIGHT,
    ACTION_EDIT_SHIFT_UP,
    ACTION_EDIT_SHIFT_DOWN,

    /* Screen */
    ACTION_SCREEN_ADD,
    ACTION_SCREEN_CLONE,
    ACTION_SCREEN_REMOVE,
    ACTION_SCREEN_PREV,
    ACTION_SCREEN_NEXT,

    /* View */
    ACTION_VIEW_TOGGLE_GRID,
    ACTION_VIEW_CHARSET_UPPER,
    ACTION_VIEW_CHARSET_LOWER,

    ACTION_VIEW_TOGGLE_FULL_SCREEN,

    ACTION_VIEW_OPEN_SETTINGS,

    /* Palette */
    ACTION_PALETTE_PETMATE,
    ACTION_PALETTE_COLODORE,
    ACTION_PALETTE_PEPTO,
    ACTION_PALETTE_VICE,

    /* Brush transforms */
    ACTION_BRUSH_FLIP_X,
    ACTION_BRUSH_FLIP_Y,
    ACTION_BRUSH_ROT90CW,
    ACTION_BRUSH_ROT180,
    ACTION_BRUSH_ROT90CCW,

    /* Export */
    ACTION_EXPORT_BAS,
    ACTION_EXPORT_ASM,
    ACTION_EXPORT_SEQ,

    ACTION_EXPORT_PRG_BAS,
    ACTION_EXPORT_PRG_ASM,

    ACTION_EXPORT_IFF_ILBM,
    ACTION_EXPORT_GIF,
    ACTION_EXPORT_PNG,

    /* Import */
    ACTION_IMPORT_IMAGE,

    /* Generate */
    ACTION_GENERATE_RANDOM_BRUSH,
    ACTION_GENERATE_MAGIC_LINE,
    ACTION_GENERATE_TRON_LINES,

    /* Recent files (slots 0..APPSETTINGS_MAX_RECENT-1) */
    ACTION_OPEN_RECENT_0,
    ACTION_OPEN_RECENT_1,
    ACTION_OPEN_RECENT_2,
    ACTION_OPEN_RECENT_3,

    /* Must be last */
    ACTION_COUNT
};

/* Initialize action system: localizes action names from MSG_* strings */
void      PmAction_Init(void);

/* Apply palette by ID: updates project, style, and menu checkmark.
 * Exposed so callers (e.g. at startup after loading) can sync state. */
void PmAction_ApplyPalette(PmActionContext *ctx, UBYTE paletteID);

/* Get action by ID (NULL if out of range) */
PmAction *PmAction_Get(ULONG actionID);

/* Execute an action (returns FALSE if actionID invalid or func is NULL) */
BOOL      PmAction_Execute(ULONG actionID, PmActionContext *context);

/* Action function declarations */
BOOL Action_ProjectNew(PmActionContext *ctx);
BOOL Action_ProjectOpen(PmActionContext *ctx);
BOOL Action_ProjectSave(PmActionContext *ctx);
BOOL Action_ProjectSaveAs(PmActionContext *ctx);
BOOL Action_ProjectAbout(PmActionContext *ctx);
BOOL Action_ProjectQuit(PmActionContext *ctx);

BOOL Action_EditUndo(PmActionContext *ctx);
BOOL Action_EditRedo(PmActionContext *ctx);
BOOL Action_EditCopyScreen(PmActionContext *ctx);
BOOL Action_EditPasteScreen(PmActionContext *ctx);
BOOL Action_EditClearScreen(PmActionContext *ctx);
BOOL Action_EditShiftLeft(PmActionContext *ctx);
BOOL Action_EditShiftRight(PmActionContext *ctx);
BOOL Action_EditShiftUp(PmActionContext *ctx);
BOOL Action_EditShiftDown(PmActionContext *ctx);

BOOL Action_ScreenAdd(PmActionContext *ctx);
BOOL Action_ScreenClone(PmActionContext *ctx);
BOOL Action_ScreenRemove(PmActionContext *ctx);
BOOL Action_ScreenPrev(PmActionContext *ctx);
BOOL Action_ScreenNext(PmActionContext *ctx);

BOOL Action_ViewToggleGrid(PmActionContext *ctx);
BOOL Action_ViewCharsetUpper(PmActionContext *ctx);
BOOL Action_ViewCharsetLower(PmActionContext *ctx);

BOOL Action_PalettePetmate(PmActionContext *ctx);
BOOL Action_PaletteColodore(PmActionContext *ctx);
BOOL Action_PalettePepto(PmActionContext *ctx);
BOOL Action_PaletteVice(PmActionContext *ctx);

BOOL Action_BrushFlipX(PmActionContext *ctx);
BOOL Action_BrushFlipY(PmActionContext *ctx);
BOOL Action_BrushRot90CW(PmActionContext *ctx);
BOOL Action_BrushRot180(PmActionContext *ctx);
BOOL Action_BrushRot90CCW(PmActionContext *ctx);

BOOL Action_ExportBAS(PmActionContext *ctx);
BOOL Action_ExportASM(PmActionContext *ctx);
BOOL Action_ExportSEQ(PmActionContext *ctx);

BOOL Action_ExportPrgBAS(PmActionContext *ctx);
BOOL Action_ExportPrgASM(PmActionContext *ctx);

BOOL Action_ExportIFFILBM(PmActionContext *ctx);
BOOL Action_ExportGif(PmActionContext *ctx);
BOOL Action_ExportPng(PmActionContext *ctx);

BOOL Action_ImportImage(PmActionContext *ctx);

BOOL Action_OpenRecent0(PmActionContext *ctx);
BOOL Action_OpenRecent1(PmActionContext *ctx);
BOOL Action_OpenRecent2(PmActionContext *ctx);
BOOL Action_OpenRecent3(PmActionContext *ctx);
BOOL Action_GenerateTronLines(PmActionContext *ctx);

BOOL Action_ProjectHelp(PmActionContext *ctx);

#endif /* PMACTION_H */
