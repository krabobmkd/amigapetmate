/*
 * petscii_export_seq.c - Export PETSCII screen as C64 SEQ file.
 *
 * Ported from petmate_original_source/src/utils/exporters/seq.ts
 *
 * SEQ format encodes PETSCII with:
 *   - $93          : clear screen (prepended)
 *   - $0E / $8E    : charset switch lower/upper (prepended)
 *   - Color control bytes before each color change
 *   - $12 / $92    : reverse-on / reverse-off for chars >= 0x80
 *   - Screen codes converted from C64 screen codes to PETSCII values
 *   - $0D / $8D    : carriage return between rows
 *
 * Options used: insClear=1, insCharset=1, insCR=1, stripBlanks=0
 */

#include <stdio.h>
#include <proto/exec.h>
#include "petscii_export.h"
#include "petscii_types.h"
#include "petscii_screen.h"

/* C64 PETSCII color control codes indexed by C64 color 0-15 */
static const UBYTE seq_colors[16] = {
    0x90, /* black   */
    0x05, /* white   */
    0x1c, /* red     */
    0x9f, /* cyan    */
    0x9c, /* purple  */
    0x1e, /* green   */
    0x1f, /* blue    */
    0x9e, /* yellow  */
    0x81, /* orange  */
    0x95, /* brown   */
    0x96, /* pink    */
    0x97, /* grey 1  */
    0x98, /* grey 2  */
    0x99, /* lt green*/
    0x9a, /* lt blue */
    0x9b  /* grey 3  */
};

/* Convert C64 screen code to PETSCII byte value for SEQ stream.
 * Ported directly from convertToSEQ in seq.ts (the nested if-else block). */
static UBYTE screenCodeToSEQ(UBYTE code)
{
    if (code <= 0x1f)                        return (UBYTE)(code + 0x40);
    if (code >= 0x40 && code <= 0x5d)        return (UBYTE)(code + 0x80);
    if (code == 0x5e)                        return 0xff;
    if (code == 0x5f)                        return 0xdf;
    if (code == 0x95)                        return 0xdf;
    if (code >= 0x60 && code <= 0x7f)        return (UBYTE)(code + 0x40);
    if (code >= 0x80 && code <= 0xbf)        return (UBYTE)(code - 0x80);
    if (code >= 0xc0)                        return (UBYTE)(code - 0x40);
    return code;
}

int PetsciiExport_SaveSEQ(const PetsciiScreen *scr, const char *path)
{
    FILE   *fp;
    UBYTE  *buf;
    LONG    bufSize;
    int     bufLen;
    int     x, y;
    int     currColor;
    int     currev;
    UBYTE   byte_char;
    UBYTE   byte_color;

    if (!scr || !path) return PETSCII_EXPORT_EOPEN;

    /* Worst case: 3 control bytes per cell + 2 header bytes + row CRs */
    bufSize = (LONG)scr->width * (LONG)scr->height * 4 + (LONG)scr->height + 4;
    buf = (UBYTE *)AllocVec((ULONG)bufSize, MEMF_ANY);

 printf("export seq1\n");
    if (!buf) return PETSCII_EXPORT_ENOMEM;
 printf("export seq2\n");
    bufLen    = 0;
    currColor = -1;
    currev    = 0;

    /* Clear screen */
    buf[bufLen++] = 0x93;
    /* Charset switch */
    buf[bufLen++] = (scr->charset == PETSCII_CHARSET_LOWER) ? 0x0e : 0x8e;

    for (y = 0; y < (int)scr->height; y++) {
        for (x = 0; x < (int)scr->width; x++) {
            PetsciiPixel *px = &scr->framebuf[y * scr->width + x];

            /* Color control byte on color change */
            byte_color = px->color & 0x0f;
            if ((int)byte_color != currColor) {
                buf[bufLen++] = seq_colors[byte_color];
                currColor = (int)byte_color;
            }

            byte_char = px->code;

            /* Chars >= 0x80 use reverse-video in PETSCII */
            if (byte_char >= 0x80) {
                if (!currev) {
                    buf[bufLen++] = 0x12; /* reverse on */
                    currev = 1;
                }
                byte_char &= 0x7f;
            } else {
                if (currev) {
                    buf[bufLen++] = 0x92; /* reverse off */
                    currev = 0;
                }
            }

            buf[bufLen++] = screenCodeToSEQ(byte_char);
        }

        /* Carriage return between rows (not after last row) */
        if (y < (int)scr->height - 1)
            buf[bufLen++] = currev ? 0x0d : 0x8d;
    }
 printf("export seq3 \n");
    fp = fopen(path, "wb");
    if (!fp) {
        FreeVec(buf);
        return PETSCII_EXPORT_EOPEN;
    }
 printf("export seq4 %d \n",bufLen);
    if ((int)fwrite(buf, 1, (size_t)bufLen, fp) != bufLen) {
        fclose(fp);
        FreeVec(buf);
        return PETSCII_EXPORT_EWRITE;
    }

    fclose(fp);
    FreeVec(buf);
    return PETSCII_EXPORT_OK;
}
