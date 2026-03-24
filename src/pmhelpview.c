/*
 * pmhelpview.c - Help window for Petmate Amiga.
 *
 * Opens PetMate.guide via the datatypes system as a BOOPSI gadget
 * embedded in a ReAction layout window.
 *
 * Key API:
 *   NewDTObject("PetMate.guide", GA_ID, GAD_HELP_DT, GA_RelVerify, TRUE, TAG_END)
 *   -> returns a BOOPSI gadget object; add it to a layout like any other gadget.
 *   The amigaguide.datatype (subclass of text.datatype) handles its own
 *   rendering, scrolling, and link navigation internally.
 *
 * If DataTypesBase is NULL or the file is not found, the window opens
 * with a plain "not found" text gadget instead.
 *
 * C89 compatible.
 */

#include <stdio.h>
#include <string.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/datatypes.h>
#include <datatypes/datatypes.h>
#include <intuition/classusr.h>
#include <intuition/icclass.h>

#include <proto/window.h>
#include <classes/window.h>

#include <proto/layout.h>
#include <gadgets/layout.h>

#include <proto/button.h>
#include <gadgets/button.h>

#include "pmhelpview.h"
#include "petmate.h"
#include <stdio.h>
extern struct Screen  *CurrentMainScreen;
extern struct Library *DataTypesBase;


//ULONG frameobject (struct GlobalData * gd)
//{
//    struct FrameInfo *fri = &gd->gd_FrameInfo;
//    struct DisplayInfo di;
//    struct dtFrameBox dtf;
//    ULONG modeid;

//    /* Get the display information */
//    modeid = GetVPModeID (&(gd->gd_Screen->ViewPort));
//    GetDisplayInfoData (NULL, (APTR) & di, sizeof (struct DisplayInfo), DTAG_DISP, modeid);

//    /* Fill in the frame info */
//    fri->fri_PropertyFlags = di.PropertyFlags;
//    fri->fri_Resolution = *(&di.Resolution);
//    fri->fri_RedBits = di.RedBits;
//    fri->fri_GreenBits = di.GreenBits;
//    fri->fri_BlueBits = di.BlueBits;
//    fri->fri_Dimensions.Width = gd->gd_Screen->Width;
//    fri->fri_Dimensions.Height = gd->gd_Screen->Height;
//    fri->fri_Dimensions.Depth = gd->gd_Screen->BitMap.Depth;
//    fri->fri_Screen = gd->gd_Screen;
//    fri->fri_ColorMap = gd->gd_Screen->ViewPort.ColorMap;

//    /* Send the message */
//    dtf.MethodID = DTM_FRAMEBOX;
//    dtf.dtf_ContentsInfo = &gd->gd_FrameInfo;
//    dtf.dtf_SizeFrameInfo = sizeof (struct FrameInfo);
//    dtf.dtf_FrameFlags = FRAMEF_SPECIFY;
//    return (DoDTMethodA (gd->gd_DisplayObject, gd->gd_Window, NULL, (Msg)&dtf));
//}


/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

BOOL PmHelpView_Init(PmHelpView *phv, const char *title, const char *guidePath)
{
    if (!phv) return FALSE;

    memset(phv, 0, sizeof(PmHelpView));
printf("DataTypesBase:%08x\n",(int)DataTypesBase);
    /*
     * Try to load the guide file as a datatype object.
     * NewDTObject() auto-detects the amigaguide.datatype from the file.
     * The returned object IS a BOOPSI gadget and can be added to a layout.
     */

    if (DataTypesBase && guidePath) {
        phv->dtObj = NewDTObject((APTR)guidePath,
                       //     DTA_Class,GID_DOCUMENT,
						GA_Immediate, TRUE,
						GA_RelVerify, TRUE,
           // DTA_GROUP, GID_DOCUMENT, // GID_TEXT,
           // ICA_TARGET, ICTARGET_IDCMP,
           GA_ID,          GAD_HELP_DT,
          // DTA_Name,(ULONG)"PetMate.guide",

            TAG_END);
            printf("dt:%08x\n",(int) phv->dtObj);
    }

    /* Build the main layout */
    if (phv->dtObj) {

        Object *msgLabel = NewObject(BUTTON_GetClass(), NULL,
            GA_ReadOnly,          TRUE,
            BUTTON_BevelStyle,    BVS_NONE,
            BUTTON_Justification, BCJ_CENTER,
            GA_Text, (ULONG)"Help file.",
            TAG_END);

        /*
         * The datatype object fills the whole layout area.
         * It manages its own scrolling and rendering.
         */
        phv->mainLayout = NewObject(LAYOUT_GetClass(), NULL,
            LAYOUT_DeferLayout,   TRUE,
            LAYOUT_Orientation,   LAYOUT_ORIENT_VERT,
            LAYOUT_BevelStyle,    BVS_NONE,
            LAYOUT_SpaceOuter,    FALSE,
            LAYOUT_SpaceInner,    FALSE,
            LAYOUT_AddChild,      (ULONG)msgLabel,
            CHILD_WeightedHeight, 0,

            LAYOUT_AddChild,      (ULONG)phv->dtObj,
            CHILD_WeightedHeight, 1,
            CHILD_DataType, TRUE,
            TAG_END);
    } else {
        /* Fallback: plain message when guide is missing or DT unavailable */
        Object *msgLabel = NewObject(BUTTON_GetClass(), NULL,
            GA_ReadOnly,          TRUE,
            BUTTON_BevelStyle,    BVS_NONE,
            BUTTON_Justification, BCJ_CENTER,
            GA_Text, (ULONG)"Help file (PetMate.guide) not found.\n"
                            "Place PetMate.guide next to the executable.",
            TAG_END);

        phv->mainLayout = NewObject(LAYOUT_GetClass(), NULL,
            LAYOUT_DeferLayout,   TRUE,
            LAYOUT_Orientation,   LAYOUT_ORIENT_VERT,
            LAYOUT_BevelStyle,    BVS_NONE,
            LAYOUT_SpaceOuter,    TRUE,
            LAYOUT_SpaceInner,    TRUE,
            LAYOUT_AddChild,      (ULONG)msgLabel,
            CHILD_WeightedHeight, 0,
            TAG_END);
    }

    if (!phv->mainLayout) {
        if (phv->dtObj) {
            DisposeDTObject(phv->dtObj);
            phv->dtObj = NULL;
        }
        return FALSE;
    }

    /* BOOPSI window object */
    phv->windowObj = NewObject(WINDOW_GetClass(), NULL,
//        WA_Left,    80,
//        WA_Top,     60,
        WA_Width,   320,
        WA_Height,  200,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_MENUPICK | IDCMP_RAWKEY |
                  IDCMP_GADGETDOWN | IDCMP_GADGETUP | IDCMP_MOUSEMOVE,
/*        WA_Flags, WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET |
                  WFLG_SIZEGADGET | WFLG_ACTIVATE | WFLG_SMART_REFRESH
                  ,
*/
        WA_IDCMP,   IDCMP_CLOSEWINDOW | IDCMP_RAWKEY,
        WA_Flags,   WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET |
                    WFLG_SIZEGADGET | WFLG_ACTIVATE | WFLG_SMART_REFRESH,
        WA_Title,   (ULONG)title,
        WINDOW_ParentGroup, (ULONG)phv->mainLayout,
        TAG_END);

    if (!phv->windowObj) {
        DisposeObject(phv->mainLayout);   /* cascades to dtObj / msgLabel */
        phv->mainLayout = NULL;
        phv->dtObj      = NULL;
        return FALSE;
    }

    return TRUE;
}

void PmHelpView_Open(PmHelpView *phv)
{
    if (!phv || !phv->windowObj) return;
    if (phv->window) return; /* already open */

    if (CurrentMainScreen) {
        SetAttrs(phv->windowObj,
                 WA_CustomScreen, (ULONG)CurrentMainScreen,
                 TAG_END);
    }

    phv->window = (struct Window *)DoMethod(phv->windowObj, WM_OPEN, NULL);

//    if(phv->dtObj && phv->window)
//    {
//         struct FrameInfo		 gd_FrameInfo;
//        struct FrameInfo *fri = &gd_FrameInfo;
//        struct DisplayInfo di;
//        struct dtFrameBox dtf;
//        ULONG modeid;

//        /* Get the display information */
//        modeid = GetVPModeID (&(CurrentMainScreen->ViewPort));
//        GetDisplayInfoData (NULL, (APTR) & di, sizeof (struct DisplayInfo), DTAG_DISP, modeid);

//        /* Fill in the frame info */
//        fri->fri_PropertyFlags = di.PropertyFlags;
//        fri->fri_Resolution = *(&di.Resolution);
//        fri->fri_RedBits = di.RedBits;
//        fri->fri_GreenBits = di.GreenBits;
//        fri->fri_BlueBits = di.BlueBits;
//        fri->fri_Dimensions.Width = CurrentMainScreen->Width;
//        fri->fri_Dimensions.Height =CurrentMainScreen->Height;
//        fri->fri_Dimensions.Depth = CurrentMainScreen->BitMap.Depth;
//        fri->fri_Screen = CurrentMainScreen;
//        fri->fri_ColorMap = CurrentMainScreen->ViewPort.ColorMap;

//        /* Send the message */
//        dtf.MethodID = DTM_FRAMEBOX;
//        dtf.dtf_ContentsInfo = fri;
//        dtf.dtf_SizeFrameInfo = sizeof (struct FrameInfo);
//        dtf.dtf_FrameFlags = FRAMEF_SPECIFY;
//        DoDTMethodA (phv->dtObj, phv->window, NULL, (Msg)&dtf);
//    }


}

void PmHelpView_Close(PmHelpView *phv)
{
    if (!phv || !phv->windowObj || !phv->window) return;

    DoMethod(phv->windowObj, WM_CLOSE, NULL);
    phv->window = NULL;
}

BOOL PmHelpView_HandleInput(PmHelpView *phv)
{
    ULONG result;

    if (!phv || !phv->windowObj) return FALSE;
    if (!phv->window) return TRUE;

    while ((result = DoMethod(phv->windowObj, WM_HANDLEINPUT, NULL))
           != WMHI_LASTMSG)
    {
        switch (result & WMHI_CLASSMASK)
        {
            case WMHI_CLOSEWINDOW:
                PmHelpView_Close(phv);
                return TRUE;

            case WMHI_RAWKEY:
                if ((result & WMHI_KEYMASK) == 0x45) { /* ESC */
                    PmHelpView_Close(phv);
                    return TRUE;
                }
                break;

            default:
                break;
        }
    }

    return TRUE;
}

ULONG PmHelpView_GetSignalMask(PmHelpView *phv)
{
    if (!phv || !phv->window) return 0;
    return (1L << phv->window->UserPort->mp_SigBit);
}

void PmHelpView_Dispose(PmHelpView *phv)
{
    if (!phv) return;

    if (phv->window) {
        DoMethod(phv->windowObj, WM_CLOSE, NULL);
        phv->window = NULL;
    }

    if (phv->windowObj) {
        DisposeObject(phv->windowObj); /* cascades: disposes layout and dtObj */
        phv->windowObj  = NULL;
        phv->mainLayout = NULL;
        phv->dtObj      = NULL;
    }
}
