/*
 * CharSelector - rendering: GM_RENDER, GM_LAYOUT, GM_HITTEST.
 *
 * Native buffer (128x128) stores pen indices for all 256 glyphs arranged
 * in a 16x16 grid.  Reuses PetsciiScreenBuf_BlitNative / BlitScaled via a
 * stack-allocated PetsciiScreenBuf wrapping the native cbuf (no extra alloc).
 *
 * Selected character is highlighted with a COMPLEMENT-mode rectangle drawn
 * on top of the blitted image.
 *
 * Aspect-ratio preservation (CHSA_KeepRatio = TRUE)
 * --------------------------------------------------
 * The 16x16 char grid is inherently square (128x128 native pixels).
 * When keepRatio is TRUE the content is letter/pillarboxed inside the
 * gadget bounds to preserve 1:1.  Unused margin strips are erased with
 * EraseRect() which fills using the screen background as set up by Intuition,
 * ensuring the margins blend with the surrounding UI.
 */

#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <intuition/gadgetclass.h>
#include "char_selector_private.h"
#include <graphics/gfx.h>
#include <bdbprintf.h>
/* ------------------------------------------------------------------ */
/* Static: rebuild the 128x128 native buffer from charset + style      */
/* ------------------------------------------------------------------ */

static void rebuildCbuf(CharSelectorData *inst)
{
    ULONG        code;
    UBYTE        col;
    UBYTE        row;
    ULONG        baseOff;
    ULONG        py;
    ULONG        px;
    UBYTE        bits;
    UBYTE        fgPen;
    UBYTE        bgPen;
    const UBYTE *glyph;
    UBYTE       *cbuf;
    const int   *order;
    int          sc;

    fgPen = (UBYTE)PetsciiStyle_Pen(inst->style, inst->fgColor);
    bgPen = (UBYTE)PetsciiStyle_Pen(inst->style, inst->bgColor);
    cbuf  = inst->cbuf;

    /* Select ordering table: grid position -> screencode */
    order = (inst->charset == PETSCII_CHARSET_LOWER)
            ? petmate_char_order_lower
            : petmate_char_order_upper;

    for (code = 0; code < 256; code++) {
        col     = (UBYTE)(code & 0x0F);
        row     = (UBYTE)(code >> 4);
        sc      = order[code];   /* screencode whose glyph goes in this cell */
        glyph   = PetsciiCharset_GetGlyph(inst->charset, (UBYTE)sc);
        baseOff = (ULONG)row * 8 * CHARSELECTOR_NATIVE_W + (ULONG)col * 8;

        for (py = 0; py < 8; py++) {
            bits = glyph[py];
            for (px = 0; px < 8; px++) {
                cbuf[baseOff + py * CHARSELECTOR_NATIVE_W + px] =
                    (bits & 0x80) ? fgPen : bgPen;
                bits = (UBYTE)(bits << 1);
            }
        }
    }

    inst->valid = 1;
}

/* ------------------------------------------------------------------ */
/* Static: ensure the scaled buffer matches current content size       */
/* ------------------------------------------------------------------ */

static BOOL ensureScaledBuf(CharSelectorData *inst, UWORD w, UWORD h)
{
    ULONG needed;

    if (inst->scaledW == w && inst->scaledH == h && inst->scaledBuf)
        return TRUE;

    if (inst->scaledBuf) {
        FreeVec(inst->scaledBuf);
        inst->scaledBuf = NULL;
        inst->scaledW   = 0;
        inst->scaledH   = 0;
    }
    if (w == 0 || h == 0) return FALSE;

    needed = (ULONG)w * h;
    inst->scaledBuf = (UBYTE *)AllocVec(needed, MEMF_ANY);
    if (!inst->scaledBuf) return FALSE;

    inst->scaledW = w;
    inst->scaledH = h;
    return TRUE;
}

/* ------------------------------------------------------------------ */
/* Static: compute the largest sub-rect that preserves idealW:idealH  */
/* Result is gadget-relative (outX/Y are offsets from gadget origin). */
/* ------------------------------------------------------------------ */

static void computeContentRect(
    WORD gadW, WORD gadH,
    ULONG idealW, ULONG idealH,
    WORD *outX, WORD *outY, WORD *outW, WORD *outH)
{
    ULONG fitW;
    ULONG fitH;

    if (idealW == 0 || idealH == 0 || gadW <= 0 || gadH <= 0) {
        *outX = 0; *outY = 0; *outW = gadW; *outH = gadH;
        return;
    }

    /* Try fitting content to full width */
    fitH = (ULONG)gadW * idealH / idealW;
    if (fitH <= (ULONG)gadH) {
        *outW = gadW;
        *outH = (WORD)fitH;
        *outX = 0;
        *outY = (WORD)(((ULONG)gadH - fitH) / 2);
        return;
    }

    /* Otherwise fit to full height */
    fitW = (ULONG)gadH * idealW / idealH;
    if (fitW > (ULONG)gadW) fitW = (ULONG)gadW; /* safety clamp */
    *outW = (WORD)fitW;
    *outH = gadH;
    *outX = (WORD)(((ULONG)gadW - fitW) / 2);
    *outY = 0;
}

/* ------------------------------------------------------------------ */
/* Static: erase letterbox/pillarbox margins using EraseRect           */
/* Coordinates are absolute (gadLeft/gadTop + gadget-relative offsets).*/
/* EraseRect fills with the background colour from screen prefs.       */
/* ------------------------------------------------------------------ */

static void eraseMargins(struct RastPort *rp,
                          WORD gadLeft, WORD gadTop,
                          WORD gadW, WORD gadH,
                          WORD cX, WORD cY, WORD cW, WORD cH)
{
    /* Top strip */
    if (cY > 0)
        EraseRect(rp,
                  (LONG)gadLeft,              (LONG)gadTop,
                  (LONG)(gadLeft + gadW - 1), (LONG)(gadTop + cY - 1));

    /* Bottom strip */
    if (cY + cH < gadH)
        EraseRect(rp,
                  (LONG)gadLeft,              (LONG)(gadTop + cY + cH),
                  (LONG)(gadLeft + gadW - 1), (LONG)(gadTop + gadH - 1));

    /* Left strip (between top and bottom strips) */
    if (cX > 0)
        EraseRect(rp,
                  (LONG)gadLeft,             (LONG)(gadTop + cY),
                  (LONG)(gadLeft + cX - 1),  (LONG)(gadTop + cY + cH - 1));

    /* Right strip */
    if (cX + cW < gadW)
        EraseRect(rp,
                  (LONG)(gadLeft + cX + cW),  (LONG)(gadTop + cY),
                  (LONG)(gadLeft + gadW - 1),  (LONG)(gadTop + cY + cH - 1));
}

/* ------------------------------------------------------------------ */
/* Static: update inst->content* from current gadget size + keepRatio */
/* ------------------------------------------------------------------ */

static void updateContentRect(CharSelectorData *inst, WORD gadW, WORD gadH)
{
    if (inst->keepRatio) {
        computeContentRect(gadW, gadH,
                           CHARSELECTOR_NATIVE_W, CHARSELECTOR_NATIVE_H,
                           &inst->contentX, &inst->contentY,
                           &inst->contentW, &inst->contentH);
    } else {
        inst->contentX = 0;
        inst->contentY = 0;
        inst->contentW = gadW;
        inst->contentH = gadH;
    }
}

/* ------------------------------------------------------------------ */
/* Static: draw COMPLEMENT rectangle around the selected character     */
/* ------------------------------------------------------------------ */

static void drawSelection(struct RastPort *rp,
                          WORD left, WORD top,
                          WORD width, WORD height,
                          WORD gridCol, WORD gridRow,
                          int selpen)
{
    UBYTE savedMode;
    int lw = (width>=128+64)?2:1;
    WORD  selX;
    WORD  selY;

    WORD  cx1  = (WORD)(left + ((gridCol*8*width)/128) )-1;
    WORD  cy1  = (WORD)(top + ((gridRow*8*height)/128) )-1;
    WORD  cx2  = (WORD)(left + (((gridCol+1)*8*width)/128) );
    WORD  cy2  = (WORD)(top + (((gridRow+1)*8*height)/128) );

    savedMode = rp->DrawMode;
    //SetDrMd(rp, COMPLEMENT);
    SetAPen(rp, selpen);
    if( lw == 1 )
    {
        Move(rp, (LONG)cx1, (LONG)cy1);

        Draw(rp, (LONG)cx2, (LONG)cy1);
        Draw(rp, (LONG)cx2, (LONG)cy2);
        Draw(rp, (LONG)cx1, (LONG)cy2);
        Draw(rp, (LONG)cx1, (LONG)cy1+1); /* do not trace 2 times the first pixel, we're in complement mode */
    } else
    {
        cx1++; cy1++;
        RectFill(rp,
            (LONG)cx1-lw,(LONG)cy1-lw,
            (LONG)(cx2+lw),(LONG)(cy1));
        RectFill(rp,
            (LONG)cx1-lw,(LONG)cy2,
            (LONG)(cx2+lw),(LONG)(cy2+lw));

        RectFill(rp,
            (LONG)cx1-lw,(LONG)cy1+1,
            (LONG)(cx1),(LONG)(cy2+lw));

        RectFill(rp,
            (LONG)cx2,(LONG)cy1+1,
            (LONG)(cx2+lw),(LONG)(cy2+lw));

    }


   // SetDrMd(rp, (LONG)savedMode);
}

/* ------------------------------------------------------------------ */
/* GM_LAYOUT                                                            */
/* ------------------------------------------------------------------ */

ULONG CharSelector_OnLayout(Class *cl, Object *o, struct gpLayout *msg)
{
    CharSelectorData *inst = (CharSelectorData *)INST_DATA(cl, o);
    WORD w = G(o)->Width;
    WORD h = G(o)->Height;

    if (w > 0 && h > 0) {
        updateContentRect(inst, w, h);
        ensureScaledBuf(inst, (UWORD)inst->contentW, (UWORD)inst->contentH);
    }
    /* when a rastport is given or created rto draw on gadget,
        It is most often delivered with no clipping at layer level.
        Optionally during draw we can force installation of a clipping rect,
        that will cut any graphics.library drawing cals using RastPort.
        Here we refresh a Region geometry that is used as clipping at draw.
      */
    if (inst->clipRegion)
    {
        struct Rectangle framerect;
        framerect.MinX = G(o)->LeftEdge + inst->contentX;
        framerect.MinY = G(o)->TopEdge + inst->contentY;
        framerect.MaxX = G(o)->LeftEdge + inst->contentX + inst->contentW -1;
        framerect.MaxY = G(o)->TopEdge + inst->contentY + inst->contentH -1;

        /* note you can go wild with Or/And boolean operation on geometry.
           But a single rect (should be) enough at Gadget level.
           Note there are issues on "Virtual.class" up to OS3.2.2 with this.
           One of the reason we don't use Virtual.class.
           */
        ClearRegion(inst->clipRegion);
        OrRectRegion(inst->clipRegion, &framerect);
    }

    inst->refreshExtraMarge = 1;

    return DoSuperMethodA(cl, o, (APTR)msg);
}


ULONG CharSelector_OnDomain(Class *cl, Object *o, struct gpDomain  *msg)
{
    struct IBox *domain = &msg->gpd_Domain;

    // Set default dimensions
    domain->Left = 0;
    domain->Top = 0;
    domain->Width = 128;  // Nominal width
    domain->Height = 128;  // Nominal height

 bdbprintf("CharSelector_OnDomain\n");
    // Adjust based on gpd_Which
    switch (msg->gpd_Which) {
        case GDOMAIN_MINIMUM:
            domain->Width = 128;
            domain->Height = 128;
            break;
        case GDOMAIN_MAXIMUM:
            domain->Width = 128*4;
            domain->Height = 128*4;
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

ULONG CharSelector_OnRender(Class *cl, Object *o, struct gpRender *msg)
{
    CharSelectorData *inst;
    struct RastPort  *rp;
    WORD              left;
    WORD              top;
    WORD              width;
    WORD              height;
    WORD              absX;  /* absolute X of content rect top-left */
    WORD              absY;
    PetsciiScreenBuf  fakeBuf;

    (void)cl;

    inst = (CharSelectorData *)INST_DATA(cl, o);
    if (!inst->cbuf || !inst->style) return 0;

    rp     = msg->gpr_RPort;
    left   = G(o)->LeftEdge;
    top    = G(o)->TopEdge;
    width  = G(o)->Width;
    height = G(o)->Height;
    if (width <= 0 || height <= 0) return 0;

    /* Recompute content rect (cheap; handles keepRatio change w/o layout) */
    updateContentRect(inst, width, height);

    /* Rebuild native buffer if invalidated */
    if (!inst->valid)
        rebuildCbuf(inst);

    /* Erase letterbox/pillarbox margins (no-op when keepRatio=FALSE) */
    if(inst->refreshExtraMarge)
    {
        eraseMargins(rp, left, top, width, height,
                 inst->contentX, inst->contentY,
                 inst->contentW, inst->contentH);

        inst->refreshExtraMarge = 0;
    }

    absX = left + inst->contentX;
    absY = top  + inst->contentY;

    /* Set up a thin wrapper so we can reuse BlitNative / BlitScaled */
    fakeBuf.chunky = inst->cbuf;
    fakeBuf.pixW   = CHARSELECTOR_NATIVE_W;
    fakeBuf.pixH   = CHARSELECTOR_NATIVE_H;
    fakeBuf.valid  = 1;

    if ((ULONG)inst->contentW == CHARSELECTOR_NATIVE_W &&
        (ULONG)inst->contentH == CHARSELECTOR_NATIVE_H) {
        PetsciiScreenBuf_BlitNative(&fakeBuf, rp, absX, absY);
    } else {
        if (ensureScaledBuf(inst, (UWORD)inst->contentW, (UWORD)inst->contentH)) {
            PetsciiScreenBuf_BlitScaled(&fakeBuf, rp,
                                        absX, absY,
                                        (UWORD)inst->contentW,
                                        (UWORD)inst->contentH,
                                        inst->scaledBuf);
        }
    }

    /* Highlight the currently selected character */
    {
        struct Region *oldClipRegion;
        int nbUse[2]={0,0}; /* black=0, white=1 usage counts */
        int selpen = 1;
        const int *order;
        int gridRow;
        int gridCol;

        /* Reverse-lookup: find which grid cell displays selectedChar */
        order = (inst->charset == PETSCII_CHARSET_LOWER)
                ? petmate_char_order_lower
                : petmate_char_order_upper;
        if (!petmate_grid_from_screencode(order, (int)inst->selectedChar,
                                         &gridRow, &gridCol)) {
            gridRow = 0;
            gridCol = 0;
        }

        /* use white to tell selction, but if white use dark or red */
        if(inst->fgColor<2) nbUse[inst->fgColor]++;
        if(inst->bgColor<2) nbUse[inst->bgColor]++;

        if(nbUse[1]==0) selpen = 1;
        else if(nbUse[0]==0)  selpen = 0;
        else selpen = 2;

        oldClipRegion = InstallClipRegion( rp->Layer, inst->clipRegion);

            drawSelection(rp, absX, absY,
                      inst->contentW, inst->contentH,
                      (WORD)gridCol, (WORD)gridRow,
                      inst->style->c64pens[selpen].pen );

        /* important to pass NULL if oldClipRegion was NULL.*/
        InstallClipRegion( rp->Layer,oldClipRegion);
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* GM_HITTEST                                                           */
/* ------------------------------------------------------------------ */

ULONG CharSelector_OnHitTest(Class *cl, Object *o, struct gpHitTest *msg)
{
    (void)cl; (void)o; (void)msg;
    return GMR_GADGETHIT;
}
