#include "petscii_palette.h"

/*
 * C64 color palettes - four variants from the Petmate editor.
 * Each color stored as 0x00RRGGBB.
 * Color order: Black, White, Red, Cyan, Purple, Green, Blue, Yellow,
 *              Orange, Brown, LightRed, DarkGrey, Grey, LightGreen,
 *              LightBlue, LightGrey
 */
const ULONG c64Palettes[PALETTE_COUNT][C64_COLOR_COUNT] = {
    /* PALETTE_PETMATE - Petmate default palette */
    {
        0x000000, 0xFFFFFF, 0x924A40, 0x84C5CC,
        0x9351B6, 0x72B14B, 0x483AA4, 0xD5DF7C,
        0x99692D, 0x675201, 0xC08178, 0x606060,
        0x8A8A8A, 0xB2EC91, 0x867ADE, 0xAEAEAE
    },
    /* PALETTE_COLODORE */
    {
        0x000000, 0xFFFFFF, 0x813338, 0x75CEC8,
        0x8E3C97, 0x56AC4D, 0x2E2C9B, 0xEDF171,
        0x8E5029, 0x553800, 0xC46C71, 0x4A4A4A,
        0x7B7B7B, 0xA9FF9F, 0x706DEB, 0xB2B2B2
    },
    /* PALETTE_PEPTO */
    {
        0x000000, 0xFFFFFF, 0x67372D, 0x73A3B1,
        0x6E3E83, 0x5B8D48, 0x362976, 0xB7C576,
        0x6C4F2A, 0x423908, 0x98675B, 0x444444,
        0x6C6C6C, 0x9DD28A, 0x6D5FB0, 0x959595
    },
    /* PALETTE_VICE */
    {
        0x000000, 0xFFFFFF, 0xB96A54, 0xACF3FE,
        0xBE73F8, 0x9AE35B, 0x695AF1, 0xFFFD84,
        0xC5913C, 0x8C7817, 0xF3AB98, 0x818181,
        0xB6B6B6, 0xDCFEA3, 0xB1A0FC, 0xE0E0E0
    }
};

const char *c64PaletteNames[PALETTE_COUNT] = {
    "PetMate",
    "Colodore",
    "Pepto",
    "VICE"
};
