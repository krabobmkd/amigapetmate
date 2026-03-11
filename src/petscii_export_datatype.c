/*
 * petscii_export_datatype.c - Export PETSCII screen as images via datatypes.
 *
 * On AmigaOS 3.x the base picture.datatype handles IFF ILBM natively via
 * DTM_WRITE.  GIF / PNG output requires an installed format subclass that
 * also implements DTM_WRITE (rare on stock OS 3.x — most format datatypes
 * are read-only).
 *
 * Correct sequence for writing:
 *   1. NewDTObject(NULL, DTST_MEMORY, GID_PICTURE, ...)
 *      Creates a memory-backed picture DT (loads picture.datatype base class).
 *   2. GetDTAttrs(PDTA_BitMapHeader)  — fill w / h / depth in the pre-allocated header.
 *   3. SetDTAttrs(PDTA_ColorRegisters, PDTA_NumColors) — 16-entry RGB palette.
 *   4. DoMethod(PDTM_WRITEPIXELARRAY) — feed LUT8 chunky pixels (1 byte = colour index).
 *   5. Open() + struct dtWrite / DTM_WRITE — write to a DOS file handle.
 *   6. DisposeDTObject().
 *
 * Why the previous attempt did not work:
 *   a. DTA_SourceType DTST_FILE instructs NewDTObject to *read* an existing
 *      file — it fails (or opens garbage) when the path does not yet exist.
 *   b. PetsciiScreenBuf was created but never rendered; its chunky[] array
 *      was uninitialised.  Even when rendered it would contain Amiga pen
 *      numbers (not C64 colour indices 0-15), so the palette would not match.
 *   c. There was no DTM_WRITE step: the object was disposed without ever
 *      writing anything to disk.
 *
 * C89 compatible.
 */

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/datatypes.h>
#include <proto/intuition.h>
#include <proto/alib.h>
#include <proto/utility.h>
#include <datatypes/datatypes.h>
#include <datatypes/datatypesclass.h>
#include <datatypes/pictureclass.h>

#include "petscii_export.h"
#include "petscii_types.h"
#include "petscii_screen.h"
#include "petscii_charset.h"
#include "petscii_style.h"
#include "petmate.h"
#include <stdio.h>


extern struct Window *CurrentMainWindow;
extern struct Library *DataTypesBase;

/* ------------------------------------------------------------------ */
/* Build flat chunky buffer: 1 byte = C64 colour index (0-15).        */
/* Includes an 8-pixel border strip filled with scr->borderColor.     */
/* Caller must FreeVec() the result.                                   */
/* ------------------------------------------------------------------ */

static UBYTE *buildChunky(const PetsciiScreen *scr, ULONG *outW, ULONG *outH)
{
    ULONG        imgW     = (ULONG)scr->width  * 8UL + 16UL;
    ULONG        imgH     = (ULONG)scr->height * 8UL + 16UL;
    ULONG        bofs     = 8UL;
    UBYTE        borderIdx = scr->borderColor     & 0x0F;
    UBYTE        bgIdx     = scr->backgroundColor & 0x0F;
    UBYTE       *chunky;
    ULONG        n, cx, cy, py, px, idx;
    const UBYTE *glyph;
    UBYTE        bits, fgIdx;
    PetsciiPixel pix;

    chunky = (UBYTE *)AllocVec(imgW * imgH, MEMF_ANY);
    if (!chunky) return NULL;

    for (n = 0; n < imgW * imgH; n++)
        chunky[n] = borderIdx;

    for (cy = 0; cy < (ULONG)scr->height; cy++) {
        for (cx = 0; cx < (ULONG)scr->width; cx++) {
            pix   = scr->framebuf[cy * (ULONG)scr->width + cx];
            fgIdx = pix.color & 0x0F;
            glyph = PetsciiCharset_GetGlyph(scr->charset, pix.code);
            for (py = 0; py < 8UL; py++) {
                bits = glyph[py];
                for (px = 0; px < 8UL; px++) {
                    idx = (bofs + cy * 8UL + py) * imgW
                        + (bofs + cx * 8UL + px);
                    chunky[idx] = (bits & 0x80) ? fgIdx : bgIdx;
                    bits = (UBYTE)(bits << 1);
                }
            }
        }
    }

    *outW = imgW;
    *outH = imgH;
    return chunky;
}

/* ------------------------------------------------------------------ */
/* PetsciiExport_SaveDatatype                                          */
/*                                                                     */
/* exportDTFenum is reserved for future format selection.              */
/* With the base picture.datatype, DTM_WRITE always produces IFF ILBM.*/
/* A format-specific subclass (gif.datatype, png.datatype …) would    */
/* write its own format if it implements DTM_WRITE.                    */
/* ------------------------------------------------------------------ */

int PetsciiExport_SaveDatatype(const PetsciiScreen *scr,
                                const char          *filename,
                                int                  exportDTFenum)
{
    UBYTE               *chunky;
    ULONG                imgW, imgH;
    Object              *dto  = NULL;
    struct BitMapHeader *bmhp = NULL;
    struct ColorRegister colors[C64_COLOR_COUNT];
    BPTR                 fh;
    struct dtWrite       dtw;
    ULONG                i;
    ULONG result;
    int                  ok = PETSCII_EXPORT_EWRITE;

    (void)exportDTFenum;

    if (!DataTypesBase || !scr || !filename)
        return PETSCII_EXPORT_ENOMEM;

    /* --- C64 colour-index chunky buffer (NOT Amiga pen numbers) ---------- */
    chunky = buildChunky(scr, &imgW, &imgH);
    if (!chunky) return PETSCII_EXPORT_ENOMEM;

    /* --- Palette from current style -------------------------------------- */
    for (i = 0; i < C64_COLOR_COUNT; i++) {
        ULONG rgb = app->style.c64pens[i].rgbcolor;
        colors[i].red   = (UBYTE)((rgb >> 16) & 0xFF);
        colors[i].green = (UBYTE)((rgb >>  8) & 0xFF);
        colors[i].blue  = (UBYTE)( rgb        & 0xFF);
    }
 printf("dt go NewDTObject() DataTypesBase:%08x\n",(int)DataTypesBase);
 UBYTE dummyData = 0;
    /* --- Create a memory-backed picture datatype object.
     *     DTST_FILE would try to *read* an existing file at that path and
     *     fail for a destination that doesn't exist yet.               ---- */
struct TagItem tags[] = {
    { DTA_SourceType, DTST_MEMORY },
    { DTA_SourceAddress, (ULONG) &dummyData },  // Address of data (can be NULL for empty)
    { DTA_SourceSize, 1 },               // Size of data (0 for empty)
    { DTA_GroupID, GID_PICTURE },        // Specify the class (e.g., picture, sound, text)
    { TAG_DONE, 0 }
};
    dto = NewDTObjectA(NULL, tags);

 printf("dt go NewDTObject(): %08x\n",(int)dto);
    if (!dto) {
        FreeVec(chunky);
        return PETSCII_EXPORT_ENOMEM;
    }

    /* --- Fill in the BitMapHeader pre-allocated by the datatype ---------- */
//    GetDTAttrs(dto, PDTA_BitMapHeader, (ULONG)&bmhp, TAG_DONE);
//    if (bmhp) {
//        bmhp->bmh_Width       = (UWORD)imgW;
//        bmhp->bmh_Height      = (UWORD)imgH;
//        bmhp->bmh_Depth       = 4;        /* 16 colours = 4 bitplanes */
//        bmhp->bmh_Masking     = mskNone;
//        bmhp->bmh_Compression = cmpNone;
//        bmhp->bmh_PageWidth   = (UWORD)imgW;
//        bmhp->bmh_PageHeight  = (UWORD)imgH;
//    }

    /* --- Provide 16-entry colour map ------------------------------------- */
    SetDTAttrs(dto, NULL, NULL,
               PDTA_ColorRegisters, (ULONG)colors,
               PDTA_NumColors,      (ULONG)C64_COLOR_COUNT,
               TAG_DONE);

    /* --- Feed pixel data into the datatype.
     *     PBPAFMT_LUT8: 1 byte per pixel = palette index.              ---- */
    result = DoDTMethod(dto,CurrentMainWindow,NULL,
             PDTM_WRITEPIXELARRAY,
             (ULONG)chunky,   /* pixel data                      */
             PBPAFMT_LUT8,    /* format: 8-bit palette index     */
             imgW,            /* bytes per row                   */
             0UL, 0UL,        /* left, top                       */
             imgW, imgH);     /* width, height                   */
 printf("dtPDTM_WRITEPIXELARRAY: %08x\n",(int)dto);
    /* --- Serialise to disk via a DOS file handle.
     *     DTWM_IFF: write in the datatype's native IFF format (ILBM for
     *     picture.datatype; a format subclass writes its own format).   --- */
    fh = Open(filename, MODE_NEWFILE);
    if (fh) {
        dtw.MethodID       = DTM_WRITE;
        dtw.dtw_GInfo      = NULL;
        dtw.dtw_FileHandle = fh;
        dtw.dtw_Mode       = DTWM_IFF;

        if (DoMethodA(dto, (Msg)(APTR)&dtw))
            ok = PETSCII_EXPORT_OK;

        Close(fh);

        if (ok != PETSCII_EXPORT_OK)
            DeleteFile(filename); /* remove empty/partial file */
    }

    DisposeDTObject(dto);
    FreeVec(chunky);
    return ok;
}
