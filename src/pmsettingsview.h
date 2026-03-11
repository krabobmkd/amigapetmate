#ifndef PmSettingsView_H
#define PmSettingsView_H

/*
 * pmsettingsview.h - Settings window for Petmate Amiga.
 *
 * Contains one group: "Fullscreen Display Mode":
 *   - Checkbox "Use Workbench screen mode"
 *   - Read-only display of the current screen ModeId (hex)
 *   - "Choose..." button that opens an ASL screen mode requester
 *   - Read-only display of the mode description from graphics.library
 *
 * When closed via the close gadget, the window is hidden (not destroyed).
 * Call PmSettingsView_GetModeId / PmSettingsView_GetUseWorkbench to read
 * back current values before or after closing.
 *
 * C89 compatible.
 */

#include <exec/types.h>
#include <intuition/classusr.h>
#include <intuition/intuition.h>

typedef struct PmSettingsView
{
    Object         *windowObj;           /* BOOPSI window object (persistent) */
    struct Window  *window;              /* Intuition window (NULL when hidden) */

    Object         *mainLayout;          /* top-level layout                   */
    Object         *screenLayout;        /* "Fullscreen Display Mode" group     */

    Object         *useWbCheck;          /* checkbox: Use Workbench mode        */
    Object         *modeIdDisplay;       /* read-only display: hex ModeId       */
    Object         *chooseModeBtn;       /* "Choose..." button                  */
    Object         *modeDescDisplay;     /* read-only display: mode description */

    char            modeIdHexStr[16];    /* "0xXXXXXXXX\0"                     */
    char            modeDescStr[80];     /* mode name from graphics.library     */

    ULONG           currentModeId;       /* current screen mode ID              */
    BOOL            useWorkbench;        /* TRUE = use WB display mode          */

} PmSettingsView;

/* Gadget IDs for this window */
#define GAD_SETTINGS_USEWB       100
#define GAD_SETTINGS_CHOOSEMODE  101

/*
 * Initialize the Settings window.
 * Creates the BOOPSI window object but does not open it.
 * Defaults: useWorkbench=TRUE, currentModeId=INVALID_ID (0xFFFFFFFF).
 */
BOOL  PmSettingsView_Init(PmSettingsView *psv, const char *title);

/* Open the Settings window on CurrentMainScreen. No-op if already open. */
void  PmSettingsView_Open(PmSettingsView *psv);

/* Close (hide) the Settings window. No-op if already closed. */
void  PmSettingsView_Close(PmSettingsView *psv);

/* Handle input messages from this window. Call when its signal fires. */
BOOL  PmSettingsView_HandleInput(PmSettingsView *psv);

/* Signal bit mask to OR into Wait(). Returns 0 when window is closed. */
ULONG PmSettingsView_GetSignalMask(PmSettingsView *psv);

/* Get/set the "Use Workbench mode" checkbox state. */
BOOL  PmSettingsView_GetUseWorkbench(PmSettingsView *psv);
void  PmSettingsView_SetUseWorkbench(PmSettingsView *psv, BOOL useWb);

/* Get/set the screen ModeId. INVALID_ID (0xFFFFFFFF) = not configured. */
ULONG PmSettingsView_GetModeId(PmSettingsView *psv);
void  PmSettingsView_SetModeId(PmSettingsView *psv, ULONG modeId);

/* Dispose all resources (closes window first if open). */
void  PmSettingsView_Dispose(PmSettingsView *psv);

#endif /* PmSettingsView_H */
