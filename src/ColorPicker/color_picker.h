#ifndef COLOR_PICKER_H
#define COLOR_PICKER_H

/*
 * ColorPicker - public interface for the BOOPSI color-picker gadget.
 *
 * Displays a 4x4 grid of C64 colour swatches (indices 0..15) rendered
 * with RectFill using the pens from a PetsciiStyle palette.
 * The selected colour cell is highlighted with a COMPLEMENT border.
 * A single click fires IDCMP_GADGETUP immediately (no drag mode).
 * The app reads CPA_SelectedColor via GetAttr() to obtain the new index.
 */

#include <exec/types.h>
#include <intuition/classes.h>

/* Grid geometry */
#define COLORPICKER_COLS  4
#define COLORPICKER_ROWS  4

/* Attribute tags */
#define CPA_Dummy         (TAG_USER | 0x0300)
#define CPA_Style         (CPA_Dummy + 1)   /* PetsciiStyle * (not owned) */
#define CPA_SelectedColor (CPA_Dummy + 2)   /* UBYTE 0..15                */

extern Class *ColorPickerClass;

int  ColorPicker_Init(void);
void ColorPicker_Exit(void);

#endif /* COLOR_PICKER_H */
