/*
 * PetsciiCanvas - BOOPSI gadget class for the PETSCII editing canvas.
 * Attribute handlers: OM_NEW, OM_DISPOSE, OM_SET, OM_GET.
 */

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/utility.h>
#include "petscii_canvas_private.h"
#include "petscii_chartransform.h"
#include <bdbprintf.h>
ULONG PetsciiCanvas_OnSet(Class *cl, Object *o, struct opSet *msg);
/* ------------------------------------------------------------------ */
/* OM_NEW                                                               */
/* ------------------------------------------------------------------ */
ULONG PetsciiCanvas_OnNew(Class *cl, Object *o, struct opSet *msg)
{
    PetsciiCanvasData *inst;
    PetsciiScreen     *screen    = NULL;
    PetsciiStyle      *style     = NULL;
    Object            *newObj;
    struct TagItem    *tag;
    ULONG              mDispose;

    /* Extract our tags before chaining to superclass */
    tag = FindTagItem(PCA_Screen, msg->ops_AttrList);
    if (tag) screen = (PetsciiScreen *)tag->ti_Data;

    tag = FindTagItem(PCA_Style, msg->ops_AttrList);
    if (tag) style = (PetsciiStyle *)tag->ti_Data;

    /* Both PCA_Screen and PCA_Style are required */
    if (!screen || !style)
        return 0;

    /* Chain to gadgetclass to allocate the object + superclass data */
    newObj = (Object *)DoSuperMethodA(cl, o, (APTR)msg);
    if (!newObj)
        return 0;

    inst            = (PetsciiCanvasData *)INST_DATA(cl, newObj);
    inst->screen    = screen;
    inst->style     = style;

    inst->clipRegion = NewRegion();
    inst->zoomLevel  = 1;
    inst->showGrid   = FALSE;
    inst->screenbuf  = NULL;
    inst->scaledBuf  = NULL;
    inst->scaledW    = 0;
    inst->scaledH    = 0;
    inst->keepRatio  = FALSE;
    inst->refreshExtraMarge = 0;
    inst->contentX   = 0;
    inst->contentY   = 0;
    inst->contentW   = 0;
    inst->contentH   = 0;

    /* Drawing tool defaults */
    inst->currentTool   = TOOL_DRAW;
    inst->selectedChar  = 0xa0;   /* space, reversed */
    inst->fgColor       = 14;   /* C64_LIGHTBLUE */
    screen->backgroundColor = 6;    /* C64_BLUE */

    /* Cursor / hover not shown until input begins */
    inst->cursorCol     = -1;
    inst->cursorRow     = -1;
    inst->isDrawing     = FALSE;
    inst->lastPaintCol  = -1;
    inst->lastPaintRow  = -1;

    /* Brush defaults: 1x1 (single char) */
    inst->brushW        = 1;
    inst->brushH        = 1;

    /* No overlay drawn yet */
    inst->prevHoverCol  = -1;
    inst->prevHoverRow  = -1;
    inst->prevBrushW    = 1;
    inst->prevBrushH    = 1;

    /* Force full blit on first render */
    inst->scaledBufDirty = TRUE;
    inst->overlayBuf     = NULL;

    /* Undo buffer (optional, not owned) */
    {
        struct TagItem *undoTag = FindTagItem(PCA_UndoBuffer, msg->ops_AttrList);
        inst->undoBuf = undoTag ? (PetsciiUndoBuffer *)undoTag->ti_Data : NULL;
    }

    /* Brush (owned; starts NULL = 1x1 single-char mode) */
    inst->brush              = NULL;
    inst->isLassoing         = FALSE;
    inst->lassoStartCol      = -1;
    inst->lassoStartRow      = -1;
    inst->lassoEndCol        = -1;
    inst->lassoEndRow        = -1;
    inst->nativeBrushBuf     = NULL;
    inst->nativeBrushBufSize = 0;

    /* Text tool cursor: start at top-left */
    inst->textCursorCol = 0;
    inst->textCursorRow = 0;

    /* Create the render buffer (size = character dimensions * 8 pixels each) */
    inst->screenbuf = PetsciiScreenBuf_Create(screen->width, screen->height);
    if (!inst->screenbuf) {
        /* Dispose via superclass (screenbuf is NULL, nothing to free in ours) */
        mDispose = OM_DISPOSE;
        DoSuperMethodA(cl, newObj, (APTR)&mDispose);
        return 0;
    }
    PetsciiCanvas_OnSet(cl, newObj,msg);

    /* Fill the buffer immediately so it is ready for first GM_RENDER */
    PetsciiScreenBuf_RebuildFull(inst->screenbuf, screen, style);

    return (ULONG)newObj;
}

/* ------------------------------------------------------------------ */
/* OM_DISPOSE                                                           */
/* ------------------------------------------------------------------ */

ULONG PetsciiCanvas_OnDispose(Class *cl, Object *o, Msg msg)
{
    PetsciiCanvasData *inst = (PetsciiCanvasData *)INST_DATA(cl, o);

    if (inst->screenbuf) {
        PetsciiScreenBuf_Destroy(inst->screenbuf);
        inst->screenbuf = NULL;
    }

    if (inst->scaledBuf) {
        FreeVec(inst->scaledBuf);
        inst->scaledBuf = NULL;
        inst->scaledW   = 0;
        inst->scaledH   = 0;
    }

    if (inst->overlayBuf) {
        FreeVec(inst->overlayBuf);
        inst->overlayBuf = NULL;
    }

    if (inst->brush) {
        PetsciiBrush_Destroy(inst->brush);
        inst->brush = NULL;
    }

    if (inst->nativeBrushBuf) {
        FreeVec(inst->nativeBrushBuf);
        inst->nativeBrushBuf     = NULL;
        inst->nativeBrushBufSize = 0;
    }

    if (inst->clipRegion) {
        DisposeRegion( inst->clipRegion );
        inst->clipRegion = NULL;
    }

    return DoSuperMethodA(cl, o, (APTR)msg);
}
void FreeBrush(PetsciiCanvasData *inst)
{
    if (inst->brush) {
        PetsciiBrush_Destroy(inst->brush);
        inst->brush = NULL;
    }
    if (inst->nativeBrushBuf) {
        FreeVec(inst->nativeBrushBuf);
        inst->nativeBrushBuf     = NULL;
        inst->nativeBrushBufSize = 0;
    }
    inst->brushW      = 1;
    inst->brushH      = 1;
    inst->isLassoing  = FALSE;
    inst->brushHotx = 0;
    inst->brushHoty = 0;
}
/* ------------------------------------------------------------------ */
/* OM_SET / OM_UPDATE                                                   */
/* ------------------------------------------------------------------ */

ULONG PetsciiCanvas_OnSet(Class *cl, Object *o, struct opSet *msg)
{
    PetsciiCanvasData *inst;
    struct TagItem    *state;
    struct TagItem    *tag;
    ULONG              result=0,redraw=0;
    ULONG              newPixW;
    ULONG              newPixH;

    inst = (PetsciiCanvasData *)INST_DATA(cl, o);

    /* Now process our PCA_* tags */
    state = msg->ops_AttrList;
    while ((tag = NextTagItem(&state)) != NULL)
    {
        switch (tag->ti_Tag)
        {
            case PCA_Screen:
                inst->screen = (PetsciiScreen *)tag->ti_Data;
                if (inst->screenbuf && inst->screen) {
                    /* If character dimensions changed, recreate buffer */
                    newPixW = (ULONG)inst->screen->width  * 8;
                    newPixH = (ULONG)inst->screen->height * 8;
                    if (newPixW != inst->screenbuf->pixW ||
                        newPixH != inst->screenbuf->pixH) {
                        PetsciiScreenBuf_Destroy(inst->screenbuf);
                        inst->screenbuf = PetsciiScreenBuf_Create(
                            inst->screen->width, inst->screen->height);
                        /* screenbuf->valid = 0 after Create; RebuildFull
                         * is called lazily in GM_RENDER                  */
                    } else {
                        inst->screenbuf->valid = 0;
                    }
                }
                inst->scaledBufDirty = TRUE;
                result = 1;
                break;

            case PCA_Style:
                inst->style = (PetsciiStyle *)tag->ti_Data;
                if (inst->screenbuf) inst->screenbuf->valid = 0;
                inst->scaledBufDirty = TRUE;
                result = 1;
                break;

            case PCA_ZoomLevel:
                inst->zoomLevel = (UWORD)tag->ti_Data;
                result = 1;
                break;

            case PCA_ShowGrid:
                inst->showGrid = (BOOL)tag->ti_Data;
                result = 1;
                break;

            case PCA_Dirty:
                if (tag->ti_Data && inst->screenbuf) {
                    inst->screenbuf->valid = 0;
                    inst->scaledBufDirty   = TRUE;
                }
                result = 1;
                break;

            case PCA_KeepRatio:
                inst->keepRatio = (BOOL)tag->ti_Data;
                /* Force content rect + scaled buf to be recomputed */
                inst->contentW  = 0;
                inst->contentH  = 0;
                inst->scaledW   = 0;
                inst->scaledH   = 0;
                result = 1;
                break;

            case PCA_CurrentTool:
                if(inst->currentTool != (UBYTE)tag->ti_Data)
                {
                    inst->currentTool = (UBYTE)tag->ti_Data;
                    FreeBrush(inst);
                    /* Cancel any active lasso when the tool changes */
                    inst->isLassoing = FALSE;
                }
                result = 1;
                break;

            case PCA_SelectedChar:
             bdbprintf("canvas received: PCA_SelectedChar\n");
                inst->selectedChar = (UBYTE)tag->ti_Data;
                /* Clicking the char selector: reset to 1x1 single-char draw mode */
                FreeBrush(inst);
                inst->currentTool = TOOL_DRAW;
                result = 1;
                break;

            case PCA_FgColor:
            bdbprintf("canvas received PCA_FgColor:\n");
                inst->fgColor = (UBYTE)tag->ti_Data;
                result = 1; /* no need for redraw, just change the drawing prefs */
                break;

            case PCA_BgColor:
                if(inst->screen)
                {
                    if(inst->screen->backgroundColor != (UBYTE)tag->ti_Data)
                    {
                        inst->screen->backgroundColor = (UBYTE)tag->ti_Data;
                        if(inst->screenbuf) inst->screenbuf->valid = 0;
                        inst->scaledBufDirty = TRUE;
                        redraw = 1; /* need a total redraw, background color affect all */
                    }
                }
                result = 1;
                break;
            case PCA_BdColor:
                if(inst->screen)
                {
                    if(inst->screen->borderColor != (UBYTE)tag->ti_Data)
                    {
                        inst->screen->borderColor = (UBYTE)tag->ti_Data;
                        if(inst->screenbuf) inst->screenbuf->valid = 0;
                        inst->scaledBufDirty = TRUE; /* could optimize just redraw bodrers... */
                        redraw = 1; /* need a total redraw, background color affect all */
                    }
                }
                result = 1;
                break;



            case PCA_UndoBuffer:
                inst->undoBuf = (PetsciiUndoBuffer *)(void *)tag->ti_Data;
                result = 1;
                break;

            case PCA_TransformBrush:
            {
                /* Apply geometric transform to the current brush.
                 * Selects the correct char-remapping table based on charset. */
                if (inst->brush && inst->screen) {
                    const UBYTE  *charTable;
                    PetsciiBrush *newBrush;
                    int           xform;
                    int           isLower;

                    xform   = (int)tag->ti_Data;
                    isLower = (inst->screen->charset == PETSCII_CHARSET_LOWER);

                    switch (xform) {
                        case BRUSH_TRANSFORM_FLIP_X:
                            charTable = isLower
                                ? petsciiLowerFlipX : petsciiUpperFlipX;
                            break;
                        case BRUSH_TRANSFORM_FLIP_Y:
                            charTable = isLower
                                ? petsciiLowerFlipY : petsciiUpperFlipY;
                            break;
                        case BRUSH_TRANSFORM_ROT90CW:
                            charTable = isLower
                                ? petsciiLowerRot90 : petsciiUpperRot90;
                            break;
                        case BRUSH_TRANSFORM_ROT180:
                            charTable = isLower
                                ? petsciiLowerRot180 : petsciiUpperRot180;
                            break;
                        case BRUSH_TRANSFORM_ROT90CCW:
                            charTable = isLower
                                ? petsciiLowerRotN90 : petsciiUpperRotN90;
                            break;
                        default:
                            charTable = NULL;
                            break;
                    }

                    newBrush = PetsciiBrush_Transform(inst->brush, xform,
                                                       charTable);
                    if (newBrush) {
                        PetsciiBrush_Destroy(inst->brush);
                        inst->brush  = newBrush;
                        inst->brushW = newBrush->w;
                        inst->brushH = newBrush->h;
                        inst->brushHotx = 0;
                        inst->brushHoty = 0;

                        /* nativeBrushBuf will be reallocated on next render */
                        if (inst->nativeBrushBuf) {
                            FreeVec(inst->nativeBrushBuf);
                            inst->nativeBrushBuf     = NULL;
                            inst->nativeBrushBufSize = 0;
                        }

                        inst->scaledBufDirty = TRUE;
                        result = 1;
                    }
                }
                break;
            }

            default:
                break;
        }
    }
   /* I'm not fan of sending redraws under setAttribs,
     * because render are better send up down on the right process.
     * any process or context can originate a SetAttrs() or SetGAdgetAttrs()
     * would be better trigger an external redraw event up->down from the process.
     * Yet, sending redraw under Setttrs is the documented way,
     * Even if it's very clear that is generate bugs when done from HandleInput.
     * well let's go.... */
    if(redraw && msg->ops_GInfo)
    {
        struct RastPort *rp = ObtainGIRPort(msg->ops_GInfo);
        if (rp)
        {
            struct gpRender  renderMsg;
            renderMsg.MethodID   = GM_RENDER;
            renderMsg.gpr_GInfo  = msg->ops_GInfo;
            renderMsg.gpr_RPort  = rp;
            renderMsg.gpr_Redraw = GREDRAW_UPDATE;

            DoMethodA(o, (Msg)&renderMsg);

            ReleaseGIRPort(rp);
        }
    }

    return result;
}

/* ------------------------------------------------------------------ */
/* OM_GET                                                               */
/* ------------------------------------------------------------------ */

ULONG PetsciiCanvas_OnGet(Class *cl, Object *o, struct opGet *msg)
{
    PetsciiCanvasData *inst = (PetsciiCanvasData *)INST_DATA(cl, o);

    switch (msg->opg_AttrID)
    {
        case PCA_ZoomLevel:
            *msg->opg_Storage = (ULONG)inst->zoomLevel;
            return TRUE;

        case PCA_ShowGrid:
            *msg->opg_Storage = (ULONG)inst->showGrid;
            return TRUE;

        case PCA_KeepRatio:
            *msg->opg_Storage = (ULONG)inst->keepRatio;
            return TRUE;

        case PCA_CurrentTool:
            *msg->opg_Storage = (ULONG)inst->currentTool;
            return TRUE;

        case PCA_SelectedChar:
            *msg->opg_Storage = (ULONG)inst->selectedChar;
            return TRUE;

        case PCA_FgColor:
            *msg->opg_Storage = (ULONG)inst->fgColor;
            return TRUE;

        case PCA_BgColor:
            if(inst->screen)
            {
                *msg->opg_Storage = (ULONG)inst->screen->backgroundColor;
                return TRUE;
            } else
            return FALSE;
        case PCA_BdColor:
            if(inst->screen)
            {
                *msg->opg_Storage = (ULONG)inst->screen->borderColor;
                return TRUE;
            } else
            return FALSE;
        case PCA_CursorCol:
            *msg->opg_Storage = (ULONG)(LONG)inst->cursorCol;
            return TRUE;

        case PCA_CursorRow:
            *msg->opg_Storage = (ULONG)(LONG)inst->cursorRow;
            return TRUE;

        default:
            return DoSuperMethodA(cl, o, (APTR)msg);
    }
}
