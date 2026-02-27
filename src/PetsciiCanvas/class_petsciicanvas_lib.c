/*
 * PetsciiCanvas - BOOPSI gadget class for the PETSCII editing canvas.
 * class lib: MakeClass / FreeClass.
 */

#include <proto/intuition.h>
#include "petscii_canvas_private.h"

/* Global class pointer */
Class *PetsciiCanvasClass = NULL;

int PetsciiCanvas_Init(void)
{
    PetsciiCanvasClass = MakeClass(
        NULL,           /* private class - no public name  */
        "gadgetclass",  /* superclass                       */
        NULL,           /* superclass by name, not pointer  */
        sizeof(PetsciiCanvasData),
        0               /* flags                            */
    );

    if (!PetsciiCanvasClass)
        return 0;

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
}
