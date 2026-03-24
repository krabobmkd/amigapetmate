#include "pmaction.h"
#include "pmlocale.h"
#include "petscii_style.h"
#include "petscii_screen.h"
#include "petscii_undo.h"
#include "petscii_fileio.h"
#include "petscii_export.h"
#include "petscii_export_ilbm.h"
#include "petscii_export_gif.h"
#include "petscii_import_image.h"
#include "petscii_import_prg.h"
#include "pmlocale.h"
#include "pmstring.h"
#include "petmate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/alib.h>
#include <intuition/intuition.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>
#include <proto/asl.h>
#include <libraries/asl.h>

#include <proto/amigaguide.h>
#include <libraries/amigaguide.h>

#include <proto/requester.h>
#include <classes/requester.h>


#include "boopsimainwindow.h"
#include "pmsettingsview.h"
#ifdef HELP_USE_DATATYPE_AND_WINDOW
#include "pmhelpview.h"
#endif
#include "pmmenu.h"
#include "petscii_canvas.h"  /* PCA_TransformBrush, PCA_Brush, BRUSH_TRANSFORM_* */
#include "petscii_brush.h"   /* PetsciiBrush */

#include <stdio.h>
/* External globals from petmate.c */
extern struct Library *AslBase;
extern struct IntuitionBase *IntuitionBase;

#ifdef HELP_USE_AGLIB
extern struct Library          *AmigaGuideBase;
#endif

extern void refreshUI();
void SetStatusBarMessage(int enumMessage);
/* Last directory used in a file requester (persists across open/save calls) */
static char *s_lastDir = NULL;


/* -----------------------------------------------------------------------
 * Minimal LCG random number generator (Knuth / Numerical Recipes).
 *
 * state = state * 1664525 + 1013904223   (mod 2^32, natural ULONG overflow)
 *
 * The full 32-bit output has good uniformity across all bit positions.
 * No low-bit aliasing like stdlib rand() on many platforms.
 *
 * pmRandSeed() re-mixes the state from dos.library DateStamp (50 Hz ticks)
 * so consecutive calls within the same second still produce different
 * sequences.  Call once before a generation loop.
 *
 * pmRand(n) returns a value in [0, n-1].  Uses the upper 16 bits to get
 * the best-quality range for small n.
 * ----------------------------------------------------------------------- */
/* s_pmRandState persists across calls: each generation run continues where
 * the previous one left off, so the starting seed is never the same twice. */
static ULONG s_pmRandState = 0xA3C5F7B1UL;

static ULONG pmRand(ULONG n)
{
    /* LCG: multiply + add, natural 32-bit overflow is the modulus.
     * Constants from Knuth / Numerical Recipes Vol.2.
     * Upper 16 bits used for range reduction (lower bits have shorter
     * period in LCG generators when n is small). */
    s_pmRandState = s_pmRandState * 1664525UL + 1013904223UL;
    return (s_pmRandState >> 16) % n;
}

// const char *PetsciiFileIO_ErrorString(int strnum)
// {
//     return LOC(MSG_PETSCII_FILEIO_OK+strnum);
// }


extern void RefreshAllColorGadgets();
static void rebuildMenuIfPossible(PmActionContext *ctx);
/* - - - Apply a new palette to the style and update the menu checkmark - - - */

void PmAction_ApplyPalette(PmActionContext *ctx, UBYTE paletteID)
{
    PetsciiProject *proj;
    PetsciiStyle   *sty;

    if (!ctx || !ctx->pproject || !*ctx->pproject) return;

    proj = *ctx->pproject;
    sty  = (PetsciiStyle *)ctx->style;

    app->appSettings.currentPalette = paletteID;
    proj->modified = 1;

    if (sty) {
        PetsciiStyle_Init(sty, paletteID);
        PetsciiStyle_Apply(sty, CurrentMainScreen);
        RefreshAllColorGadgets();
    }

    if (ctx->pmenu && CurrentMainWindow) {
        PmMenu_UpdatePaletteCheck((PmMenu *)ctx->pmenu,
                                  CurrentMainWindow, paletteID);
    }

}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   Project actions
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* - - - Helper: ask "save changes?" if project is modified.
 * Returns TRUE if the caller may proceed (user chose Discard or saved OK).
 * Returns FALSE if user chose Cancel or save failed.              - - - */
static BOOL confirmDiscard(PmActionContext *ctx)
{
    static struct EasyStruct es = {
        sizeof(struct EasyStruct), 0,
        (STRPTR)"PetMate",
        (STRPTR)"Save changes to the current project?",
        (STRPTR)"Save|Discard|Cancel"
    };
    LONG answer;
    PetsciiProject *proj;

    if (!ctx || !ctx->pproject || !*ctx->pproject) return TRUE;
    proj = *ctx->pproject;

    if (!proj->modified) return TRUE; /* nothing to lose */

    answer = EasyRequest(CurrentMainWindow, &es, NULL, NULL);
    /* answer: 2=Save chosen, 1=Discard, 0=Cancel/window-close */

    if (answer == 0) return FALSE; /* Cancel */

    if (answer == 2) {
        /* User wants to save first */
        return Action_ProjectSave(ctx);
    }

    return TRUE; /* Discard */
}

/* - - - Helper: show an error requester for file I/O failures - - - */
// static void showFileError(const char *title, const char *errMsg)
// {
//     static struct EasyStruct es = {
//         sizeof(struct EasyStruct), 0,
//         NULL, NULL,
//         (STRPTR)"OK"
//     };
//     es.es_Title   = (STRPTR)title;
//     es.es_TextFormat = (STRPTR)errMsg;
//     EasyRequest(CurrentMainWindow, &es, NULL, NULL);

// }

/* - - - Helper: run an ASL file requester.
 * saveMode=TRUE for Save As, FALSE for Open.
 * Returns an AllocVec'd full path string on success, NULL if cancelled.
 * Caller must PmStr_Free() the result.                          - - - */
static char *aslFileRequest(BOOL saveMode)
{
    struct FileRequester *req;
    char  *result = NULL;
    BOOL   ok;
    ULONG  dirLen;
    ULONG  fileLen;
    char  *buf;

    req = (struct FileRequester *)AllocAslRequestTags(ASL_FileRequest,
            TAG_END);
    if (!req) return NULL;

    ok = (BOOL)AslRequestTags(req,
            ASLFR_Window,          (ULONG)CurrentMainWindow,
            ASLFR_TitleText,       saveMode ? (ULONG)"Save .petmate file"
                                            : (ULONG)"Open .petmate file",
            ASLFR_InitialPattern,  (ULONG)"#?.petmate",
            ASLFR_DoPatterns,      TRUE,
            ASLFR_DoSaveMode,      (ULONG)saveMode,
            s_lastDir ? ASLFR_InitialDrawer : TAG_IGNORE,
            s_lastDir ? (ULONG)s_lastDir    : 0UL,
            TAG_END);

    if (ok) {
        PmStr_Free(s_lastDir);
        s_lastDir = PmStr_Alloc(req->rf_Dir);

        dirLen  = (ULONG)strlen(req->rf_Dir);
        fileLen = (ULONG)strlen(req->rf_File);
        buf = (char *)AllocVec(dirLen + fileLen + 2, MEMF_ANY);
        if (buf) {
            CopyMem((APTR)req->rf_Dir, (APTR)buf, dirLen + 1);
            AddPart(buf, req->rf_File, dirLen + fileLen + 2);
            result = buf;
        }
    }

    FreeAslRequest(req);
    return result;
}

BOOL Action_ProjectNew(PmActionContext *ctx)
{
    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;

    if (!confirmDiscard(ctx)) return FALSE;

    PetsciiProject_Reset(*ctx->pproject);
    return TRUE;
}

BOOL Action_ProjectOpen(PmActionContext *ctx)
{
    char *pathbuf;
    int   err;

    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;

 //   if (!confirmDiscard(ctx)) return FALSE;

    pathbuf = aslFileRequest(FALSE);
    if (!pathbuf) return FALSE; /* user cancelled */

    err = PetsciiFileIO_Load(*ctx->pproject, pathbuf);

    if (err == PETSCII_FILEIO_OK && app) {
        AppSettings_AddRecentFile(&app->appSettings, pathbuf);
        rebuildMenuIfPossible(ctx);
    }
    SetStatusBarMessage(MSG_PETSCII_FILEIO_OK+err);


    PmStr_Free(pathbuf);
    refreshUI();
    return TRUE;
}

BOOL Action_ProjectSave(PmActionContext *ctx)
{
    int  err;
    PetsciiProject *proj;

    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;
    proj = *ctx->pproject;

    /* If no path is set yet, delegate to Save As */
    if (!proj->filepath)
        return Action_ProjectSaveAs(ctx);

    err = PetsciiFileIO_Save(proj, proj->filepath);

     SetStatusBarMessage(
         (err == PETSCII_FILEIO_OK)?MSG_PETSCII_FILEIO_WRITEOK:
         (MSG_PETSCII_FILEIO_OK+err)
         );

    return TRUE;
}

BOOL Action_ProjectSaveAs(PmActionContext *ctx)
{
    char *pathbuf;
    char *actualPath;
    int   err;

    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;

    pathbuf = aslFileRequest(TRUE);
    if (!pathbuf) return FALSE; /* user cancelled */

    actualPath = PmStr_WithExt(pathbuf, ".petmate");
    if (!actualPath) return PETSCII_FILEIO_EALLOC;


    err = PetsciiFileIO_Save(*ctx->pproject, actualPath);

    SetStatusBarMessage(
        (err == PETSCII_FILEIO_OK)?MSG_PETSCII_FILEIO_WRITEOK:
        (MSG_PETSCII_FILEIO_OK+err)
        );

    if (err == PETSCII_FILEIO_OK && app) {
        AppSettings_AddRecentFile(&app->appSettings, actualPath);
        rebuildMenuIfPossible(ctx);
    }
    PmStr_Free(actualPath);
    PmStr_Free(pathbuf);
    return TRUE;
}

BOOL Action_ProjectIconify(PmActionContext *ctx)
{
    if(!app || !CurrentMainWindow) return;

    #define DO_ICONIFY 1
    BMainWindow_Close(&app->mainwindow,app->window_obj,DO_ICONIFY);
    return TRUE;
}
BOOL Action_ProjectAbout(PmActionContext *ctx)
{
    Object *req;
    if(!CurrentMainWindow) return;
    if(!app->aboutRequester)
    {
        snprintf(app->aboutrequestertext,2048-1,LOC(MSG_ABOUT_TEXT),PETMATE_VERSION);

        app->aboutRequester = NewObject( REQUESTER_GetClass(), NULL,
			REQ_Image,REQIMAGE_INFO,
			REQ_BodyText,(ULONG)&app->aboutrequestertext[0],
            REQ_TitleText,(ULONG)"About Amiga PetMate",
			REQ_GadgetText,(ULONG)"_Ok",
            TAG_END);
    }
    if(!app->aboutRequester) return FALSE;

    OpenRequester(app->aboutRequester,CurrentMainWindow);

    return TRUE;
}
BOOL Action_ProjectHelp(PmActionContext *ctx)
{
    (void)ctx;

#ifdef HELP_USE_AGLIB

    if(!AmigaGuideBase)
    {
        AmigaGuideBase = OpenLibrary("amigaguide.library", 39);
    }
    if(!AmigaGuideBase) return;

    if(!app->amigaGuideHandle && CurrentMainScreen != NULL)
    {
       // app->nAmigaGuide= {0};
     // app->nAmigaGuide.nag_PubScreen = "Workbench";
        app->nAmigaGuide.nag_Screen = CurrentMainScreen;
        app->nAmigaGuide.nag_Name = "PetMate.guide";
        app->nAmigaGuide.nag_BaseName = "PetMate";
        app->nAmigaGuide.nag_Context = &app->agcontext;
        app->agcontext[0] = "MAIN";
        app->agcontext[1] = NULL;

        app->amigaGuideHandle = OpenAmigaGuideAsync( &app->nAmigaGuide,
        //    AGA_Path,(ULONG)"PROGDIR:",
            AGA_Activate,TRUE,
            TAG_END
                                );
        //AGA_Path

        printf("amigaGuideHandle:%08x\n",app->amigaGuideHandle);
    }

 /* Allocation description structure */
//struct NewAmigaGuide
//{
//    BPTR		 nag_Lock;			/* Lock on the document directory */
//    STRPTR		 nag_Name;			/* Name of document file */
//    struct Screen	*nag_Screen;			/* Screen to place windows within */
//    STRPTR		 nag_PubScreen;			/* Public screen name to open on */
//    STRPTR		 nag_HostPort;			/* Application's ARexx port name */
//    STRPTR		 nag_ClientPort;		/* Name to assign to the clients ARexx port */
//    STRPTR		 nag_BaseName;			/* Base name of the application */
//    ULONG		 nag_Flags;			/* Flags */
//    STRPTR		*nag_Context;			/* NULL terminated context table */
//    STRPTR		 nag_Node;			/* Node to align on first (defaults to Main) */
//    LONG		 nag_Line;			/* Line to align on */
//    struct TagItem	*nag_Extens;			/* Tag array extension */
//    VOID		*nag_Client;			/* Private! MUST be NULL */
//};



#elif defined(HELP_USE_DATATYPE_AND_WINDOW)
    PmHelpView_Open(&app->helpView);
#else
    SystemTagList("multiview PetMate.guide",TAG_END);
#endif




    return TRUE;
}

BOOL Action_ProjectQuit(PmActionContext *ctx)
{
    (void)ctx;
    /* TODO: ask for save if modified */
    exit(0);
    return TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   Edit actions
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
extern void setTool(ULONG newTool);
BOOL Action_Draw1(PmActionContext *ctx)
{
    setTool( TOOL_DRAW );
    return TRUE;
}
BOOL Action_Draw2(PmActionContext *ctx)
{
    setTool( TOOL_COLORIZE );
    return TRUE;
}BOOL Action_Draw3(PmActionContext *ctx)
{
    setTool( TOOL_CHARDRAW );
    return TRUE;
}BOOL Action_Draw4(PmActionContext *ctx)
{
    setTool( TOOL_LASSOBRUSH );
    return TRUE;
}BOOL Action_Draw5(PmActionContext *ctx)
{
    setTool( TOOL_TEXT );
    return TRUE;
}
BOOL Action_EditUndo(PmActionContext *ctx)
{
    PetsciiProject     *proj;
    PetsciiScreen      *scr;

//    printf("Action_EditUndo\n");

    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;

    proj = *ctx->pproject;
    scr  = PetsciiProject_GetCurrentScreen(proj);

    if (!scr) return FALSE;

    if (PetsciiUndoBuffer_Undo( scr)) {
        proj->modified = 1;
        refreshUI();
        return TRUE;
    }

    refreshUI();

    return FALSE;
}

BOOL Action_EditRedo(PmActionContext *ctx)
{
    PetsciiProject     *proj;
    PetsciiScreen      *scr;

//printf("Action_EditRedo\n");

    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;

    proj = *ctx->pproject;
    scr  = PetsciiProject_GetCurrentScreen(proj);

    if (!scr) return FALSE;

    if (PetsciiUndoBuffer_Redo( scr)) {
        proj->modified = 1;
        refreshUI();
        return TRUE;
    }
    refreshUI();

    return FALSE;
}

BOOL Action_EditCopyScreen(PmActionContext *ctx)
{
    PetsciiProject *proj;
    PetsciiScreen  *src;
    PetsciiScreen **clip;

    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;
    if (!ctx->clipScreen) return FALSE;

    proj = *ctx->pproject;
    src  = PetsciiProject_GetCurrentScreen(proj);
    clip = (PetsciiScreen **)ctx->clipScreen;

    if (!src) return FALSE;

    /* Discard any previous clipboard */
    if (*clip) {
        PetsciiScreen_Destroy(*clip);
        *clip = NULL;
    }

    *clip = PetsciiScreen_Clone(src);

    refreshUI();

    return (*clip != NULL);
}

BOOL Action_EditPasteScreen(PmActionContext *ctx)
{
    PetsciiProject *proj;
    PetsciiScreen  *dst;
    PetsciiScreen  *clip;

    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;
    if (!ctx->clipScreen) return FALSE;

    proj = *ctx->pproject;
    dst  = PetsciiProject_GetCurrentScreen(proj);
    clip = *(PetsciiScreen **)ctx->clipScreen;

    if (!dst || !clip) return FALSE;
    if (dst->width != clip->width || dst->height != clip->height) return FALSE;

    PetsciiScreen_CopyData(dst, clip);
    proj->modified = 1;
    return TRUE;
}

BOOL Action_EditClearScreen(PmActionContext *ctx)
{
    PetsciiProject *proj;
    PetsciiScreen  *scr;

 //printf("Action_EditClearScreen\n");

    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;

    proj = *ctx->pproject;
    scr  = PetsciiProject_GetCurrentScreen(proj);
    if (!scr) return FALSE;

    PetsciiScreen_Clear(scr, scr->borderColor);
    proj->modified = 1;
    return TRUE;
}

BOOL Action_EditShiftLeft(PmActionContext *ctx)
{
    PetsciiProject *proj;
    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;

    proj = *ctx->pproject;
    PetsciiScreen_ShiftLeft(PetsciiProject_GetCurrentScreen(proj));
    proj->modified = 1;
    return TRUE;
}

BOOL Action_EditShiftRight(PmActionContext *ctx)
{
    PetsciiProject *proj;
    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;

    proj = *ctx->pproject;
    PetsciiScreen_ShiftRight(PetsciiProject_GetCurrentScreen(proj));
    proj->modified = 1;
    return TRUE;
}

BOOL Action_EditShiftUp(PmActionContext *ctx)
{
    PetsciiProject *proj;
    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;

    proj = *ctx->pproject;
    PetsciiScreen_ShiftUp(PetsciiProject_GetCurrentScreen(proj));
    proj->modified = 1;
    return TRUE;
}

BOOL Action_EditShiftDown(PmActionContext *ctx)
{
    PetsciiProject *proj;
    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;

    proj = *ctx->pproject;
    PetsciiScreen_ShiftDown(PetsciiProject_GetCurrentScreen(proj));
    proj->modified = 1;
    return TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   Screen management actions
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

BOOL Action_ScreenAdd(PmActionContext *ctx)
{
    int idx;
    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;

    idx = PetsciiProject_AddScreen(*ctx->pproject);
    if (idx < 0) return FALSE;

    PetsciiProject_SetCurrentScreen(*ctx->pproject, (UWORD)idx);
    return TRUE;
}

BOOL Action_ScreenClone(PmActionContext *ctx)
{
    int idx;
    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;

    idx = PetsciiProject_CloneCurrentScreen(*ctx->pproject);
    if (idx < 0) return FALSE;

    PetsciiProject_SetCurrentScreen(*ctx->pproject, (UWORD)idx);
    return TRUE;
}

BOOL Action_ScreenRemove(PmActionContext *ctx)
{
    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;
    return (BOOL)PetsciiProject_RemoveScreen(*ctx->pproject,
                     (*ctx->pproject)->currentScreen);
}

BOOL Action_ScreenPrev(PmActionContext *ctx)
{
    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;
    PetsciiProject_NavigateScreen(*ctx->pproject, -1);
    return TRUE;
}

BOOL Action_ScreenNext(PmActionContext *ctx)
{
    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;
    PetsciiProject_NavigateScreen(*ctx->pproject, +1);
    return TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   View actions
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

BOOL Action_ViewToggleGrid(PmActionContext *ctx)
{
    ToolState *ts;
    if (!ctx || !ctx->toolState) return FALSE;

    ts = (ToolState *)ctx->toolState;
    ts->showGrid = (UBYTE)(ts->showGrid ? 0 : 1);

    SetGdAttrs(app->canvasGadget,
                PCA_ShowGrid,ts->showGrid,TAG_END);

    return TRUE;
}

BOOL Action_ViewCharsetUpper(PmActionContext *ctx)
{
    SetGdAttrs(
        (struct Gadget *)app->charsetUpperBtn,
        GA_Selected, TRUE, TAG_END);

    return TRUE;
}

BOOL Action_ViewCharsetLower(PmActionContext *ctx)
{
    SetGdAttrs(
        (struct Gadget *)app->charsetLowerBtn,
        GA_Selected, TRUE, TAG_END);
    return TRUE;
}


BOOL Action_ViewToggleFullScreen(PmActionContext *ctx)
{
    BMainWindow_Toggle(
        &app->mainwindow,
        app->window_obj,
        &app->appSettings);

    return TRUE;
}


BOOL Action_OpenSettings(PmActionContext *ctx)
{

    PmSettingsView_Open(&app->settingsView);
    return TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   Palette actions
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


BOOL Action_PalettePetmate(PmActionContext *ctx)  { PmAction_ApplyPalette(ctx, PALETTE_PETMATE);  return TRUE; }
BOOL Action_PaletteColodore(PmActionContext *ctx) { PmAction_ApplyPalette(ctx, PALETTE_COLODORE); return TRUE; }
BOOL Action_PalettePepto(PmActionContext *ctx)    { PmAction_ApplyPalette(ctx, PALETTE_PEPTO);    return TRUE; }
BOOL Action_PaletteVice(PmActionContext *ctx)     { PmAction_ApplyPalette(ctx, PALETTE_VICE);     return TRUE; }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   Brush transform actions
   Applies a geometric transformation to the current brush in PetsciiCanvas.
   No-op if no brush is active (canvas ignores PCA_TransformBrush then).
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static BOOL brushTransform(int xform)
{
    if (!app || !app->canvasGadget) return FALSE;
    SetAttrs(app->canvasGadget,
             PCA_TransformBrush, (ULONG)xform,
             TAG_END);
    RefreshGList((struct Gadget *)app->canvasGadget,
                 CurrentMainWindow, NULL, 1);
    return TRUE;
}

BOOL Action_BrushFlipX(PmActionContext *ctx)    { (void)ctx; return brushTransform(BRUSH_TRANSFORM_FLIP_X);   }
BOOL Action_BrushFlipY(PmActionContext *ctx)    { (void)ctx; return brushTransform(BRUSH_TRANSFORM_FLIP_Y);   }
BOOL Action_BrushRot90CW(PmActionContext *ctx)  { (void)ctx; return brushTransform(BRUSH_TRANSFORM_ROT90CW);  }
BOOL Action_BrushRot180(PmActionContext *ctx)   { (void)ctx; return brushTransform(BRUSH_TRANSFORM_ROT180);   }
BOOL Action_BrushRot90CCW(PmActionContext *ctx) { (void)ctx; return brushTransform(BRUSH_TRANSFORM_ROT90CCW); }

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   Export actions
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* Helper: ASL save requester for export.  Uses s_lastDir for persistence.
 * Returns an AllocVec'd full path string on success, NULL if cancelled.
 * Caller must PmStr_Free() the result.                                     */
static char *aslExportRequest(const char *title, const char *pattern)
{
    struct FileRequester *req;
    char  *result = NULL;
    BOOL   ok;
    ULONG  dirLen;
    ULONG  fileLen;
    char  *buf;

    req = (struct FileRequester *)AllocAslRequestTags(ASL_FileRequest, TAG_END);
    if (!req) return NULL;

    ok = (BOOL)AslRequestTags(req,
            ASLFR_Window,         (ULONG)CurrentMainWindow,
            ASLFR_TitleText,      (ULONG)title,
            ASLFR_InitialPattern, (ULONG)pattern,
            ASLFR_DoPatterns,     TRUE,
            ASLFR_DoSaveMode,     TRUE,
            s_lastDir ? ASLFR_InitialDrawer : TAG_IGNORE,
            s_lastDir ? (ULONG)s_lastDir    : 0UL,
            TAG_END);

    if (ok) {
        PmStr_Free(s_lastDir);
        s_lastDir = PmStr_Alloc(req->rf_Dir);

        dirLen  = (ULONG)strlen(req->rf_Dir);
        fileLen = (ULONG)strlen(req->rf_File);
        buf = (char *)AllocVec(dirLen + fileLen + 2, MEMF_ANY);
        if (buf) {
            CopyMem((APTR)req->rf_Dir, (APTR)buf, dirLen + 1);
            AddPart(buf, req->rf_File, dirLen + fileLen + 2);
            result = buf;
        }
    }

    FreeAslRequest(req);
    return result;
}

BOOL Action_ExportBAS(PmActionContext *ctx)
{
    char          *rawpath;
    char          *pathbuf;
    PetsciiScreen *scr;
    BOOL           ok;

    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;
    scr = PetsciiProject_GetCurrentScreen(*ctx->pproject);
    if (!scr) return FALSE;

    rawpath = aslExportRequest("Export as BASIC (.bas)", "#?.bas");
    if (!rawpath) return FALSE;

    pathbuf = PmStr_WithExt(rawpath, ".bas");
    PmStr_Free(rawpath);
    if (!pathbuf) return FALSE;

    ok = (BOOL)(PetsciiExport_SaveBAS(scr, pathbuf) == PETSCII_EXPORT_OK);
    PmStr_Free(pathbuf);
    SetStatusBarMessage(ok ? MSG_PETSCII_FILEIO_WRITEOK : MSG_PETSCII_FILEIO_EWRITE);
    return ok;
}

BOOL Action_ExportASM(PmActionContext *ctx)
{
    char          *rawpath;
    char          *pathbuf;
    PetsciiScreen *scr;
    BOOL           ok;

    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;
    scr = PetsciiProject_GetCurrentScreen(*ctx->pproject);
    if (!scr) return FALSE;

    rawpath = aslExportRequest("Export as ASM (.asm)", "#?.asm");
    if (!rawpath) return FALSE;

    pathbuf = PmStr_WithExt(rawpath, ".asm");
    PmStr_Free(rawpath);
    if (!pathbuf) return FALSE;

    ok = (BOOL)(PetsciiExport_SaveASM(scr, pathbuf) == PETSCII_EXPORT_OK);
    PmStr_Free(pathbuf);
    SetStatusBarMessage(ok ? MSG_PETSCII_FILEIO_WRITEOK : MSG_PETSCII_FILEIO_EWRITE);
    return ok;
}

// BOOL Action_ExportSEQ(PmActionContext *ctx)
// {
//     char          *rawpath;
//     char          *pathbuf;
//     PetsciiScreen *scr;
//     BOOL           ok;

//     if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;
//     scr = PetsciiProject_GetCurrentScreen(*ctx->pproject);
//     if (!scr) return FALSE;

//     rawpath = aslExportRequest("Export as SEQ (.seq)", "#?.seq");
//     if (!rawpath) return FALSE;

//     pathbuf = PmStr_WithExt(rawpath, ".seq");
//     PmStr_Free(rawpath);
//     if (!pathbuf) return FALSE;

//     ok = (BOOL)(PetsciiExport_SaveSEQ(scr, pathbuf) == PETSCII_EXPORT_OK);
//     PmStr_Free(pathbuf);
//     SetStatusBarMessage(ok ? MSG_PETSCII_FILEIO_WRITEOK : MSG_PETSCII_FILEIO_EWRITE);
//     return ok;
// }

BOOL Action_ExportPrgBAS(PmActionContext *ctx)
{
    char          *rawpath;
    char          *pathbuf;
    PetsciiScreen *scr;
    BOOL           ok;

    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;
    scr = PetsciiProject_GetCurrentScreen(*ctx->pproject);
    if (!scr) return FALSE;

    rawpath = aslExportRequest("Export PRG with BASIC (.prg)", "#?.prg");
    if (!rawpath) return FALSE;

    pathbuf = PmStr_WithExt(rawpath, ".prg");
    PmStr_Free(rawpath);
    if (!pathbuf) return FALSE;

    ok = (BOOL)(PetsciiExport_SavePrgBAS(scr, pathbuf) == PETSCII_EXPORT_OK);
    PmStr_Free(pathbuf);
    SetStatusBarMessage(ok ? MSG_PETSCII_FILEIO_WRITEOK : MSG_PETSCII_FILEIO_EWRITE);
    return ok;
}

BOOL Action_ExportPrgASM(PmActionContext *ctx)
{
    char          *rawpath;
    char          *pathbuf;
    PetsciiScreen *scr;
    BOOL           ok;

    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;
    scr = PetsciiProject_GetCurrentScreen(*ctx->pproject);
    if (!scr) return FALSE;

    rawpath = aslExportRequest("Export PRG from ASM (.prg)", "#?.prg");
    if (!rawpath) return FALSE;

    pathbuf = PmStr_WithExt(rawpath, ".prg");
    PmStr_Free(rawpath);
    if (!pathbuf) return FALSE;

    ok = (BOOL)(PetsciiExport_SavePrgASM(scr, pathbuf) == PETSCII_EXPORT_OK);
    PmStr_Free(pathbuf);
    SetStatusBarMessage(ok ? MSG_PETSCII_FILEIO_WRITEOK : MSG_PETSCII_FILEIO_EWRITE);
    return ok;
}

extern struct Library *DataTypesBase;
int PetsciiExport_SaveDatatype(const PetsciiScreen *scr, const char *filename, int exportDTFenum);

BOOL Action_ExportIFFILBM(PmActionContext *ctx)
{
    char              *rawpath;
    char              *pathbuf;
    PetsciiScreen     *scr;
    const PetsciiStyle *style;
    int                err;

    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;
    scr   = PetsciiProject_GetCurrentScreen(*ctx->pproject);
    style = (const PetsciiStyle *)ctx->style;
    if (!scr || !style) return FALSE;

    rawpath = aslExportRequest("Export IFF image (.ilbm)", "#?.ilbm");
    if (!rawpath) return FALSE;

    pathbuf = PmStr_WithExt(rawpath, ".ilbm");
    PmStr_Free(rawpath);
    if (!pathbuf) return FALSE;

    err = PetsciiExport_SaveILBM(scr, style, pathbuf, FALSE);
    PmStr_Free(pathbuf);
    SetStatusBarMessage(err == PETSCII_EXPORT_OK
                        ? MSG_PETSCII_FILEIO_WRITEOK
                        : MSG_PETSCII_FILEIO_EWRITE);
    return (BOOL)(err == PETSCII_EXPORT_OK);
}
BOOL Action_ExportGif(PmActionContext *ctx)
{
    char               *rawpath;
    char               *pathbuf;
    PetsciiScreen      *scr;
    const PetsciiStyle *style;
    int                 err;

    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;
    scr   = PetsciiProject_GetCurrentScreen(*ctx->pproject);
    style = (const PetsciiStyle *)ctx->style;
    if (!scr || !style) return FALSE;

    rawpath = aslExportRequest("Export as GIF (.gif)", "#?.gif");
    if (!rawpath) return FALSE;

    pathbuf = PmStr_WithExt(rawpath, ".gif");
    PmStr_Free(rawpath);
    if (!pathbuf) return FALSE;

    err = PetsciiExport_SaveGIF(scr, style, pathbuf, FALSE);
    PmStr_Free(pathbuf);
    SetStatusBarMessage(err == PETSCII_EXPORT_OK
                        ? MSG_PETSCII_FILEIO_WRITEOK
                        : MSG_PETSCII_FILEIO_EWRITE);
    return (BOOL)(err == PETSCII_EXPORT_OK);
}
BOOL Action_ExportPng(PmActionContext *ctx)
{
/*
   char          *rawpath;
    char          *pathbuf;
    PetsciiScreen *scr;
    BOOL           ok;

    if(!DataTypesBase)
    {
        SetStatusBarMessage(MSG_PETSCII_FILEIO_NEED_DT);
        return FALSE;
    }

    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;
    scr = PetsciiProject_GetCurrentScreen(*ctx->pproject);
    if (!scr) return FALSE;

    rawpath = aslExportRequest("Export as PNG (.png)", "#?.png");
    if (!rawpath) return FALSE;

    pathbuf = PmStr_WithExt(rawpath, ".png");
    PmStr_Free(rawpath);
    if (!pathbuf) return FALSE;

    ok = (BOOL)(PetsciiExport_SaveDatatype(scr, pathbuf,2) == PETSCII_EXPORT_OK);
    PmStr_Free(pathbuf);
    SetStatusBarMessage(ok ? MSG_PETSCII_FILEIO_WRITEOK : MSG_PETSCII_FILEIO_EWRITE);
    return ok;*/
    return FALSE;
}


BOOL Action_ImportImage(PmActionContext *ctx)
{
    char               *rawpath;
    char               *pathbuf;
    PetsciiScreen      *scr;
    const PetsciiStyle *style;
    int                 err;

    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;
    scr   = PetsciiProject_GetCurrentScreen(*ctx->pproject);
    style = (const PetsciiStyle *)ctx->style;
    if (!scr || !style) return FALSE;

    rawpath = aslExportRequest("Import image as PETSCII", "#?.(png|gif|bmp|iff|ilbm|jpg|jpeg)");
    if (!rawpath) return FALSE;

    pathbuf = rawpath;  /* use path as-is (no forced extension for import) */

    err = PetsciiImport_FromImage(pathbuf, scr, style);
    PmStr_Free(rawpath);

    if (err == PETSCII_IMPORT_ENOMATCH || err == PETSCII_IMPORT_ESIZE) {
        SetStatusBarMessage(MSG_IMPORT_NOMATCH);
        return FALSE;
    }
    SetStatusBarMessage(err == PETSCII_IMPORT_OK
                        ? MSG_PETSCII_FILEIO_WRITEOK
                        : MSG_PETSCII_FILEIO_EWRITE);
    return (BOOL)(err == PETSCII_IMPORT_OK);
}

BOOL Action_ImportPrg(PmActionContext *ctx)
{
    char          *rawpath;
    PetsciiScreen *scr;
    int            err;

    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;
    scr = PetsciiProject_GetCurrentScreen(*ctx->pproject);
    if (!scr) return FALSE;

    rawpath = aslExportRequest(LOC(MSG_IMPORT_PRG), "#?.prg");
    if (!rawpath) return FALSE;

    err = PetsciiImport_FromPrg(rawpath, scr);
    PmStr_Free(rawpath);

    if (err == PETSCII_IMPORT_PRG_EFORMAT || err == PETSCII_IMPORT_PRG_ESIZE) {
        SetStatusBarMessage(MSG_IMPORT_PRG_BADFORMAT);
        return FALSE;
    }
    if (err != PETSCII_IMPORT_PRG_OK) {
        SetStatusBarMessage(MSG_PETSCII_FILEIO_EOPEN);
        return FALSE;
    }

    (*ctx->pproject)->modified = 1;
    refreshUI();
    SetStatusBarMessage(MSG_PETSCII_FILEIO_OK);
    return TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   Recent files helper
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void rebuildMenuIfPossible(PmActionContext *ctx)
{
    if (ctx->pmenu && CurrentMainWindow && CurrentMainScreen) {
        PmMenu_Rebuild((PmMenu *)ctx->pmenu,
                       CurrentMainScreen, CurrentMainWindow,
                       &app->appSettings);
    }
}

static BOOL openRecentFile(PmActionContext *ctx, int slot)
{
    const char *path;
    int err;

    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;
    if (!app) return FALSE;

    path = AppSettings_GetRecentFile(&app->appSettings, slot);
    if (!path) return FALSE;

    err = PetsciiFileIO_Load(*ctx->pproject, path);
    SetStatusBarMessage(MSG_PETSCII_FILEIO_OK + err);

    if (err == PETSCII_FILEIO_OK) {
        AppSettings_AddRecentFile(&app->appSettings, path);
        rebuildMenuIfPossible(ctx);
    }
    refreshUI();
    return (BOOL)(err == PETSCII_FILEIO_OK);
}

BOOL Action_OpenRecent0(PmActionContext *ctx) { return openRecentFile(ctx, 0); }
BOOL Action_OpenRecent1(PmActionContext *ctx) { return openRecentFile(ctx, 1); }
BOOL Action_OpenRecent2(PmActionContext *ctx) { return openRecentFile(ctx, 2); }
BOOL Action_OpenRecent3(PmActionContext *ctx) { return openRecentFile(ctx, 3); }
BOOL Action_Dummy(PmActionContext *ctx) { return FALSE; }


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   Generate actions
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*
 * Action_GenerateRandomFromBrush
 *
 * Fills every cell of the current screen with a randomly chosen cell from the
 * current brush.  The draw tool controls what is applied to each cell:
 *   TOOL_DRAW      - char AND color from the brush (default)
 *   TOOL_COLORIZE  - color only from the brush
 *   TOOL_CHARDRAW  - char only from the brush (existing color kept)
 *
 * If no brush is active (brush == NULL or empty), a random character is chosen
 * between code 77 and 78 (the two diagonal half-block glyphs); the color is
 * left unchanged regardless of the current tool.
 */
BOOL Action_GenerateRandomFromBrush(PmActionContext *ctx)
{
    PetsciiProject *proj;
    PetsciiScreen  *scr;
    ToolState      *ts;
    PetsciiBrush   *brush;
    ULONG           brushVal;
    ULONG           cellCount;
    ULONG           i;
    UBYTE           currentTool;
    int             brushCells;

    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;

    proj = *ctx->pproject;
    scr  = PetsciiProject_GetCurrentScreen(proj);
    if (!scr) return FALSE;

    /* Push undo snapshot before modifying the screen */

    PetsciiUndoBuffer_Push(scr);


    ts = (ToolState *)ctx->toolState;
    currentTool = ts ? ts->currentTool : TOOL_DRAW;

    /* Retrieve the brush from the canvas gadget */
    brush    = NULL;
    brushVal = 0;
    if (app && app->canvasGadget)
        GetAttr(PCA_Brush, app->canvasGadget, &brushVal);
    brush = (PetsciiBrush *)brushVal;

    /* Validate brush: must have cells and non-zero dimensions */
    if (brush && (!brush->cells || brush->w == 0 || brush->h == 0))
        brush = NULL;

    brushCells = brush ? (int)brush->w * (int)brush->h : 0;
    cellCount  = (ULONG)scr->width * (ULONG)scr->height;

    for (i = 0; i < cellCount; i++) {
        PetsciiPixel *px = &scr->framebuf[i];

        if (brush) {
            int           srcIdx = (int)pmRand((ULONG)brushCells);
            PetsciiPixel *src    = &brush->cells[srcIdx];

            switch (currentTool) {
                case TOOL_COLORIZE:
                    px->color = src->color;
                    break;
                case TOOL_CHARDRAW:
                    px->code = src->code;
                    break;
                default:    /* TOOL_DRAW: char and color */
                    px->code  = src->code;
                    px->color = src->color;
                    break;
            }
        } else {
            /* No brush: randomly pick char 77 or 78 */
            if (currentTool != TOOL_COLORIZE)
                px->code = (UBYTE)(77 + (int)pmRand(2));
        }
    }

    proj->modified = 1;

    /* Refresh canvas */
    if (app && app->canvasGadget) {
        SetAttrs(app->canvasGadget, PCA_Dirty, TRUE, TAG_END);
        RefreshGList((struct Gadget *)app->canvasGadget,
                     CurrentMainWindow, NULL, 1);
    }
    return TRUE;
}

/* char link tables for magic lines */
#define CLINK_UP 	1
#define CLINK_DOWN	2
#define CLINK_LEFT	4
#define CLINK_RIGHT	8
/* the 16 cases where upper charset chars are a line that goes from a border to another, or cross another line */
static UBYTE charlinelinks[16]={
	32, // (empty char)
	33, // CLINK_UP  (terminate line)
	58, // CLINK_DOWN (terminate line)
	66, // CLINK_UP|CLINK_DOWN
	45, // CLINK_LEFT (terminate line)
	75, // CLINK_LEFT|CLINK_UP
	73, // CLINK_LEFT|CLINK_DOWN
	115, // CLINK_LEFT|CLINK_UP|CLINK_DOWN

	45, // CLINK_RIGHT (terminate line)
	74, // CLINK_RIGHT|CLINK_UP
	85, // CLINK_RIGHT|CCLINK_DOWN
	107, // CLINK_RIGHT|CCLINK_UP|CLINK_DOWN
	64, // CLINK_RIGHT|CCLINK_LEFT
	113, // CLINK_RIGHT|CCLINK_LEFT|CLINK_UP
	114, // CLINK_RIGHT|CCLINK_LEFT|CLINK_DOWN
	91, // CLINK_RIGHT|CCLINK_LEFT|CLINK_UP|CLINK_DOWN
};


/* =======================================================================
 * Magic Line � common infrastructure
 *
 * connMap: a UBYTE array parallel to scr->framebuf.
 *   0x00        cell is empty (code==32), available for drawing.
 *   0x01-0x0F   live CLINK_* connection bits OR'd in so far.
 *   ML_OBSTACLE cell was non-empty on entry; never modified.
 *
 * Direction encoding used throughout:
 *   0 = UP   (row-1)
 *   1 = DOWN (row+1)
 *   2 = LEFT (col-1)
 *   3 = RIGHT(col+1)
 * ======================================================================= */
#define ML_OBSTACLE 0x80

static const UBYTE s_ml_clinkbit[4] = { CLINK_UP, CLINK_DOWN, CLINK_LEFT, CLINK_RIGHT };
static const int   s_ml_dc[4]       = {  0,  0, -1,  1 };
static const int   s_ml_dr[4]       = { -1,  1,  0,  0 };
static const int   s_ml_opp[4]      = {  1,  0,  3,  2 };

static void mlInitMap(const PetsciiScreen *scr, UBYTE *connMap)
{
    int i;
    int total = (int)scr->width * (int)scr->height;
    for (i = 0; i < total; i++)
        connMap[i] = (scr->framebuf[i].code == 32) ? 0 : ML_OBSTACLE;
}

/* OR bits into cell, update screen char.  Returns TRUE if cell was empty. */
static BOOL mlPlace(PetsciiScreen *scr, UBYTE *connMap,
                    int col, int row, UBYTE bits, UBYTE color)
{
    int  idx = row * (int)scr->width + col;
    BOOL was;
    if (connMap[idx] & ML_OBSTACLE) return FALSE;
    was = (connMap[idx] == 0);
    connMap[idx] |= bits;
    scr->framebuf[idx].code  = charlinelinks[connMap[idx] & 0x0F];
    scr->framebuf[idx].color = color;
    return was;
}

/* ----------------------------------------------------------------------- *
 * Algorithm A � Random worm walk                                           *
 *                                                                          *
 * Multiple short worms start at random empty cells and wander freely,     *
 * changing direction ~20% of the time.  A new worm is spawned whenever    *
 * the current one gets stuck.  Crossing an existing line OR-s the new     *
 * connection bits to produce the correct junction character automatically. *
 * ----------------------------------------------------------------------- */
static void mlWalkRandom(PetsciiScreen *scr, UBYTE *connMap,
                         int percent, UBYTE color)
{
    int   total  = (int)scr->width * (int)scr->height;
    int   target = (total * percent) / 100;
    int   filled = 0;
    int   tries    = 0;
    int   steps    = 0;
    int   col, row, dir, nc, nr, blocked;
    UBYTE prevBit;

    while (filled < target && tries < total * 4) {
        col = (int)pmRand((ULONG)scr->width);
        row = (int)pmRand((ULONG)scr->height);
        if (connMap[row * (int)scr->width + col] != 0) { tries++; continue; }

        dir     = (int)pmRand(4);
        prevBit = 0;
        blocked = 0;
        steps   = 0;

        /* Step budget = total cells: each worm can visit every cell at most once */
        while (blocked < 4 && steps < total) {
            nc = col + s_ml_dc[dir];
            nr = row + s_ml_dr[dir];
            if (nc < 0 || nc >= (int)scr->width ||
                nr < 0 || nr >= (int)scr->height ||
                (connMap[nr * (int)scr->width + nc] & ML_OBSTACLE)) {
                dir = (dir + 1) & 3;
                blocked++;
                continue;
            }
            blocked = 0;
            steps++;
            if (mlPlace(scr, connMap, col, row, prevBit | s_ml_clinkbit[dir], color))
                filled++;
            prevBit = s_ml_clinkbit[s_ml_opp[dir]];
            col = nc;
            row = nr;
            /* ~20% chance: turn left or right */
            if (pmRand(5) == 0)
                dir = (pmRand(2) == 0) ? (dir + 1) & 3 : (dir + 3) & 3;
        }
        if (mlPlace(scr, connMap, col, row, prevBit, color))
            filled++;
        tries++;
    }
}

/* ----------------------------------------------------------------------- *
 * Algorithm B � Reconnecting worm walk                                    *
 *                                                                          *
 * Same as A but 40% of steps bias the direction toward the worm's own     *
 * start cell, encouraging closed loops.  When the worm reaches start      *
 * the loop is sealed and a new worm begins elsewhere.                      *
 * ----------------------------------------------------------------------- */
static void mlWalkReconnect(PetsciiScreen *scr, UBYTE *connMap,
                             int percent, UBYTE color)
{
    int   total  = (int)scr->width * (int)scr->height;
    int   target = (total * percent) / 100;
    int   filled = 0;
    int   tries  = 0;
    int   steps  = 0;
    int   startC, startR, col, row, dir, nc, nr;
    int   blocked, d, dC, dR, dist, bestDist, bestDir;
    UBYTE prevBit;

    while (filled < target && tries < total * 4) {
        startC = (int)pmRand((ULONG)scr->width);
        startR = (int)pmRand((ULONG)scr->height);
        if (connMap[startR * (int)scr->width + startC] != 0) { tries++; continue; }

        col     = startC;
        row     = startR;
        dir     = (int)pmRand(4);
        prevBit = 0;
        blocked = 0;
        steps   = 0;

        while (blocked < 4 && steps < total) {
            /* 40% of steps: steer toward start using best reachable direction */
            if (pmRand(10) < 4 && (col != startC || row != startR)) {
                bestDist = 0x7FFF;
                bestDir  = -1;  /* -1 = none found yet */
                for (d = 0; d < 4; d++) {
                    nc = col + s_ml_dc[d];
                    nr = row + s_ml_dr[d];
                    if (nc < 0 || nc >= (int)scr->width ||
                        nr < 0 || nr >= (int)scr->height ||
                        (connMap[nr * (int)scr->width + nc] & ML_OBSTACLE)) continue;
                    dC = nc - startC;  dR = nr - startR;
                    dist = dC * dC + dR * dR;
                    if (dist < bestDist) { bestDist = dist; bestDir = d; }
                }
                /* Only steer if a valid closer direction was found */
                if (bestDir >= 0) dir = bestDir;
            }

            nc = col + s_ml_dc[dir];
            nr = row + s_ml_dr[dir];
            if (nc < 0 || nc >= (int)scr->width ||
                nr < 0 || nr >= (int)scr->height ||
                (connMap[nr * (int)scr->width + nc] & ML_OBSTACLE)) {
                dir = (dir + 1) & 3;
                blocked++;
                continue;
            }
            blocked = 0;

            /* Returned to start: close the loop */
            if (nc == startC && nr == startR && prevBit != 0) {
                if (mlPlace(scr, connMap, col, row,
                            prevBit | s_ml_clinkbit[dir], color))
                    filled++;
                mlPlace(scr, connMap, startC, startR,
                        s_ml_clinkbit[s_ml_opp[dir]], color);
                break;
            }

            steps++;
            if (mlPlace(scr, connMap, col, row,
                        prevBit | s_ml_clinkbit[dir], color))
                filled++;
            prevBit = s_ml_clinkbit[s_ml_opp[dir]];
            col = nc;
            row = nr;
            if (pmRand(4) == 0)
                dir = (pmRand(2) == 0) ? (dir + 1) & 3 : (dir + 3) & 3;
        }
        if ((blocked >= 4 || steps >= total) && (col != startC || row != startR)) {
            if (mlPlace(scr, connMap, col, row, prevBit, color))
                filled++;
        }
        tries++;
    }
}

/* ----------------------------------------------------------------------- *
 * Algorithm C � Recursive fractal quad-cross                              *
 *                                                                          *
 * Draws a cross at the midpoint of a rectangle, then recurses on the four *
 * quadrants in shuffled order.  Recursion depth is derived from            *
 * log2(max_dim) so the pattern always adapts to the screen size.           *
 * Stops early when the fill target is reached.                             *
 * ----------------------------------------------------------------------- */
static void mlFractalRec(PetsciiScreen *scr, UBYTE *connMap,
                          int x, int y, int w, int h,
                          int depth, UBYTE color,
                          int *filledPtr, int target)
{
    int   midC, midR, c, r;
    int   order[4], i, j, tmp;
    UBYTE bits;

    if (depth <= 0 || w < 2 || h < 2 || *filledPtr >= target) return;
    midC = x + w / 2;
    midR = y + h / 2;

    for (r = y; r < y + h; r++) {          /* vertical bar at midC */
        bits = 0;
        if (r > y)       bits |= CLINK_UP;
        if (r < y + h-1) bits |= CLINK_DOWN;
        if (mlPlace(scr, connMap, midC, r, bits, color)) (*filledPtr)++;
    }
    for (c = x; c < x + w; c++) {          /* horizontal bar at midR */
        bits = 0;
        if (c > x)       bits |= CLINK_LEFT;
        if (c < x + w-1) bits |= CLINK_RIGHT;
        if (mlPlace(scr, connMap, c, midR, bits, color)) (*filledPtr)++;
    }

    /* Shuffle quadrant order so partial fills look asymmetric / organic */
    order[0] = 0;  order[1] = 1;  order[2] = 2;  order[3] = 3;
    for (i = 3; i > 0; i--) {
        j = (int)pmRand((ULONG)(i + 1));
        tmp = order[i];  order[i] = order[j];  order[j] = tmp;
    }
    for (i = 0; i < 4; i++) {
        if (*filledPtr >= target) return;
        switch (order[i]) {
            case 0:
                mlFractalRec(scr, connMap, x, y,
                             midC - x, midR - y,
                             depth-1, color, filledPtr, target);
                break;
            case 1:
                mlFractalRec(scr, connMap, midC+1, y,
                             x + w - midC - 1, midR - y,
                             depth-1, color, filledPtr, target);
                break;
            case 2:
                mlFractalRec(scr, connMap, x, midR+1,
                             midC - x, y + h - midR - 1,
                             depth-1, color, filledPtr, target);
                break;
            case 3:
                mlFractalRec(scr, connMap, midC+1, midR+1,
                             x + w - midC - 1, y + h - midR - 1,
                             depth-1, color, filledPtr, target);
                break;
        }
    }
}

static void mlFractal(PetsciiScreen *scr, UBYTE *connMap,
                      int percent, UBYTE color)
{
    int total  = (int)scr->width * (int)scr->height;
    int target = (total * percent) / 100;
    int filled = 0;
    int maxDim = (scr->width > scr->height) ? (int)scr->width : (int)scr->height;
    int depth  = 0;
    int d      = maxDim;
    while (d > 1 && depth < 8) { d >>= 1; depth++; }
    mlFractalRec(scr, connMap, 0, 0,
                 (int)scr->width, (int)scr->height,
                 depth, color, &filled, target);
}

/* ----------------------------------------------------------------------- *
 * Algorithm D Tron light-cycle trajectories (Action_GenerateTronLines)  *
 *                                                                          *
 * Four players start from the four screen corners heading inward.          *
 * Each moves straight and turns 90 degrees (left or right) after a random  *
 * number of straight steps, or when forced to by a wall or screen edge.    *
 * Paths may cross, generating junction characters.  A player is eliminated *
 * when all four directions are blocked.                                     *
 * ----------------------------------------------------------------------- */
static void mlTron(PetsciiScreen *scr, UBYTE *connMap,
                   int maxChars, UBYTE color)
{
    int   target   = maxChars;
    int   filled   = 0;
    int   aliveCnt = 4;
    int   i, d, newDir, nc, nr, col, row, dir, blocked;
    int   cols[4], rows[4], dirs[4], straight[4];
    UBYTE prevBit[4];
    BOOL  alive[4];

    /* Place each player at a random empty cell with a random initial direction.
     * Using fixed corners fails after the first call since the corners and
     * their neighbours are already filled and the players are immediately trapped. */
    {
        int total = (int)scr->width * (int)scr->height;
        int t, tc, tr;
        for (i = 0; i < 4; i++) {
            cols[i] = -1;
            rows[i] = -1;
            dirs[i] = (int)pmRand(4);
            for (t = 0; t < total * 2; t++) {
                tc = (int)pmRand((ULONG)scr->width);
                tr = (int)pmRand((ULONG)scr->height);
                if (connMap[tr * (int)scr->width + tc] == 0) {
                    cols[i] = tc;
                    rows[i] = tr;
                    break;
                }
            }
        }
    }

    for (i = 0; i < 4; i++) {
        prevBit[i]  = 0;
        straight[i] = 0;
        alive[i]    = (cols[i] >= 0);  /* FALSE if no empty cell was found */
        if (!alive[i]) {
            aliveCnt--;
            //printf("mlTron: player %d could not find an empty start cell\n", i);
        }
    }
    /*
    if (aliveCnt == 0)
        printf("mlTron: no players could start � screen may be full\n");
    */
    /*
     * In Tron, any cell that already has connection bits is treated as a wall:
     * the light trail itself becomes an obstacle.  connMap == 0 is the only
     * passable state (empty cell not yet visited by anyone).
     */
#define ML_TRON_BLOCKED(cm, nr, nc, w) \
    ((nc) < 0 || (nc) >= (w) || (nr) < 0 || (nr) >= (int)scr->height || \
     (cm)[(nr) * (w) + (nc)] != 0)

    while (aliveCnt > 0 && filled < target) {
        for (i = 0; i < 4; i++) {
            if (!alive[i]) continue;

            col = cols[i];  row = rows[i];  dir = dirs[i];
            straight[i]++;

            /* Random 90-degree turn after 2..7 straight steps */
            if (straight[i] > 2 + (int)pmRand(6)) {
                newDir = (dir + 1 + (int)(pmRand(2) * 2)) & 3;
                nc = col + s_ml_dc[newDir];
                nr = row + s_ml_dr[newDir];
                if (!ML_TRON_BLOCKED(connMap, nr, nc, (int)scr->width)) {
                    dir = newDir;
                    straight[i] = 0;
                }
            }

            nc = col + s_ml_dc[dir];
            nr = row + s_ml_dr[dir];

            /* Current direction blocked: try the three alternatives */
            if (ML_TRON_BLOCKED(connMap, nr, nc, (int)scr->width)) {
                blocked = 0;
                for (d = 1; d <= 3; d++) {
                    newDir = (dir + d) & 3;
                    nc = col + s_ml_dc[newDir];
                    nr = row + s_ml_dr[newDir];
                    if (!ML_TRON_BLOCKED(connMap, nr, nc, (int)scr->width)) {
                        dir = newDir;
                        straight[i] = 0;
                        break;
                    }
                    blocked++;
                }
                if (blocked == 3) {
                    /* Eliminated */
                    if (mlPlace(scr, connMap, col, row, prevBit[i], color))
                        filled++;
                    // if (prevBit[i] == 0)
                    //     printf("mlTron: player %d eliminated immediately at (%d,%d) � all directions blocked\n", i, col, row);
                    // else
                    //     printf("mlTron: player %d eliminated at (%d,%d) after %d steps\n", i, col, row, straight[i]);
                    alive[i] = FALSE;
                    aliveCnt--;
                    continue;
                }
                nc = col + s_ml_dc[dir];
                nr = row + s_ml_dr[dir];
            }

            if (mlPlace(scr, connMap, col, row,
                        prevBit[i] | s_ml_clinkbit[dir], color))
                filled++;
            prevBit[i] = s_ml_clinkbit[s_ml_opp[dir]];
            cols[i] = nc;
            rows[i] = nr;
            dirs[i] = dir;
        }
    }
    for (i = 0; i < 4; i++) {
        if (alive[i])
            mlPlace(scr, connMap, cols[i], rows[i], prevBit[i], color);
    }
  //  printf("mlTron: done  filled=%d target=%d\n", filled, target);
}

/* Common setup / teardown shared by both Generate actions */
static BOOL mlRunSetup(PmActionContext *ctx, PetsciiProject **projOut,
                       PetsciiScreen **scrOut, UBYTE **connMapOut, UBYTE *colorOut)
{
    PetsciiProject    *proj;
    PetsciiScreen     *scr;
    PetsciiUndoBuffer *undoBuf;
    ToolState         *ts;
    UBYTE             *connMap;

    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;
    proj = *ctx->pproject;
    scr  = PetsciiProject_GetCurrentScreen(proj);
    if (!scr) return FALSE;

    /* Push undo snapshot before modifying the screen */

    PetsciiUndoBuffer_Push(scr);


    ts     = (ToolState *)ctx->toolState;
    *colorOut   = ts ? ts->fgColor : 14;
    connMap = (UBYTE *)AllocVec((ULONG)((int)scr->width * (int)scr->height), MEMF_ANY);
    if (!connMap) return FALSE;
    mlInitMap(scr, connMap);
    *projOut    = proj;
    *scrOut     = scr;
    *connMapOut = connMap;
    return TRUE;
}

static void mlRunFinish(PetsciiProject *proj, UBYTE *connMap)
{
    FreeVec(connMap);
    proj->modified = 1;
    if (app && app->canvasGadget) {
        SetAttrs(app->canvasGadget, PCA_Dirty, TRUE, TAG_END);
        RefreshGList((struct Gadget *)app->canvasGadget,
                     CurrentMainWindow, NULL, 1);
    }
}

/* -----------------------------------------------------------------------
 * Action_GenerateMagicLine
 *
 * Picks one of three line-generation algorithms at random:
 *   0 � Random worm walk    (mlWalkRandom,    50% fill)
 *   1 � Reconnecting worms  (mlWalkReconnect, 40% fill)
 *   2 � Fractal quad-cross  (mlFractal,       60% fill)
 * ----------------------------------------------------------------------- */
BOOL Action_GenerateMagicLine(PmActionContext *ctx)
{

    PetsciiProject *proj;
    PetsciiScreen  *scr;
    UBYTE          *connMap;
    UBYTE           color;
    int             algo;

    if (!mlRunSetup(ctx, &proj, &scr, &connMap, &color)) return FALSE;

    algo = (int)pmRand(3);
    switch (algo) {
        case 0: mlWalkRandom   (scr, connMap, 50, color); break;
        case 1: mlWalkReconnect(scr, connMap, 40, color); break;
        default: mlFractal     (scr, connMap, 60, color); break;
    }

    mlRunFinish(proj, connMap);
    return TRUE;
}

/* -----------------------------------------------------------------------
 * Action_GenerateTronLines
 *
 * Fills the screen using the Tron light-cycle algorithm: four players
 * start from the four corners, each heading inward.  They draw straight
 * lines with occasional 90-degree turns, creating a pattern of right-angle
 * corridors.  Paths may cross, producing junction characters.
 * Target fill: 70% of empty cells.
 * ----------------------------------------------------------------------- */
BOOL Action_GenerateTronLines(PmActionContext *ctx)
{
    PetsciiProject *proj;
    PetsciiScreen  *scr;
    UBYTE          *connMap;
    UBYTE           color;

    if (!mlRunSetup(ctx, &proj, &scr, &connMap, &color)) return FALSE;
    mlTron(scr, connMap, 120, color);
    mlRunFinish(proj, connMap);
    return TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   Action table - C89 sequential initialization.
   Order MUST match the ACTION_* enum exactly.
   Fields: { func, nameStringID, name (NULL until Init), shortcutKey, shortcutQual }
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
static PmAction actionTable[ACTION_COUNT] = {
    /* 0  ACTION_PROJECT_NEW */
    {Action_ProjectNew,      MSG_FILE_NEW,    NULL, 0, 0},
    /* 1  ACTION_PROJECT_OPEN */
    {Action_ProjectOpen,     MSG_FILE_OPEN,   NULL, 0, 0},
    /* 2  ACTION_PROJECT_SAVE */
    {Action_ProjectSave,     MSG_FILE_SAVE,   NULL, 0, 0},
    /* 3  ACTION_PROJECT_SAVEAS */
    {Action_ProjectSaveAs,   MSG_FILE_SAVEAS, NULL, 0, 0},

    /*  */
    {Action_ProjectIconify,    MSG_MENU_ICONIFY,  NULL, 0, 0},
    /* 4  ACTION_PROJECT_ABOUT */
    {Action_ProjectAbout,    MSG_MENU_ABOUT,  NULL, 0, 0},
    {Action_ProjectHelp,    MSG_MENU_HELP,  NULL, 0, 0}, // rawkey is 0x5F
    /* 5  ACTION_PROJECT_QUIT */
    {Action_ProjectQuit,     MSG_MENU_QUIT,   NULL, 0x45, 0}, /* ESC raw key */


    {Action_Draw1,MSG_TOOL_DRAW,        NULL, 0, 0},
    {Action_Draw2,MSG_TOOL_COLORIZE,        NULL, 0, 0},
    {Action_Draw3,MSG_TOOL_CHARDRAW,        NULL, 0, 0},
    {Action_Draw4,MSG_TOOL_BRUSH,        NULL, 0, 0},
    {Action_Draw5,MSG_TOOL_TEXT,        NULL, 0, 0},

    /* 6  ACTION_EDIT_UNDO */
    {Action_EditUndo,        MSG_EDIT_UNDO,        NULL, 0, 0},
    /* 7  ACTION_EDIT_REDO */
    {Action_EditRedo,        MSG_EDIT_REDO,        NULL, 0, 0},
    /* 8  ACTION_EDIT_COPY_SCREEN */
    {Action_EditCopyScreen,  MSG_EDIT_COPY_SCREEN,  NULL, 0, 0},
    /* 9  ACTION_EDIT_PASTE_SCREEN */
    {Action_EditPasteScreen, MSG_EDIT_PASTE_SCREEN, NULL, 0, 0},
    /* 10 ACTION_EDIT_CLEAR_SCREEN */
    {Action_EditClearScreen, MSG_EDIT_CLEAR_SCREEN, NULL, 0, 0},
    /* 11 ACTION_EDIT_SHIFT_LEFT */
    {Action_EditShiftLeft,   MSG_EDIT_SHIFT_LEFT,   NULL, 0, 0},
    /* 12 ACTION_EDIT_SHIFT_RIGHT */
    {Action_EditShiftRight,  MSG_EDIT_SHIFT_RIGHT,  NULL, 0, 0},
    /* 13 ACTION_EDIT_SHIFT_UP */
    {Action_EditShiftUp,     MSG_EDIT_SHIFT_UP,     NULL, 0, 0},
    /* 14 ACTION_EDIT_SHIFT_DOWN */
    {Action_EditShiftDown,   MSG_EDIT_SHIFT_DOWN,   NULL, 0, 0},

    /* 15 ACTION_SCREEN_ADD */
    {Action_ScreenAdd,    MSG_SCREEN_ADD,    NULL, 0, 0},
    /* 16 ACTION_SCREEN_CLONE */
    {Action_ScreenClone,  MSG_SCREEN_CLONE,  NULL, 0, 0},
    /* 17 ACTION_SCREEN_REMOVE */
    {Action_ScreenRemove, MSG_SCREEN_REMOVE, NULL, 0, 0},
    /* 18 ACTION_SCREEN_PREV */
    {Action_ScreenPrev,   MSG_SCREEN_PREV,   NULL, 0, 0},
    /* 19 ACTION_SCREEN_NEXT */
    {Action_ScreenNext,   MSG_SCREEN_NEXT,   NULL, 0, 0},

    /* 20 ACTION_VIEW_TOGGLE_GRID */
    {Action_ViewToggleGrid,    MSG_VIEW_TOGGLE_GRID,    NULL, 0, 0},
    /* 21 ACTION_VIEW_CHARSET_UPPER */
    {Action_ViewCharsetUpper,  MSG_VIEW_CHARSET_UPPER,  NULL, 0, 0},
    /* 22 ACTION_VIEW_CHARSET_LOWER */
    {Action_ViewCharsetLower,  MSG_VIEW_CHARSET_LOWER,  NULL, 0, 0},

    /* 22 ACTION_VIEW_CHARSET_LOWER */
    {Action_ViewToggleFullScreen,  MSG_TOGGLE_FULLSCREEN,  NULL, 0, 0},

   {Action_OpenSettings,  MSG_SETTINGS,  NULL, 0, 0},

    /* 23 ACTION_PALETTE_PETMATE */
    {Action_PalettePetmate,  MSG_PALETTE_PETMATE,  NULL, 0, 0},
    /* 24 ACTION_PALETTE_COLODORE */
    {Action_PaletteColodore, MSG_PALETTE_COLODORE, NULL, 0, 0},
    /* 25 ACTION_PALETTE_PEPTO */
    {Action_PalettePepto,    MSG_PALETTE_PEPTO,    NULL, 0, 0},
    /* 26 ACTION_PALETTE_VICE */
    {Action_PaletteVice,     MSG_PALETTE_VICE,     NULL, 0, 0},

    /* 27 ACTION_BRUSH_FLIP_X */
    {Action_BrushFlipX,    MSG_BRUSH_FLIP_X,    NULL, 0, 0},
    /* 28 ACTION_BRUSH_FLIP_Y */
    {Action_BrushFlipY,    MSG_BRUSH_FLIP_Y,    NULL, 0, 0},
    /* 29 ACTION_BRUSH_ROT90CW */
    {Action_BrushRot90CW,  MSG_BRUSH_ROT90CW,   NULL, 0, 0},
    /* 30 ACTION_BRUSH_ROT180 */
    {Action_BrushRot180,   MSG_BRUSH_ROT180,    NULL, 0, 0},
    /* 31 ACTION_BRUSH_ROT90CCW */
    {Action_BrushRot90CCW, MSG_BRUSH_ROT90CCW,  NULL, 0, 0},

    /* 32 ACTION_EXPORT_BAS */
    {Action_ExportBAS, MSG_EXPORT_BAS, NULL, 0, 0},
    /* 33 ACTION_EXPORT_ASM */
    {Action_ExportASM, MSG_EXPORT_ASM, NULL, 0, 0},
    /* ACTION_EXPORT_SEQ */
    {/*Action_ExportSEQ*/Action_Dummy, MSG_EXPORT_SEQ, NULL, 0, 0},

    /* ACTION_EXPORT_PRG_BAS */
    {Action_ExportPrgBAS, MSG_EXPORT_PRG_BAS, NULL, 0, 0},
    /* ACTION_EXPORT_PRG_ASM */
    {Action_ExportPrgASM, MSG_EXPORT_PRG_ASM, NULL, 0, 0},

    /* ACTION_EXPORT_IFF_ILBM */
    {Action_ExportIFFILBM, MSG_EXPORT_IFF, NULL, 0, 0},
    /* ACTION_EXPORT_GIF */
    {Action_ExportGif, MSG_EXPORT_GIF, NULL, 0, 0},
    /* ACTION_EXPORT_PNG */
    {Action_ExportPng, MSG_EXPORT_PNG, NULL, 0, 0},

    /* ACTION_IMPORT_IMAGE */
    {Action_ImportImage, MSG_IMPORT_IMAGE, NULL, 0, 0},
    /* ACTION_IMPORT_PRG */
    {Action_ImportPrg, MSG_IMPORT_PRG, NULL, 0, 0},

    /* ACTION_GENERATE_RANDOM_BRUSH */
    {Action_GenerateRandomFromBrush, MSG_GENERATE_RANDOM_BRUSH, NULL, 0, 0},
    /* ACTION_GENERATE_MAGIC_LINE */
    {Action_GenerateMagicLine, MSG_GENERATE_MAGIC_LINE, NULL, 0, 0},
    /* ACTION_GENERATE_TRON_LINES */
    {Action_GenerateTronLines, MSG_GENERATE_TRON_LINES, NULL, 0, 0},

    /* ACTION_OPEN_RECENT_0..3 */
    {Action_OpenRecent0, MSG_MENU_OPEN_RECENT, NULL, 0, 0},
    {Action_OpenRecent1, MSG_MENU_OPEN_RECENT, NULL, 0, 0},
    {Action_OpenRecent2, MSG_MENU_OPEN_RECENT, NULL, 0, 0},
    {Action_OpenRecent3, MSG_MENU_OPEN_RECENT, NULL, 0, 0},

    {Action_Dummy, MSG_MENU_EXPORT_AS, NULL, 0, 0}


};

void PmAction_Init(void)
{
    ULONG i;
    for (i = 0; i < ACTION_COUNT; i++) {
        actionTable[i].name = LOC(actionTable[i].nameStringID);
    }
}

PmAction *PmAction_Get(ULONG actionID)
{
    if (actionID >= ACTION_COUNT) return NULL;
    return &actionTable[actionID];
}

BOOL PmAction_Execute(ULONG actionID, PmActionContext *context)
{
    PmAction *action;

    if (actionID >= ACTION_COUNT) return FALSE;

    action = &actionTable[actionID];
    if (!action->func) return FALSE;

    return action->func(context);
}
