#ifndef GADGETID_H
#define GADGETID_H

/**
 * Gadget ID definitions for Petmate BOOPSI gadgets.
 *
 * These IDs identify specific gadget instances for event routing
 * through the BoopsiDelay message queue.
 */

/* Main canvas area */
#define GAD_CANVAS              1

/* Right panel gadgets */
#define GAD_CHARSELECTOR        2
#define GAD_COLORPICKER_FG      3
//#define GAD_COLORPICKER_BG      4
#define GAD_COLORPICKER_POPUP  5

#define GAD_COLORWATCH_BG       6
#define GAD_COLORWATCH_BD       7


/* Toolbar tool buttons (mutually exclusive toggle group) */
#define GAD_TOOL_DRAW           10
#define GAD_TOOL_COLORIZE       11
#define GAD_TOOL_CHARDRAW       12
#define GAD_TOOL_BRUSH          13
#define GAD_TOOL_TEXT           14

#define GAD_TOOL_FIRST          GAD_TOOL_DRAW
#define GAD_TOOL_LAST           GAD_TOOL_TEXT

/* Toolbar action buttons */
#define GAD_TOOL_UNDO           20
#define GAD_TOOL_REDO           21
#define GAD_TOOL_CLEAR          22

/* Charset toggle buttons */
#define GAD_CHARSET_UPPER       30
#define GAD_CHARSET_LOWER       31

/* Screen carousel gadget (vertical thumbnail strip, replaces screen tab bar) */
#define GAD_SCREENCAROUSEL      40

/* Status bar */
#define GAD_STATUSBAR           50

#endif /* GADGETID_H */
