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
//#define COLORPICKER_COLS  4
//#define COLORPICKER_ROWS  4
#define COLORPICKER_COLS  8
#define COLORPICKER_ROWS  2

/* Attribute tags */
#define CPA_Dummy         (TAG_USER | 0x0300)
#define CPA_Style         (CPA_Dummy + 1)   /* PetsciiStyle * (not owned) */
#define CPA_SelectedColor (CPA_Dummy + 2)   /* UBYTE 0..15                */
#define CPA_ColorsPerWidth (CPA_Dummy + 3)   /* 4 or 8   */
#define CPA_ColorRole (CPA_Dummy + 4)   /*  currently managing that color */
#define CPA_NumPadKeys (CPA_Dummy + 5) /* receive keys even when not activated */
/* need for popup cancelation */
#define CPA_Deactivated (CPA_Dummy + 6)



extern Class *ColorPickerClass;

int  ColorPicker_Init(void);
void ColorPicker_Exit(void);

#endif /* COLOR_PICKER_H */
