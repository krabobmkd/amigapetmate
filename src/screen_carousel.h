#ifndef SCREEN_CAROUSEL_H
#define SCREEN_CAROUSEL_H

/*
 * ScreenCarousel - BOOPSI gadget class for the screen thumbnail strip.
 *
 * Displays a vertical list of 42x27 miniature thumbnails (one per
 * PetsciiScreen in the project).  Handles scroll when there are more
 * screens than fit in the gadget height.  Sends notification when the
 * user clicks a miniature so the host can switch the current screen.
 *
 * Attribute tags:
 *   SCA_Project        (PetsciiProject *) - project whose screens are shown [new/set]
 *   SCA_Style          (PetsciiStyle *)   - colour palette for rendering    [new/set]
 *   SCA_CurrentScreen  (ULONG)            - highlighted screen index        [new/set/get]
 *   SCA_ScreenModified (ULONG)            - re-render one thumbnail by index [set]
 *   SCA_AllModified    (BOOL) TRUE        - re-render all thumbnails        [set]
 *   SCA_SignalScreen   (ULONG)            - index of last clicked screen    [get]
 *
 * Usage (static-link):
 *   ScreenCarousel_Init();   // once at startup
 *   gadget = NewObject(ScreenCarouselClass, NULL,
 *                      GA_Left, x, GA_Top, y, GA_Width, w, GA_Height, h,
 *                      SCA_Project, proj, SCA_Style, style,
 *                      TAG_END);
 *   ...
 *   // When a screen is modified:
 *   SetAttrs(gadget, SCA_ScreenModified, screenIdx, TAG_END);
 *   RefreshGList((struct Gadget *)gadget, window, NULL, 1);
 *
 *   // After GADGETUP: query clicked index:
 *   GetAttr(SCA_SignalScreen, gadget, &clickedIdx);
 *
 *   ScreenCarousel_Exit();   // once at shutdown, after DisposeObject
 */

#include <exec/types.h>
#include <utility/tagitem.h>
#include <intuition/classes.h>

/* Attribute tag base — must not clash with PCA_Dummy (TAG_USER | 0x0100) */
#define SCA_Dummy          (TAG_USER | 0x0200)
#define SCA_Project        (SCA_Dummy + 1) /* (PetsciiProject *) new/set       */
#define SCA_Style          (SCA_Dummy + 2) /* (PetsciiStyle *)   new/set       */
#define SCA_CurrentScreen  (SCA_Dummy + 3) /* (ULONG) highlighted index  new/set/get */
#define SCA_ScreenModified (SCA_Dummy + 4) /* (ULONG) rebuild 1 mini     set         */
#define SCA_AllModified    (SCA_Dummy + 5) /* (BOOL)  rebuild all minis  set         */
#define SCA_SignalScreen   (SCA_Dummy + 6) /* (ULONG) last clicked index get         */

/* Global class pointer, set by ScreenCarousel_Init() */
extern Class *ScreenCarouselClass;

/* Create the gadget class. Returns 1 on success, 0 on failure. */
int  ScreenCarousel_Init(void);

/* Free the gadget class. Call after ALL instances have been disposed. */
void ScreenCarousel_Exit(void);

#endif /* SCREEN_CAROUSEL_H */
