#include "pmaction.h"
#include "pmlocale.h"
#include "petscii_style.h"
#include "petscii_screen.h"
#include "petscii_undo.h"
#include "petscii_fileio.h"
#include "petscii_export.h"
#include "petscii_export_ilbm.h"
#include "pmlocale.h"
#include "pmstring.h"
#include "petmate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <intuition/intuition.h>
#include <proto/asl.h>
#include <libraries/asl.h>

#include <proto/requester.h>
#include <classes/requester.h>


#include "boopsimainwindow.h"
#include "petscii_canvas.h"  /* PCA_TransformBrush, BRUSH_TRANSFORM_* */

/* External globals from petmate.c */
extern struct Library *AslBase;
extern struct IntuitionBase *IntuitionBase;

void SetStatusBarMessage(int enumMessage);
/* Last directory used in a file requester (persists across open/save calls) */
static char *s_lastDir = NULL;

// const char *PetsciiFileIO_ErrorString(int strnum)
// {
//     return LOC(MSG_PETSCII_FILEIO_OK+strnum);
// }

/* - - - Helper: apply a new palette to the style and re-obtain pens - - - */
static void applyPalette(PmActionContext *ctx, UBYTE paletteID)
{
    PetsciiProject *proj;
    PetsciiStyle   *sty;

    if (!ctx || !ctx->pproject || !*ctx->pproject) return;

    proj = *ctx->pproject;
    sty  = (PetsciiStyle *)ctx->style;

    proj->currentPalette = paletteID;
    proj->modified = 1;

    if (sty) {
        PetsciiStyle_Init(sty, paletteID);
        PetsciiStyle_Apply(sty, CurrentMainScreen);
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
    PmStr_Free(pathbuf);
    SetStatusBarMessage(MSG_PETSCII_FILEIO_OK+err);

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
         (err == MSG_PETSCII_FILEIO_OK)?MSG_PETSCII_FILEIO_WRITEOK:
         (MSG_PETSCII_FILEIO_OK+err)
         );

    return TRUE;
}

BOOL Action_ProjectSaveAs(PmActionContext *ctx)
{
    char *pathbuf;
    int   err;

    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;

    pathbuf = aslFileRequest(TRUE);
    if (!pathbuf) return FALSE; /* user cancelled */

    err = PetsciiFileIO_Save(*ctx->pproject, pathbuf);
    PmStr_Free(pathbuf);
    SetStatusBarMessage(
        (err == MSG_PETSCII_FILEIO_OK)?MSG_PETSCII_FILEIO_WRITEOK:
        (MSG_PETSCII_FILEIO_OK+err)
        );
    return TRUE;
}

BOOL Action_ProjectAbout(PmActionContext *ctx)
{
    Object *req;
    if(!CurrentMainWindow) return;
    if(!app->aboutRequester)
    {
        app->aboutRequester = NewObject( REQUESTER_GetClass(), NULL,
			REQ_Image,REQIMAGE_INFO,
			REQ_BodyText,(ULONG)
"\33b\33uAmiga PetMate - C64 PETSCII Art Editor \33n - port v0.1\n\n"
"\33cThis is all about drawing with the character sets that were\n"
"integrated with old Commodore 8 Bit machines, the first of\n"
"which was the \"PET\", hence the name PETSCII in regard to ASCII.\n\n"
"Forked and ported to C by Krb from the original TypeScript source.\33n\n\n"
"     \33c\33b (c)2026 License is MIT, read file LICENSE.\33n\n\n"
"\33cSources for Amiga PetMate are hosted at:\n"
"https://github.com/krabobmkd/amigapetmate\33n\n\n"
" \33bOriginal PetMate projects are hosted at:\33n\n"
"\33c\33bnurpax https://github.com/nurpax/petmate\33n\n"
"\33c\33bwbochar https://github.com/wbochar/petmate9\33n\n\n"
"\33cThis is also a project made to explore Intuition BOOPSI and\n"
"wouldn't exist without the Official Amiga developer forum:\n"
"https://developer.amigaos3.net/forum"
			,
            REQ_TitleText,(ULONG)"About Amiga PetMate",
			REQ_GadgetText,(ULONG)"_Ok",
            TAG_END);
    }
    if(!app->aboutRequester) return FALSE;

    OpenRequester(app->aboutRequester,CurrentMainWindow);

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
    PetsciiUndoBuffer **bufs;

    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;
    if (!ctx->undoBufs) return FALSE;

    proj = *ctx->pproject;
    bufs = (PetsciiUndoBuffer **)ctx->undoBufs;
    scr  = PetsciiProject_GetCurrentScreen(proj);

    if (!scr || !bufs[proj->currentScreen]) return FALSE;

    if (PetsciiUndoBuffer_Undo(bufs[proj->currentScreen], scr)) {
        proj->modified = 1;
        return TRUE;
    }
    return FALSE;
}

BOOL Action_EditRedo(PmActionContext *ctx)
{
    PetsciiProject     *proj;
    PetsciiScreen      *scr;
    PetsciiUndoBuffer **bufs;

    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;
    if (!ctx->undoBufs) return FALSE;

    proj = *ctx->pproject;
    bufs = (PetsciiUndoBuffer **)ctx->undoBufs;
    scr  = PetsciiProject_GetCurrentScreen(proj);

    if (!scr || !bufs[proj->currentScreen]) return FALSE;

    if (PetsciiUndoBuffer_Redo(bufs[proj->currentScreen], scr)) {
        proj->modified = 1;
        return TRUE;
    }
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
    return TRUE;
}

BOOL Action_ViewCharsetUpper(PmActionContext *ctx)
{
    PetsciiScreen *scr;
    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;

    scr = PetsciiProject_GetCurrentScreen(*ctx->pproject);
    if (!scr) return FALSE;

    scr->charset = PETSCII_CHARSET_UPPER;
    (*ctx->pproject)->modified = 1;
    return TRUE;
}

BOOL Action_ViewCharsetLower(PmActionContext *ctx)
{
    PetsciiScreen *scr;
    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;

    scr = PetsciiProject_GetCurrentScreen(*ctx->pproject);
    if (!scr) return FALSE;

    scr->charset = PETSCII_CHARSET_LOWER;
    (*ctx->pproject)->modified = 1;
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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   Palette actions
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

BOOL Action_PalettePetmate(PmActionContext *ctx)  { applyPalette(ctx, PALETTE_PETMATE);  return TRUE; }
BOOL Action_PaletteColodore(PmActionContext *ctx) { applyPalette(ctx, PALETTE_COLODORE); return TRUE; }
BOOL Action_PalettePepto(PmActionContext *ctx)    { applyPalette(ctx, PALETTE_PEPTO);    return TRUE; }
BOOL Action_PaletteVice(PmActionContext *ctx)     { applyPalette(ctx, PALETTE_VICE);     return TRUE; }

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

BOOL Action_ExportSEQ(PmActionContext *ctx)
{
    char          *rawpath;
    char          *pathbuf;
    PetsciiScreen *scr;
    BOOL           ok;

    if (!ctx || !ctx->pproject || !*ctx->pproject) return FALSE;
    scr = PetsciiProject_GetCurrentScreen(*ctx->pproject);
    if (!scr) return FALSE;

    rawpath = aslExportRequest("Export as SEQ (.seq)", "#?.seq");
    if (!rawpath) return FALSE;

    pathbuf = PmStr_WithExt(rawpath, ".seq");
    PmStr_Free(rawpath);
    if (!pathbuf) return FALSE;

    ok = (BOOL)(PetsciiExport_SaveSEQ(scr, pathbuf) == PETSCII_EXPORT_OK);
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

    rawpath = aslExportRequest("Export as GIF ILBM (.gif)", "#?.gif");
    if (!rawpath) return FALSE;

    pathbuf = PmStr_WithExt(rawpath, ".gif");
    PmStr_Free(rawpath);
    if (!pathbuf) return FALSE;

    ok = (BOOL)(PetsciiExport_SaveDatatype(scr, pathbuf,2) == PETSCII_EXPORT_OK);
    PmStr_Free(pathbuf);
    SetStatusBarMessage(ok ? MSG_PETSCII_FILEIO_WRITEOK : MSG_PETSCII_FILEIO_EWRITE);
    return ok;
    */
        return FALSE;
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
    /* 4  ACTION_PROJECT_ABOUT */
    {Action_ProjectAbout,    MSG_MENU_ABOUT,  NULL, 0, 0},
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
    /* 34 ACTION_EXPORT_SEQ */
    {Action_ExportSEQ, MSG_EXPORT_SEQ, NULL, 0, 0},

    /* 32 ACTION_EXPORT_BAS */
    {Action_ExportIFFILBM, MSG_EXPORT_IFF, NULL, 0, 0},
    /* 33 ACTION_EXPORT_ASM */
    {Action_ExportGif, MSG_EXPORT_GIF, NULL, 0, 0},
    /* 34 ACTION_EXPORT_SEQ */
    {Action_ExportPng, MSG_EXPORT_PNG, NULL, 0, 0}

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
