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
    void *screen;               /* struct Screen * - for style re-apply */
    void *clipScreen;           /* PetsciiScreen ** - clipboard for copy/paste */
    void *undoBufs;             /* PetsciiUndoBuffer *[PETSCII_MAX_SCREENS]  */
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
    ACTION_PROJECT_ABOUT,
    ACTION_PROJECT_QUIT,

    /* Edit */
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

    /* Palette */
    ACTION_PALETTE_PETMATE,
    ACTION_PALETTE_COLODORE,
    ACTION_PALETTE_PEPTO,
    ACTION_PALETTE_VICE,

    /* Must be last */
    ACTION_COUNT
};

/* Initialize action system: localizes action names from MSG_* strings */
void      PmAction_Init(void);

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

#endif /* PMACTION_H */
