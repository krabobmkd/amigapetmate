/*
 * petscii_export_asm.c - Export PETSCII screen as ACME assembler source.
 *
 * Ported from petmate_original_source/src/utils/exporters/asm.ts
 *
 * Output format (ACME assembler, non-standalone data-only):
 *
 *   ; acme --cpu 6510 --format cbm --outfile out.prg out.asm
 *   ;
 *   ; PETSCII memory layout:
 *   ; byte  0         = border color
 *   ; byte  1         = background color
 *   ; bytes 2-<n+1>   = screencodes
 *   ; bytes <n+2>-... = colors
 *
 *   screenname:
 *   !byte <border>,<bgcolor>
 *   !byte <row0 screencodes ...>
 *   ...
 *   !byte <row0 colors ...>
 *   ...
 */

#include <stdio.h>
#include <string.h>
#include "petscii_export.h"
#include "petscii_types.h"
#include "petscii_screen.h"

int PetsciiExport_SaveASM(const PetsciiScreen *scr, const char *path)
{
    FILE *fp;
    int   x, y;
    int   totalCells;
    const char *label;

    if (!scr || !path) return PETSCII_EXPORT_EOPEN;

    fp = fopen(path, "w");
    if (!fp) return PETSCII_EXPORT_EOPEN;

    totalCells = (int)scr->width * (int)scr->height;

    /* Use screen name as label, fall back to "untitled" */
    label = (scr->name[0] != '\0') ? scr->name : "untitled";

    /* Header comment */
    fprintf(fp, "; acme --cpu 6510 --format cbm --outfile out.prg %s\n", path);
    fprintf(fp, ";\n");
    fprintf(fp, "; PETSCII memory layout:\n");
    fprintf(fp, "; byte  0         = border color\n");
    fprintf(fp, "; byte  1         = background color\n");
    fprintf(fp, "; bytes 2-%d   = screencodes\n", totalCells + 1);
    fprintf(fp, "; bytes %d-... = colors\n", totalCells + 2);
    fprintf(fp, "\n");

    /* Label */
    fprintf(fp, "%s:\n", label);

    /* Border and background as first two bytes */
    fprintf(fp, "!byte %d,%d\n", (int)scr->borderColor, (int)scr->backgroundColor);

    /* Screen codes: one !byte line per row */
    for (y = 0; y < (int)scr->height; y++) {
        fprintf(fp, "!byte ");
        for (x = 0; x < (int)scr->width; x++) {
            UBYTE code = scr->framebuf[y * scr->width + x].code;
            if (x > 0) fprintf(fp, ",");
            fprintf(fp, "$%02X", (unsigned)code);
        }
        fprintf(fp, "\n");
    }

    /* Color data: one !byte line per row */
    for (y = 0; y < (int)scr->height; y++) {
        fprintf(fp, "!byte ");
        for (x = 0; x < (int)scr->width; x++) {
            UBYTE color = scr->framebuf[y * scr->width + x].color;
            if (x > 0) fprintf(fp, ",");
            fprintf(fp, "$%02X", (unsigned)(color & 0x0f));
        }
        fprintf(fp, "\n");
    }

    fclose(fp);
    return PETSCII_EXPORT_OK;
}
