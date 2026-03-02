#ifndef PETSCII_PROJECT_H
#define PETSCII_PROJECT_H

#include <exec/types.h>
#include "petscii_types.h"
#include "petscii_screen.h"

/*
 * PetsciiProject: a collection of screens forming a .petmate workspace.
 */
typedef struct PetsciiProject {
    PetsciiScreen *screens[PETSCII_MAX_SCREENS];
    UWORD screenCount;          /* number of screens */
    UWORD currentScreen;        /* index of currently displayed screen */
    char filepath[PETSCII_PATH_LEN]; /* last saved/loaded path */
    UBYTE modified;             /* dirty flag: non-zero if unsaved changes */
    UBYTE currentPalette;       /* palette index 0-3 */
} PetsciiProject;

/* Create a new empty project (with one default screen) */
PetsciiProject *PetsciiProject_Create(void);

/* Destroy project and all its screens */
void PetsciiProject_Destroy(PetsciiProject *proj);

/* Add a new default screen. Returns screen index or -1 on failure */
int PetsciiProject_AddScreen(PetsciiProject *proj);

/* Clone the current screen and add it. Returns new index or -1 */
int PetsciiProject_CloneCurrentScreen(PetsciiProject *proj);

/* Remove screen at index. Returns 1 on success, 0 if last screen */
int PetsciiProject_RemoveScreen(PetsciiProject *proj, UWORD index);

/* Get the currently active screen */
PetsciiScreen *PetsciiProject_GetCurrentScreen(PetsciiProject *proj);

/* Set current screen index (clamped to valid range) */
void PetsciiProject_SetCurrentScreen(PetsciiProject *proj, UWORD index);

/* Navigate screens: delta = +1 for next, -1 for previous */
void PetsciiProject_NavigateScreen(PetsciiProject *proj, int delta);

/* Get screen count */
#define PetsciiProject_ScreenCount(proj) ((proj)->screenCount)

/* Reset project to single empty screen */
void PetsciiProject_Reset(PetsciiProject *proj);

#endif /* PETSCII_PROJECT_H */
