#ifndef PETSCII_IMPORT_IMAGE_H
#define PETSCII_IMPORT_IMAGE_H

/*
 * petscii_import_image.h - Import a bitmap image into a PETSCII screen.
 *
 * Ported from petmate/src/utils/importers/png2petscii.ts.
 *
 * Reads any picture file supported by Amiga datatypes (picture.datatype),
 * quantizes to the active 16-color C64 palette, then tries all 16 possible
 * background colors to find a valid PETSCII decoding.
 *
 * Supported input dimensions:
 *   - 320x200  (borderless: standard C64 display area)
 *   - 384x272  (bordered: 32px left/right, 35px top, 37px bottom)
 *
 * Requires datatypes.library (picture.datatype v40+, OS 3.5 or later).
 *
 * C89 compatible.
 */

#include "petscii_screen.h"
#include "petscii_style.h"
#include <exec/types.h>

/* Return codes */
#define PETSCII_IMPORT_OK       0
#define PETSCII_IMPORT_EOPEN    1   /* cannot open / load file              */
#define PETSCII_IMPORT_ENOMEM   3   /* out of memory                        */
#define PETSCII_IMPORT_ENOMATCH 4   /* no background color gave full match  */
#define PETSCII_IMPORT_ESIZE    5   /* unsupported image dimensions         */

/*
 * Load an image file and convert it to a PETSCII screen.
 *
 *   path    - full path to the image file (any format with a picture DT)
 *   scr     - destination screen: framebuf, backgroundColor, borderColor
 *             are updated on success; width and height must be 40x25
 *   style   - current style providing the 16-color RGB palette for quantization
 *
 * On success (PETSCII_IMPORT_OK):
 *   scr->framebuf[]   is filled with {code, color} for all 40*25 cells
 *   scr->backgroundColor is set to the detected background color index
 *   scr->borderColor   is set from the border pixels (bordered input only;
 *                       unchanged for borderless input)
 *
 * Returns one of the PETSCII_IMPORT_* codes above.
 */
int PetsciiImport_FromImage(const char        *path,
                             PetsciiScreen     *scr,
                             const PetsciiStyle *style);

#endif /* PETSCII_IMPORT_IMAGE_H */
