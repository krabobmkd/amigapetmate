#ifndef PETSCII_IMPORT_PRG_H
#define PETSCII_IMPORT_PRG_H

/*
 * petscii_import_prg.h - Import a C64 ASM-export PRG into a PETSCII screen.
 *
 * Analyses the 6510 machine code inside the PRG to locate the data block:
 *   - Scans for  LDA abs -> STA $D020  to find the border-color byte address,
 *     which is the start of the data block in C64 memory space.
 *   - Scans for  LDA #imm -> STA $D018  to detect the charset (upper/lower).
 *   - Converts the C64 address to a file offset using the PRG load address.
 *   - Reads:  [border][bg][1000 screen codes][1000 color codes]
 *
 * This tolerates different load addresses and minor code-layout variations as
 * long as the data structure matches our ASM export format.
 *
 * C89 compatible.  Uses AllocVec/FreeVec for the file buffer.
 */

#include "petscii_screen.h"
#include <exec/types.h>

/* Return codes */
#define PETSCII_IMPORT_PRG_OK       0   /* success                          */
#define PETSCII_IMPORT_PRG_EOPEN    1   /* cannot open/read file            */
#define PETSCII_IMPORT_PRG_ENOMEM   2   /* out of memory                    */
#define PETSCII_IMPORT_PRG_EFORMAT  3   /* no LDA->STA $D020 pattern found  */
#define PETSCII_IMPORT_PRG_ESIZE    4   /* file too small for data block    */

/*
 * Load a C64 ASM-export PRG and fill scr with the decoded PETSCII screen.
 *
 *   path - full path to the .prg file
 *   scr  - destination screen: framebuf, backgroundColor, borderColor,
 *           and charset are updated on success
 *
 * Returns one of the PETSCII_IMPORT_PRG_* codes above.
 */
int PetsciiImport_FromPrg(const char *path, PetsciiScreen *scr);

#endif /* PETSCII_IMPORT_PRG_H */
