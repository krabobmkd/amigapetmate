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
    WORD             cellW;
    WORD             cellH;
    UBYTE            colorIdx;
    UBYTE            savedMode;
    WORD             col;
    WORD             row;
    WORD             x;
    WORD             y;
    WORD             selCol;
    WORD             selRow;
    WORD             selX;
    WORD             selY;

    (void)cl;

    inst = (ColorPickerData *)INST_DATA(cl, o);
    if (!inst->style) return 0;

    rp     = msg->gpr_RPort;
    left   = G(o)->LeftEdge;
    top    = G(o)->TopEdge;
    width  = G(o)->Width;
    height = G(o)->Height;

    if (width <= 0 || height <= 0) return 0;

    cellW = width  / COLORPICKER_COLS;
    cellH = height / COLORPICKER_ROWS;
    if (cellW < 1 || cellH < 1) return 0;

    /* Draw all 16 colour swatches */
    for (colorIdx = 0; colorIdx < 16; colorIdx++) {
        col = (WORD)(colorIdx % COLORPICKER_COLS);
        row = (WORD)(colorIdx / COLORPICKER_COLS);
        x   = left + col * cellW;
        y   = top  + row * cellH;

        SetAPen(rp, (LONG)PetsciiStyle_Pen(inst->style, colorIdx));
        RectFill(rp,
                 (LONG)x,              (LONG)y,
                 (LONG)(x + cellW - 1),(LONG)(y + cellH - 1));
    }

    /* COMPLEMENT border around the selected colour */
    selCol = (WORD)(inst->selectedColor % COLORPICKER_COLS);
    selRow = (WORD)(inst->selectedColor / COLORPICKER_COLS);
    selX   = left + selCol * cellW;
    selY   = top  + selRow * cellH;

    savedMode = rp->DrawMode;
    SetDrMd(rp, COMPLEMENT);
    SetAPen(rp, 1);

    Move(rp, (LONG)selX,               (LONG)selY);
    Draw(rp, (LONG)(selX + cellW - 1), (LONG)selY);
    Draw(rp, (LONG)(selX + cellW - 1), (LONG)(selY + cellH - 1));
    Draw(rp, (LONG)selX,               (LONG)(selY + cellH - 1));
    Draw(rp, (LONG)selX,               (LONG)selY);

    SetDrMd(rp, (LONG)savedMode);

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
