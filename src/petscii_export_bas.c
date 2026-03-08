/*
 * petscii_export_bas.c - Export PETSCII screen as C64 BASIC source.
 *
 * Ported from petmate_original_source/src/utils/exporters/basic.ts
 *
 * Output format:
 *   10 rem created with petmate
 *   20 poke 53280,<border>
 *   30 poke 53281,<bgcolor>
 *   40 poke 53272,<charsetbits>   ; 21=upper, 23=lower
 *  100 for i = 1024 to 1024 + <cells-1>
 *  110 read a: poke i,a: next i
 *  120 for i = 55296 to 55296 + <cells-1>
 *  130 read a: poke i,a: next i
 *  140 goto 140
 *  200 data <16 screencodes per line>
 *  ...
 *  <n> data <16 color values per line>
 *  ...
 */

#include <stdio.h>
#include "petscii_export.h"
#include "petscii_types.h"
#include "petscii_screen.h"

#define BAS_BYTES_PER_DATA_LINE 16

int PetsciiExport_SaveBAS(const PetsciiScreen *scr, const char *path)
{
    FILE  *fp;
    int    x, y;
    int    count;
    int    linenum;
    int    totalCells;
    UBYTE  charsetBits;

    if (!scr || !path) return PETSCII_EXPORT_EOPEN;

    fp = fopen(path, "w");
    if (!fp) return PETSCII_EXPORT_EOPEN;

    charsetBits = (scr->charset == PETSCII_CHARSET_LOWER) ? 23 : 21; /* $17 or $15 */
    totalCells  = (int)scr->width * (int)scr->height;

    /* Init code */
    fprintf(fp, "10 rem created with petmate\n");
    fprintf(fp, "20 poke 53280,%d\n", (int)scr->borderColor);
    fprintf(fp, "30 poke 53281,%d\n", (int)scr->backgroundColor);
    fprintf(fp, "40 poke 53272,%d\n", (int)charsetBits);
    fprintf(fp, "100 for i = 1024 to 1024 + %d\n", totalCells - 1);
    fprintf(fp, "110 read a: poke i,a: next i\n");
    fprintf(fp, "120 for i = 55296 to 55296 + %d\n", totalCells - 1);
    fprintf(fp, "130 read a: poke i,a: next i\n");
    fprintf(fp, "140 goto 140\n");

    /* DATA lines: all screen codes first, then all color values.
     * 16 values per DATA line, starting at line 200. */
    linenum = 200;
    count   = 0;

    for (y = 0; y < (int)scr->height; y++) {
        for (x = 0; x < (int)scr->width; x++) {
            UBYTE code = scr->framebuf[y * scr->width + x].code;
            if (count == 0)
                fprintf(fp, "%d data ", linenum++);
            else
                fprintf(fp, ",");
            fprintf(fp, "%d", (int)code);
            if (++count == BAS_BYTES_PER_DATA_LINE) {
                fprintf(fp, "\n");
                count = 0;
            }
        }
    }

    for (y = 0; y < (int)scr->height; y++) {
        for (x = 0; x < (int)scr->width; x++) {
            UBYTE color = scr->framebuf[y * scr->width + x].color;
            if (count == 0)
                fprintf(fp, "%d data ", linenum++);
            else
                fprintf(fp, ",");
            fprintf(fp, "%d", (int)color);
            if (++count == BAS_BYTES_PER_DATA_LINE) {
                fprintf(fp, "\n");
                count = 0;
            }
        }
    }

    if (count > 0)
        fprintf(fp, "\n");

    fclose(fp);
    return PETSCII_EXPORT_OK;
}
