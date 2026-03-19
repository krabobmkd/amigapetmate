#ifndef PmHelpView_H
#define PmHelpView_H

/*
 * pmhelpview.h - Help window for Petmate Amiga.
 *
 * Displays PetMate.guide using the AmigaGuide datatype embedded as a
 * BOOPSI gadget inside a ReAction layout window.
 *
 * If datatypes.library is absent or the guide file is not found, the
 * window opens with a plain "not found" message instead.
 *
 * C89 compatible.
 */

#include <exec/types.h>
#include <intuition/classusr.h>
#include <intuition/intuition.h>

typedef struct PmHelpView
{
    Object         *windowObj;       /* BOOPSI window object (persistent) */
    struct Window  *window;          /* Intuition window (NULL when hidden) */

    Object         *mainLayout;      /* top-level layout                   */
    Object         *dtObj;           /* AmigaGuide datatype object, or NULL */

} PmHelpView;

/* Gadget ID for the datatype object */
#define GAD_HELP_DT     200

/*
 * Initialize the Help window.
 * guidePath: path to the AmigaGuide file (e.g. "PetMate.guide").
 * Creates the BOOPSI window object but does not open it.
 * Returns TRUE even if the guide file was not found (shows fallback msg).
 * Returns FALSE only if the window itself could not be created.
 */
BOOL  PmHelpView_Init(PmHelpView *phv, const char *title, const char *guidePath);

/* Open the Help window on CurrentMainScreen. No-op if already open. */
void  PmHelpView_Open(PmHelpView *phv);

/* Close (hide) the Help window. No-op if already closed. */
void  PmHelpView_Close(PmHelpView *phv);

/* Handle input messages from this window. Call when its signal fires. */
BOOL  PmHelpView_HandleInput(PmHelpView *phv);

/* Signal bit mask to OR into Wait(). Returns 0 when window is closed. */
ULONG PmHelpView_GetSignalMask(PmHelpView *phv);

/* Dispose all resources (closes window first if open). */
void  PmHelpView_Dispose(PmHelpView *phv);

#endif /* PmHelpView_H */
