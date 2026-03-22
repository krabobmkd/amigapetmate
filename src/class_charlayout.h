#ifndef CLASS_CHARLAYOUT_H
#define CLASS_CHARLAYOUT_H

/*
 * CharLayout - custom BOOPSI layout class inheriting from layout.gadget.
 *
 * Overrides only OM_NEW and GM_LAYOUT.  Children are placed top-to-bottom
 * with full available width.  Heights are assigned by GA_ID:
 *
 *   GAD_CHARSELECTOR   : height = width  (keeps 16×16 grid square)
 *   GAD_COLORPICKER_FG : height = width / 4  (8×2 grid, cells square)
 *   GAD_COLORPICKER_BG : same formula
 *   any other child    : nominal height from GM_DOMAIN
 *
 * Unused space at the bottom is left empty.
 *
 * Usage:
 *   CharLayout_Init();
 *   rightPanel = NewObject(CharLayoutClass, NULL,
 *       LAYOUT_AddChild, colorPickerFg,
 *       LAYOUT_AddChild, colorPickerBg,
 *       LAYOUT_AddChild, charSelector,
 *       LAYOUT_AddChild, charsetLayout,
 *       TAG_DONE);
 *   CharLayout_Exit();  -- call after DisposeObject
 */

#include <exec/types.h>
#include <intuition/classes.h>

/* Maximum number of direct children supported */
#define CHARLAYOUT_MAXCHILDREN 16

/* Global class pointer, set by CharLayout_Init() */
extern Class *CharLayoutClass;

/* Create the class.  Returns 1 on success, 0 on failure. */
int  CharLayout_Init(void);

/* Free the class.  Call after ALL instances have been disposed. */
void CharLayout_Exit(void);

#endif /* CLASS_CHARLAYOUT_H */
