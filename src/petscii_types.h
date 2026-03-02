#ifndef PETSCII_TYPES_H
#define PETSCII_TYPES_H

#include <exec/types.h>

/* A single character cell on the PETSCII screen */
typedef struct PetsciiPixel {
    UBYTE code;     /* 0-255 PETSCII character code (screen code) */
    UBYTE color;    /* 0-15 C64 color index */
} PetsciiPixel;

/* Default C64 screen dimensions */
#define PETSCII_DEFAULT_WIDTH   40
#define PETSCII_DEFAULT_HEIGHT  25

/* Maximum supported screen dimensions */
#define PETSCII_MAX_WIDTH       80
#define PETSCII_MAX_HEIGHT      50

/* Character set selection */
#define PETSCII_CHARSET_UPPER   0
#define PETSCII_CHARSET_LOWER   1

/* C64 color count */
#define C64_COLOR_COUNT 16

/* Character dimensions in pixels */
#define PETSCII_CHAR_WIDTH  8
#define PETSCII_CHAR_HEIGHT 8

/* Characters per charset */
#define PETSCII_CHAR_COUNT 256

/* Bytes per charset ROM (256 chars * 8 bytes each) */
#define PETSCII_CHARSET_SIZE 2048

/* Maximum number of screens in a project */
#define PETSCII_MAX_SCREENS 64

/* Maximum screen name length */
#define PETSCII_NAME_LEN 64

/* Maximum file path length */
#define PETSCII_PATH_LEN 256

/* Tool enumeration - matches Petmate tool order */
enum PetsciiTool {
    TOOL_DRAW = 0,      /* Place selected char with selected color */
    TOOL_COLORIZE,      /* Change color of existing char */
    TOOL_CHARDRAW,      /* Place selected char, keep existing color */
    TOOL_BRUSH,         /* Paint with captured brush selection */
    TOOL_TEXT,          /* Type text with keyboard */
    TOOL_COUNT
};

/* C64 color indices */
enum C64Color {
    C64_BLACK = 0,
    C64_WHITE,
    C64_RED,
    C64_CYAN,
    C64_PURPLE,
    C64_GREEN,
    C64_BLUE,
    C64_YELLOW,
    C64_ORANGE,
    C64_BROWN,
    C64_LIGHTRED,
    C64_DARKGREY,
    C64_GREY,
    C64_LIGHTGREEN,
    C64_LIGHTBLUE,
    C64_LIGHTGREY
};

/* Palette selection */
enum PetsciiPaletteID {
    PALETTE_PETMATE = 0,
    PALETTE_COLODORE,
    PALETTE_PEPTO,
    PALETTE_VICE,
    PALETTE_COUNT
};

#endif /* PETSCII_TYPES_H */
