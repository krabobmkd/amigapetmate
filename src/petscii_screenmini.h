#ifndef PETSCII_SCREENMINI_H
#define PETSCII_SCREENMINI_H

/*
 * PetsciiScreenMini - 42x27 chunky-pixel miniature thumbnail of a PETSCII screen.
 *
 * Layout: 40 chars wide + 1 border pixel on each side = 42 pixels wide.
 *         25 chars tall + 1 border pixel on each side = 27 pixels tall.
 * Each pixel stores one Amiga pen index (ready for WriteChunkyPixels).
 *
 * Char density: uses petsciiUpperMinify / petsciiLowerMinify tables
 * (from petscii_chartransform.h) to decide whether each char's 1-pixel
 * representative is the foreground (char) color or background color.
 * table value 1 = char has >= 32 bits set (dense) -> use char color.
 * table value 0 = sparse -> use background color.
 */

#include <exec/types.h>
#include "petscii_screen.h"
#include "petscii_style.h"

/* Forward declaration */
struct RastPort;

#define SCREENMINI_W  42   /* 40 chars + 1 border pixel left + 1 right */
#define SCREENMINI_H  27   /* 25 chars + 1 border pixel top  + 1 bottom */

typedef struct PetsciiScreenMini {
    UBYTE pixels[SCREENMINI_W * SCREENMINI_H]; /* Amiga pen indices, row-major */
    UBYTE valid;   /* 0 = needs rebuild, 1 = ready to blit */
} PetsciiScreenMini;

/* Allocate and zero-initialize a miniature buffer. Returns NULL on failure. */
PetsciiScreenMini *PetsciiScreenMini_Create(void);

/* Free a miniature buffer. */
void PetsciiScreenMini_Destroy(PetsciiScreenMini *mini);

/* Render the miniature from a screen and style.
 * Uses petsciiUpperMinify / petsciiLowerMinify for char density.
 * Sets mini->valid = 1 on completion. */
void PetsciiScreenMini_Render(PetsciiScreenMini   *mini,
                               const PetsciiScreen *scr,
                               const PetsciiStyle  *style);

/* Blit the miniature to a RastPort at pixel position (destX, destY).
 * No-op if mini->valid == 0. */
void PetsciiScreenMini_Blit(const PetsciiScreenMini *mini,
                             struct RastPort         *rp,
                             WORD                     destX,
                             WORD                     destY);

void PetsciiScreenMini_BlitRGB(const PetsciiScreenMini *mini,
                             struct RastPort         *rp,
                             WORD                     destX,
                             WORD                     destY);

#endif /* PETSCII_SCREENMINI_H */
