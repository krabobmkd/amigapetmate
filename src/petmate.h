#ifndef PETMATE_H
#define PETMATE_H

/*
 * Petmate Amiga - C64 PETSCII Art Editor
 * Main application header
 */

#include <exec/types.h>
#include "petscii_types.h"
#include "petscii_project.h"

/* Forward declarations */
struct App;

/* Current tool state */
typedef struct ToolState {
    UBYTE currentTool;      /* TOOL_DRAW, TOOL_COLORIZE, etc. */
    UBYTE selectedChar;     /* 0-255 screen code */
    UBYTE fgColor;          /* 0-15 foreground color */
    UBYTE bgColor;          /* 0-15 background color */
    UBYTE showGrid;         /* grid overlay toggle */
} ToolState;

#endif /* PETMATE_H */
