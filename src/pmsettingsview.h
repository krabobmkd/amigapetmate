#ifndef PmSettingsView_H
#define PmSettingsView_H

#include <intuition/classusr.h>
#include <intuition/intuition.h>


/*
    Manages the Settings window.
    Contains two groups: "App Settings" and "Project Settings".
    When closed, it hides (does not quit the app).
*/
typedef struct PmSettingsView
{
    Object *windowObj;          /* Window BOOPSI object */
    struct Window *window;      /* Current window (NULL when closed/hidden) */

    /* Main layout */
    Object *mainLayout;

    /* App Settings group */
    Object *appSettingsLayout;
    Object *tempDirGetFile;     /* GetFile gadget for temp directory */
    Object *tempDirLabel;       /* Label for temp dir */

    /* Project Settings group */
    Object *projSettingsLayout;


} PmSettingsView;

/* Gadget IDs for this window */
#define GAD_PROJSETTINGS_TEMPDIR 100

/*
 * Initialize the Settings window.
 * Creates the window object but does not open it.
 */
BOOL PmSettingsView_Init(PmSettingsView *psv,
                              const char *title);

/* Open the Settings window. Does nothing if already open. */
void PmSettingsView_Open(PmSettingsView *psv);

/* Close (hide) the Settings window. */
void PmSettingsView_Close(PmSettingsView *psv);

/* Handle input messages. Call in main loop when window is open. */
BOOL PmSettingsView_HandleInput(PmSettingsView *psv);

/* Get signal bit for waiting on this window. Returns 0 if not open. */
ULONG PmSettingsView_GetSignalMask(PmSettingsView *psv);

/* Get current temp directory path. */
const char *PmSettingsView_GetTempDir(PmSettingsView *psv);

/* Set the temp directory path. */
void PmSettingsView_SetTempDir(PmSettingsView *psv, const char *path);

/* Dispose all resources. */
void PmSettingsView_Dispose(PmSettingsView *psv);

#endif
