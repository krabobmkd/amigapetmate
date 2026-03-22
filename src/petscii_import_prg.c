/*
 * petscii_import_prg.c - Import a C64 ASM-export PRG into a PETSCII screen.
 *
 * The PRG format produced by our ASM exporter is:
 *
 *   Bytes 0-1   : PRG load address (little-endian), typically $0801.
 *   Bytes 2-..  : 6510 machine code.  Key patterns inside:
 *
 *     LDA abs  ($AD lo hi)  STA $D020 ($8D 20 D0)  <- loads border color
 *     LDA abs  ($AD lo hi)  STA $D021 ($8D 21 D0)  <- loads bg color
 *     LDA #imm ($A9 val)    STA $D018 ($8D 18 D0)  <- sets charset
 *     LDA abs,X ($BD ...)                            <- copies screen/color
 *
 *   After the machine code, a data block of 2002 bytes:
 *     [0]       border color  (0-15)
 *     [1]       background color  (0-15)
 *     [2..1001] screen codes, row-major, 40x25
 *     [1002..2001] color codes, row-major, 40x25
 *
 * Detection strategy (robust across load addresses and minor code changes):
 *   1. Scan for the exact 6-byte pattern
 *        AD <lo> <hi> 8D 20 D0
 *      (LDA abs immediately followed by STA $D020).
 *      The abs address <hi>:<lo> is the C64 address of the data block start.
 *   2. Compute file offset:  fileOff = (dataC64Addr - loadAddr) + 2
 *   3. Optionally scan for  A9 <val> 8D 18 D0  to detect charset.
 *   4. Validate the file has at least fileOff + 2002 bytes, then parse.
 *
 * C89 compatible.  Uses AllocVec/FreeVec for the file buffer.
 */

#include "petscii_import_prg.h"
#include "petscii_types.h"
#include "petscii_screen.h"

#include <stdio.h>
#include <string.h>
#include <proto/exec.h>

/* 40 * 25 cells */
#define CELLS 1000

/* 6510 opcodes */
#define OP_LDA_ABS  0xAD
#define OP_LDA_IMM  0xA9
#define OP_STA_ABS  0x8D

/* C64 I/O register addresses */
#define C64_D018  0xD018u
#define C64_D020  0xD020u

/* $D018 charset values */
#define D018_UPPER  0x15u
#define D018_LOWER  0x17u

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static UWORD readLE16(const UBYTE *buf, ULONG pos)
{
    return (UWORD)((UWORD)buf[pos] | ((UWORD)buf[pos + 1] << 8));
}

/*
 * Scan the full file buffer for:
 *   LDA abs ($AD lo hi)  immediately followed by  STA $D020 ($8D 20 D0)
 *
 * Returns the C64 address of the data block (the abs operand of the LDA),
 * or 0 if the pattern is not found.
 *
 * We require the abs address to be >= $0200 so we don't confuse zero-page
 * or stack accesses with the data block pointer.
 */
static UWORD findDataC64Addr(const UBYTE *buf, ULONG len)
{
    ULONG i;

    /* Need at least 6 bytes for the full pattern */
    if (len < 6) return 0;

    for (i = 0; i + 5 < len; i++) {
        if (buf[i]     != OP_LDA_ABS)              continue;
        if (buf[i + 3] != OP_STA_ABS)              continue;
        if (buf[i + 4] != (UBYTE)(C64_D020 & 0xFF)) continue;
        if (buf[i + 5] != (UBYTE)(C64_D020 >> 8))   continue;

        {
            UWORD addr = readLE16(buf, i + 1);
            if (addr >= 0x0200u) return addr;
        }
    }
    return 0;
}

/*
 * Scan the full file buffer for:
 *   LDA #imm ($A9 val)  immediately followed by  STA $D018 ($8D 18 D0)
 *
 * Returns the immediate operand byte, or 0xFF if not found.
 */
static UBYTE findCharsetImm(const UBYTE *buf, ULONG len)
{
    ULONG i;

    if (len < 5) return 0xFF;

    for (i = 0; i + 4 < len; i++) {
        if (buf[i]     != OP_LDA_IMM)              continue;
        if (buf[i + 2] != OP_STA_ABS)              continue;
        if (buf[i + 3] != (UBYTE)(C64_D018 & 0xFF)) continue;
        if (buf[i + 4] != (UBYTE)(C64_D018 >> 8))   continue;
        return buf[i + 1];
    }
    return 0xFF;
}

/* ------------------------------------------------------------------ */
/* Public entry point                                                  */
/* ------------------------------------------------------------------ */

int PetsciiImport_FromPrg(const char *path, PetsciiScreen *scr)
{
    FILE  *fp;
    ULONG  fileLen;
    UBYTE *buf;
    ULONG  loadAddr;
    UWORD  dataC64;
    ULONG  dataOff;
    UBYTE  charsetImm;
    int    i, x, y;

    if (!path || !scr) return PETSCII_IMPORT_PRG_EOPEN;

    /* --- Read entire file into memory ----------------------------------- */
    fp = fopen(path, "rb");
    if (!fp) return PETSCII_IMPORT_PRG_EOPEN;

    fseek(fp, 0, SEEK_END);
    fileLen = (ULONG)ftell(fp);
    fseek(fp, 0, SEEK_SET);

    /* Sanity: load address (2) + at least some code + 2002 data bytes */
    if (fileLen < (ULONG)(2 + 4 + 2 + CELLS + CELLS)) {
        fclose(fp);
        return PETSCII_IMPORT_PRG_ESIZE;
    }

    buf = (UBYTE *)AllocVec(fileLen, MEMF_ANY);
    if (!buf) {
        fclose(fp);
        return PETSCII_IMPORT_PRG_ENOMEM;
    }

    if (fread(buf, 1, (size_t)fileLen, fp) != (size_t)fileLen) {
        fclose(fp);
        FreeVec(buf);
        return PETSCII_IMPORT_PRG_EOPEN;
    }
    fclose(fp);

    /* --- Parse PRG load address (bytes 0-1, little-endian) -------------- */
    loadAddr = (ULONG)readLE16(buf, 0);

    /* --- Locate the data block via LDA abs -> STA $D020 pattern --------- */
    dataC64 = findDataC64Addr(buf, fileLen);
    if (dataC64 == 0) {
        FreeVec(buf);
        return PETSCII_IMPORT_PRG_EFORMAT;
    }

    if ((ULONG)dataC64 < loadAddr) {
        /* Data address is below load address — can't convert to file offset */
        FreeVec(buf);
        return PETSCII_IMPORT_PRG_EFORMAT;
    }

    /* PRG files have a 2-byte header before the first loaded byte */
    dataOff = ((ULONG)dataC64 - loadAddr) + 2UL;

    if (dataOff + 2UL + (ULONG)CELLS + (ULONG)CELLS > fileLen) {
        FreeVec(buf);
        return PETSCII_IMPORT_PRG_ESIZE;
    }

    /* --- Detect charset from LDA #imm -> STA $D018 ---------------------- */
    charsetImm = findCharsetImm(buf, fileLen);
    if (charsetImm == (UBYTE)D018_LOWER)
        scr->charset = PETSCII_CHARSET_LOWER;
    else
        scr->charset = PETSCII_CHARSET_UPPER;   /* $15 or unknown: default upper */

    /* --- Fill the screen framebuffer ------------------------------------ */
    scr->borderColor     = buf[dataOff];
    scr->backgroundColor = buf[dataOff + 1];

    for (y = 0; y < 25; y++) {
        for (x = 0; x < 40; x++) {
            i = y * 40 + x;
            scr->framebuf[i].code  = buf[dataOff + 2 + i];
            scr->framebuf[i].color = buf[dataOff + 2 + CELLS + i] & 0x0F;
        }
    }

    FreeVec(buf);
    return PETSCII_IMPORT_PRG_OK;
}
