/*
 * PetsciiCanvas - BOOPSI gadget class for the PETSCII editing canvas.
 * class lib: MakeClass / FreeClass.
 */

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/keymap.h>
#include "petscii_canvas_private.h"

/* Global class pointer */
Class *PetsciiCanvasClass = NULL;

/* keymap.library base — used by handleinput.c for MapRawKey() */
struct Library *KeymapBase = NULL;

int PetsciiCanvas_Init(void)
{
    KeymapBase = OpenLibrary("keymap.library", 36);
    if (!KeymapBase)
        return 0;

    PetsciiCanvasClass = MakeClass(
        NULL,           /* private class - no public name  */
        "gadgetclass",  /* superclass                       */
        NULL,           /* superclass by name, not pointer  */
        sizeof(PetsciiCanvasData),
        0               /* flags                            */
    );

    if (!PetsciiCanvasClass) {
        CloseLibrary(KeymapBase);
        KeymapBase = NULL;
        return 0;
    }

    PetsciiCanvasClass->cl_Dispatcher.h_Entry    = (HOOKFUNC)PetsciiCanvas_Dispatch;
    PetsciiCanvasClass->cl_Dispatcher.h_SubEntry = NULL;
    PetsciiCanvasClass->cl_Dispatcher.h_Data     = NULL;

    return 1;
}

void PetsciiCanvas_Exit(void)
{
    if (PetsciiCanvasClass) {
        FreeClass(PetsciiCanvasClass);
        PetsciiCanvasClass = NULL;
    }
    if (KeymapBase) {
        CloseLibrary(KeymapBase);
        KeymapBase = NULL;
    }
}
