/*
 * petscii_import_image.c - Import a bitmap image into a PETSCII screen.
 *
 * Algorithm (ported from petmate/src/utils/importers/png2petscii.ts):
 *
 *  1. Load image via Amiga picture.datatype -> 24-bit RGB chunky buffer.
 *  2. Verify dimensions: 320x200 (borderless) or 384x272 (bordered).
 *  3. Quantize each RGB pixel to the nearest C64 color index (0-15) using
 *     the squared-distance metric against the active palette.
 *  4. If bordered: crop to 320x200 center; detect border color from left
 *     border strip.
 *  5. For each of the 16 possible background colors (0..15):
 *       For each 8x8 block across the 40x25 grid:
 *         - Build 8-byte bit pattern: bit set where pixel != bg.
 *         - Bail if any non-bg pixel differs in color from others (>2 colors).
 *         - Match bit pattern against all 256 glyphs in the active charset.
 *         - Bail if no glyph matches.
 *       If all 1000 blocks matched: write results to screen and stop.
 *  6. Return PETSCII_IMPORT_ENOMATCH if no background worked.
 *
 * Requires picture.datatype v40+ (AmigaOS 3.5+) for PDTM_READPIXELARRAY.
 *
 * C89 compatible.  Uses AllocVec/FreeVec for heap allocations.
 */

#include "petscii_import_image.h"
#include "petscii_charset.h"
#include "petscii_types.h"

#include <stdio.h>
#include <string.h>

#include <proto/exec.h>
#include <proto/datatypes.h>
#include <datatypes/datatypes.h>
#include <datatypes/pictureclass.h>

/* Screen geometry */
#define SCREEN_W  320
#define SCREEN_H  200
#define CELLS_W    40
#define CELLS_H    25

/* Border geometry (VICE PNG export convention) */
#define BORDER_LEFT    32
#define BORDER_RIGHT   32
#define BORDER_TOP     35
#define BORDER_BOTTOM  37
#define BORDERED_W    (SCREEN_W + BORDER_LEFT + BORDER_RIGHT)   /* 384 */
#define BORDERED_H    (SCREEN_H + BORDER_TOP  + BORDER_BOTTOM)  /* 272 */

/* ------------------------------------------------------------------ */
/* Palette quantization                                                */
/* ------------------------------------------------------------------ */

/* Find the nearest C64 color index for an RGB triple. */
static UBYTE nearestC64Color(UBYTE r, UBYTE g, UBYTE b,
                              const PetsciiStyle *style)
{
    ULONG minDist = (ULONG)3 * 256UL * 256UL;
    UBYTE best    = 0;
    int   i;

    for (i = 0; i < 16; i++) {
        ULONG rgb = style->c64pens[i].rgbcolor;
        LONG  pr  = (LONG)((rgb >> 16) & 0xFF);
        LONG  pg  = (LONG)((rgb >>  8) & 0xFF);
        LONG  pb  = (LONG)( rgb        & 0xFF);
        LONG  dr  = (LONG)r - pr;
        LONG  dg  = (LONG)g - pg;
        LONG  db  = (LONG)b - pb;
        ULONG d   = (ULONG)(dr * dr + dg * dg + db * db);
        if (d < minDist) { minDist = d; best = (UBYTE)i; }
    }
    return best;
}

/* Convert a 24-bit RGB buffer (3 bytes/pixel) to C64 indexed (1 byte/pixel). */
static void quantizeBuffer(const UBYTE *rgb, UBYTE *idx,
                            ULONG numPixels, const PetsciiStyle *style)
{
    ULONG n;
    for (n = 0; n < numPixels; n++) {
        idx[n] = nearestC64Color(rgb[n * 3], rgb[n * 3 + 1], rgb[n * 3 + 2],
                                  style);
    }
}

/* ------------------------------------------------------------------ */
/* Block decode                                                        */
/* ------------------------------------------------------------------ */

/*
 * Extract bit pattern for one 8x8 block.
 * startX, startY: pixel coords in the 320x200 indexed buffer.
 * bg: background color index (pixels equal to this are "off").
 * outBits[8]: filled with the 8-byte glyph pattern.
 * outFgColor: C64 color index of the foreground (0 if block is all bg).
 *
 * Returns FALSE if the block contains more than two colors
 * (background + more than one distinct foreground color).
 */
static BOOL getBlockBits(const UBYTE *pixels,
                          int startX, int startY,
                          UBYTE bg,
                          UBYTE *outBits, UBYTE *outFgColor)
{
    int   y, x;
    UBYTE fgColor = 0;
    BOOL  fgFound = FALSE;

    for (y = 0; y < 8; y++) {
        UBYTE bits = 0;
        for (x = 0; x < 8; x++) {
            UBYTE pix = pixels[(startY + y) * SCREEN_W + (startX + x)];
            if (pix != bg) {
                bits = (UBYTE)(bits | (UBYTE)(0x80U >> x));
                if (!fgFound) {
                    fgColor = pix;
                    fgFound = TRUE;
                } else if (pix != fgColor) {
                    return FALSE; /* more than two colors in this block */
                }
            }
        }
        outBits[y] = bits;
    }
    *outFgColor = fgColor;
    return TRUE;
}

/*
 * Match an 8-byte bit pattern against all 256 glyphs of the given charset.
 * Returns the screen code (0-255) on match, -1 if not found.
 */
static int matchGlyph(UBYTE charset, const UBYTE *blockBits)
{
    int ci;
    for (ci = 0; ci < 256; ci++) {
        const UBYTE *glyph = PetsciiCharset_GetGlyph(charset, (UBYTE)ci);
        int row;
        BOOL ok = TRUE;
        for (row = 0; row < 8; row++) {
            if (glyph[row] != blockBits[row]) { ok = FALSE; break; }
        }
        if (ok) return ci;
    }
    return -1;
}

/*
 * Try decoding the full 320x200 indexed image with a given background color.
 * On success, writes all 40x25 cells to scr->framebuf and returns TRUE.
 * On failure (any block doesn't match a glyph), returns FALSE immediately.
 */
static BOOL tryBackground(const UBYTE *pixels, UBYTE bg,
                           UBYTE charset, PetsciiScreen *scr)
{
    /* Store results in temporaries; only commit if ALL blocks succeed. */
    UBYTE tmpCodes [CELLS_W * CELLS_H];
    UBYTE tmpColors[CELLS_W * CELLS_H];
    int bx, by, idx;

    for (by = 0; by < CELLS_H; by++) {
        for (bx = 0; bx < CELLS_W; bx++) {
            UBYTE blockBits[8];
            UBYTE fgColor;
            int   code;

            if (!getBlockBits(pixels, bx * 8, by * 8, bg, blockBits, &fgColor))
                return FALSE;

            code = matchGlyph(charset, blockBits);
            if (code < 0)
                return FALSE;

            idx = by * CELLS_W + bx;
            tmpCodes [idx] = (UBYTE)code;
            tmpColors[idx] = fgColor;
        }
    }

    /* All 1000 blocks matched — commit to screen. */
    for (idx = 0; idx < CELLS_W * CELLS_H; idx++) {
        scr->framebuf[idx].code  = tmpCodes [idx];
        scr->framebuf[idx].color = tmpColors[idx];
    }
    return TRUE;
}

/* ------------------------------------------------------------------ */
/* Public entry point                                                  */
/* ------------------------------------------------------------------ */

int PetsciiImport_FromImage(const char        *path,
                             PetsciiScreen     *scr,
                             const PetsciiStyle *style)
{
    struct TagItem       tags[3];
    struct BitMapHeader *bmh;
    struct pdtBlitPixelArray pba;
    Object *dto;
    ULONG   imgW, imgH;
    BOOL    hasBorder;
    UBYTE   borderColor;
    UBYTE  *rgbBuf;       /* 3 bytes/pixel from PDTM_READPIXELARRAY */
    UBYTE  *idxBuf;       /* 1 byte/pixel  C64 color index          */
    UBYTE  *screenBuf;    /* pointer into idxBuf at screen origin    */
    UBYTE  *cropBuf;      /* allocated crop buffer when hasBorder    */
    int     bg, result;

    if (!path || !scr || !style) return PETSCII_IMPORT_EOPEN;

    /* --- Open picture datatype from file -------------------------------- */
    tags[0].ti_Tag  = DTA_GroupID;
    tags[0].ti_Data = GID_PICTURE;
    tags[1].ti_Tag  = DTA_SourceType;
    tags[1].ti_Data = DTST_FILE;
    tags[2].ti_Tag  = TAG_END;
    tags[2].ti_Data = 0;

    dto = NewDTObjectA((APTR)(char *)path, tags);
    if (!dto) return PETSCII_IMPORT_EOPEN;

    /* Trigger full decode (DTM_PROCLAYOUT) so pixels are available. */
    DoMethod(dto, DTM_PROCLAYOUT, NULL, 1L);

    /* Get image dimensions from BitMapHeader. */
    bmh = NULL;
    GetDTAttrs(dto, PDTA_BitMapHeader, (ULONG)&bmh, TAG_END);
    if (!bmh) {
        DisposeDTObject(dto);
        return PETSCII_IMPORT_EOPEN;
    }
    imgW = (ULONG)bmh->bmh_Width;
    imgH = (ULONG)bmh->bmh_Height;

    /* --- Validate dimensions -------------------------------------------- */
    hasBorder  = FALSE;
    borderColor = scr->borderColor;
    if (imgW == (ULONG)BORDERED_W && imgH == (ULONG)BORDERED_H) {
        hasBorder = TRUE;
    } else if (imgW != (ULONG)SCREEN_W || imgH != (ULONG)SCREEN_H) {
        DisposeDTObject(dto);
        return PETSCII_IMPORT_ESIZE;
    }

    /* --- Allocate 24-bit RGB buffer and read pixels --------------------- */
    rgbBuf = (UBYTE *)AllocVec(imgW * imgH * 3UL, MEMF_ANY);
    if (!rgbBuf) {
        DisposeDTObject(dto);
        return PETSCII_IMPORT_ENOMEM;
    }

    pba.MethodID           = PDTM_READPIXELARRAY;
    pba.pbpa_PixelData     = (APTR)rgbBuf;
    pba.pbpa_PixelFormat   = PBPAF_RGB;
    pba.pbpa_PixelArrayMod = imgW * 3UL;
    pba.pbpa_Left          = 0;
    pba.pbpa_Top           = 0;
    pba.pbpa_Width         = imgW;
    pba.pbpa_Height        = imgH;
    DoMethodA(dto, (Msg)&pba);

    DisposeDTObject(dto);

    /* --- Quantize RGB -> C64 indexed ------------------------------------ */
    idxBuf = (UBYTE *)AllocVec(imgW * imgH, MEMF_ANY);
    if (!idxBuf) {
        FreeVec(rgbBuf);
        return PETSCII_IMPORT_ENOMEM;
    }
    quantizeBuffer(rgbBuf, idxBuf, imgW * imgH, style);
    FreeVec(rgbBuf);
    rgbBuf = NULL;

    /* --- Crop to 320x200 and extract border color if needed ------------- */
    cropBuf    = NULL;
    screenBuf  = idxBuf;  /* default: use full buffer directly */

    if (hasBorder) {
        ULONG r, c;

        /* Sample border color from middle of the left border strip. */
        borderColor = idxBuf[(imgH / 2) * imgW + (BORDER_LEFT / 2)];

        /* Crop the 320x200 active area out of the full indexed buffer. */
        cropBuf = (UBYTE *)AllocVec((ULONG)SCREEN_W * SCREEN_H, MEMF_ANY);
        if (!cropBuf) {
            FreeVec(idxBuf);
            return PETSCII_IMPORT_ENOMEM;
        }
        for (r = 0; r < (ULONG)SCREEN_H; r++) {
            for (c = 0; c < (ULONG)SCREEN_W; c++) {
                cropBuf[r * SCREEN_W + c] =
                    idxBuf[(r + BORDER_TOP) * imgW + (c + BORDER_LEFT)];
            }
        }
        screenBuf = cropBuf;
    }

    /* --- Try each of the 16 possible background colors ------------------ */
    result = PETSCII_IMPORT_ENOMATCH;
    for (bg = 0; bg < 16; bg++) {
        if (tryBackground(screenBuf, (UBYTE)bg, scr->charset, scr)) {
            scr->backgroundColor = (UBYTE)bg;
            scr->borderColor     = borderColor;
            result = PETSCII_IMPORT_OK;
            break;
        }
    }

    /* --- Cleanup -------------------------------------------------------- */
    if (cropBuf) FreeVec(cropBuf);
    FreeVec(idxBuf);

    return result;
}
