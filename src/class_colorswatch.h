#ifndef CLASS_COLORSWATCH_H
#define CLASS_COLORSWATCH_H

/*
 * ColorSwatch - BOOPSI gadget class.
 *
 * Displays a single solid colour rectangle (the whole gadget area).
 * A click fires a CSW_Clicked OM_NOTIFY to ICA_TARGET, which the app
 * can use to open the colour-picker popup positioned below this gadget.
 */

#include <exec/types.h>
#include <intuition/classes.h>

/* Attribute tags */
#define CSW_Dummy        (TAG_USER | 0x0500)
#define CSW_Style        (CSW_Dummy + 1)   /* PetsciiStyle * (not owned)        */
#define CSW_ColorIndex   (CSW_Dummy + 2)   /* UBYTE 0..15 – colour to display   */
#define CSW_Clicked      (CSW_Dummy + 3)   /* ULONG: notified on click (value = index) */

extern Class *ColorSwatchClass;

int  ColorSwatch_Init(void);
void ColorSwatch_Exit(void);

#endif /* CLASS_COLORSWATCH_H */
