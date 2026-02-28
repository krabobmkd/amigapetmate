#ifndef PMTOOLBAR_H
#define PMTOOLBAR_H

/*
 * PmToolbar - Left-side vertical toolbar for Petmate Amiga.
 *
 * Contains two groups of buttons:
 *   Tool group (mutually exclusive): Draw, Color, Char, Brush, Text
 *   Action group:                    Undo, Redo, Clear
 *
 * The layout object (toolbar.layout) is a ReAction VLayout ready to be
 * embedded into the main window layout as a fixed-width left panel.
 */

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/classusr.h>

/* Number of mutually-exclusive tool buttons */
#define TOOLBAR_TOOL_COUNT 5

typedef struct PmToolbar {
    Object *layout;                      /* outer VLayout (place in main layout) */
    Object *toolBtns[TOOLBAR_TOOL_COUNT]; /* Draw/Colorize/CharDraw/Brush/Text   */
    Object *undoBtn;
    Object *redoBtn;
    Object *clearBtn;
} PmToolbar;

/* Build the toolbar layout.  di must be valid.
 * Returns 1 on success, 0 on allocation failure. */
int  PmToolbar_Create(PmToolbar *tb);

/* Update which tool button shows as selected.
 * tool is one of the TOOL_* constants from petscii_types.h.
 * win may be NULL if the window is not yet open. */
void PmToolbar_SetActiveTool(PmToolbar *tb, UBYTE tool, struct Window *win);

/* just so the selected tool doesnt toggle up when reclicked. */
void PmToolbar_ForceDownTrick(PmToolbar *tb, UBYTE tool, struct Window *win);

#endif /* PMTOOLBAR_H */
