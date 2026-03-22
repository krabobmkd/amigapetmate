#ifndef CLASS_LAYOUTWITHPOPUP_H
#define CLASS_LAYOUTWITHPOPUP_H

/*
 * LayoutWithPopup - custom BOOPSI layout class inheriting from layout.gadget.
 * EXPERIMENTAL
 * Overrides OM_NEW, OM_DISPOSE, GM_LAYOUT, OM_SET, OM_GET.
 * Supports one popup child that can be shown/hidden at arbitrary coordinates.
 * The popup is added to the superclass child list with zero-height constraints
 * and its position is overridden in GM_LAYOUT.
 */

#include <exec/types.h>
#include <intuition/classes.h>
#include <gadgets/layout.h>

#define LayoutWithPopup_MAXCHILDREN  16

/* Global class pointer, set by LayoutWithPopup_Init() */
extern Class *LayoutWithPopupClass;

/* Create the class.  Returns 1 on success, 0 on failure. */
int  LayoutWithPopup_Init(void);

/* Free the class.  Call after ALL instances have been disposed. */
void LayoutWithPopup_Exit(void);

#define LAYOUTWP_Dummy          (LAYOUT_Dummy + 0x288)

#define LAYOUTWP_POPUPGADGET    (LAYOUTWP_Dummy + 1)  /* Object *: popup child       */
#define LAYOUTWP_POPUPX         (LAYOUTWP_Dummy + 2)  /* WORD:     popup X offset    */
#define LAYOUTWP_POPUPY         (LAYOUTWP_Dummy + 3)  /* WORD:     popup Y offset    */
#define LAYOUTWP_POPUPVISIBLE   (LAYOUTWP_Dummy + 4)  /* BOOL:     show/hide popup   */
#define LAYOUTWP_SENDERID       (LAYOUTWP_Dummy + 5)  /* id: who asked for opening   */

#endif /* CLASS_LAYOUTWITHPOPUP_H */
