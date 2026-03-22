/*
 * ScreenCarousel - BOOPSI gadget class for the screen thumbnail strip.
 * Class registration: MakeClass / FreeClass.
 */

#include <proto/exec.h>
#include <proto/intuition.h>
#include "screen_carousel_private.h"

/* Global class pointer */
Class *ScreenCarouselClass = NULL;

int ScreenCarousel_Init(void)
{
    ScreenCarouselClass = MakeClass(
        NULL,           /* private class - no public name */
        "gadgetclass",  /* superclass                     */
        NULL,
        sizeof(ScreenCarouselData),
        0
    );

    if (!ScreenCarouselClass)
        return 0;

    ScreenCarouselClass->cl_Dispatcher.h_Entry    = (HOOKFUNC)ScreenCarousel_Dispatch;
    ScreenCarouselClass->cl_Dispatcher.h_SubEntry = NULL;
    ScreenCarouselClass->cl_Dispatcher.h_Data     = NULL;

    return 1;
}

void ScreenCarousel_Exit(void)
{
    if (ScreenCarouselClass) {
        FreeClass(ScreenCarouselClass);
        ScreenCarouselClass = NULL;
    }
}
