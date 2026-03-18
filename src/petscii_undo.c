/*
 * petscii_undo - Per-screen circular undo/redo history buffer.
 *
 * Ring layout (all indices are modulo PETSCII_UNDO_LEVELS):
 *   slots[start]                           oldest undo snapshot
 *   slots[start + undos - 1]               newest undo snapshot
 *   slots[start + undos]                   oldest (most recent) redo snapshot
 *   slots[start + undos + redos - 1]       newest (oldest-history) redo snapshot
 *
 * Undo:
 *   undo_slot = (start + undos - 1) % LEVELS   <- newest undo entry
 *   Swap slot <-> current screen
 *   undos-- ; redos++
 *   The swapped slot is now the most recent redo entry at (start + undos).
 *
 * Redo:
 *   redo_slot = (start + undos) % LEVELS        <- most-recently-undone entry
 *   Swap slot <-> current screen
 *   redos-- ; undos++
 *   The swapped slot is now the newest undo entry at (start + undos - 1).
 *
 * Both operations use pointer-swaps (no allocation at undo/redo time).
 */

#include "petscii_undo.h"
#include "petscii_screen.h"
#include <string.h>
#include <proto/exec.h>

/* ------------------------------------------------------------------ */
/* Static helper: swap all data between two PetsciiScreen structs.     */
/* Only valid when both screens have the same dimensions.              */
/* ------------------------------------------------------------------ */

static void swapScreenData(PetsciiScreen *a, PetsciiScreen *b)
{
    PetsciiPixel *tmpBuf;
    UBYTE         tmpByte;
    char          tmpName[PETSCII_NAME_LEN];

    if (!a || !b) return;
    if (a->width != b->width || a->height != b->height) return;

    /* Swap framebuf pointers (no deep copy - just the pointer) */
    tmpBuf      = a->framebuf;
    a->framebuf = b->framebuf;
    b->framebuf = tmpBuf;

    /* Swap metadata fields */
    tmpByte            = a->backgroundColor;
    a->backgroundColor = b->backgroundColor;
    b->backgroundColor = tmpByte;

    tmpByte        = a->borderColor;
    a->borderColor = b->borderColor;
    b->borderColor = tmpByte;

    tmpByte    = a->charset;
    a->charset = b->charset;
    b->charset = tmpByte;

    memcpy(tmpName, a->name, PETSCII_NAME_LEN);
    memcpy(a->name, b->name, PETSCII_NAME_LEN);
    memcpy(b->name, tmpName, PETSCII_NAME_LEN);
}

/* ------------------------------------------------------------------ */
/* PetsciiUndoBuffer_Create                                            */
/* ------------------------------------------------------------------ */

PetsciiUndoBuffer *PetsciiUndoBuffer_Create(void)
{
    PetsciiUndoBuffer *buf;
    int i;

    buf = (PetsciiUndoBuffer *)AllocVec(sizeof(PetsciiUndoBuffer), MEMF_CLEAR);
    if (!buf) return NULL;

    for (i = 0; i < PETSCII_UNDO_LEVELS; i++)
        buf->slots[i] = NULL;

    buf->start = 0;
    buf->undos = 0;
    buf->redos = 0;

    return buf;
}

/* ------------------------------------------------------------------ */
/* PetsciiUndoBuffer_Destroy                                           */
/* ------------------------------------------------------------------ */

void PetsciiUndoBuffer_Destroy(PetsciiUndoBuffer *buf)
{
    int i;

    if (!buf) return;

    for (i = 0; i < PETSCII_UNDO_LEVELS; i++) {
        if (buf->slots[i]) {
            PetsciiScreen_Destroy(buf->slots[i]);
            buf->slots[i] = NULL;
        }
    }

    FreeVec(buf);
}

/* ------------------------------------------------------------------ */
/* PetsciiUndoBuffer_Push                                              */
/* ------------------------------------------------------------------ */

void PetsciiUndoBuffer_Push(PetsciiScreen *current)
{
    UBYTE new_slot;
    UBYTE i;
    PetsciiUndoBuffer *buf;
    if ( !current) return;

    if(!current->undoBuf)
    {
        current->undoBuf = PetsciiUndoBuffer_Create();
    }
    buf = current->undoBuf;
    if(!buf) return;


    /* Discard all redo snapshots (branching: new stroke kills redo history) */
    if (buf->redos > 0) {
        for (i = 0; i < buf->redos; i++) {
            UBYTE idx = (UBYTE)((buf->start + buf->undos + (UBYTE)i)
                                % PETSCII_UNDO_LEVELS);
            if (buf->slots[idx]) {
                PetsciiScreen_Destroy(buf->slots[idx]);
                buf->slots[idx] = NULL;
            }
        }
        buf->redos = 0;
    }

    /* Compute the target slot */
    new_slot = (UBYTE)((buf->start + buf->undos) % PETSCII_UNDO_LEVELS);

    /* If the ring is full, evict the oldest undo to make room */
    if (buf->undos == PETSCII_UNDO_LEVELS) {
        if (buf->slots[buf->start]) {
            PetsciiScreen_Destroy(buf->slots[buf->start]);
            buf->slots[buf->start] = NULL;
        }
        buf->start = (UBYTE)((buf->start + 1) % PETSCII_UNDO_LEVELS);
        buf->undos--;
        new_slot = (UBYTE)((buf->start + buf->undos) % PETSCII_UNDO_LEVELS);
    }

    /* Clone the current screen into the new slot */
    buf->slots[new_slot] = PetsciiScreen_Clone(current);
    if (buf->slots[new_slot])
        buf->undos++;
}

/* ------------------------------------------------------------------ */
/* PetsciiUndoBuffer_Undo                                              */
/* ------------------------------------------------------------------ */

BOOL PetsciiUndoBuffer_Undo( PetsciiScreen *current)
{
    UBYTE undo_slot;
    PetsciiUndoBuffer *buf;
    if ( !current) return FALSE;
    buf = current->undoBuf ;
    if (!buf || buf->undos == 0) return FALSE;

    /* Newest undo snapshot is at (start + undos - 1) */
    undo_slot = (UBYTE)((buf->start + buf->undos - 1) % PETSCII_UNDO_LEVELS);

    /* Swap slot data <-> current screen.
     * After swap: slot holds the pre-undo (current) state -> becomes redo. */
    swapScreenData(current, buf->slots[undo_slot]);

    buf->undos--;
    buf->redos++;

    return TRUE;
}

/* ------------------------------------------------------------------ */
/* PetsciiUndoBuffer_Redo                                              */
/* ------------------------------------------------------------------ */

BOOL PetsciiUndoBuffer_Redo(PetsciiScreen *current)
{
    UBYTE redo_slot;
    PetsciiUndoBuffer *buf;
    if ( !current) return FALSE;
    buf = current->undoBuf ;
    if (!buf || buf->redos == 0) return FALSE;

    /* Most-recently-undone redo snapshot is at (start + undos) */
    redo_slot = (UBYTE)((buf->start + buf->undos) % PETSCII_UNDO_LEVELS);

    /* Swap slot data <-> current screen.
     * After swap: slot holds the pre-redo (current) state -> becomes undo. */
    swapScreenData(current, buf->slots[redo_slot]);

    buf->redos--;
    buf->undos++;

    return TRUE;
}
