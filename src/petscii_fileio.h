#ifndef PETSCII_FILEIO_H
#define PETSCII_FILEIO_H

/*
 * PetsciiFileIO - Load/save .petmate JSON files.
 *
 * .petmate format (version 1):
 * {
 *   "version": 1,
 *   "framebufs": [
 *     {
 *       "width": 40,
 *       "height": 25,
 *       "screencodes": [...],    (width*height integers 0-255)
 *       "colors": [...],         (width*height integers 0-15)
 *       "backgroundColor": 6,
 *       "borderColor": 14,
 *       "charset": "upper",      ("upper" or "lower")
 *       "name": "Screen 1"
 *     }
 *   ]
 * }
 *
 * Uses cJSON (cjson/cJSON.h) for parsing and generating JSON.
 * cJSON is already present as a forked Amiga build (no float support).
 *
 * Error codes returned by Load/Save:
 *   PETSCII_FILEIO_OK       - success
 *   PETSCII_FILEIO_EOPEN    - could not open file
 *   PETSCII_FILEIO_EREAD    - file read error
 *   PETSCII_FILEIO_EPARSE   - JSON parse error
 *   PETSCII_FILEIO_EFORMAT  - JSON structure unexpected
 *   PETSCII_FILEIO_EALLOC   - memory allocation failure
 *   PETSCII_FILEIO_EWRITE   - file write error
 */

#include <exec/types.h>
#include "petscii_project.h"

/* Error codes */
#define PETSCII_FILEIO_OK      0
#define PETSCII_FILEIO_EOPEN   1
#define PETSCII_FILEIO_EREAD   2
#define PETSCII_FILEIO_EPARSE  3
#define PETSCII_FILEIO_EFORMAT 4
#define PETSCII_FILEIO_EALLOC  5
#define PETSCII_FILEIO_EWRITE  6

/*
 * Load a .petmate file into an existing project.
 * Resets the project first (destroys all current screens).
 * Returns PETSCII_FILEIO_OK on success, error code otherwise.
 * On success, proj->filepath is set to path.
 */
int PetsciiFileIO_Load(PetsciiProject *proj, const char *path);

/*
 * Save the project as a .petmate file.
 * Returns PETSCII_FILEIO_OK on success, error code otherwise.
 * On success, proj->filepath is set to path and proj->modified is cleared.
 */
int PetsciiFileIO_Save(const PetsciiProject *proj, const char *path);

/*
 * Human-readable string for an error code (for EasyRequester messages).
 */
const char *PetsciiFileIO_ErrorString(int errCode);

#endif /* PETSCII_FILEIO_H */
