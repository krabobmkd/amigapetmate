#ifndef PETSCII_UNDO_H
#define PETSCII_UNDO_H

/*
 * petscii_undo - Per-screen circular undo/redo history buffer.
 *
 * Stores up to PETSCII_UNDO_LEVELS (32) full screen snapshots in a
 * circular ring.  Memory cost: up to 32 x 2 KB = 64 KB per screen.
 *
 * Usage:
 *   1. Before a draw stroke, call PetsciiUndoBuffer_Push(buf, screen)
 *      to snapshot the BEFORE state.
 *   2. The draw stroke then modifies the screen in-place.
 *   3. PetsciiUndoBuffer_Undo  swaps back to the before state.
 *   4. PetsciiUndoBuffer_Redo  re-applies a reverted change.
 *   5. Any new push discards all redo history (branching rule).
 *
 * The ring holds both undo and redo snapshots, separated by a cursor:
 *   slots[start ... start+undos-1]       = undo history (oldest first)
 *   slots[start+undos ... start+undos+redos-1] = redo history
 *
 * Undo and Redo use pointer-swaps on the framebuf, so no extra
 * allocation is needed at undo/redo time.
 *
 * The current screen state is NOT stored in the buffer; it lives in the
 * project's PetsciiScreen struct.
 */

#include <exec/types.h>
#include "petscii_screen.h"

#define PETSCII_UNDO_LEVELS 32

typedef struct PetsciiUndoBuffer {
    PetsciiScreen *slots[PETSCII_UNDO_LEVELS]; /* circular ring of snapshots */
    UBYTE          start;   /* ring index of oldest undo entry              */
    UBYTE          undos;   /* undo steps available  (0 = nothing to undo)  */
    UBYTE          redos;   /* redo steps available  (0 = nothing to redo)  */
} PetsciiUndoBuffer;

/* Allocate and initialise an empty undo buffer.
 * Returns NULL on allocation failure. */
PetsciiUndoBuffer *PetsciiUndoBuffer_Create(void);

/* Destroy the buffer and free all snapshots it holds. */
void PetsciiUndoBuffer_Destroy(PetsciiUndoBuffer *buf);

/* Push a snapshot of current onto the undo stack.
 * Call this BEFORE making changes to the screen.
 * Discards any pending redo history (branch rule). */
void PetsciiUndoBuffer_Push(PetsciiUndoBuffer *buf,
                             const PetsciiScreen *current);

/* Undo one step: swap the current screen data with the most recent
 * snapshot.  Returns TRUE if an undo was available, FALSE if not. */
BOOL PetsciiUndoBuffer_Undo(PetsciiUndoBuffer *buf, PetsciiScreen *current);

/* Redo one step: restore the most recently undone state.
 * Returns TRUE if a redo was available, FALSE if not. */
BOOL PetsciiUndoBuffer_Redo(PetsciiUndoBuffer *buf, PetsciiScreen *current);

/* Query availability */
#define PetsciiUndoBuffer_CanUndo(buf) ((buf) && (buf)->undos > 0)
#define PetsciiUndoBuffer_CanRedo(buf) ((buf) && (buf)->redos > 0)

#endif /* PETSCII_UNDO_H */
