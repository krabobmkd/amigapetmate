/*
 * ColorPicker - BOOPSI gadget class registration.
 * MakeClass / FreeClass wrappers.
 */

#include <proto/intuition.h>
#include <intuition/gadgetclass.h>
#include "color_picker_private.h"

Class *ColorPickerClass = NULL;

int ColorPicker_Init(void)
{
    if (ColorPickerClass) return 1;

    ColorPickerClass = MakeClass(NULL, "gadgetclass", NULL,
                                 sizeof(ColorPickerData), 0);
    if (!ColorPickerClass) return 0;

    ColorPickerClass->cl_Dispatcher.h_Entry = (HOOKFUNC)ColorPicker_Dispatch;
    return 1;
}

void ColorPicker_Exit(void)
{
    if (ColorPickerClass) {
        FreeClass(ColorPickerClass);
        ColorPickerClass = NULL;
    }
}
