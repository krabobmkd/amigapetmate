/*
 * ColorPicker - rendering: GM_RENDER, GM_HITTEST.
 *
 * Draws a 4x4 grid of C64 colour swatches using RectFill - no chunky
 * buffer needed.  The selected colour cell gets a COMPLEMENT border
 * drawn on top so it is always visible regardless of the swatch colour.
 */

#include <proto/graphics.h>
#include <proto/intuition.h>
#include <intuition/gadgetclass.h>
#include "color_picker_private.h"

#include <proto/cybergraphics.h>
#include <cybergraphics/cybergraphics.h>

extern struct Library *CyberGfxBase;


ULONG ColorPicker_OnDomain(Class *cl, Object *o, struct gpDomain  *msg)
{
    struct IBox *domain = &msg->gpd_Domain;

    // Set default dimensions
    domain->Left = 0;
    domain->Top = 0;
    domain->Width = 128;  // Nominal width
    domain->Height = 32;  // Nominal height

    // Adjust based on gpd_Which
    switch (msg->gpd_Which) {
        case GDOMAIN_MINIMUM:
            domain->Width = 96;
            domain->Height = 96/4;
            break;
        case GDOMAIN_MAXIMUM:
            domain->Width = 128*4;
            domain->Height = 128;
            break;
        case GDOMAIN_NOMINAL:
        default:
            // Use default values
            break;
    }

    return 1;
}


/* ------------------------------------------------------------------ */
/* GM_RENDER                                                            */
/* ------------------------------------------------------------------ */

ULONG ColorPicker_OnRender(Class *cl, Object *o, struct gpRender *msg)
{
    ColorPickerData *inst;
    struct RastPort *rp;
    WORD             left;
    WORD             top;
    WORD             width;
    WORD             height;
    UBYTE            colorIdx;
    UBYTE            useCgx;
    WORD             col;
    WORD             row;
    WORD             x,x2;
    WORD             y,y2;
    WORD             selCol;
    WORD             selRow;

    inst = (ColorPickerData *)INST_DATA(cl, o);
    if (!inst->style) return 0;

    useCgx = (CyberGfxBase &&
        msg->gpr_GInfo->gi_Screen &&
       msg->gpr_GInfo->gi_Screen->RastPort.BitMap &&
       (GetCyberMapAttr(msg->gpr_GInfo->gi_Screen->RastPort.BitMap, CYBRMATTR_ISCYBERGFX) != 0) &&
       (GetCyberMapAttr(msg->gpr_GInfo->gi_Screen->RastPort.BitMap, CYBRMATTR_DEPTH) > 8)
        );

    rp     = msg->gpr_RPort;
    left   = G(o)->LeftEdge;
    top    = G(o)->TopEdge;
    width  = G(o)->Width;
    height = G(o)->Height;

    if (width <= 2 || height <= 2) return 0;

    /* grid */
    SetAPen(rp, (LONG)PetsciiStyle_Pen(inst->style, 0));
    Move(rp, (LONG)left,(LONG)top+height-1);
    Draw(rp, (LONG)left,(LONG)top);
    Draw(rp, (LONG)left+width-1, (LONG)top);

    /* Draw all 16 colour swatches */
    for (colorIdx = 0; colorIdx < 16; colorIdx++) {
        col = (WORD)(colorIdx % COLORPICKER_COLS);
        row = (WORD)(colorIdx / COLORPICKER_COLS);

        x   = left +1+  (col *(width-1) /COLORPICKER_COLS);
        y   = top  +1+  (row *(height-1)/COLORPICKER_ROWS);
        x2   = left +1+  ((col+1) *(width-1) /COLORPICKER_COLS);
        y2   = top  +1+  ((row+1) *(height-1)/COLORPICKER_ROWS);

        if(useCgx)
        {
            //FillPixelArray(RastPort,DestX, DestY,SizeX,SizeY,ARGB)
            FillPixelArray(rp,(UWORD)x,(UWORD)y,(UWORD)(x2-1-x),(UWORD)(y2-1-y),
                inst->style->paletteARGB[colorIdx]);

        } else {
            SetAPen(rp, (LONG)PetsciiStyle_Pen(inst->style, colorIdx));
            RectFill(rp,
                 (LONG)x, (LONG)y,
                 (LONG)(x2 - 2),(LONG)(y2 - 2));
        }
                 /* grid like this */

//        if((colorIdx == inst-> selectedColor))
//        {
//            SetAPen(rp, (LONG)PetsciiStyle_Pen(inst->style,1));
//            Move(rp, (LONG)x-1, (LONG)y2-1);
//            Draw(rp, (LONG)x2-1, (LONG)y2-1);
//            Draw(rp, (LONG)x2-1, (LONG)y-1);
//            Draw(rp, (LONG)x-1, (LONG)y-1);
//            Draw(rp, (LONG)x-1, (LONG)y2-1);
//        } else
//        {
            SetAPen(rp, (LONG)PetsciiStyle_Pen(inst->style, 0));
            Move(rp, (LONG)x, (LONG)y2-1);
            Draw(rp, (LONG)x2-1, (LONG)y2-1);
            Draw(rp, (LONG)x2-1, (LONG)y-1);
//        }

    }

    /* COMPLEMENT border around the selected colour */
    selCol = (WORD)(inst->selectedColor % COLORPICKER_COLS);
    selRow = (WORD)(inst->selectedColor / COLORPICKER_COLS);

    x   = left +1+  (selCol *(width-1) /COLORPICKER_COLS);
    y   = top  +1+  (selRow *(height-1)/COLORPICKER_ROWS);
    x2   = left +1+  ((selCol+1) *(width-1) /COLORPICKER_COLS);
    y2   = top  +1+  ((selRow+1) *(height-1)/COLORPICKER_ROWS);

    SetAPen(rp, (LONG)PetsciiStyle_Pen(inst->style,1));
    Move(rp, (LONG)x-1, (LONG)y2-1);
    Draw(rp, (LONG)x2-1, (LONG)y2-1);
    Draw(rp, (LONG)x2-1, (LONG)y-1);
    Draw(rp, (LONG)x-1, (LONG)y-1);
    Draw(rp, (LONG)x-1, (LONG)y2-1);

//    selX   = left + selCol * cellW;
//    selY   = top  + selRow * cellH;


//    savedMode = rp->DrawMode;
//    SetDrMd(rp, COMPLEMENT);
//    SetAPen(rp, 1);

//    Move(rp, (LONG)selX,               (LONG)selY);
//    Draw(rp, (LONG)(selX + cellW - 1), (LONG)selY);
//    Draw(rp, (LONG)(selX + cellW - 1), (LONG)(selY + cellH - 1));
//    Draw(rp, (LONG)selX,               (LONG)(selY + cellH - 1));
//    Draw(rp, (LONG)selX,               (LONG)selY);

////    SetDrMd(rp, (LONG)savedMode);

//    drawSelection(rp,
//              WORD left, WORD top,
//              WORD width, WORD height,
//              WORD gridCol, WORD gridRow,
//              1);
    return 0;
}

/* ------------------------------------------------------------------ */
/* GM_HITTEST                                                           */
/* ------------------------------------------------------------------ */

ULONG ColorPicker_OnHitTest(Class *cl, Object *o, struct gpHitTest *msg)
{
    (void)cl; (void)o; (void)msg;
    return GMR_GADGETHIT;
}
