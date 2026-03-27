/*
 * Petmate Amiga - C64 PETSCII Art Editor
 * Main application: library init, window creation, event loop, cleanup.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <clib/alib_protos.h>

#include <intuition/screens.h>
#include <intuition/icclass.h>

#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/dos.h>
#include <proto/icon.h>
#include <proto/amigaguide.h>
#include <proto/locale.h>
#include <libraries/locale.h>

#include <proto/window.h>
#include <classes/window.h>

#include <proto/layout.h>
#include <gadgets/layout.h>

#include <proto/button.h>
#include <gadgets/button.h>

#include <proto/scroller.h>
#include <gadgets/scroller.h>

#include <proto/checkbox.h>
#include <gadgets/checkbox.h>

#include <proto/label.h>
#include <images/label.h>

#include <proto/asl.h>
#include <libraries/asl.h>

#include "compilers.h"
#include "bdbprintf.h"
#include "gadgetid.h"
#include "petmate.h"
#include "petscii_screen.h"
#include "petscii_project.h"
#include "petscii_palette.h"
#include "petscii_charset.h"

#include "class_colorswatch.h"
#include "class_layoutwithpopup.h"
#include "class_charlayout.h"

#include "petscii_style.h"
#include "pmlocale.h"
#include "pmaction.h"
#include "pmmenu.h"
#include "boopsimessage.h"
#include "boopsimainwindow.h"
#include "pmsettingsview.h"
#include "pmhelpview.h"
#include "appsettings.h"

#include "petscii_canvas.h"

#include "char_selector.h"
#include "color_picker.h"

#include "pmtoolbar.h"
#include "screen_carousel.h"


#include "petscii_undo.h"

#include "petscii_fileio.h"

#include "tooltypepref.h"
#include <workbench/startup.h>

#include <stdio.h>


struct Task *myTask = NULL;


const char *pVersion = "$VER: PetMate " PETMATE_VERSION;

/* Library bases */
struct IntuitionBase   *IntuitionBase = NULL;
struct GfxBase         *GfxBase       = NULL;
struct Library         *UtilityBase   = NULL;
struct Library         *LayersBase    = NULL;
struct Library         *IconBase      = NULL;
struct Library         *AslBase       = NULL;
struct Library         *GadToolsBase  = NULL;
/* libraries we consider optional and accept NULL values */
struct LocaleBase      *LocaleBase    = NULL;
struct Library         *DataTypesBase = NULL;
struct Library          *CyberGfxBase = NULL;
#ifdef HELP_USE_AGLIB
struct Library          *AmigaGuideBase = NULL;
#endif

/* BOOPSI class bases */
struct Library *WindowBase = NULL;
struct Library *LayoutBase = NULL;
struct Library *ButtonBase = NULL;
struct Library *LabelBase  = NULL;
struct Library *CheckBoxBase=NULL;
struct Library *GetFileBase=NULL;
struct Library *RequesterBase=NULL;

/* Library table for automated opening/closing */
typedef struct {
    const char    *name;
    ULONG          version;
    struct Library **base;
} LibraryEntry;

/* Note, for library we need a minimal version number.
 *  It's the library OS number which goes more or less like that:
 * v34 -> OS2.x (Commodore)
 * v39 ->OS3.0
 * v40 ->OS3.1 (1993, commodore)
 * v44 ->OS3.5 (1998, Haage&p)
 * v45 ->OS3.9 (1999, Haage&p)  -> nowadays the base for some free distributions. (Coffin?)
 * v46 ->OS3.14 (2019, Hyperion)
 * v47 ->OS3.2 (2021, Hyperion) -note you can always upgrade -
 * (v50>= are meant to beOS4 PowerPC)

 But watch out, if we want to have BOOPSI classes OS3.9 compatible:
 OS3.9 consider it just upgrades OS3.5, so a few BOOPSI class version are still "44".
*/
/* In this table are "mandatory" library and minimal versions */
static LibraryEntry libraryTable[] = {
    /* System libraries */
    {"intuition.library", 39, (struct Library **)&IntuitionBase},
    {"graphics.library",  39, (struct Library **)&GfxBase},
    {"utility.library",   39, &UtilityBase},
    {"layers.library",    39, &LayersBase},
    {"icon.library",      39, &IconBase},
    {"asl.library",       39, &AslBase},
    {"gadtools.library",  39, &GadToolsBase},
    /* BOOPSI class libraries - with minimal version of OS3.9 (not related to os!) */
    {"window.class",           42, &WindowBase},
    {"images/label.image",     42, &LabelBase},
    {"gadgets/layout.gadget",  42, &LayoutBase},
    {"gadgets/button.gadget",  42, &ButtonBase},
    {"gadgets/checkbox.gadget",  42, &CheckBoxBase},
    {"gadgets/getfile.gadget", 42, &GetFileBase},
    {"requester.class", 42, &RequesterBase},
    {NULL, 0, NULL} /* Terminator */
};

/* Clipboard for Edit > Copy/Paste Screen */
static PetsciiScreen *g_clipScreen = NULL;

/* from tooltypepref.c use both for tooltipes and appicon */
extern struct DiskObject *AppDiskObject;

struct App *app = NULL;

/* Forward declarations */
void UpdateCarouselThumbNail();
void cleanexit(const char *pmessage);
void exitclose(void);

void refreshUI(void);

void cleanexit(const char *pmessage)
{
    if (pmessage) printf("%s\n", pmessage);
    exit(0);
}

/*
 * refreshUI - sync all gadgets with the current project state.
 * Call after any action that may change the current screen, charset,
 * screen count, or other visible state.
 */
void refreshUI(void)
{
    /* delay and avoid agregation */
    if(!app) return;
    app->doRefreshUI++;
    if (myTask) Signal(myTask, SIGBREAKF_CTRL_F);
}

void refreshUI_Apply(void)
{
    PetsciiScreen *scr;
    UBYTE          charset;

    if (!app || !app->project) return;

// bdbprintf("refreshUI_Apply\n");

    scr = PetsciiProject_GetCurrentScreen(app->project);
    if (!scr) return;

    charset = scr->charset;

    /* Sync canvas to the current screen */
    SetGdAttrs(app->canvasGadget,
        PCA_Screen,   (ULONG)scr,
        PCA_ShowGrid, (ULONG)(BOOL)app->toolState.showGrid,

        PCA_Dirty,    (ULONG)TRUE,
        TAG_END);

    /* Sync CharSelector charset and background color */
    SetGdAttrs(app->charSelectorGadget,
        CHSA_Charset,  (ULONG)charset,
        CHSA_BgColor,  (ULONG)scr->backgroundColor,
        TAG_END);

    /* Sync charset toggle buttons */
    if (CurrentMainWindow && app->charsetUpperBtn && app->charsetLowerBtn) {
        SetGdAttrs(app->charsetUpperBtn,
            GA_Selected, (ULONG)(charset == PETSCII_CHARSET_UPPER),
            TAG_END);
        SetGdAttrs(app->charsetLowerBtn,
            GA_Selected, (ULONG)(charset == PETSCII_CHARSET_LOWER),
            TAG_END);
    }

    /* Sync bg/border color watches to the current screen's colors */
    if (CurrentMainWindow && app->toolbar.bgColorWatch && app->toolbar.borderColorWatch) {
        SetGdAttrs(app->toolbar.bgColorWatch,
            CSW_ColorIndex, (ULONG)scr->backgroundColor,
            TAG_END);
        SetGdAttrs(app->toolbar.borderColorWatch,
            CSW_ColorIndex, (ULONG)scr->borderColor,
            TAG_END);
    }

    /* Sync screen carousel: update current highlight and all thumbnails */
    if (app->carouselGadget) {
        SetGdAttrs(app->carouselGadget,
            SCA_CurrentScreen, (ULONG)app->project->currentScreen,
            SCA_AllModified,   TRUE,
            TAG_END);

    }

    /* Sync toolbar selected tool */
    PmToolbar_SetActiveTool(&app->toolbar,
                            app->toolState.currentTool,
                            CurrentMainWindow);


}


void SetStatusBarMessage(int enumMessage)
{
    if(!app) return;

    SetGdAttrs(app->statusBarLabel,
        GA_Text,(ULONG)LOC(enumMessage),
        TAG_END
    );

}

void setTool(ULONG newTool)
{
    int emess;
    if(newTool == app->toolState.currentTool) return;

    if(newTool>=TOOL_DRAW && newTool<=TOOL_CHARDRAW)
        app->toolState.lastDrawTool = newTool;

    app->toolState.currentTool = newTool;

    SetGdAttrs(app->canvasGadget,
        PCA_CurrentTool, (ULONG)newTool,TAG_END);

    PmToolbar_SetActiveTool(&app->toolbar, newTool,
                        CurrentMainWindow);

    /* the localization enum follow the tool enum order (TOOL_REVERSE is separate): */
    if (newTool == TOOL_REVERSE)
        SetStatusBarMessage(MSG_STATUS_REVERSE);
    else
        SetStatusBarMessage(MSG_STATUS_DRAW+newTool);

}

void updateCharSelectedLabel(ULONG ichar);

int testGadgetRect(Object *o, int x, int y)
{
    struct Gadget *g= (struct Gadget *)o;
    return (x >= (int)g->LeftEdge  &&
            x < (int)(g->LeftEdge+g->Width) &&
            y >= (int)g->TopEdge  &&
            y < (int)(g->TopEdge+g->Height)
            );
}
void SetCurrentScreen_BgColor(ULONG c)
{
    if(!app) return;
    app->toolState.bgColor = (UBYTE)c;

    SetGdAttrs(app->canvasGadget,
        PCA_BgColor, c,
        TAG_END);
    SetGdAttrs(app->charSelectorGadget,
        CHSA_BgColor, c,
        TAG_END);
    SetGdAttrs(app->toolbar.bgColorWatch,
        CSW_ColorIndex, c,
        TAG_END);
}
void SetCurrentScreen_BdColor(ULONG c)
{
    if(!app) return;
    app->toolState.bdColor = (UBYTE)c;

    SetGdAttrs(app->toolbar.borderColorWatch,
        CSW_ColorIndex, c,
        TAG_END);

    SetGdAttrs(app->canvasGadget,
        PCA_BdColor, c,
        TAG_END);
}


/* - - - - - - - - - MAIN - - - - - - - - - */

int main(int argc, char **argv)
{

    myTask = FindTask(NULL);
    atexit(&exitclose);
    /* Open all required libraries via table */
    {
        LibraryEntry *entry;
        char errorMsg[80];

        for (entry = libraryTable; entry->name != NULL; entry++) {
            *(entry->base) = OpenLibrary(entry->name, entry->version);
            if (!*(entry->base)) {
                snprintf(errorMsg, 79, "Can't open %s", entry->name);
                cleanexit(errorMsg);
            }
        }
    }


    /* optional libs, can return NULL */
    DataTypesBase = OpenLibrary("datatypes.library",39);
    CyberGfxBase  = OpenLibrary("cybergraphics.library", 1);
    /* at first need ! AmigaGuideBase = OpenLibrary("amigaguide.library", 39); */

    /* Open locale.library (optional - soft failure) */
    LocaleBase = (struct LocaleBase *)OpenLibrary("locale.library", 38);

    /* Allocate app struct (zero-initialized so disposeQ.count = 0 etc.) */
    app = (struct App *)AllocVec(sizeof(struct App), MEMF_CLEAR);
    if (!app) cleanexit("Can't allocate app");

    /* Initialize tool state defaults */
    app->toolState.currentTool = TOOL_DRAW;
    app->toolState.selectedChar = 0xa0;   /* space reverted */
    app->toolState.fgColor = C64_LIGHTBLUE;
    app->toolState.bgColor = C64_BLUE;
    app->toolState.showGrid = 0;

    /* read settnigs from icon tooltypes (very Amigaish thing)
     *  if launch from command line */
    if(argc>0)
    {
        int result = ToolTypePrefs_Init(argv[0]);
    } else
    if(argc ==0 && argv != 0)
    { /*  if launch from Workbench, Amiga C startup must give us that struct that way: */
        int i;
        struct WBStartup *WBenchMsg = (struct WBStartup *)argv; // Amiga C startup magic.
        struct WBArg *wbarg=WBenchMsg->sm_ArgList;
        for( i=0 ;
            i < WBenchMsg->sm_NumArgs;
            i++, wbarg++)
        {
            if((*wbarg->wa_Name) && (ToolTypePrefs_Init(wbarg->wa_Name))) break;
        } // end loop by wbarg
    }

    /* Create initial project */
    app->project = PetsciiProject_Create();
    if (!app->project) cleanexit("Can't create project");

    /* Initialize locale (no catalog for now - English built-in) */
    PmLocale_Init("PetMate.catalog", 0);

    /* Initialize action system (localizes action names) */
    PmAction_Init();

    /* Set up action context */
    app->actionCtx.pproject  = &app->project;
    app->actionCtx.toolState = (void *)&app->toolState;
    app->actionCtx.style     = (void *)&app->style;
    app->actionCtx.clipScreen = (void *)&g_clipScreen;

    app->actionCtx.pmenu     = (void *)&app->mainwindow.menu;

    /* Load application settings from icon tooltypes and sync to settings view */
    AppSettings_Load(&app->appSettings);

    /* Initialize the BOOPSI message target model */
    if (!initMessageTargetModel()) cleanexit("Can't create BOOPSI message target");

    /* Initialize PetsciiCanvas BOOPSI class */
    if (!PetsciiCanvas_Init()) cleanexit("Can't create PetsciiCanvas class");

    /* Initialize CharSelector and ColorPicker BOOPSI classes */
    if (!CharSelector_Init()) cleanexit("Can't create CharSelector class");
    if (!ColorPicker_Init())  cleanexit("Can't create ColorPicker class");

    if (!CharLayout_Init() ||
        !ColorSwatch_Init() ||
        !LayoutWithPopup_Init()
            )  cleanexit("Can't create CharLayout class");


    /* Create canvas gadget for the main editing area */
    app->canvasGadget = (Object *)NewObject(PetsciiCanvasClass, NULL,
        GA_ID,           (ULONG)GAD_CANVAS,
        ICA_TARGET,        (ULONG)TargetInstance,
        //tests
    // GA_FollowMouse, TRUE,
     GA_Immediate,   TRUE,
    // GA_RelVerify,   TRUE,
        PCA_Screen,      (ULONG)app->project->screens[0],
        PCA_Style,       (ULONG)&app->style,
        PCA_ZoomLevel,   (ULONG)1,
        PCA_ShowGrid,    (ULONG)FALSE,
        PCA_KeepRatio,   (ULONG)TRUE,
        PCA_CurrentTool, (ULONG)app->toolState.currentTool,
        PCA_SelectedChar,(ULONG)app->toolState.selectedChar,
        PCA_FgColor,     (ULONG)app->toolState.fgColor,
        PCA_BgColor,     (ULONG)app->toolState.bgColor,

        TAG_END);
    if (!app->canvasGadget) cleanexit("Can't create canvas gadget");

    /* Create character selector gadget */
    app->charSelectorGadget = (Object *)NewObject(CharSelectorClass, NULL,
        GA_ID,             (ULONG)GAD_CHARSELECTOR,
        ICA_TARGET,        (ULONG)TargetInstance,
        CHSA_Style,        (ULONG)&app->style,
        CHSA_Charset,      (ULONG)app->project->screens[0]->charset,
        CHSA_SelectedChar, (ULONG)app->toolState.selectedChar,
        CHSA_FgColor,      (ULONG)app->toolState.fgColor,
        CHSA_BgColor,      (ULONG)app->toolState.bgColor,
        CHSA_KeepRatio,    (ULONG)TRUE,
        TAG_END);
    if (!app->charSelectorGadget) cleanexit("Can't create char selector gadget");


    app->currentCharLabel = (Object *)NewObject(BUTTON_GetClass(), NULL,
        GA_ReadOnly, TRUE,
        BUTTON_BevelStyle, BVS_NONE,
      //  BUTTON_Transparent, TRUE,
        BUTTON_Justification, BCJ_LEFT,
        GA_Text,"C: ",
        TAG_END);

    /* Create foreground colour picker gadget */
    app->colorPickerFgGadget = (Object *)NewObject(ColorPickerClass, NULL,
        GA_ID,             (ULONG)GAD_COLORPICKER_FG,
        ICA_TARGET,        (ULONG)TargetInstance,
        CPA_Style,         (ULONG)&app->style,
        CPA_SelectedColor, (ULONG)app->toolState.fgColor,
        TAG_END);
    if (!app->colorPickerFgGadget) cleanexit("Can't create fg color picker");
    /* Create background colour picker gadget */
//    app->colorPickerBgGadget = (Object *)NewObject(ColorPickerClass, NULL,
//        GA_ID,             (ULONG)GAD_COLORPICKER_BG,

//        ICA_TARGET,        (ULONG)TargetInstance,
//        CPA_Style,         (ULONG)&app->style,
//        CPA_SelectedColor, (ULONG)app->toolState.bgColor,
//        TAG_END);
//    if (!app->colorPickerBgGadget) cleanexit("Can't create bg color picker");

    app->colorPickerPopUp = (Object *)NewObject(ColorPickerClass, NULL,
        GA_ID,             (ULONG)GAD_COLORPICKER_POPUP,
        ICA_TARGET,        (ULONG)TargetInstance,
        CPA_Style,         (ULONG)&app->style,
        CPA_SelectedColor, (ULONG)app->toolState.bdColor,
        TAG_END);
    app->colorPickerPopUpLabel = (Object *)NewObject(BUTTON_GetClass(), NULL,
        GA_ReadOnly, TRUE,
        BUTTON_BevelStyle, BVS_NONE,
        BUTTON_Transparent, FALSE,
        BUTTON_Justification, BCJ_LEFT,
       // GA_Text, (ULONG)"Label text here",
        TAG_END);

    app->colorPickerPopUpLayout = (Object *)NewObject(LAYOUT_GetClass(), NULL,
            LAYOUT_Orientation,  LAYOUT_ORIENT_VERT,
            LAYOUT_InnerSpacing, 0,
            LAYOUT_AddChild, (ULONG)app->colorPickerPopUpLabel,
                CHILD_WeightedHeight, 0,
            LAYOUT_AddChild, (ULONG)app->colorPickerPopUp,
                CHILD_WeightedHeight, 1,
            TAG_END);

    /* Phase 7: create toolbar and screen-tab bar */
    if (!PmToolbar_Create(&app->toolbar,&app->style))
        cleanexit("Can't create toolbar");

    if (!ScreenCarousel_Init())
        cleanexit("Can't create ScreenCarousel class");

    app->carouselGadget = (Object *)NewObject(ScreenCarouselClass, NULL,
        GA_ID,         (ULONG)GAD_SCREENCAROUSEL,
        GA_RELVERIFY,  TRUE,
        SCA_Project,   (ULONG)app->project,
        SCA_Style,     (ULONG)&app->style,
        TAG_END);
    if (!app->carouselGadget) cleanexit("Can't create screen carousel");

   // app->carouselScroller = (Object *)NewObject(SCROLLER_GetClass(), NULL,
   //      GA_ID,         (ULONG)GAD_CAROUSELSCROLLER,
   //      SCROLLER_Top, 0,
   //      SCROLLER_Total, 1,
   //      SCROLLER_Visible, 1, /* should be number of screens */
   //      SCROLLER_Orientation, FREEHORIZ,
   //      SCROLLER_Stretch,TRUE,
   //      SCROLLER_ArrowDelta,1,
   //      ICA_TARGET, TargetInstance,
   //      /* TODO set initial attribs here */
   //      TAG_END);
   //  if (!app->carouselScroller) cleanexit("Can't create scroller");


    /* Charset toggle buttons (placed at bottom of right panel) */
    app->charsetUpperBtn = (Object *)NewObject(BUTTON_GetClass(), NULL,
        GA_ID,       (ULONG)GAD_CHARSET_UPPER,
        BUTTON_PushButton, TRUE, /* this is a toggle button */
        ICA_TARGET,TargetInstance,
        GA_Selected, (ULONG)TRUE,  /* default: upper charset */
        GA_Text,     (ULONG)LOC(MSG_BTN_CHARSET_UPPER),
        TAG_END);
    if (!app->charsetUpperBtn) cleanexit("Can't create charset upper button");

    app->charsetLowerBtn = (Object *)NewObject(BUTTON_GetClass(), NULL,
        GA_ID,       (ULONG)GAD_CHARSET_LOWER,
        BUTTON_PushButton, TRUE, /* this is a toggle button */
        ICA_TARGET,TargetInstance,
        GA_Selected, (ULONG)FALSE,
        GA_Text,     (ULONG)LOC(MSG_BTN_CHARSET_LOWER),
        TAG_END);
    if (!app->charsetLowerBtn) cleanexit("Can't create charset lower button");

    /* Create status bar */
    app->statusBarLabel = (Object *)NewObject(BUTTON_GetClass(), NULL,
        GA_ReadOnly, TRUE,
        BUTTON_BevelStyle, BVS_NONE,
        BUTTON_Transparent, TRUE,
        BUTTON_Justification, BCJ_LEFT,
        GA_Text, (ULONG)LOC(MSG_STATUS_READY),
        TAG_END);

    app->statusBarLayout = (Object *)NewObject(LAYOUT_GetClass(), NULL,
        LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ,
        LAYOUT_BevelStyle, BVS_SBAR_VERT,
        LAYOUT_AddChild, (ULONG)app->statusBarLabel,
        TAG_END);

    /*
     * Create main layout - Phase 7 full layout:
     *
     *   [ScreenTabs        - WeightedHeight=0]
     *   HLayout
     *     [Toolbar         - WeightedWidth=0]
     *     [PetsciiCanvas   - WeightedWidth=1000]
     *     VLayout (right panel)
     *       [ColorPicker FG - WeightedHeight=0]
     *       [ColorPicker BG - WeightedHeight=0]
     *       [CharSelector   - WeightedHeight=1000]
     *       HLayout (charset)
     *         [Upper] [Lower]
     *   [StatusBar         - WeightedHeight=0]
     */
    {
        Object *charsetLayout;
        Object *rightPanelLayout;
        Object *workAreaLayout;
        Object *canvhl;

        /* Charset toggle buttons (bottom of right panel) */
        charsetLayout = (Object *)NewObject(LAYOUT_GetClass(), NULL,
            LAYOUT_Orientation,  LAYOUT_ORIENT_HORIZ,
            LAYOUT_InnerSpacing, 1,

            LAYOUT_AddChild, (ULONG)app->charsetUpperBtn,
                CHILD_WeightedWidth, 1000,
            LAYOUT_AddChild, (ULONG)app->charsetLowerBtn,
                CHILD_WeightedWidth, 1000,
            TAG_END);

        /* Right panel: pickers on top, char selector, charset at bottom */
        {
            Object *DrawColorLabel = (Object *)NewObject(BUTTON_GetClass(), NULL,
                GA_ReadOnly, TRUE,
                BUTTON_BevelStyle, BVS_NONE,
                BUTTON_Transparent, TRUE,
                BUTTON_Justification, BCJ_LEFT,
                GA_Text, (ULONG)LOC(MSG_LABEL_DRAWCOLOR),
                TAG_END);
//            Object *BGColorLabel = (Object *)NewObject(BUTTON_GetClass(), NULL,
//                GA_ReadOnly, TRUE,
//                BUTTON_BevelStyle, BVS_NONE,
//                BUTTON_Transparent, TRUE,
//                BUTTON_Justification, BCJ_LEFT,
//                GA_Text, (ULONG)LOC(MSG_LABEL_BACKGROUNDCOLOR),
//                TAG_END);
            // Object *Spacer = (Object *)NewObject(BUTTON_GetClass(), NULL,
            //     GA_ReadOnly, TRUE,
            //     BUTTON_BevelStyle, BVS_NONE,
            //     BUTTON_Transparent, TRUE,
            //     BUTTON_Justification, BCJ_LEFT,
            //     GA_Text,"",
            //     TAG_END);

        rightPanelLayout = (Object *)NewObject(/*LAYOUT_GetClass()*/ CharLayoutClass, NULL,
            LAYOUT_Orientation,  LAYOUT_ORIENT_VERT,
            LAYOUT_InnerSpacing, 2,

            LAYOUT_AddChild, (ULONG)DrawColorLabel,
                CHILD_WeightedHeight, 0,

            LAYOUT_AddChild, (ULONG)app->colorPickerFgGadget,
                CHILD_WeightedHeight, 4,
                CHILD_MinHeight,      16,

            LAYOUT_AddChild, (ULONG)app->charSelectorGadget,
                CHILD_WeightedHeight, 16,

             LAYOUT_AddChild, (ULONG)app->currentCharLabel,
                CHILD_WeightedHeight, 0,

            LAYOUT_AddChild, (ULONG)charsetLayout,
                CHILD_WeightedHeight, 0,

            TAG_END);

        }

        canvhl = (Object *)NewObject(LAYOUT_GetClass(), NULL,
            LAYOUT_Orientation,  LAYOUT_ORIENT_VERT,
            LAYOUT_InnerSpacing, 4,
            LAYOUT_TopSpacing,2,
            LAYOUT_AddChild, (ULONG)app->toolbar.layout,
                CHILD_WeightedHeight, 3,
            LAYOUT_AddChild, (ULONG)app->canvasGadget,
                CHILD_WeightedHeight, 25, /* use width in char as weight */
            LAYOUT_AddChild, (ULONG)app->toolbar.layoutUndoRedo,
                CHILD_WeightedHeight, 3, /* use width in char as weight */

            TAG_END);


        /* Work area: toolbar+canvas | right panel */
        workAreaLayout = (Object *)NewObject(LAYOUT_GetClass(), NULL,
            LAYOUT_Orientation,  LAYOUT_ORIENT_HORIZ,
            LAYOUT_InnerSpacing, 2,

            LAYOUT_AddChild, (ULONG)canvhl,
                CHILD_WeightedWidth, 42, /* use width in char as weight */

            LAYOUT_AddChild, (ULONG)rightPanelLayout,
                CHILD_WeightedWidth, 16, /* use width of the char selector in char as weight */

            TAG_END);

        /* Main vertical layout: carousel | work area | status bar */
        app->mainvlayout = (Object *)NewObject(LayoutWithPopupClass, NULL,
            LAYOUT_DeferLayout,   TRUE,
            LAYOUT_SpaceOuter,    TRUE,
            LAYOUT_BottomSpacing, 2,
            LAYOUT_TopSpacing,    0,
            LAYOUT_LeftSpacing,   0,
            LAYOUT_RightSpacing,  0,
            LAYOUT_InnerSpacing,  0,
            LAYOUT_Orientation,   LAYOUT_ORIENT_VERT,
            LAYOUT_AddChild,(ULONG)app->colorPickerPopUpLayout,
               CHILD_MaxWidth,0,
               CHILD_MinWidth,0,
               CHILD_MaxHeight,0,
               CHILD_MinHeight,0,
               CHILD_WeightedHeight,0,

            LAYOUT_AddChild, (ULONG)app->carouselGadget,
                CHILD_WeightedHeight, 0,
                CHILD_MinHeight,      33+4,  /* SCREENMINI_H(27)+ITEM_INDENT(3)*2+SEL_BORDER*2 */
                CHILD_MaxHeight,      33+4,
            // LAYOUT_AddChild, (ULONG)app->carouselScroller,
            //     CHILD_WeightedHeight, 0,
            LAYOUT_AddChild, (ULONG)workAreaLayout,
                CHILD_WeightedHeight, 1000,

            LAYOUT_AddChild, (ULONG)app->statusBarLayout,
                CHILD_WeightedHeight, 0,

            LAYOUTWP_POPUPGADGET,(ULONG)app->colorPickerPopUpLayout,
            TAG_END);
    }

    if (!app->mainvlayout) cleanexit("Layout error");

    /* Create message port */
    app->app_port = CreateMsgPort();


    /* Create window object */
    {
        ULONG lasttag = TAG_END;
        ULONG lastValue = 0;
        ULONG backfill = BMainWindow_GetBackFillFromSettings(&app->appSettings);
        if(AppDiskObject)
        {
            lasttag = WINDOW_Icon;
            lastValue = (ULONG)AppDiskObject;
        }
    app->window_obj = (Object *)NewObject(WINDOW_GetClass(), NULL,
        WA_Left, 40,
        WA_Top, 40,
        WA_Width, 640,
        WA_Height, 400,
    /*given at opening    WA_CustomScreen, (ULONG)app->lockedscreen, */
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_MENUPICK | IDCMP_RAWKEY |
                  IDCMP_GADGETDOWN | IDCMP_GADGETUP | IDCMP_MOUSEMOVE,
        WA_Flags, WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET |
                  WFLG_SIZEGADGET | WFLG_ACTIVATE | WFLG_SMART_REFRESH

                  ,

        WA_ReportMouse,TRUE, //test
        WA_Title, (ULONG)LOC(MSG_WINDOW_TITLE),
        WINDOW_ParentGroup, (ULONG)app->mainvlayout,
        WINDOW_IconifyGadget, TRUE,
        WINDOW_IconTitle, (ULONG)"PetMate",
        WINDOW_AppPort, (ULONG)app->app_port,
        WA_BackFill,backfill, /* giving it early help earlier version of layout.gadgets. Better updated onr econfiguration on recent versions, it seems */
        lasttag,lastValue,
        TAG_END);
    }
    if (!app->window_obj) cleanexit("Can't create window");

    /* Initialize Project Settings window */
    if(!PmSettingsView_Init(&app->settingsView,
                                  LOC(MSG_SETTINGS)))
    {

      //  printf("Warning: Could not create Project Settings window\n");
    }

    /* Initialize Help window (loads PetMate.guide via datatypes) */
#ifdef HELP_USE_DATATYPE_AND_WINDOW
   PmHelpView_Init(&app->helpView, "PetMate Help", "PROGDIR:PetMate.guide");
#endif

    PmSettingsView_SetFSModeIdLikeWorkbench(&app->settingsView,app->appSettings.screenModeIdLikeWorkbench);
    PmSettingsView_SetModeId(&app->settingsView,app->appSettings.screenModeId);
    PmSettingsView_SetUseOneColorBg(&app->settingsView,app->appSettings.useOneColorBg);
    PmSettingsView_SetBgImagePath(&app->settingsView,app->appSettings.bgImagePath);

    /* Open the window */
    BMainWindow_SetTitle(&app->mainwindow,"PetMate v" PETMATE_VERSION);
    /*  Open the window or screen accoring to last prefs. */
    BMainWindow_Show(&app->mainwindow,app->window_obj,&app->appSettings);


    /* - - - Input Event Loop - - - */
    {
        ULONG winsignal;
        BOOL ok = TRUE;

        GetAttr(WINDOW_SigMask, app->window_obj, &winsignal);

        while (ok)
        {
        #ifdef HELP_USE_AGLIB
            struct AmigaGuideMsg *agm;
        #endif
            ULONG aGuideSignal = 0;
            ULONG helpWinSig = 0;
            ULONG result, waitedSignals, currentSignals, settingsSig;

            flushbdbprint();
        #ifdef HELP_USE_AGLIB
            aGuideSignal = (app->amigaGuideHandle)?(AmigaGuideSignal(app->amigaGuideHandle)):0;
        #endif
        #ifdef HELP_USE_DATATYPE_AND_WINDOW
            helpWinSig   = PmHelpView_GetSignalMask(&app->helpView);
        #endif
            settingsSig = PmSettingsView_GetSignalMask(&app->settingsView);

            waitedSignals = winsignal |
                settingsSig |
                helpWinSig |
                (1L << app->app_port->mp_SigBit) |
                aGuideSignal |
                SIGBREAKF_CTRL_C |
                SIGBREAKF_CTRL_F;

            currentSignals = Wait(waitedSignals);

            /* exit app at any moment from Ctrl-C signal, atexit() magic does anything needed. */
            if(currentSignals & SIGBREAKF_CTRL_C) exit(0);

            /* Handle settings window input when its signal fires */
            if (settingsSig && (currentSignals & settingsSig)) {
                PmSettingsView_HandleInput(&app->settingsView);
            }

            /* Handle help window input when its signal fires */
            #ifdef HELP_USE_DATATYPE_AND_WINDOW
            if (currentSignals & helpWinSig) {
                PmHelpView_HandleInput(&app->helpView);
            }
            #endif

            /* Process AmigaGuide messages using amigaguide.library (experimental doesnt work) */
            #ifdef HELP_USE_AGLIB
            if (currentSignals & aGuideSignal)
            {
                /* process amigaguide messages */
                while (agm = GetAmigaGuideMsg (app->amigaGuideHandle))
                {
                /* check message types */
                switch (agm->agm_Type)
                {
                    case ShutdownMsgID:
                     printf("aguide down\n");
                        //app->ai_AmigaGuide = NULL;
//                        if (agm->agm_Pri_Ret)
//                            DisplayError (agm->agm_Sec_Ret);
                    break;
                        case ToolStatusID:
                        if (agm->agm_Pri_Ret)
                            printf("aguide ToolStatusID:%d\n", (agm->agm_Sec_Ret));
                        break;
                        case ActiveToolID: // say hello ?
                        {

                        }
                        break;


                    default:
                    break;
                }
 printf("aguide signal %d\n",agm->agm_Type);
                /* Reply to the message */
                ReplyAmigaGuideMsg (agm);
                }
            }
            #endif


            while ((result = DoMethod(app->window_obj, WM_HANDLEINPUT, NULL))
                   != WMHI_LASTMSG)
            {
                flushbdbprint();
                switch (result & WMHI_CLASSMASK)
                {
                    case WMHI_RAWKEY:
                    {
                        /* keys managed at mainwindow level */
                        ULONG key = (result & 0x7f);
                        ULONG isup =  (result & 0x80);
                        ULONG qualifiers = 0;
                        if(WindowBase->lib_Version>=47)
                        {
                            GetAttr(app->window_obj,WINDOW_Qualifier,&qualifiers);
                        }

                        if(key>=0x50 && key<=0x55)
                        {
                            setTool( TOOL_DRAW + key - 0x50);
                        } else
                        /* arrows and tab apply to the char selector in all cases */
                        if((key>=0x4c && key<=0x4f) && !isup )
                        {
                            ULONG c = key;
                            if(qualifiers & (IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT)) c|=128;
                            if(app->charSelectorGadget)
                                SetGdAttrs(app->charSelectorGadget,CHSA_ArrowKey,c,TAG_END);
                        } else
                        if(( key == 0x42) && isup)
                        {
                            ULONG c = key;
                            if(app->charSelectorGadget)
                                SetGdAttrs(app->charSelectorGadget,CHSA_ArrowKey,c,TAG_END);
                        } else
                        switch(key)
                        {
                            /* F10 */
                            case 0x59: BMainWindow_Toggle(&app->mainwindow,app->window_obj,&app->appSettings); break;
                          //no, use close bt or amiga+Q  case 0x45:  ok = FALSE; break; /* ESC key to quit */
                            case 0x5F: Action_ProjectHelp(&app->actionCtx); break;
                            default:
                            break;
                        }
                    }
                        break;

                    case WMHI_CLOSEWINDOW:
                        ok = FALSE;
                        break;

                    case WMHI_GADGETUP:
                    {
                        /* Queue gadget-up event for dispatch on main task context,
                         * alongside OM_NOTIFY messages from ICA_TARGET gadgets. */

                        ULONG senderId = result & WMHI_GADGETMASK;
                // printf("add WMHI_GADGETUP mess %d\n",senderId);
                        BoopsiDelay_BeginMessage(DelayQueue, senderId);
                        BoopsiDelay_AddTag(DelayQueue, WMHI_GADGETUP, 1);
                        BoopsiDelay_EndMessage(DelayQueue);
                        break;
                    }
                    case WMHI_MOUSEMOVE:

                        if(CurrentMainWindow &&
                        (( CurrentMainWindow->Flags & WFLG_WINDOWACTIVE) !=0 )&&
                            app->canvasGadget)
                        {
                            /* do not force activation if popup is currently on */
                            ULONG popupIsOn;
                            GetAttr(LAYOUTWP_POPUPVISIBLE,app->mainvlayout,&popupIsOn);

                            /* trick to allow "hovering" with the canvas, before it is even clicked */
                            if(!popupIsOn &&
                              testGadgetRect(app->canvasGadget,CurrentMainWindow->MouseX,CurrentMainWindow->MouseY))
                            {
                                struct Gadget *canvas = (struct Gadget *)app->canvasGadget;
                                if((canvas->Activation & GACT_ACTIVEGADGET)==0 )
                                {
                                    /* This will make the canva receive mouse move. */
                                     ActivateGadget(canvas,CurrentMainWindow,NULL );
                                }
                            }

                        }
                        break;
                    case WMHI_ICONIFY:
                        {
                            #define DO_ICONIFY 1
                            BMainWindow_Close(&app->mainwindow,app->window_obj,DO_ICONIFY);
                        }
                        break;
                    case WMHI_UNICONIFY:
                        {
                           BMainWindow_Show(&app->mainwindow,app->window_obj,&app->appSettings);
                            if (!CurrentMainWindow) cleanexit("can't re-open window");
                        }
                        break;

                    case WMHI_MENUPICK:
                    {
                        /* Dispatch menu selection to action system */
                        UWORD menuCode = (UWORD)(result & WMHI_MENUMASK);
                        if (menuCode != MENUNULL) {
                            LONG actionID = PmMenu_ToActionID(&app->mainwindow.menu,
                                                              menuCode);
                            if (actionID >= 0) {
                                /* Snapshot before actions that modify screen data */
                                if (actionID == ACTION_EDIT_CLEAR_SCREEN ||
                                    actionID == ACTION_EDIT_SHIFT_LEFT   ||
                                    actionID == ACTION_EDIT_SHIFT_RIGHT  ||
                                    actionID == ACTION_EDIT_SHIFT_UP     ||
                                    actionID == ACTION_EDIT_SHIFT_DOWN   ||
                                    actionID == ACTION_EDIT_PASTE_SCREEN) {
                                    PetsciiScreen *mscr =

                                        PetsciiProject_GetCurrentScreen(app->project);
                                    PetsciiUndoBuffer_Push(mscr);

                                }

                                PmAction_Execute((ULONG)actionID,
                                                 &app->actionCtx);

                            }
                        }
                        break;
                    }

                    default:
                        break;
                }
            } /* end WM_HANDLEINPUT loop */

            /* Process delayed BOOPSI notifications.
             * Both WMHI_GADGETUP events (queued above) and OM_NOTIFY from
             * gadgets that have ICA_TARGET=TargetInstance land here, serialised
             * on the main task so it is safe to call any Intuition/gadget API. */
            if (DelayQueue && BoopsiDelay_HasMessages(DelayQueue)) {
                struct TagItem *msg;
                while ((msg = BoopsiDelay_NextMessage(DelayQueue)) != NULL) {
                    struct TagItem *ptag;
                    ULONG sender_ID = 0;

                    ptag = FindTagItem(GA_ID, msg);
                    if (ptag) sender_ID = ptag->ti_Data;

                    /* Screen carousel click: switch to the clicked screen */
                    if (sender_ID == GAD_SCREENCAROUSEL) {
                        if ((ptag = FindTagItem(WMHI_GADGETUP, msg)) != NULL &&
                            ptag->ti_Data != 0 && app->carouselGadget) {
                            ULONG clickedIdx = 0;
                            GetAttr(SCA_SignalScreen, app->carouselGadget, &clickedIdx);
                            if (clickedIdx < app->project->screenCount) {
                                PetsciiProject_SetCurrentScreen(app->project,
                                                                (UWORD)clickedIdx);


                            }
                        }
                    }

                    /* Tool select button range */
                    else if (sender_ID >= GAD_TOOL_FIRST &&
                             sender_ID <= GAD_TOOL_LAST &&
                             ((ptag = FindTagItem(GA_SELECTED, msg))!=0))
                     {
                         UBYTE newTool = (UBYTE)(sender_ID - GAD_TOOL_FIRST);
                        if(ptag->ti_Data
                        //&&  (newTool != app->toolState.currentTool)
                            )
                        {
                             /* down key */
                             setTool(newTool);
                        } else
                        {
                            // means signal up... force toggle button down
                            if(app->toolState.currentTool == newTool)
                            {
                                PmToolbar_ForceDownTrick(&app->toolbar, newTool,CurrentMainWindow);
                            }
                        }
                    }

                    else {
                        switch (sender_ID) {
                            case GAD_COLORWATCH_BD:
                            case GAD_COLORWATCH_BG:
                            {
                                ptag = FindTagItem(CSW_Clicked, msg);
                                if (ptag)
                                {
                                    LONG x,y, currentColor;
                                    ULONG labelEnum =(sender_ID==GAD_COLORWATCH_BG)?
                                         MSG_LABEL_BACKGROUNDCOLOR:MSG_LABEL_BORDERCOLOR;
                                    Object *sender = (sender_ID==GAD_COLORWATCH_BG)?
                                            app->toolbar.bgColorWatch:
                                            app->toolbar.borderColorWatch;
                                    GetAttr(GA_Left,sender,&x);
                                    GetAttr(GA_Top,sender,&y);
                                    GetAttr(CSW_ColorIndex,sender,&currentColor);

                                    SetAttrs(app->colorPickerPopUp,
                                            CPA_ColorRole,sender_ID,
                                            CPA_SelectedColor,currentColor,
                                            TAG_END
                                            );

                                    /* set popup label */
                                    SetGdAttrs(app->colorPickerPopUpLabel,
                                        GA_TEXT,(ULONG)LOC(labelEnum),
                                        TAG_END);
                                    // open the popup
                                    SetGdAttrs(app->mainvlayout,
                                        LAYOUTWP_SENDERID,sender_ID,
                                        LAYOUTWP_POPUPX,x,
                                        LAYOUTWP_POPUPY,y-64,
                                        LAYOUTWP_POPUPVISIBLE,TRUE,
                                        TAG_END
                                            );
                                    /* to be sure inactivation of this close the popup*/
                                    //noActivateGadget(app->colorPickerPopUp,CurrentMainWindow,NULL);

                                }

                            } break;
                            case GAD_CANVAS:
                                /* Draw stroke complete - canvas updated in-place */
                            {
                                ptag = FindTagItem(PCA_SignalStopTool, msg);
                                if (ptag)
                                {
                                    /* lasso/text ended: back to previous draw tool */
                                    setTool(app->toolState.lastDrawTool);
                                    /* brush may have just been created (lasso) */
                                    PmMenu_UpdateBrushMenu(&app->mainwindow.menu,
                                        CurrentMainWindow, app->canvasGadget);
                                }


                                ptag = FindTagItem(PCA_BrushRemoved, msg);
                                if (ptag)
                                {
                                    PmMenu_UpdateBrushMenu(&app->mainwindow.menu,
                                        CurrentMainWindow, app->canvasGadget);
                                }
                            }
                            break;

                            case GAD_CHARSELECTOR:
                            {                             
                                /* Value comes from OM_NOTIFY (preferred) or WMHI_GADGETUP */
                                ULONG newChar = 0;
                                ptag = FindTagItem(CHSA_SelectedChar, msg);
                                if (ptag)
                                {
                                    newChar = ptag->ti_Data;

                                    app->toolState.selectedChar = (UBYTE)newChar;
                                    SetGdAttrs(app->canvasGadget,
                                        PCA_SelectedChar, newChar,
                                        TAG_END);
                                    /* to be clear, set draw if was brush/lasso tool */
                                    if(app->toolState.currentTool == TOOL_LASSOBRUSH ||
                                       app->toolState.currentTool == TOOL_TEXT )
                                    {
                                        setTool(app->toolState.lastDrawTool);
                                    }
                                }
                                updateCharSelectedLabel(newChar);
                                break;
                            }

                            case GAD_COLORPICKER_FG:
                            {
                                ULONG newColor = 0;
                                ptag = FindTagItem(CPA_SelectedColor, msg);
                                if (ptag) newColor = ptag->ti_Data;
                                else GetAttr(CPA_SelectedColor,
                                             app->colorPickerFgGadget, &newColor);
                                app->toolState.fgColor = (UBYTE)newColor;

                                SetGdAttrs(app->canvasGadget,
                                    PCA_FgColor, newColor,
                                    TAG_END);
                                SetGdAttrs(app->charSelectorGadget,
                                    CHSA_FgColor, newColor,
                                    TAG_END);

                                break;
                            }
                            case GAD_COLORPICKER_POPUP:
                            {
                                /* message from the colorpicker popup */
                                ptag = FindTagItem(CPA_SelectedColor, msg);
                                if (ptag)
                                {
                                    ULONG newColor = ptag->ti_Data;

                                    /* close the popup first */
                                    SetGdAttrs(app->mainvlayout,
                                            LAYOUTWP_POPUPVISIBLE,FALSE,
                                            TAG_END);

                                     ptag = FindTagItem(CPA_ColorRole, msg);
                                     if(ptag)
                                     {
                                        ULONG role = ptag->ti_Data; /* the colorwatch id */
                                        if(role == GAD_COLORWATCH_BG)
                                        {
                                            SetCurrentScreen_BgColor(newColor);
                                        }else
                                        if(role == GAD_COLORWATCH_BD)
                                        {
                                           SetCurrentScreen_BdColor(newColor);
                                        }

                                        UpdateCarouselThumbNail();

                                     }

                                } // end if message is CPA_SelectedColor

                                break;
                            }
                            break;

                            case GAD_CHARSET_UPPER:
                            {
                                /* note message is bursted when keepping the bt click down */
                                if((ptag = FindTagItem(GA_SELECTED, msg))!=0)
                                {
                                    if(ptag->ti_Data !=0)
                                    {
                                        PetsciiScreen *scr =
                                            PetsciiProject_GetCurrentScreen(app->project);


                                        if (scr && scr->charset != PETSCII_CHARSET_UPPER) {
                                            scr->charset = PETSCII_CHARSET_UPPER;
                                            app->project->modified = 1;

                                            SetGdAttrs(app->charSelectorGadget,
                                                CHSA_Charset, PETSCII_CHARSET_UPPER,
                                                TAG_END);
                                                bdbprintf("PCA_Dirty from GAD_CHARSET_UPPER\n");
                                            SetGdAttrs(app->canvasGadget,
                                                PCA_Dirty, (ULONG)TRUE,
                                                TAG_END);
                                            SetGdAttrs(
                                                app->charsetLowerBtn,
                                                GA_Selected, FALSE, TAG_END);

                                        }
                                    } else
                                    {
                                        ULONG charsetSate=0;
                                        GetAttr(CHSA_Charset,app->charSelectorGadget,&charsetSate);
                                        if(charsetSate == PETSCII_CHARSET_UPPER)
                                        {
                                            SetGdAttrs(app->charsetUpperBtn,
                                                GA_Selected, (ULONG)TRUE,TAG_END);
                                        }

                                    }
                                } /* end if GA_SELECTED */
                                break;
                            }

                            case GAD_CHARSET_LOWER:
                            {
                                if(((ptag = FindTagItem(GA_SELECTED, msg))!=0) )
                                {

                                    if(ptag->ti_Data != 0)
                                    {
                                        PetsciiScreen *scr =
                                            PetsciiProject_GetCurrentScreen(app->project);


                                        if (scr && scr->charset != PETSCII_CHARSET_LOWER) {
                                            scr->charset = PETSCII_CHARSET_LOWER;
                                            app->project->modified = 1;
                                            SetGdAttrs(app->charSelectorGadget,
                                                CHSA_Charset, (ULONG)PETSCII_CHARSET_LOWER,
                                                TAG_END);
                                                bdbprintf("PCA_Dirty from GAD_CHARSET_LOWER\n");
                                            SetGdAttrs(app->canvasGadget,
                                                PCA_Dirty, (ULONG)TRUE,
                                                TAG_END);
                                            SetGdAttrs(
                                                (struct Gadget *)app->charsetUpperBtn,
                                                GA_Selected, FALSE, TAG_END);
                                        }
                                    } else
                                    {
                                        /* got up but was the selected one*/
                                        ULONG charsetSate=0;
                                        GetAttr(CHSA_Charset,app->charSelectorGadget,&charsetSate);
                                        if(charsetSate == PETSCII_CHARSET_LOWER)
                                        {
                                            SetGdAttrs(app->charsetLowerBtn,
                                                GA_Selected, (ULONG)TRUE,TAG_END);
                                        }

                                    }
                                } /* end if GA_SELECTED */
                                break;
                            }
                            /* thos 3 button actions are better managed on Up message (down burst many messages) */
                            case GAD_TOOL_UNDO:
                                if(((ptag = FindTagItem(WMHI_GADGETUP, msg))!=0) &&
                                    (ptag->ti_Data != 0) )
                                {
                                    PmAction_Execute(ACTION_EDIT_UNDO, &app->actionCtx);

                                }
                                break;

                            case GAD_TOOL_REDO:
                                if(((ptag = FindTagItem(WMHI_GADGETUP, msg))!=0) &&
                                    (ptag->ti_Data != 0) )
                                {
                                    PmAction_Execute(ACTION_EDIT_REDO, &app->actionCtx);

                                }
                                break;

                            case GAD_TOOL_CLEAR:
                            {
                                if(((ptag = FindTagItem(WMHI_GADGETUP, msg))!=0) &&
                                    (ptag->ti_Data != 0) )
                                {
                                    /* Snapshot before clear so it's undoable */
                                    PetsciiScreen *clrScr =
                                        PetsciiProject_GetCurrentScreen(app->project);


                                        PetsciiUndoBuffer_Push(clrScr);

                                    PmAction_Execute(ACTION_EDIT_CLEAR_SCREEN,
                                                     &app->actionCtx);

                                }
                                break;
                            }

                            default:
                                // bdbprintf("BoopsiDelay: unhandled GA_ID=%ld\n",
                                //           (long)sender_ID);
                                break;
                        }
                    }
                } /* end while BoopsiDelay_NextMessage */
            } /* end if BoopsiDelay_HasMessages */

        if(app->doRefreshUI>0)
        {
            //printf("app->doRefreshUI:%d\n",app->doRefreshUI);
            refreshUI_Apply();
            app->doRefreshUI = 0;
        }


        } /* end main loop */
    }

    return 0;
}

/* - - - - - - - - - CLEANUP - - - - - - - - - */

void exitclose(void)
{
    flushbdbprint();

   // printf("exitclose()\n");

    if (app)
    {
        /* save settings*/
        AppSettings_Save(&app->appSettings);
        AppSettings_Close(&app->appSettings);

        /* Destroy project */
        if (app->project) {
            PetsciiProject_Destroy(app->project);
            app->project = NULL;
        }


        PmSettingsView_Dispose(&app->settingsView);

        #ifdef HELP_USE_DATATYPE_AND_WINDOW
            PmHelpView_Dispose(&app->helpView);
        #endif

        if(app->aboutRequester)
        {
            DisposeObject(app->aboutRequester);
            app->aboutRequester = NULL;
        }


        if(app->window_obj)
        {
            BMainWindow_Close(&app->mainwindow,app->window_obj,0);
            /* this should cascade all OM_DISPOSE: */
            DisposeObject(app->window_obj);
        }


        /* Close the BOOPSI message target */
        closeMessageTargetModel();


        /* Free BOOPSI classes (instances disposed by window cascade above) */
        LayoutWithPopup_Exit();
        ColorSwatch_Exit();
        CharLayout_Exit();
        PetsciiCanvas_Exit();
        CharSelector_Exit();
        ColorPicker_Exit();
        ScreenCarousel_Exit();


        /* Delete message port */
        if (app->app_port)
            DeleteMsgPort(app->app_port);


        FreeVec(app);
        app = NULL;
    }

    /* Free clipboard screen */
    if (g_clipScreen) {
        PetsciiScreen_Destroy(g_clipScreen);
        g_clipScreen = NULL;
    }

    /* Close locale */
    PmLocale_Close();
    if (LocaleBase) {
        CloseLibrary((struct Library *)LocaleBase);
        LocaleBase = NULL;
    }

    /* Debug leak reports */
    bdbprintf_report_leaks();
    bdbprintf_report_classes();
    flushbdbprint();


    if(CyberGfxBase) CloseLibrary(CyberGfxBase);
    if(DataTypesBase) CloseLibrary(DataTypesBase);
#ifdef HELP_USE_AGLIB
    if(AmigaGuideBase) CloseLibrary(AmigaGuideBase);
#endif
    /* Close all other libraries in reverse order */
    {
        LibraryEntry *entry;
        int i;

        for (i = 0; libraryTable[i].name != NULL; i++)
            ;

        for (i = i - 1; i >= 0; i--) {
            entry = &libraryTable[i];
            if (*(entry->base)) {
                CloseLibrary(*(entry->base));
                *(entry->base) = NULL;
            }
        }
    }
}

void updateCharSelectedLabel(ULONG ichar)
{
    if(!app || !app->currentCharLabel) return;

    snprintf(&app->selectedCharLabelText[0],31,"C: $%02x / %d",ichar,ichar);
    SetGdAttrs(app->currentCharLabel,
                GA_TEXT,&app->selectedCharLabelText[0], TAG_END);


}
/* when color index changed, because of window/fullscreen switch or else */
 void RefreshAllColorGadgets()
 {
      if(!CurrentMainWindow) return;

    /* we do a full RefreshGList on main layout just after, so use SetAttrs()
     *  and not SetGadgetAttrs) so state change without redraw
     or it would redraw twice for nothing. */
     SetAttrs(app->charSelectorGadget,
         CHSA_Dirty,TRUE,TAG_END);

     SetAttrs(app->carouselGadget,
         SCA_Style,(ULONG)&app->style,TAG_END);

    /*force whole redraw*/
    if(CurrentMainWindow)
     RefreshGList((struct Gadget *)app->mainvlayout,
                  CurrentMainWindow, NULL, 1);


 }
void UpdateCarouselThumbNail()
{
    if(!app->carouselGadget) return;
        SetGdAttrs(app->carouselGadget,
            SCA_ScreenModified,app->project->currentScreen,TAG_END);

}
