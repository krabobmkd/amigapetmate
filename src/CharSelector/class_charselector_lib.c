/*
 * CharSelector - BOOPSI gadget class lib: MakeClass / FreeClass.
 */

#include <proto/intuition.h>
#include "char_selector_private.h"

Class *CharSelectorClass = NULL;

int CharSelector_Init(void)
{
    CharSelectorClass = MakeClass(
        NULL,
        "gadgetclass",
        NULL,
        sizeof(CharSelectorData),
        0
    );
    if (!CharSelectorClass) return 0;

    CharSelectorClass->cl_Dispatcher.h_Entry    = (HOOKFUNC)CharSelector_Dispatch;
    CharSelectorClass->cl_Dispatcher.h_SubEntry = NULL;
    CharSelectorClass->cl_Dispatcher.h_Data     = NULL;
    return 1;
}

void CharSelector_Exit(void)
{
    if (CharSelectorClass) {
        FreeClass(CharSelectorClass);
        CharSelectorClass = NULL;
    }
}
