#ifndef PETSCII_CANVAS_H
#define PETSCII_CANVAS_H

/*
 * PetsciiCanvas - BOOPSI gadget class for the main PETSCII editing canvas.
 *
 * Renders a PetsciiScreen via its PetsciiScreenBuf onto the window, scaled
 * to fit the gadget bounds (BlitNative at 1:1, BlitScaled otherwise).
 *
 * Attributes:
 *   PCA_Screen    (PetsciiScreen *)  - screen to display [new/set]
 *   PCA_Style     (PetsciiStyle *)   - colour palette     [new/set]
 *   PCA_ZoomLevel (UWORD) 1/2/4/8   - informational zoom [new/set/get]
 *   PCA_ShowGrid  (BOOL)             - grid overlay       [new/set/get]
 *   PCA_Dirty     (BOOL) set TRUE    - force full redraw  [set]
 *
 * Static-link usage (PETSCIICANVAS_STATICLINK=1):
 *   PetsciiCanvas_Init()  - call once before any NewObject
 *   PetsciiCanvas_Exit()  - call after all instances are disposed
 *   NewObject(PetsciiCanvasClass, NULL, GA_ID, ..., PCA_Screen, ..., TAG_END)
 */

#include <exec/types.h>
#include <utility/tagitem.h>
#include <intuition/classes.h>

/* Custom attribute tags */
#define PCA_Dummy       (TAG_USER | 0x0100)
#define PCA_Screen      (PCA_Dummy + 1) /* (PetsciiScreen *)  new/set     */
#define PCA_Style       (PCA_Dummy + 2) /* (PetsciiStyle *)   new/set     */
#define PCA_ZoomLevel   (PCA_Dummy + 3) /* (UWORD) 1,2,4,8   new/set/get */
#define PCA_ShowGrid    (PCA_Dummy + 4) /* (BOOL)             new/set/get */
#define PCA_Dirty       (PCA_Dummy + 5) /* (BOOL) TRUE=force full rebuild  */
#define PCA_KeepRatio   (PCA_Dummy + 6) /* (BOOL) preserve pixel ratio     */

/* Drawing tool state - set from outside via OM_SET                         */
#define PCA_CurrentTool  (PCA_Dummy + 7) /* (UBYTE) TOOL_*    new/set/get  */
#define PCA_SelectedChar (PCA_Dummy + 8) /* (UBYTE) 0-255     new/set/get  */
#define PCA_FgColor      (PCA_Dummy + 9) /* (UBYTE) 0-15      new/set/get  */
#define PCA_BgColor      (PCA_Dummy +10) /* (UBYTE) 0-15      new/set/get  */

/* Cursor position (updated by input handler; -1 = not shown) */
#define PCA_CursorCol    (PCA_Dummy +11) /* (WORD) col, -1=hidden     get  */
#define PCA_CursorRow    (PCA_Dummy +12) /* (WORD) row, -1=hidden     get  */

/* Undo/redo buffer for this screen (not owned by the gadget).
 * Set via OM_NEW or OM_SET to enable in-stroke undo snapshots.   */
#define PCA_UndoBuffer   (PCA_Dummy +13) /* (PetsciiUndoBuffer *) new/set  */

/* Signal: tell outside this tool is finisehd
   -brush lasso ends, meaning a new brush.
   -Text pressed Esc
*/
#define PCA_SignalStopTool   (PCA_Dummy +14)


/* Global class pointer, set by PetsciiCanvas_Init() */
extern Class *PetsciiCanvasClass;

/* Create the class. Returns 1 on success, 0 on failure. */
int  PetsciiCanvas_Init(void);

/* Free the class. Call after ALL instances have been disposed. */
void PetsciiCanvas_Exit(void);

#endif /* PETSCII_CANVAS_H */
