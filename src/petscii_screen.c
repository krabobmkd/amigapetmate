#include "petscii_screen.h"

#include <proto/exec.h>
#include <string.h>

extern void refreshUI();

PetsciiScreen *PetsciiScreen_Create(UWORD width, UWORD height)
{
    PetsciiScreen *scr;
    ULONG bufSize;

    if (width == 0 || height == 0) return NULL;
    if (width > PETSCII_MAX_WIDTH || height > PETSCII_MAX_HEIGHT) return NULL;

    scr = (PetsciiScreen *)AllocVec(sizeof(PetsciiScreen), MEMF_CLEAR);
    if (!scr) return NULL;

    bufSize = (ULONG)width * (ULONG)height * sizeof(PetsciiPixel);
    scr->framebuf = (PetsciiPixel *)AllocVec(bufSize, MEMF_CLEAR);
    if (!scr->framebuf) {
        FreeVec(scr);
        return NULL;
    }

    scr->width = width;
    scr->height = height;
    scr->backgroundColor = C64_BLUE;
    scr->borderColor = C64_LIGHTBLUE;
    scr->charset = PETSCII_CHARSET_UPPER;
    scr->name[0] = '\0';

    /* Fill with spaces (code 32) */
    {
        ULONG count = (ULONG)width * (ULONG)height;
        ULONG i;
        for (i = 0; i < count; i++) {
            scr->framebuf[i].code = 32;
            scr->framebuf[i].color = C64_LIGHTBLUE;
        }
    }

    return scr;
}

PetsciiScreen *PetsciiScreen_CreateDefault(void)
{
    return PetsciiScreen_Create(PETSCII_DEFAULT_WIDTH, PETSCII_DEFAULT_HEIGHT);
}
/* also used internally by undo stack */
PetsciiScreen *PetsciiScreen_Clone(const PetsciiScreen *src)
{
    PetsciiScreen *dst;
    ULONG bufSize;

    if (!src) return NULL;

    dst = (PetsciiScreen *)AllocVec(sizeof(PetsciiScreen), MEMF_CLEAR);
    if (!dst) return NULL;

    bufSize = (ULONG)src->width * (ULONG)src->height * sizeof(PetsciiPixel);
    dst->framebuf = (PetsciiPixel *)AllocVec(bufSize, MEMF_ANY);
    if (!dst->framebuf) {
        FreeVec(dst);
        return NULL;
    }

    memcpy(dst->framebuf, src->framebuf, bufSize);
    dst->width = src->width;
    dst->height = src->height;
    dst->backgroundColor = src->backgroundColor;
    dst->borderColor = src->borderColor;
    dst->charset = src->charset;
    memcpy(dst->name, src->name, PETSCII_NAME_LEN);

   //not here, because used by undo push refreshUI();

    return dst;
}
void PetsciiUndoBuffer_Destroy(struct PetsciiUndoBuffer *buf);
/* also used to flush undo stack */
void PetsciiScreen_Destroy(PetsciiScreen *scr)
{
    if (!scr) return;
    if(scr->undoBuf) PetsciiUndoBuffer_Destroy(scr->undoBuf);

    if (scr->framebuf) FreeVec(scr->framebuf);
    FreeVec(scr);

   // refreshUI();
}

void PetsciiScreen_Clear(PetsciiScreen *scr, UBYTE color)
{
    ULONG count, i;
    if (!scr || !scr->framebuf) return;

    count = PetsciiScreen_CellCount(scr);
    for (i = 0; i < count; i++) {
        scr->framebuf[i].code = 32;
        scr->framebuf[i].color = color;
    }
    refreshUI();

}

void PetsciiScreen_SetPixel(PetsciiScreen *scr, UWORD col, UWORD row,
                             UBYTE code, UBYTE color)
{
    PetsciiPixel *px;
    if (!scr || col >= scr->width || row >= scr->height) return;
    px = PetsciiScreen_PixelAt(scr, col, row);
    px->code = code;
    px->color = color;
}

PetsciiPixel PetsciiScreen_GetPixel(const PetsciiScreen *scr,
                                     UWORD col, UWORD row)
{
    PetsciiPixel empty;
    if (!scr || col >= scr->width || row >= scr->height) {
        empty.code = 32;
        empty.color = 0;
        return empty;
    }
    return *PetsciiScreen_PixelAt(scr, col, row);
}

void PetsciiScreen_SetCode(PetsciiScreen *scr, UWORD col, UWORD row,
                            UBYTE code)
{
    if (!scr || col >= scr->width || row >= scr->height) return;
    PetsciiScreen_PixelAt(scr, col, row)->code = code;
}

void PetsciiScreen_SetColor(PetsciiScreen *scr, UWORD col, UWORD row,
                             UBYTE color)
{
    if (!scr || col >= scr->width || row >= scr->height) return;
    PetsciiScreen_PixelAt(scr, col, row)->color = color;
}

void PetsciiScreen_CopyData(PetsciiScreen *dst, const PetsciiScreen *src)
{
    ULONG bufSize;
    if (!dst || !src) return;
    if (dst->width != src->width || dst->height != src->height) return;

    bufSize = (ULONG)src->width * (ULONG)src->height * sizeof(PetsciiPixel);
    memcpy(dst->framebuf, src->framebuf, bufSize);
    dst->backgroundColor = src->backgroundColor;
    dst->borderColor = src->borderColor;
   refreshUI();
}

void PetsciiScreen_ShiftLeft(PetsciiScreen *scr)
{
    UWORD row, col;
    if (!scr) return;

    for (row = 0; row < scr->height; row++) {
        PetsciiPixel first = *PetsciiScreen_PixelAt(scr, 0, row);
        for (col = 0; col < scr->width - 1; col++) {
            *PetsciiScreen_PixelAt(scr, col, row) =
                *PetsciiScreen_PixelAt(scr, col + 1, row);
        }
        *PetsciiScreen_PixelAt(scr, scr->width - 1, row) = first;
    }
    refreshUI();
}

void PetsciiScreen_ShiftRight(PetsciiScreen *scr)
{
    UWORD row;
    int col;
    if (!scr) return;

    for (row = 0; row < scr->height; row++) {
        PetsciiPixel last = *PetsciiScreen_PixelAt(scr, scr->width - 1, row);
        for (col = scr->width - 1; col > 0; col--) {
            *PetsciiScreen_PixelAt(scr, col, row) =
                *PetsciiScreen_PixelAt(scr, col - 1, row);
        }
        *PetsciiScreen_PixelAt(scr, 0, row) = last;
    }
    refreshUI();
}

void PetsciiScreen_ShiftUp(PetsciiScreen *scr)
{
    UWORD row, col;
    PetsciiPixel *tmpRow;
    ULONG rowSize;

    if (!scr) return;

    rowSize = (ULONG)scr->width * sizeof(PetsciiPixel);
    tmpRow = (PetsciiPixel *)AllocVec(rowSize, MEMF_ANY);
    if (!tmpRow) return;

    /* Save first row */
    memcpy(tmpRow, PetsciiScreen_PixelAt(scr, 0, 0), rowSize);

    /* Shift all rows up */
    for (row = 0; row < scr->height - 1; row++) {
        memcpy(PetsciiScreen_PixelAt(scr, 0, row),
               PetsciiScreen_PixelAt(scr, 0, row + 1), rowSize);
    }

    /* Wrap first row to bottom */
    memcpy(PetsciiScreen_PixelAt(scr, 0, scr->height - 1), tmpRow, rowSize);

    FreeVec(tmpRow);
   refreshUI();
}

void PetsciiScreen_ShiftDown(PetsciiScreen *scr)
{
    UWORD row;
    PetsciiPixel *tmpRow;
    ULONG rowSize;

    if (!scr) return;

    rowSize = (ULONG)scr->width * sizeof(PetsciiPixel);
    tmpRow = (PetsciiPixel *)AllocVec(rowSize, MEMF_ANY);
    if (!tmpRow) return;

    /* Save last row */
    memcpy(tmpRow, PetsciiScreen_PixelAt(scr, 0, scr->height - 1), rowSize);

    /* Shift all rows down */
    for (row = scr->height - 1; row > 0; row--) {
        memcpy(PetsciiScreen_PixelAt(scr, 0, row),
               PetsciiScreen_PixelAt(scr, 0, row - 1), rowSize);
    }

    /* Wrap last row to top */
    memcpy(PetsciiScreen_PixelAt(scr, 0, 0), tmpRow, rowSize);

    FreeVec(tmpRow);
   refreshUI();
}
