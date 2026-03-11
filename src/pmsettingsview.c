/*
 * pmsettingsview.c - Settings window for Petmate Amiga.
 *
 * Screen mode group:
 *   [ ] Use Workbench screen mode          <- checkbox
 *   Screen Mode: [ 0xXXXXXXXX ] [Choose..]  <- display + ASL requester button
 *   Description: [ PAL: 320x256 High Res ] <- read-only mode name display
 *
 * "Choose..." opens an ASL_ScreenModeRequest requester.
 * Mode description is obtained via GetDisplayInfoData(DTAG_NAME).
 * When "Use Workbench mode" is checked, the mode controls are disabled.
 *
 * C89 compatible.
 */

#include <stdio.h>
#include <string.h>

#include <clib/alib_protos.h>

#include <intuition/screens.h>
#include <intuition/icclass.h>
#include <graphics/displayinfo.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/utility.h>
#include <proto/asl.h>
#include <libraries/asl.h>

#include <proto/window.h>
#include <classes/window.h>

#include <proto/layout.h>
#include <gadgets/layout.h>

#include <proto/button.h>
#include <gadgets/button.h>

#include <proto/checkbox.h>
#include <gadgets/checkbox.h>

#include <proto/label.h>
#include <images/label.h>

#include "compilers.h"
#include "pmsettingsview.h"

extern struct Screen *CurrentMainScreen;
extern struct Library *AslBase;

/* ------------------------------------------------------------------ */
/* Internal helpers                                                    */
/* ------------------------------------------------------------------ */

/* Fill modeIdHexStr and modeDescStr from currentModeId (no gadget update). */
static void fillModeStrings(PmSettingsView *psv)
{
    struct NameInfo ni;

    sprintf(psv->modeIdHexStr, "0x%08lX", (unsigned long)psv->currentModeId);

    if (psv->currentModeId == INVALID_ID) {
        strcpy(psv->modeDescStr, "Invalid mode ID");
    } else {
        memset(&ni, 0, sizeof(ni));
        if (GetDisplayInfoData(NULL, (UBYTE *)&ni, sizeof(ni),
                               DTAG_NAME, psv->currentModeId)) {
            strncpy(psv->modeDescStr, ni.Name, sizeof(psv->modeDescStr) - 1);
            psv->modeDescStr[sizeof(psv->modeDescStr) - 1] = '\0';
        } else {
            strcpy(psv->modeDescStr, "Unknown display mode");
        }
    }
}

/* Push current strings and disabled state to gadgets (window must be open). */
static void syncGadgetsToState(PmSettingsView *psv)
{
    ULONG disabled;

    if (!psv->window) return;

    disabled = psv->useWorkbench ? (ULONG)TRUE : (ULONG)FALSE;

    SetGadgetAttrs((struct Gadget *)psv->modeIdDisplay,
                   psv->window, NULL,
                   GA_Text,     (ULONG)psv->modeIdHexStr,
                   GA_Disabled, disabled,
                   TAG_END);
    SetGadgetAttrs((struct Gadget *)psv->chooseModeBtn,
                   psv->window, NULL,
                   GA_Disabled, disabled,
                   TAG_END);
    SetGadgetAttrs((struct Gadget *)psv->modeDescDisplay,
                   psv->window, NULL,
                   GA_Text,     (ULONG)psv->modeDescStr,
                   GA_Disabled, disabled,
                   TAG_END);
    RefreshGList((struct Gadget *)psv->mainLayout, psv->window, NULL, -1);
}

/* Open ASL screen mode requester and update state if user confirms. */
static void openScreenModeRequester(PmSettingsView *psv)
{
    struct ScreenModeRequester *smr;
    BOOL ok;

    smr = (struct ScreenModeRequester *)AllocAslRequestTags(
              ASL_ScreenModeRequest, TAG_END);
    if (!smr) return;

    ok = (BOOL)AslRequestTags(smr,
              ASLSM_Window,           (ULONG)psv->window,
              ASLSM_InitialDisplayID, (ULONG)psv->currentModeId,
              TAG_END);
    if (ok) {
        psv->currentModeId = smr->sm_DisplayID;
        fillModeStrings(psv);
        syncGadgetsToState(psv);
    }

    FreeAslRequest(smr);
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

BOOL PmSettingsView_Init(PmSettingsView *psv, const char *title)
{
    Object *useWbLabel;
    Object *modeIdLabel;
    Object *modeDescLabel;
    Object *modeIdRow;

    if (!psv) return FALSE;

    memset(psv, 0, sizeof(PmSettingsView));

    /* Defaults: Use Workbench mode, no specific mode ID */
    psv->useWorkbench  = TRUE;
    psv->currentModeId = INVALID_ID;
    fillModeStrings(psv);

    /* --- Checkbox: Use Workbench screen mode --- */
    psv->useWbCheck = NewObject(CHECKBOX_GetClass(), NULL,
                          GA_ID,        GAD_SETTINGS_USEWB,
                          GA_RelVerify, TRUE,
                          GA_Selected,  (ULONG)TRUE,
                          TAG_END);
    if (!psv->useWbCheck) return FALSE;

    useWbLabel = NewObject(LABEL_GetClass(), NULL,
                     LABEL_Text, (ULONG)"Use Workbench screen mode",
                     TAG_END);

    /* --- Mode ID read-only display (starts disabled when useWb=TRUE) --- */
    psv->modeIdDisplay = NewObject(BUTTON_GetClass(), NULL,
                             GA_ReadOnly, TRUE,
                             GA_Text,     (ULONG)psv->modeIdHexStr,
                             GA_Disabled, (ULONG)TRUE,
                             TAG_END);
    if (!psv->modeIdDisplay) { return FALSE; }

    /* --- Choose... button (starts disabled) --- */
    psv->chooseModeBtn = NewObject(BUTTON_GetClass(), NULL,
                             GA_ID,        GAD_SETTINGS_CHOOSEMODE,
                             GA_RelVerify, TRUE,
                             GA_Text,      (ULONG)"Choose...",
                             GA_Disabled,  (ULONG)TRUE,
                             TAG_END);
    if (!psv->chooseModeBtn) { return FALSE; }

    /* --- Mode description read-only display (starts disabled) --- */
    psv->modeDescDisplay = NewObject(BUTTON_GetClass(), NULL,
                               GA_ReadOnly, TRUE,
                               GA_Text,     (ULONG)psv->modeDescStr,
                               GA_Disabled, (ULONG)TRUE,
                               TAG_END);
    if (!psv->modeDescDisplay) { return FALSE; }

    /* Labels for the mode ID row and description row */
    modeIdLabel = NewObject(LABEL_GetClass(), NULL,
                      LABEL_Text, (ULONG)"Screen Mode:",
                      TAG_END);

    modeDescLabel = NewObject(LABEL_GetClass(), NULL,
                        LABEL_Text, (ULONG)"Description:",
                        TAG_END);

    /* --- Horizontal sub-layout: [modeIdDisplay | Choose...] --- */
    modeIdRow = NewObject(LAYOUT_GetClass(), NULL,
                    LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ,
                    LAYOUT_BevelStyle,  BVS_NONE,
                    LAYOUT_SpaceInner,  TRUE,

                    LAYOUT_AddChild,    (ULONG)psv->modeIdDisplay,

                    LAYOUT_AddChild,    (ULONG)psv->chooseModeBtn,
                    CHILD_WeightedWidth, 0,

                    TAG_END);
    if (!modeIdRow) return FALSE;

    /* --- Screen settings group --- */
    psv->screenLayout = NewObject(LAYOUT_GetClass(), NULL,
                            LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
                            LAYOUT_BevelStyle,  BVS_GROUP,
                            LAYOUT_Label,       (ULONG)"Fullscreen Display Mode",
                            LAYOUT_SpaceOuter,  TRUE,
                            LAYOUT_SpaceInner,  TRUE,

                            LAYOUT_AddChild,     (ULONG)psv->useWbCheck,
                            CHILD_WeightedHeight, 0,
                            CHILD_Label,          (ULONG)useWbLabel,

                            LAYOUT_AddChild,     (ULONG)modeIdRow,
                            CHILD_WeightedHeight, 0,
                            CHILD_Label,          (ULONG)modeIdLabel,

                            LAYOUT_AddChild,     (ULONG)psv->modeDescDisplay,
                            CHILD_WeightedHeight, 0,
                            CHILD_Label,          (ULONG)modeDescLabel,

                            TAG_END);
    if (!psv->screenLayout) return FALSE;

    /* --- Main top-level layout --- */
    psv->mainLayout = NewObject(LAYOUT_GetClass(), NULL,
                          LAYOUT_DeferLayout,   TRUE,
                          LAYOUT_Orientation,   LAYOUT_ORIENT_VERT,
                          LAYOUT_BevelStyle,    BVS_NONE,
                          LAYOUT_SpaceOuter,    TRUE,
                          LAYOUT_SpaceInner,    TRUE,

                          LAYOUT_AddChild,      (ULONG)psv->screenLayout,
                          CHILD_WeightedHeight, 0,

                          TAG_END);
    if (!psv->mainLayout) {
        DisposeObject(psv->screenLayout);
        psv->screenLayout = NULL;
        return FALSE;
    }

    /* --- BOOPSI window object --- */
    psv->windowObj = NewObject(WINDOW_GetClass(), NULL,
                         WA_Left,   100,
                         WA_Top,    60,
                         WA_Width,  420,
                         WA_Height, 170,
                         WA_IDCMP,  IDCMP_CLOSEWINDOW | IDCMP_GADGETUP | IDCMP_RAWKEY,
                         WA_Flags,  WFLG_DRAGBAR | WFLG_DEPTHGADGET |
                                    WFLG_CLOSEGADGET | WFLG_ACTIVATE |
                                    WFLG_SMART_REFRESH,
                         WA_Title,  (ULONG)title,
                         WINDOW_ParentGroup, (ULONG)psv->mainLayout,
                         TAG_END);
    if (!psv->windowObj) {
        DisposeObject(psv->mainLayout);
        psv->mainLayout    = NULL;
        psv->screenLayout  = NULL;
        return FALSE;
    }

    return TRUE;
}

void PmSettingsView_Open(PmSettingsView *psv)
{
    if (!psv || !psv->windowObj) return;
    if (psv->window) return; /* already open */

    if (CurrentMainScreen) {
        SetAttrs(psv->windowObj,
                 WA_CustomScreen, (ULONG)CurrentMainScreen,
                 TAG_END);
    }

    psv->window = (struct Window *)DoMethod(psv->windowObj, WM_OPEN, NULL);
}

void PmSettingsView_Close(PmSettingsView *psv)
{
    if (!psv || !psv->windowObj || !psv->window) return;

    DoMethod(psv->windowObj, WM_CLOSE, NULL);
    psv->window = NULL;
}

BOOL PmSettingsView_HandleInput(PmSettingsView *psv)
{
    ULONG result;

    if (!psv || !psv->windowObj) return FALSE;
    if (!psv->window) return TRUE; /* window closed, that's fine */

    while ((result = DoMethod(psv->windowObj, WM_HANDLEINPUT, NULL))
           != WMHI_LASTMSG)
    {
        switch (result & WMHI_CLASSMASK)
        {
            case WMHI_CLOSEWINDOW:
                PmSettingsView_Close(psv);
                return TRUE;

            case WMHI_GADGETUP:
            {
                ULONG gadId = result & WMHI_GADGETMASK;
                if (gadId == GAD_SETTINGS_USEWB) {
                    ULONG checked = 0;
                    GetAttr(GA_Selected, psv->useWbCheck, &checked);
                    psv->useWorkbench = checked ? TRUE : FALSE;
                    syncGadgetsToState(psv);
                } else if (gadId == GAD_SETTINGS_CHOOSEMODE) {
                    openScreenModeRequester(psv);
                }
                break;
            }

            default:
                break;
        }
    }

    return TRUE;
}

ULONG PmSettingsView_GetSignalMask(PmSettingsView *psv)
{
    if (!psv || !psv->window) return 0;
    return (1L << psv->window->UserPort->mp_SigBit);
}

BOOL PmSettingsView_GetUseWorkbench(PmSettingsView *psv)
{
    if (!psv) return TRUE;
    return psv->useWorkbench;
}

void PmSettingsView_SetUseWorkbench(PmSettingsView *psv, BOOL useWb)
{
    if (!psv) return;
    psv->useWorkbench = useWb;

    if (psv->window && psv->useWbCheck) {
        SetGadgetAttrs((struct Gadget *)psv->useWbCheck,
                       psv->window, NULL,
                       GA_Selected, (ULONG)(useWb ? TRUE : FALSE),
                       TAG_END);
        RefreshGList((struct Gadget *)psv->useWbCheck, psv->window, NULL, 1);
    }

    syncGadgetsToState(psv);
}

ULONG PmSettingsView_GetModeId(PmSettingsView *psv)
{
    if (!psv) return INVALID_ID;
    return psv->currentModeId;
}

void PmSettingsView_SetModeId(PmSettingsView *psv, ULONG modeId)
{
    if (!psv) return;
    psv->currentModeId = modeId;
    fillModeStrings(psv);
    syncGadgetsToState(psv);
}

void PmSettingsView_Dispose(PmSettingsView *psv)
{
    if (!psv) return;

    if (psv->window) {
        DoMethod(psv->windowObj, WM_CLOSE, NULL);
        psv->window = NULL;
    }

    if (psv->windowObj) {
        DisposeObject(psv->windowObj);
        psv->windowObj    = NULL;
        psv->mainLayout   = NULL;
        psv->screenLayout = NULL;
        psv->useWbCheck      = NULL;
        psv->modeIdDisplay   = NULL;
        psv->chooseModeBtn   = NULL;
        psv->modeDescDisplay = NULL;
    }
}
