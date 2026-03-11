#ifndef PETSCII_EXPORT_GIF_H
#define PETSCII_EXPORT_GIF_H

/*
 * petscii_export_gif.h - Export PETSCII screen as a GIF image via giflib.
 *
 * Writes a GIF87a file with:
 *   - 16-colour global colour map from style->c64pens[].rgbcolor
 *   - Pixel indices are C64 colour indices 0-15 (not Amiga pen numbers)
 *   - Optional 8-pixel C64 border strip around the active display area
 *
 * Return codes: PETSCII_EXPORT_OK / PETSCII_EXPORT_EOPEN /
 *               PETSCII_EXPORT_EWRITE / PETSCII_EXPORT_ENOMEM
 */

#include "petscii_screen.h"
#include "petscii_style.h"
#include <exec/types.h>

/*
 * Save the current screen as a GIF file.
 *   scr        : PETSCII screen to export
 *   style      : active style (provides the 16-colour RGB palette)
 *   path       : destination file path (should end in ".gif")
 *   withBorder : if TRUE, adds an 8-pixel C64 border strip around the image
 *
 * Image dimensions:
 *   FALSE -> width*8  x height*8   (e.g. 320x200 for 40x25 screen)
 *   TRUE  -> width*8+16 x height*8+16
 */
int PetsciiExport_SaveGIF(const PetsciiScreen *scr,
                          const PetsciiStyle  *style,
                          const char          *path,
                          BOOL                 withBorder);

#endif /* PETSCII_EXPORT_GIF_H */
