#include "boopsimainwindow.h"

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/alib.h>

#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>

#include <proto/window.h>
#include <classes/window.h>
#include "petscii_palette.h"
#include "petscii_style.h"

#include "pmmenu.h"
//#include "appsettings.h"
#include "petmate.h"

#include <stdio.h>
#include <string.h>
// This is the intuition level Window, on OS3 it's recreated when iconizing/reopening !
// when  iconizing/reopening BOOPSI objects are kept, but Intuition level instances and buffers are wiped out.
// Yet, it's needed for most Gadget method calls, and this is not retained by boopsi objects.
// note there could be many windows.
struct Window *CurrentMainWindow=NULL;

/* can be either the WB locked screen, or our private screen */
struct Screen *CurrentMainScreen=NULL;

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
    /* Sync temp dir from settings view back to AppSettings */
    {
        const char *tempDir = PmSettingsView_GetTempDir(&app->settingsView);
        if (tempDir) {
            AppSettings_SetTempDir(&app->appSettings, tempDir);
        }
    }
    PmSettingsView_Close(&app->settingsView);
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

/* at iconify or close */
void BMainWindow_Close(struct BoopsiMainWindow *mw,Object *window_obj, int iconify)
{
    if(!mw) return;

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
    if (!PmMenu_Create(&mw->menu, CurrentMainScreen, CurrentMainWindow)) {
       // printf("Warning: Could not create menus\n");
       return;
    }

    /* Rebuild menus with recent files from loaded settings */
    // if (AppSettings_GetRecentCount(appSettings) > 0) {
    //     PmMenu_Rebuild(&mw->menu, CurrentMainScreen, CurrentMainWindow, appSettings );
    // }

}
/* This function realloc pens if screen changed  */
extern void UpdatePensToCurrentMainScreen();


// static const ULONG c64ColorsScreen[C64_COLOR_COUNT] =
// {
//     /* original order
//     0x000000, 0xFFFFFF, 0x924A40, 0x84C5CC,
//     0x9351B6, 0x72B14B, 0x483AA4, 0xD5DF7C,
//     0x99692D, 0x675201, 0xC08178, 0x606060,
//     0x8A8A8A, 0xB2EC91, 0x867ADE, 0xAEAEAE
//     */
//     /* gey-black-white-first */
//     0x8A8A8A, 0x000000, 0xFFFFFF, 0x924A40, 0x84C5CC,
//     0x9351B6, 0x72B14B, 0x483AA4, 0xD5DF7C,
//     0x99692D, 0x675201, 0xC08178, 0x606060,
//     0xB2EC91, 0x867ADE, 0xAEAEAE
// };
/* particular treatment for palette in fullscreen mode:
  since there is a colormap per screen, if we create the screen
  we can just ask 16 colors and set the C64 paletteId directly to it.
  Since Amiga windows and buttons needs the 4 first color to be
 "grey-black-white-blue", we can just swap a few entries in the C64
 palette to have that.
 */
static void pmPaletteToColorSpec(struct ColorSpec * pccolor)
{
    UWORD i;
    int paletteID;
    ULONG t;
    ULONG pal[C64_COLOR_COUNT];

    if(!app ) return;
    paletteID = app->style.paletteId;

    for (i = 0; i < C64_COLOR_COUNT; i++)
    {
        pal[i] = c64Palettes[paletteID][i];
    }
    /* C64 palette magic swap to Amiga Intuition palette.
     - In intuition since OS2 the 4 first colors are likely gey,black,white,blue
      then other color doesn't import much, amiga apps remaps to what is available.
    */
    t = pal[15];
    pal[15] = pal[2];
    pal[2] = pal[1];
    pal[1] = pal[0];
    pal[0] = t;


    for (i = 0; i < C64_COLOR_COUNT; i++) {
        ULONG cRGB = pal[i];

        pccolor->ColorIndex = i;
        pccolor->Red = cRGB>>(16+4) & 0x0f;
        pccolor->Green = cRGB>>(8+4) & 0x0f;
        pccolor->Blue = cRGB>>(4) & 0x0f;
        pccolor++;
    }
    pccolor->ColorIndex = -1;
}


void BMainWindow_SwitchToFullScreen(struct BoopsiMainWindow *mw,Object *window_obj,struct AppSettings *appSettings)
{
        int x1,y1,w,h;
   // note: all this is regular OS intuition, no CGX
	struct ColorSpec colspec[16+1];
	// ={ // let's do it amiga default like
 //                0,  0,0,0, //black  inverte black and grey against amiga default.
 //                                // this will draw screen black by default.
 //                1,  8,8,8,  // grey
 //                2,  15,15,15,
 //                3,  1,8,15, // blue 1
 //                4,  0,1,8, // blue 2
 //                // end
 //                -1,0,0,0};
    struct Screen *myScreen;
    printf("go fs\n");
    if(!mw || !window_obj) return;
    if(mw->fullPubScreen) return; // already ok

    pmPaletteToColorSpec(&colspec[0]);

    PetsciiStyle_Release(&app->style );

    CloseSettingsWindow();

    myScreen = OpenScreenTags(NULL,
        SA_Type,       PUBLICSCREEN,
        SA_PubName,   (ULONG) "PetMate",  // Optional: custom name
        SA_LikeWorkbench, TRUE,             // Inherit Workbench settings
        SA_Title,     (ULONG) &mw->title[0],
        SA_Colors,(ULONG)&colspec[0],
        TAG_DONE);
    if(!myScreen) return;

    mw->fullPubScreen = myScreen;
    /* You may set these while the window is NOT open, at NewObject() time or SetAttrs() - between WM_CLOSE and WM_OPEN for example.
        WA_PubScreen WA_CustomScreen, ...
     */     
    if(CurrentMainWindow)
    {
        /* save window position if there is one */
        GetAttr(WA_Top,window_obj,&mw->top);
        GetAttr(WA_Left,window_obj,&mw->left);
        GetAttr(WA_Width,window_obj,&mw->width);
        GetAttr(WA_Height,window_obj,&mw->height);
        if(mw->width<128) mw->width=128;
        if(mw->height<64) mw->height=64;

        PmMenu_Close(&mw->menu, CurrentMainWindow);
        DoMethod(window_obj, WM_CLOSE );
        CurrentMainWindow = NULL;

        if(mw->lockedScreen)
        {
            UnlockPubScreen(0, mw->lockedScreen);
            mw->lockedScreen = NULL;
        }
        //CurrentMainScreen = NULL;

    }
    CurrentMainScreen = myScreen;
 printf("new screen ok\n");
    /* need to reattribute pens on this screen */
    UpdatePensToCurrentMainScreen();


    /* reconfigure persistant boopsi window object while closed */
    {
        /* get screen dimension */
        x1 =0;
        y1 = myScreen->BarHeight;
        w = myScreen->Width;
        h = myScreen->Height - y1;
        printf("fs w:%d h:%d myScreen->BarHeight:%d\n",w,h,myScreen->BarHeight);

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

            TAG_END);
    }
    /* re-open */
    GenericOpenWindow( mw, window_obj, appSettings );

    if(CurrentMainWindow)
    {
        struct Gadget *mlayout=NULL;
        GetAttr(WINDOW_ParentGroup,window_obj,(ULONG *)&mlayout);
        //printf("got WINDOW_ParentGroup:%08x\n",(int)mlayout);
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
    UpdatePensToCurrentMainScreen();

    {
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
            WA_Flags,WFLG_ACTIVATE | WFLG_SMART_REFRESH ,
           // WA_Flags, WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET | WFLG_SIZEGADGET | WFLG_ACTIVATE | WFLG_SMART_REFRESH,
            WA_DragBar,TRUE,
            WA_SizeGadget,TRUE,
            WA_DepthGadget,TRUE,
            WA_CloseGadget,TRUE,
            WA_Title,(ULONG)&mw->title[0],
            WINDOW_IconifyGadget, TRUE,
            WA_Top,y1,
            WA_Left,x1,
            WA_Width,w,
            WA_Height,h,
            TAG_END);
    }
    mw->fullscreen = FALSE;

    /* re-open */
    GenericOpenWindow( mw, window_obj, appSettings );


}
