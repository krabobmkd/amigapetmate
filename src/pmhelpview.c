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
#include "petmate.h"
#include "appsettings.h"
#include "pmlocale.h"
#include <stdio.h>
extern struct Screen *CurrentMainScreen;
extern struct Library *AslBase;

/* ------------------------------------------------------------------ */
/* Internal helpers                                                    */
/* ------------------------------------------------------------------ */

/* Fill modeIdHexStr and modeDescStr from currentModeId (no gadget update). */
static void fillModeStrings(PmSettingsView *psv)
{
    struct NameInfo ni;

    sprintf(psv->modeIdHexStr, "0x%08lX", (unsigned long)app->appSettings.screenModeId);

    if (app->appSettings.screenModeId == INVALID_ID) {
        strcpy(psv->modeDescStr, "Invalid mode ID");
    } else {
        memset(&ni, 0, sizeof(ni));
        if (GetDisplayInfoData(NULL, (UBYTE *)&ni, sizeof(ni),
                               DTAG_NAME, app->appSettings.screenModeId)) {
            strncpy(psv->modeDescStr, ni.Name, sizeof(psv->modeDescStr) - 1);
            psv->modeDescStr[sizeof(psv->modeDescStr) - 1] = '\0';
        } else {
            strcpy(psv->modeDescStr, "Unknown display mode");
        }
    }
}

/* Fill bgImagePathBuf from current AppSettings value. */
static void fillBgImagePathBuf(PmSettingsView *psv)
{
    const char *src = app->appSettings.bgImagePath;
    if (src && src[0] != '\0') {
        strncpy(psv->bgImagePathBuf, src, sizeof(psv->bgImagePathBuf) - 1);
        psv->bgImagePathBuf[sizeof(psv->bgImagePathBuf) - 1] = '\0';
    } else {
        strncpy(psv->bgImagePathBuf, LOC(MSG_SETTINGS_BGIMAGE_NONE),
                sizeof(psv->bgImagePathBuf) - 1);
        psv->bgImagePathBuf[sizeof(psv->bgImagePathBuf) - 1] = '\0';
    }
}

/* Push current strings and disabled state to gadgets (window must be open). */
static void syncGadgetsToState(PmSettingsView *psv)
{
    ULONG disabled;
    ULONG bgImageDisabled;

    disabled = app->appSettings.screenModeIdLikeWorkbench ? (ULONG)TRUE : (ULONG)FALSE;
    /* bg image controls are disabled when "Use one color" is checked */
    bgImageDisabled = app->appSettings.useOneColorBg ? (ULONG)TRUE : (ULONG)FALSE;

    fillBgImagePathBuf(psv);

    if (psv->window)
    {
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
        if (psv->bgImagePathDisplay)
            SetGadgetAttrs((struct Gadget *)psv->bgImagePathDisplay,
                           psv->window, NULL,
                           GA_Text,     (ULONG)psv->bgImagePathBuf,
                           GA_Disabled, bgImageDisabled,
                           TAG_END);
        if (psv->chooseBgImageBtn)
            SetGadgetAttrs((struct Gadget *)psv->chooseBgImageBtn,
                           psv->window, NULL,
                           GA_Disabled, bgImageDisabled,
                           TAG_END);
        if (psv->removeBgImageBtn)
            SetGadgetAttrs((struct Gadget *)psv->removeBgImageBtn,
                           psv->window, NULL,
                           GA_Disabled, bgImageDisabled,
                           TAG_END);
    } else
    {
        SetAttrs((struct Gadget *)psv->modeIdDisplay,
                       GA_Text,     (ULONG)psv->modeIdHexStr,
                       GA_Disabled, disabled,
                       TAG_END);
        SetAttrs((struct Gadget *)psv->chooseModeBtn,
                       GA_Disabled, disabled,
                       TAG_END);
        SetAttrs((struct Gadget *)psv->modeDescDisplay,
                       GA_Text,     (ULONG)psv->modeDescStr,
                       GA_Disabled, disabled,
                       TAG_END);
        if (psv->bgImagePathDisplay)
            SetAttrs((struct Gadget *)psv->bgImagePathDisplay,
                           GA_Text,     (ULONG)psv->bgImagePathBuf,
                           GA_Disabled, bgImageDisabled,
                           TAG_END);
        if (psv->chooseBgImageBtn)
            SetAttrs((struct Gadget *)psv->chooseBgImageBtn,
                           GA_Disabled, bgImageDisabled,
                           TAG_END);
        if (psv->removeBgImageBtn)
            SetAttrs((struct Gadget *)psv->removeBgImageBtn,
                           GA_Disabled, bgImageDisabled,
                           TAG_END);
    }
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
              ASLSM_InitialDisplayID, (ULONG)app->appSettings.screenModeId,
              TAG_END);
    if (ok) {
        app->appSettings.screenModeId = smr->sm_DisplayID;
        fillModeStrings(psv);
        syncGadgetsToState(psv);
    }

    FreeAslRequest(smr);
}

/* Open ASL file requester for the background image path. */
static void openBgImageRequester(PmSettingsView *psv)
{
    struct FileRequester *req;
    BOOL ok;
    ULONG dirLen, fileLen;
    char *buf;

    req = (struct FileRequester *)AllocAslRequestTags(ASL_FileRequest, TAG_END);
    if (!req) return;

    ok = (BOOL)AslRequestTags(req,
              ASLFR_Window,          (ULONG)psv->window,
              ASLFR_TitleText,       (ULONG)LOC(MSG_SETTINGS_BGIMAGE_TITLE),
              ASLFR_InitialPattern,  (ULONG)"#?.(png|gif|bmp|iff|ilbm|jpg|jpeg)",
              ASLFR_DoPatterns,      TRUE,
              ASLFR_DoSaveMode,      FALSE,
              TAG_END);

    if (ok) {
        dirLen  = (ULONG)strlen(req->rf_Dir);
        fileLen = (ULONG)strlen(req->rf_File);
        buf = (char *)AllocVec(dirLen + fileLen + 2, MEMF_ANY);
        if (buf) {
            CopyMem((APTR)req->rf_Dir, (APTR)buf, dirLen + 1);
            AddPart(buf, req->rf_File, dirLen + fileLen + 2);
            AppSettings_SetBgImagePath(&app->appSettings, buf);
            FreeVec(buf);
        }
        syncGadgetsToState(psv);
    }

    FreeAslRequest(req);
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

    Object *useOneColorBgLabel;
    Object *bgImageLabel;
    Object *bgImageRow;

    if (!psv) return FALSE;

    memset(psv, 0, sizeof(PmSettingsView));

    /* Defaults: Use Workbench mode, no specific mode ID */
    app->appSettings.screenModeIdLikeWorkbench  = TRUE;
    app->appSettings.screenModeId = INVALID_ID;
    fillModeStrings(psv);
    fillBgImagePathBuf(psv);

    /* --- Checkbox: Use Workbench screen mode --- */
    psv->useWbCheck = NewObject(CHECKBOX_GetClass(), NULL,
                          GA_ID,        GAD_SETTINGS_USEWB,
                          GA_RelVerify, TRUE,
                          GA_Selected,  (ULONG)TRUE,
                          TAG_END);
    if (!psv->useWbCheck) return FALSE;

    useWbLabel = NewObject(LABEL_GetClass(), NULL,
                     LABEL_Text, (ULONG)LOC(MSG_SETTINGS_FSUSEWBMODE),
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
                             GA_Text,      (ULONG)LOC(MSG_SETTINGS_CHOOSEDOTS),
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
                      LABEL_Text, (ULONG)LOC(MSG_SETTINGS_SCREENMODECL),
                      TAG_END);

    modeDescLabel = NewObject(LABEL_GetClass(), NULL,
                        LABEL_Text, (ULONG)LOC(MSG_SETTINGS_DESCRIPTIONCL),
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
                            LAYOUT_Label,       (ULONG)LOC(MSG_SETTINGS_FULLSCREENDISPLAYMODE),
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

    /* --- Checkbox: Use one color for background --- */
    psv->useOneColorBgCheck = NewObject(CHECKBOX_GetClass(), NULL,
                                  GA_ID,        GAD_SETTINGS_USEONECLORBG,
                                  GA_RelVerify, TRUE,
                                  GA_Selected,  (ULONG)app->appSettings.useOneColorBg,
                                  TAG_END);
    if (!psv->useOneColorBgCheck) return FALSE;

    useOneColorBgLabel = NewObject(LABEL_GetClass(), NULL,
                             LABEL_Text, (ULONG)LOC(MSG_SETTINGS_USEONECLORBG),
                             TAG_END);

    /* --- Background image path read-only display (disabled when useOneColorBg) --- */
    psv->bgImagePathDisplay = NewObject(BUTTON_GetClass(), NULL,
                                  GA_ReadOnly, TRUE,
                                  GA_Text,     (ULONG)psv->bgImagePathBuf,
                                  GA_Disabled, (ULONG)app->appSettings.useOneColorBg,
                                  TAG_END);
    if (!psv->bgImagePathDisplay) return FALSE;

    /* --- Choose... button for background image (disabled when useOneColorBg) --- */
    psv->chooseBgImageBtn = NewObject(BUTTON_GetClass(), NULL,
                                GA_ID,        GAD_SETTINGS_CHOOSEBGIMAGE,
                                GA_RelVerify, TRUE,
                                GA_Text,      (ULONG)LOC(MSG_SETTINGS_CHOOSE_BGIMAGE),
                                GA_Disabled,  (ULONG)app->appSettings.useOneColorBg,
                                TAG_END);
    if (!psv->chooseBgImageBtn) return FALSE;

    /* --- Remove button for background image (disabled when useOneColorBg) --- */
    psv->removeBgImageBtn = NewObject(BUTTON_GetClass(), NULL,
                                GA_ID,        GAD_SETTINGS_REMOVEBGIMAGE,
                                GA_RelVerify, TRUE,
                                GA_Text,      (ULONG)LOC(MSG_SETTINGS_REMOVEBGIMAGE),
                                GA_Disabled,  (ULONG)app->appSettings.useOneColorBg,
                                TAG_END);
    if (!psv->removeBgImageBtn) return FALSE;

    bgImageLabel = NewObject(LABEL_GetClass(), NULL,
                       LABEL_Text, (ULONG)LOC(MSG_SETTINGS_BGIMAGE_LABEL),
                       TAG_END);

    /* --- Horizontal sub-layout: [bgImagePathDisplay | Choose... | Remove] --- */
    bgImageRow = NewObject(LAYOUT_GetClass(), NULL,
                     LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ,
                     LAYOUT_BevelStyle,  BVS_NONE,
                     LAYOUT_SpaceInner,  TRUE,

                     LAYOUT_AddChild,    (ULONG)psv->bgImagePathDisplay,

                     LAYOUT_AddChild,    (ULONG)psv->chooseBgImageBtn,
                     CHILD_WeightedWidth, 0,

                     LAYOUT_AddChild,    (ULONG)psv->removeBgImageBtn,
                     CHILD_WeightedWidth, 0,

                     TAG_END);
    if (!bgImageRow) return FALSE;

    /* --- UI Background group --- */
    psv->bgLayout = NewObject(LAYOUT_GetClass(), NULL,
                        LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
                        LAYOUT_BevelStyle,  BVS_GROUP,
                        LAYOUT_Label,       (ULONG)LOC(MSG_SETTINGS_UI_BG_GROUP),
                        LAYOUT_SpaceOuter,  TRUE,
                        LAYOUT_SpaceInner,  TRUE,

                        LAYOUT_AddChild,     (ULONG)psv->useOneColorBgCheck,
                        CHILD_WeightedHeight, 0,
                        CHILD_Label,          (ULONG)useOneColorBgLabel,

                        LAYOUT_AddChild,     (ULONG)bgImageRow,
                        CHILD_WeightedHeight, 0,
                        CHILD_Label,          (ULONG)bgImageLabel,

                        TAG_END);
    if (!psv->bgLayout) return FALSE;

    /* --- Main top-level layout --- */
    psv->mainLayout = NewObject(LAYOUT_GetClass(), NULL,
                          LAYOUT_DeferLayout,   TRUE,
                          LAYOUT_Orientation,   LAYOUT_ORIENT_VERT,
                          LAYOUT_BevelStyle,    BVS_NONE,
                          LAYOUT_SpaceOuter,    TRUE,
                          LAYOUT_SpaceInner,    TRUE,

                          LAYOUT_AddChild,      (ULONG)psv->screenLayout,
                          CHILD_WeightedHeight, 0,

                          LAYOUT_AddChild,      (ULONG)psv->bgLayout,
                          CHILD_WeightedHeight, 0,

                          TAG_END);
    if (!psv->mainLayout) {
        DisposeObject(psv->bgLayout);
        psv->bgLayout = NULL;
        DisposeObject(psv->screenLayout);
        psv->screenLayout = NULL;
        return FALSE;
    }

    /* --- BOOPSI window object --- */
    psv->windowObj = NewObject(WINDOW_GetClass(), NULL,
                         WA_Left,   100,
                         WA_Top,    60,
                         WA_Width,  420,
                         WA_Height, 270,
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
                AppSettings_Save(&app->appSettings);
                PmSettingsView_Close(psv);
                return TRUE;
            case WMHI_RAWKEY:
                    {
                        /* keys managed at window level */
                        ULONG key = (result & WMHI_KEYMASK);
                        if(key == 0x45) {
                            AppSettings_Save(&app->appSettings);
                            PmSettingsView_Close(psv);
                            return TRUE;
                        }
                    }
                        break;
            case WMHI_GADGETUP:
            {
                ULONG gadId = result & WMHI_GADGETMASK;
                if (gadId == GAD_SETTINGS_USEWB) {
                    ULONG checked = 0;
                    GetAttr(GA_Selected, psv->useWbCheck, &checked);
                    app->appSettings.screenModeIdLikeWorkbench = checked ? TRUE : FALSE;
                    syncGadgetsToState(psv);
                } else if (gadId == GAD_SETTINGS_CHOOSEMODE) {
                    openScreenModeRequester(psv);
                } else if (gadId == GAD_SETTINGS_USEONECLORBG) {
                    ULONG checked = 0;
                    GetAttr(GA_Selected, psv->useOneColorBgCheck, &checked);
                    app->appSettings.useOneColorBg = checked ? TRUE : FALSE;
                    syncGadgetsToState(psv);
                } else if (gadId == GAD_SETTINGS_CHOOSEBGIMAGE) {
                    openBgImageRequester(psv);
                } else if (gadId == GAD_SETTINGS_REMOVEBGIMAGE) {
                    if (app->appSettings.bgImagePath) {
                        FreeVec(app->appSettings.bgImagePath);
                        app->appSettings.bgImagePath = NULL;
                    }
                    psv->bgImagePathBuf[0] = '\0';
                    syncGadgetsToState(psv);
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


void PmSettingsView_SetFSModeIdLikeWorkbench(PmSettingsView *psv, ULONG fsUseWBMode)
{
    if (!psv) return;
    app->appSettings.screenModeIdLikeWorkbench = fsUseWBMode;

    if (psv->useWbCheck) {
        if(psv->window)
            SetGadgetAttrs((struct Gadget *)psv->useWbCheck,
                       psv->window, NULL,
                       GA_Selected, fsUseWBMode,
                       TAG_END);
        else
            SetAttrs((struct Gadget *)psv->useWbCheck,
                       GA_Selected, fsUseWBMode,
                       TAG_END);
    }

    syncGadgetsToState(psv);
}

ULONG PmSettingsView_GetModeId(PmSettingsView *psv)
{
    if (!psv) return INVALID_ID;
    return app->appSettings.screenModeId;
}

void PmSettingsView_SetModeId(PmSettingsView *psv, ULONG modeId)
{
    if (!psv) return;
    app->appSettings.screenModeId = modeId;
    fillModeStrings(psv);
    syncGadgetsToState(psv);
}

void PmSettingsView_SetUseOneColorBg(PmSettingsView *psv, int useOneColor)
{
    if (!psv) return;
    app->appSettings.useOneColorBg = useOneColor;

    if (psv->useOneColorBgCheck) {
        if (psv->window)
            SetGadgetAttrs((struct Gadget *)psv->useOneColorBgCheck,
                           psv->window, NULL,
                           GA_Selected, (ULONG)useOneColor,
                           TAG_END);
        else
            SetAttrs((struct Gadget *)psv->useOneColorBgCheck,
                           GA_Selected, (ULONG)useOneColor,
                           TAG_END);
    }

    syncGadgetsToState(psv);
}

void PmSettingsView_SetBgImagePath(PmSettingsView *psv, const char *path)
{
    if (!psv) return;
   // AppSettings_SetBgImagePath(&app->appSettings, path);
    fillBgImagePathBuf(psv);
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
        psv->useWbCheck         = NULL;
        psv->modeIdDisplay      = NULL;
        psv->chooseModeBtn      = NULL;
        psv->modeDescDisplay    = NULL;
        psv->bgLayout           = NULL;
        psv->useOneColorBgCheck = NULL;
        psv->bgImagePathDisplay = NULL;
        psv->chooseBgImageBtn   = NULL;
        psv->removeBgImageBtn   = NULL;
    }
}
