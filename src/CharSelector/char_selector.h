#ifndef CHAR_SELECTOR_H
#define CHAR_SELECTOR_H

/*
 * CharSelector - BOOPSI gadget showing all 256 PETSCII characters in a
 * 16-column x 16-row grid.  The user clicks or drags to select a character.
 *
 * Native buffer: 128 x 128 pixels (16 cols * 8px  x  16 rows * 8px).
 * Rendered from charset ROM data in the fg/bg colours from PetsciiStyle.
 * Scales to gadget bounds via BlitScaled (cached scaled buffer, no alloc
 * on each render).
 *
 * On selection: returns GMR_VERIFY|GMR_NOREUSE to generate WMHI_GADGETUP.
 * Caller reads the new value with GetAttr(CHSA_SelectedChar, ...).
 */

#include <exec/types.h>
#include <utility/tagitem.h>
#include <intuition/classes.h>

/* Native grid dimensions */
#define CHARSELECTOR_COLS       16
#define CHARSELECTOR_ROWS       16
#define CHARSELECTOR_NATIVE_W   128   /* COLS * 8 */
#define CHARSELECTOR_NATIVE_H   128   /* ROWS * 8 */

/* Custom attribute tags */
#define CHSA_Dummy        (TAG_USER | 0x0200)
#define CHSA_Style        (CHSA_Dummy + 1)  /* (PetsciiStyle *) new/set       */
#define CHSA_Charset      (CHSA_Dummy + 2)  /* (UBYTE) UPPER/LOWER  new/set   */
#define CHSA_SelectedChar (CHSA_Dummy + 3)  /* (UBYTE) 0-255   new/set/get    */
#define CHSA_FgColor      (CHSA_Dummy + 4)  /* (UBYTE) 0-15    new/set        */
#define CHSA_BgColor      (CHSA_Dummy + 5)  /* (UBYTE) 0-15    new/set        */
#define CHSA_Dirty        (CHSA_Dummy + 6)  /* (BOOL) force rebuild   set     */
#define CHSA_KeepRatio    (CHSA_Dummy + 7)  /* (BOOL) preserve 1:1 ratio      */

extern Class *CharSelectorClass;

int  CharSelector_Init(void);
void CharSelector_Exit(void);

#endif /* CHAR_SELECTOR_H */
