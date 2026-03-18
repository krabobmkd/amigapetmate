#include "boopsimainwindow.h"

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/alib.h>
#include <proto/graphics.h>
#include <proto/datatypes.h>

#include <proto/cybergraphics.h>
#include <cybergraphics/cybergraphics.h>

#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <graphics/displayinfo.h>   /* INVALID_ID */
#include <graphics/gfx.h>
#include <datatypes/datatypes.h>
#include <datatypes/pictureclass.h>

#include <proto/window.h>
#include <classes/window.h>
#include "petscii_palette.h"
#include "petscii_style.h"

#include "pmmenu.h"
#include "pmaction.h"
//#include "appsettings.h"
#include "petmate.h"

#include <proto/requester.h>
#include <classes/requester.h>
#include "compilers.h"

#include <stdio.h>
#include <string.h>
// This is the intuition level Window, on OS3 it's recreated when iconizing/reopening !
// when  iconizing/reopening BOOPSI objects are kept, but Intuition level instances and buffers are wiped out.
// Yet, it's needed for most Gadget method calls, and this is not retained by boopsi objects.
// note there could be many windows.
struct Window *CurrentMainWindow=NULL;

/* can be either the WB locked screen, or our private screen */
struct Screen *CurrentMainScreen=NULL;

int  CurrentMainScreen_PreIndexed=0;

extern void RefreshAllColorGadgets();

static void FreeBgBitmap(void);

 void RefreshAllColorGadgets();

extern struct Library *CyberGfxBase;

/* external window management */
void OpenSettingsWindow()
{
    if(!app) return;
    PmSettingsView_Open(&app->settingsView);
}
/* external window management */
void CloseSettingsWindow()
{
    if(!app) return;

    /* Sync screen mode settings back to AppSettings and save */
    if(app->settingsView.window)
    {
        AppSettings_Save(&app->appSettings);
        PmSettingsView_Close(&app->settingsView);
    }
    /* would close requester, but ther'es nio method for this
     if(app->aboutRequester)
     {
     	DoMethod(obj, ..., , TAG_DONE)
     }*/
}


/* this is used when changing screen */
void UpdatePensToCurrentMainScreen()
{
    if(!app || !CurrentMainScreen) return;

    /* maybe need to resynchronize palette from palette id */
    PetsciiStyle_Init(&app->style,app->style.paletteId);

    /* Open fonts from specifications, and may remap pens */
    PetsciiStyle_Apply(&app->style, CurrentMainScreen );

}


void BMainWindow_Init(struct BoopsiMainWindow *mw)
{
    mw->title[0] = 0;
//    if(!mw) return;
//    mw->lockedScreen = LockPubScreen(NULL);
//    if(mw->lockedScreen)
//    {
//        mw->drawInfo = GetScreenDrawInfo(app->lockedScreen);
//    }

}

/* would either set the window title or Screen title according to mode */
void BMainWindow_SetTitle(struct BoopsiMainWindow *mw, const char *title)
{
    strncpy(&mw->title[0],title,sizeof(mw->title)-1);
    mw->title[sizeof(mw->title)-1] = 0;

    /* if fullscreen open, update */
    if(mw->fullscreen && mw->fullPubScreen && CurrentMainScreen)
    {
        /* seriously ? validate this. */
        SetAttrs((Object *)CurrentMainScreen,
                        SA_Title,&mw->title[0],NULL);

    } else if(!mw->fullscreen && mw->lockedScreen && CurrentMainWindow)
    {
        /* if wb window open, update */
        SetWindowTitles(CurrentMainWindow,&mw->title[0],NULL);
    }

}
void BMainWindow_Show(struct BoopsiMainWindow *mw,Object *window_obj,struct AppSettings *appSettings)
{
    if(!mw) return;
    if(mw->fullscreen)
    {
        BMainWindow_SwitchToFullScreen(mw,window_obj,appSettings);
    } else
    {   // WB window
        BMainWindow_SwitchToWB(mw,window_obj,appSettings);
    }
}
void BMainWindow_Toggle(struct BoopsiMainWindow *mw,Object *window_obj,struct AppSettings *appSettings)
{
    if(!mw) return;
    if(!mw->fullscreen)
    {
        BMainWindow_SwitchToFullScreen(mw,window_obj,appSettings);
    } else
    {   // WB window
        BMainWindow_SwitchToWB(mw,window_obj,appSettings);
    }
}
/* at iconify or close */
void BMainWindow_Close(struct BoopsiMainWindow *mw,Object *window_obj, int iconify)
{
    if(!mw) return;

    FreeBgBitmap();
    PetsciiStyle_Release(&app->style );
    CloseSettingsWindow();

    if(CurrentMainWindow)
    {
        // todo save window position
        PmMenu_Close(&mw->menu, CurrentMainWindow);
        if(iconify)
        {
            DoMethod(window_obj, WM_ICONIFY, NULL);
        } else
        {
            DoMethod(window_obj, WM_CLOSE );
        }
        CurrentMainWindow = NULL;
    }

    /* if was in fullscreen mode */
    if(mw->fullPubScreen)
    {
        CloseScreen(mw->fullPubScreen);
        mw->fullPubScreen = NULL;
    }
    if(mw->lockedScreen)
    {
        UnlockPubScreen(0, mw->lockedScreen);
        mw->lockedScreen = NULL;
    }
    CurrentMainScreen = NULL;
}

/* Used in both WB window and fullscreen cases. doing WM_OPEN implies:
    - recreating and refreshing the menu.
*/
void GenericOpenWindow(BoopsiMainWindow *mw,Object *window_obj,struct AppSettings *appSettings)
{
    if(CurrentMainWindow) return; /* if already exists don't open */
    if(!CurrentMainScreen) return; /* need an active screen */

    CurrentMainWindow = (struct Window *)DoMethod(window_obj, WM_OPEN, NULL);

    if(!CurrentMainWindow) return;

    /* Create and attach menus */
    if (!PmMenu_Create(&mw->menu, CurrentMainScreen, CurrentMainWindow,
                       app ? &app->appSettings : NULL)) {
       // printf("Warning: Could not create menus\n");
       return;
    }

    /* Sync palette checkmark with the current project palette */
    if (app && app->project) {
        PmMenu_UpdatePaletteCheck(&mw->menu, CurrentMainWindow,
                                  (UBYTE)app->project->currentPalette);
    }

    /* Rebuild menus with recent files from loaded settings */
    // if (AppSettings_GetRecentCount(appSettings) > 0) {
    //     PmMenu_Rebuild(&mw->menu, CurrentMainScreen, CurrentMainWindow, appSettings );
    // }

}
/* This function realloc pens if screen changed  */
extern void UpdatePensToCurrentMainScreen();


void BMainWindow_GetWindowPos(struct BoopsiMainWindow *mw,Object *window_obj)
{
    /* save window position if there is one */
    GetAttr(WA_Top,window_obj,&mw->top);
    GetAttr(WA_Left,window_obj,&mw->left);
    GetAttr(WA_Width,window_obj,&mw->width);
    GetAttr(WA_Height,window_obj,&mw->height);
    if(mw->width<128) mw->width=128;
    if(mw->height<64) mw->height=64;
}
/*****************************************************************************/
/* This is the message packet passed by layers.library to a backfill hook.
 * It contains a pointer to the layer that has been damaged, a Rectangle
 * structure that defines the bounds of the damage. No rendering can occur
 * outside of these coordinates.
 *
 * The backfill hook is also passed a RastPort in which the rendering
 * should be performed.
 */
struct BackFillMsg
{
    struct Layer     *bf_Layer;
    struct Rectangle  bf_Bounds;
    LONG              bf_OffsetX;
    LONG              bf_OffsetY;
};

static void BackFillHook_MonoColor(
            REG(a2, struct RastPort    *rp),
            REG(a1,struct BackFillMsg *bfm) )
{
    int ipen=0;
    struct RastPort crp;
    /* krb note: a "BackFill hook" is meant to be executed under Layer.library repair
     * following Layer descrition, so no "RastPort with layer" draw function
     * should be used here, only the "Bitmap" draw API blits.*/
    crp       = *rp;            /* copy the rastport                      */
    crp.Layer = NULL;           /* eliminate bogus clipping from our copy */

    if(app) ipen = app->style.c64pens[4].pen; /* purple */

    SetAPen(&crp,ipen);                     /* set the pen to color         */
    SetDrMd(&crp,JAM2);                  /* set the rendering mode we need */
    RectFill(&crp,bfm->bf_Bounds.MinX,   /* clear the whole area           */
                  bfm->bf_Bounds.MinY,
                  bfm->bf_Bounds.MaxX,
                  bfm->bf_Bounds.MaxY);
}
static struct Hook monoHook={{NULL,NULL},( ULONG (*)() )&BackFillHook_MonoColor,NULL,NULL};

/*****************************************************************************/
/* Pattern (tiled image) backfill                                            */
/*****************************************************************************/

static struct BitMap *bgBitmap  = NULL;
static LONG           bgBitmapW = 0;
static LONG           bgBitmapH = 0;

static void FreeBgBitmap(void)
{
    if (bgBitmap) { FreeBitMap(bgBitmap); bgBitmap = NULL; }
    bgBitmapW = bgBitmapH = 0;
}

/* Load and copy the image at 'path' remapped to 'scr' into bgBitmap. */
static void LoadBgBitmap(const char *path, struct Screen *scr)
{
    struct TagItem       tags[4];
    Object              *dto;
    struct BitMapHeader *bmh;
    struct BitMap       *dtBm;

    FreeBgBitmap();
    if (!path || !scr) return;

    tags[0].ti_Tag  = DTA_GroupID;
    tags[0].ti_Data = GID_PICTURE;
    tags[1].ti_Tag  = DTA_SourceType;
    tags[1].ti_Data = DTST_FILE;
    tags[2].ti_Tag  = PDTA_Screen;
    tags[2].ti_Data = (ULONG)scr;
    tags[3].ti_Tag  = TAG_END;
    tags[3].ti_Data = 0;

    dto = NewDTObjectA((APTR)(char *)path, tags);
    if (!dto) return;

    DoMethod(dto, DTM_PROCLAYOUT, NULL, 1L);

    bmh  = NULL;
    dtBm = NULL;
    GetDTAttrs(dto,
               PDTA_BitMapHeader, (ULONG)&bmh,
               PDTA_BitMap,       (ULONG)&dtBm,
               TAG_END);

    if (bmh && dtBm)
    {
        ULONG w = (ULONG)bmh->bmh_Width;
        ULONG h = (ULONG)bmh->bmh_Height;

        bgBitmap = AllocBitMap(w, h, (ULONG)dtBm->Depth, BMF_CLEAR, dtBm);
        if (bgBitmap)
        {
            BltBitMap(dtBm, 0, 0, bgBitmap, 0, 0,
                      (WORD)w, (WORD)h, 0xC0, 0xFF, NULL);
            bgBitmapW = (LONG)w;
            bgBitmapH = (LONG)h;
        }
    }

    DisposeDTObject(dto);
}

/*
 * BackFill hook that tiles bgBitmap across the damage rectangle.
 * bf_OffsetX/bf_OffsetY give the pattern phase so tiles align to the
 * window/layer origin.  Each axis wraps at most once per call, so a
 * damage rect no larger than one tile results in at most 4 BltBitMap calls.
 */
static void BackFillHook_Pattern(
            REG(a2, struct RastPort    *rp),
            REG(a1, struct BackFillMsg *bfm) )
{
    struct BitMap *dst;
    LONG dstX, dstY, dstW, dstH;
    LONG srcStartX, srcStartY;
    LONG x, y, srcX, srcY, chunkW, chunkH;

    if (!bgBitmap || bgBitmapW <= 0 || bgBitmapH <= 0) return;

    dst  = rp->BitMap;
    dstX = (LONG)bfm->bf_Bounds.MinX;
    dstY = (LONG)bfm->bf_Bounds.MinY;
    dstW = (LONG)bfm->bf_Bounds.MaxX - dstX + 1;
    dstH = (LONG)bfm->bf_Bounds.MaxY - dstY + 1;

    if (dstW <= 0 || dstH <= 0) return;

    /* Phase of the pattern at the top-left of the damage rect */
    srcStartX = ((dstX /*+ (LONG)bfm->bf_OffsetX*/) % bgBitmapW + bgBitmapW) % bgBitmapW;
    srcStartY = ((dstY /*+ (LONG)bfm->bf_OffsetY*/) % bgBitmapH + bgBitmapH) % bgBitmapH;

    y = 0;
    while (y < dstH)
    {
        srcY   = (srcStartY + y) % bgBitmapH;
        chunkH = bgBitmapH - srcY;
        if (chunkH > dstH - y) chunkH = dstH - y;

        x = 0;
        while (x < dstW)
        {
            srcX   = (srcStartX + x) % bgBitmapW;
            chunkW = bgBitmapW - srcX;
            if (chunkW > dstW - x) chunkW = dstW - x;

            BltBitMap(bgBitmap, srcX, srcY,
                      dst, dstX + x, dstY + y,
                      (WORD)chunkW, (WORD)chunkH, 0xC0, 0xFF, NULL);
            x += chunkW;
        }
        y += chunkH;
    }
}
static struct Hook patternHook={{NULL,NULL},( ULONG (*)() )&BackFillHook_Pattern,NULL,NULL};

void BMainWindow_SwitchToFullScreen(struct BoopsiMainWindow *mw,Object *window_obj,struct AppSettings *appSettings)
{
        int x1,y1,w,h;
        ULONG backfill;
	struct ColorSpec colspec[16+1];
    struct Screen *myScreen;
    UWORD fakepens[1]={~0};
/*
SA_Pens is also used to decide that a screen is ready to support
the full-blown "new look" graphics. If you want the 3D embossed look,
 you must provide this tag, and the ti_Data value cannot be NULL.
 If it points to a "minimal" array, containing just the terminator ~0,
  you can specify "new look" without providing any values for the pen array.
*/

    if(!mw || !window_obj) return;
    if(mw->fullPubScreen) return; // already ok

    /* close resources opened on the old mainscreen while it's set. */
    PetsciiStyle_Release(&app->style );

    CloseSettingsWindow();

    if(CurrentMainWindow)
    {
        /* save window position if there is one */
        BMainWindow_GetWindowPos(mw,window_obj);

        PmMenu_Close(&mw->menu, CurrentMainWindow);
        DoMethod(window_obj, WM_CLOSE );
        CurrentMainWindow = NULL;

        if(mw->lockedScreen)
        {
            UnlockPubScreen(0, mw->lockedScreen);
            mw->lockedScreen = NULL;
        }
        CurrentMainScreen = NULL;
    }


   /* update RGB32 palette from PetsciiStyle with a NULL screen,
     * so the RGB32 table is updated, and given at screen init.
    */
    CurrentMainScreen_PreIndexed = 1;
    PetsciiStyle_Init(&app->style,app->style.paletteId);  /* maybe need to resynchronize palette from palette id */
    PetsciiStyle_Apply(&app->style, /*CurrentMainScreen*/NULL );  /* in that case create palette and set pen index */

    /* Use the user-configured mode ID when available, otherwise inherit WB. */
    if (appSettings &&
        !appSettings->screenModeIdLikeWorkbench &&
        appSettings->screenModeId != INVALID_ID)
    {

        myScreen = OpenScreenTags(NULL,
            SA_Type,      PUBLICSCREEN,
            SA_PubName,   (ULONG)"PetMate",
            SA_DisplayID, (ULONG)appSettings->screenModeId,
            SA_Depth, 4, /* is ok for even OCS/ECS machine in highres */
            SA_Title,     (ULONG)&mw->title[0],
            SA_Colors32, (ULONG)&app->style.paletteRGB32[0],
            SA_Pens,(ULONG)&fakepens[0],
            TAG_DONE);
    } else {
        myScreen = OpenScreenTags(NULL,
            SA_Type,          PUBLICSCREEN,
            SA_PubName,       (ULONG)"PetMate",
            SA_LikeWorkbench, TRUE,
            SA_Title,         (ULONG)&mw->title[0],
            SA_Colors32, (ULONG)&app->style.paletteRGB32[0],
            SA_Pens,(ULONG)&fakepens[0],
            TAG_DONE);
    }
    if(!myScreen) return; /* likely because chipram is not infinite*/

    mw->fullPubScreen = myScreen;

    CurrentMainScreen = myScreen;

    /* Load pattern bitmap now that the screen is known */
    if (!appSettings->useOneColorBg && appSettings->bgImagePath)
        LoadBgBitmap(appSettings->bgImagePath, CurrentMainScreen);
    else
        FreeBgBitmap();

    /* reconfigure persistant boopsi window object while closed */


    if(appSettings->useOneColorBg)
    {
 
        backfill = (ULONG)&monoHook;
    } else if(bgBitmap)
    {
        backfill = (ULONG)&patternHook;
    } else
    {
        backfill = (ULONG)NULL; /* use the default BackFill pattern */
    }
    /* get screen dimension */
    x1 =0;
    y1 = myScreen->BarHeight;
    w = myScreen->Width;
    h = myScreen->Height - y1;



    SetAttrs(window_obj,
        WA_CustomScreen,(ULONG)myScreen,
        WA_Borderless, TRUE,
        WA_SizeGadget,FALSE,
        WA_DepthGadget,FALSE,
        WA_CloseGadget,FALSE,
        WA_DragBar,FALSE,
        WA_Title,NULL,
        WA_Flags,WFLG_ACTIVATE | WFLG_SMART_REFRESH ,
        WA_Backdrop,TRUE,

        WINDOW_IconifyGadget, FALSE,
        WA_Top,y1+1,
        WA_Left,x1,
        WA_Width,w+32, /* need some more pixel at window level so the backdrop UI takes all place */
        WA_Height,h+y1+1,
        /* must be last */
        WA_BackFill,backfill,
        TAG_END);

    /* need to reattribute pens on this screen */
    UpdatePensToCurrentMainScreen();

    /* re-open */
    GenericOpenWindow( mw, window_obj, appSettings );


    if(CurrentMainWindow)
    {
        struct Gadget *mlayout=NULL;
        GetAttr(WINDOW_ParentGroup,window_obj,(ULONG *)&mlayout);

        if(mlayout)
        {
            SetGadgetAttrs(mlayout,CurrentMainWindow,NULL,
                GA_Width,w,
                GA_Height,h,
                TAG_END
                    );
        }
    }
    mw->fullscreen = TRUE;

    /* some may need reindexation and window must be opened */
    RefreshAllColorGadgets();
}





void BMainWindow_SwitchToWB(struct BoopsiMainWindow *mw,Object *window_obj,struct AppSettings *appSettings)
{
    if(!mw || !window_obj) return;

    PetsciiStyle_Release(&app->style );
    CloseSettingsWindow();

    /* close backdrop window at boopsi level */
    if(CurrentMainWindow)
    {
        PmMenu_Close(&mw->menu, CurrentMainWindow);
        DoMethod(window_obj, WM_CLOSE );
        CurrentMainWindow = NULL;
    }

    /* close extra screen */
    if(mw->fullPubScreen)
    {
        CloseScreen(mw->fullPubScreen);
        mw->fullPubScreen = NULL;
        CurrentMainScreen = NULL;
    }

    if(!mw->lockedScreen)
    {
        mw->lockedScreen = LockPubScreen(NULL);
    }
    if(! mw->lockedScreen) return;

    CurrentMainScreen =  mw->lockedScreen;

    /* need to reattribute pens on this screen */
    CurrentMainScreen_PreIndexed = 0;
    /* before CyberGfxBase on WB it would have been FALSE,
      but with CGX we can  draw using our own RGB32 CLUT and not screen pens.
     WriteLUTPixelArray() */
    // CurrentMainScreen_PreIndexed =(
    //     (CyberGfxBase != NULL) &&
    //     (GetCyberMapAttr(CurrentMainScreen->RastPort.BitMap, CYBRMATTR_ISCYBERGFX) != 0) &&
    //     (GetCyberMapAttr(CurrentMainScreen->RastPort.BitMap, CYBRMATTR_DEPTH) > 8)
    //    );
    UpdatePensToCurrentMainScreen();

    /* Load pattern bitmap now that the screen is known */
    if (!appSettings->useOneColorBg && appSettings->bgImagePath)
        LoadBgBitmap(appSettings->bgImagePath, CurrentMainScreen);
    else
        FreeBgBitmap();

    {
        ULONG lastTag,lastValue;
        if(appSettings->useOneColorBg)
        {
            lastTag = WA_BackFill;
            lastValue = (ULONG)&monoHook;
        } else if(bgBitmap)
        {
            lastTag = WA_BackFill;
            lastValue = (ULONG)&patternHook;
        } else
        {
            lastTag = WA_BackFill;
            lastValue = (ULONG)NULL; /* use the default BackFill pattern */
        }

        int x1,y1,w,h;
        /* if dimension has been kept by settings or screen switch, recover them */
        if(mw->width>0)
        {
            x1 = mw->left;
            y1 = mw->top;
            w = mw->width;
            h = mw->height;
        } else
        {
            /* else some default */
            x1 = 40;
            y1 = 40;
            w = 320;
            h= 240;
        }

        /* reconfigure persistant boopsi window object while closed */
        SetAttrs(window_obj,
            WA_CustomScreen,(ULONG)mw->lockedScreen,
            WA_Borderless, FALSE,
            WA_Backdrop,FALSE,
            WA_Flags,WFLG_ACTIVATE | WFLG_SMART_REFRESH,

           // WA_Flags, WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET | WFLG_SIZEGADGET | WFLG_ACTIVATE | WFLG_SMART_REFRESH,
            WA_DragBar,TRUE,
            WA_SizeGadget,TRUE,
            WA_DepthGadget,TRUE,
            WA_CloseGadget,TRUE,
            WA_ReportMouse, TRUE,
            WA_Title,(ULONG)&mw->title[0],
            WINDOW_IconifyGadget, TRUE,
            WA_Top,y1,
            WA_Left,x1,
            WA_Width,w,
            WA_Height,h,
            lastTag,lastValue,
            TAG_END);
    }

    mw->fullscreen = FALSE;


    /* re-open */
    GenericOpenWindow( mw, window_obj, appSettings );

    RefreshAllColorGadgets();

    /* There may exists more screens than just WB when closing fs. make sure WB the visible screen. */
    ScreenToFront(CurrentMainScreen);

}
