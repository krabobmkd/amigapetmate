#ifndef PETSCII_EXPORT_H
#define PETSCII_EXPORT_H

/*
 * petscii_export.h - Export current screen to various C64 formats.
 *
 * Ported from petmate_original_source/src/utils/exporters/
 */

#include "petscii_screen.h"

/* Return codes (shared by all exporters) */
#define PETSCII_EXPORT_OK     0
#define PETSCII_EXPORT_EOPEN  1
#define PETSCII_EXPORT_EWRITE 2
#define PETSCII_EXPORT_ENOMEM 3

/*
 * Export as C64 BASIC text source (.bas).
 * Ported from basic.ts.
 * Generates BASIC lines 10-140 (init + data loader) and DATA lines from 200.
 */
int PetsciiExport_SaveBAS(const PetsciiScreen *scr, const char *path);

/*
 * Export as ACME assembler source (.asm).
 * Ported from asm.ts.
 * Generates a label, !byte border,bg header, then screen codes and color data.
 * Assemble with: acme --cpu 6510 --format cbm --outfile out.prg out.asm
 */
int PetsciiExport_SaveASM(const PetsciiScreen *scr, const char *path);

/*
 * Export as C64 SEQ file (.seq).
 * Ported from seq.ts.
 * Encodes PETSCII with color control codes and reverse-video sequences.
 */
int PetsciiExport_SaveSEQ(const PetsciiScreen *scr, const char *path);

/*
 * Export as a standalone C64 PRG binary (.prg).
 * Self-contained executable: BASIC SYS stub + 6510 loader machine code +
 * inline screen data.  Only standard 40x25 screens are supported.
 * Returns PETSCII_EXPORT_EOPEN if the screen is not 40x25.
 */
int PetsciiExport_SavePrgASM(const PetsciiScreen *scr, const char *path);

/*
 * Export as a C64 BASIC PRG binary (.prg).
 * Produces a tokenized C64 BASIC 2.0 program (load address $0801) that
 * POKEs colors, then READs screen codes and color values from DATA lines.
 * Works with any screen dimensions (variables match totalCells).
 */
int PetsciiExport_SavePrgBAS(const PetsciiScreen *scr, const char *path);

#endif /* PETSCII_EXPORT_H */
