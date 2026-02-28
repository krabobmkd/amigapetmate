#ifndef COLOR_PICKER_PRIVATE_H
#define COLOR_PICKER_PRIVATE_H

/*
 * ColorPicker - private header.
 * Shared by all class_colorpicker_*.c files only.
 * Do NOT include from outside ColorPicker/.
 */

#include <exec/types.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>
#include <intuition/intuition.h>

#include "compilers.h"
#include "color_picker.h"
#include "petscii_style.h"

/* Convenience cast: Object * -> struct Gadget * */
#ifndef G
#define G(o) ((struct Gadget *)(o))
#endif

/* Per-instance data stored inside the BOOPSI object */
typedef struct ColorPickerData {
    PetsciiStyle *style;          /* colour palette (not owned) */
    UBYTE         selectedColor;  /* currently selected C64 colour index 0..15 */
    UBYTE         ColorsPerWidth;

} ColorPickerData;

/* ------------------------------------------------------------------
 * Method handlers - each implemented in a separate .c file
 * ------------------------------------------------------------------ */

ULONG ColorPicker_OnNew        (Class *cl, Object *o, struct opSet       *msg);
ULONG ColorPicker_OnDispose    (Class *cl, Object *o, Msg                 msg);
ULONG ColorPicker_OnSet        (Class *cl, Object *o, struct opSet       *msg);
ULONG ColorPicker_OnGet        (Class *cl, Object *o, struct opGet       *msg);
ULONG ColorPicker_OnRender     (Class *cl, Object *o, struct gpRender    *msg);
ULONG ColorPicker_OnHitTest    (Class *cl, Object *o, struct gpHitTest   *msg);
ULONG ColorPicker_OnGoActive   (Class *cl, Object *o, struct gpInput     *msg);
ULONG ColorPicker_OnHandleInput(Class *cl, Object *o, struct gpInput     *msg);
ULONG ColorPicker_OnGoInactive (Class *cl, Object *o, struct gpGoInactive *msg);

/* Main dispatcher (class_colorpicker_dispatch.c) */
ULONG ASM SAVEDS ColorPicker_Dispatch(
    REG(a0, Class  *cl),
    REG(a2, Object *o),
    REG(a1, Msg     msg));

#endif /* COLOR_PICKER_PRIVATE_H */
