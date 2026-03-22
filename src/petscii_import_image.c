/*
 * petscii_import_image.c - Import a bitmap image into a PETSCII screen.
 *
 * Algorithm:
 *  1. Load image via Amiga picture.datatype -> 24-bit RGB chunky buffer.
 *  2. Accept any image size; reject only if smaller than 320x200.
 *  3. Quantize each RGB pixel to the nearest C64 color index (0-15) using
 *     the squared-distance metric against the active palette.
 *  4. Detect the border: find the dominant color along the outer pixel edge,
 *     then scan inward on all four sides, stripping rows/columns that are
 *     entirely that color.  This handles any border width, including the
 *     variable-size borders often found in PETSCII images downloaded from
 *     the internet.
 *  5. For each sub-pixel alignment offset (ox, oy) in [0..7] x [0..7]
 *     within the detected content rectangle:
 *       For each of the 16 possible background colors (0..15):
 *         For each 8x8 block across the 40x25 grid:
 *           - Build 8-byte bit pattern: bit set where pixel != bg.
 *           - Bail if the block contains more than 2 distinct colors.
 *           - Match the bit pattern against all 256 glyphs in the charset.
 *           - Bail if no glyph matches.
 *         If all 1000 blocks matched: write results to screen and stop.
 *  6. Return PETSCII_IMPORT_ENOMATCH if no (offset, background) pair worked.
 *     Return PETSCII_IMPORT_ESIZE   if the image (or its content after border
 *     stripping) is smaller than 320x200.
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

/* Maximum sub-pixel alignment search range (one character cell minus one) */
#define MAX_OFFSET  7

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
/* Border detection                                                    */
/* ------------------------------------------------------------------ */

/*
 * Find the most common C64 color index along the outermost pixel frame of
 * the image (top row, bottom row, left column, right column).
 * This is used as the border color for subsequent strip operations.
 */
static UBYTE detectBorderColor(const UBYTE *idx, ULONG w, ULONG h)
{
    ULONG freq[16];
    ULONG x, y;
    UBYTE best    = 0;
    ULONG bestFreq = 0;
    int   i;

    for (i = 0; i < 16; i++) freq[i] = 0;

    /* Top and bottom rows */
    for (x = 0; x < w; x++) {
        freq[idx[x]]++;
        freq[idx[(h - 1) * w + x]]++;
    }
    /* Left and right columns (corners already counted above) */
    for (y = 1; y < h - 1; y++) {
        freq[idx[y * w]]++;
        freq[idx[y * w + w - 1]]++;
    }

    for (i = 0; i < 16; i++) {
        if (freq[i] > bestFreq) { bestFreq = freq[i]; best = (UBYTE)i; }
    }
    return best;
}

/*
 * Compute the content bounding box by stripping rows/columns that are
 * entirely the given border color.
 *
 * outLeft, outTop   : first pixel column / row that is NOT all-border.
 * outRight, outBottom: one past the last such column / row.
 *
 * If the image is entirely one color, outRight == outLeft (empty rect).
 */
static void findContentRect(const UBYTE *idx, ULONG imgW, ULONG imgH,
                             UBYTE borderColor,
                             ULONG *outLeft,  ULONG *outTop,
                             ULONG *outRight, ULONG *outBottom)
{
    ULONG x, y;
    BOOL  allBorder;

    /* Start with the full image */
    *outLeft   = 0;
    *outTop    = 0;
    *outRight  = imgW;
    *outBottom = imgH;

    /* Shrink top: skip rows that are entirely the border color */
    for (y = 0; y < imgH; y++) {
        allBorder = TRUE;
        for (x = 0; x < imgW; x++) {
            if (idx[y * imgW + x] != borderColor) { allBorder = FALSE; break; }
        }
        if (!allBorder) break;
        *outTop = y + 1;
    }

    /* Shrink bottom */
    for (y = imgH; y > *outTop; ) {
        y--;
        allBorder = TRUE;
        for (x = 0; x < imgW; x++) {
            if (idx[y * imgW + x] != borderColor) { allBorder = FALSE; break; }
        }
        if (!allBorder) break;
        *outBottom = y;
    }

    /* Shrink left (only within the already-narrowed vertical range) */
    for (x = 0; x < imgW; x++) {
        allBorder = TRUE;
        for (y = *outTop; y < *outBottom; y++) {
            if (idx[y * imgW + x] != borderColor) { allBorder = FALSE; break; }
        }
        if (!allBorder) break;
        *outLeft = x + 1;
    }

    /* Shrink right */
    for (x = imgW; x > *outLeft; ) {
        x--;
        allBorder = TRUE;
        for (y = *outTop; y < *outBottom; y++) {
            if (idx[y * imgW + x] != borderColor) { allBorder = FALSE; break; }
        }
        if (!allBorder) break;
        *outRight = x;
    }
}

/* ------------------------------------------------------------------ */
/* Block decode                                                        */
/* ------------------------------------------------------------------ */

/*
 * Extract bit pattern for one 8x8 block.
 * stride: the full image width (row pitch of the indexed buffer).
 * startX, startY: pixel coords of the block's top-left corner.
 * bg: background color index (pixels equal to this are "off").
 * outBits[8]: filled with the 8-byte glyph pattern.
 * outFgColor: C64 color index of the foreground (0 if block is all bg).
 *
 * Returns FALSE if the block contains more than two colors
 * (background + more than one distinct foreground color).
 */
static BOOL getBlockBits(const UBYTE *pixels, ULONG stride,
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
            UBYTE pix = pixels[(startY + y) * stride + (startX + x)];
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
 * Try decoding the full 40x25 character grid starting at pixel (baseX, baseY)
 * in the indexed buffer, using the given background color.
 * stride is the full image width (row pitch).
 *
 * On success, writes all 40x25 cells to scr->framebuf and returns TRUE.
 * On the first block that fails (no glyph match or >2 colors), returns FALSE.
 */
static BOOL tryBackground(const UBYTE *pixels, ULONG stride,
                           int baseX, int baseY,
                           UBYTE bg, UBYTE charset, PetsciiScreen *scr)
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

            if (!getBlockBits(pixels, stride,
                              baseX + bx * 8, baseY + by * 8,
                              bg, blockBits, &fgColor))
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
    UBYTE  *rgbBuf;
    UBYTE  *idxBuf;
    UBYTE   borderColor;
    ULONG   contentLeft, contentTop, contentRight, contentBottom;
    ULONG   contentW, contentH;
    ULONG   maxOx, maxOy;
    int     ox, oy, bg, result;

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

    /* Trigger full decode so pixels are available. */
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

    /* Minimum size: must be at least a full C64 screen. */
    if (imgW < (ULONG)SCREEN_W || imgH < (ULONG)SCREEN_H) {
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
    pba.pbpa_PixelFormat   = PBPAFMT_RGB;
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

    /* --- Detect border and find content bounding box -------------------- */
    /*
     * The border color is detected from the outermost pixel frame.
     * Rows and columns that are entirely that color are stripped on all
     * four sides.  This handles any border width (fixed VICE borders,
     * arbitrary web-exported borders, or no border at all).
     */
    borderColor = detectBorderColor(idxBuf, imgW, imgH);
    findContentRect(idxBuf, imgW, imgH, borderColor,
                    &contentLeft, &contentTop,
                    &contentRight, &contentBottom);

    contentW = (contentRight  > contentLeft) ? contentRight  - contentLeft : 0;
    contentH = (contentBottom > contentTop ) ? contentBottom - contentTop  : 0;

    /* Content area must accommodate at least one full 320x200 screen. */
    if (contentW < (ULONG)SCREEN_W || contentH < (ULONG)SCREEN_H) {
        FreeVec(idxBuf);
        return PETSCII_IMPORT_ESIZE;
    }

    /* --- Try all sub-pixel alignment offsets and all backgrounds --------- */
    /*
     * The character grid might not start exactly at the first non-border
     * pixel: the content origin may be shifted by up to 7 pixels in X or Y.
     * We try all (ox, oy) in [0..min(7, contentW-320)] x [0..min(7, contentH-200)]
     * and for each, all 16 background colors.  tryBackground() aborts as soon
     * as a single block fails, so wrong offsets are rejected very quickly.
     */
    maxOx = contentW - (ULONG)SCREEN_W;
    maxOy = contentH - (ULONG)SCREEN_H;
    if (maxOx > (ULONG)MAX_OFFSET) maxOx = (ULONG)MAX_OFFSET;
    if (maxOy > (ULONG)MAX_OFFSET) maxOy = (ULONG)MAX_OFFSET;

    result = PETSCII_IMPORT_ENOMATCH;
    for (oy = 0; oy <= (int)maxOy && result != PETSCII_IMPORT_OK; oy++) {
        for (ox = 0; ox <= (int)maxOx && result != PETSCII_IMPORT_OK; ox++) {
            for (bg = 0; bg < 16; bg++) {
                if (tryBackground(idxBuf, imgW,
                                  (int)contentLeft + ox,
                                  (int)contentTop  + oy,
                                  (UBYTE)bg, scr->charset, scr)) {
                    scr->backgroundColor = (UBYTE)bg;
                    scr->borderColor     = borderColor;
                    result = PETSCII_IMPORT_OK;
                    break;
                }
            }
        }
    }

    /* --- Cleanup -------------------------------------------------------- */
    FreeVec(idxBuf);
    return result;
}
