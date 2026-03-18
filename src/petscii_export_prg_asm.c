/*
 * petscii_export_prg_asm.c - Export PETSCII screen as a standalone C64 PRG.
 *
 * Generates a self-contained binary that, when loaded on a real C64 or
 * emulator, sets colors and copies the screen/color data into RAM, then
 * loops forever.
 *
 * The machine code is the direct binary encoding of asm6510exportedimage.asm:
 *
 *   * = $0801
 *   ; BASIC stub: line 0 - SYS 2061  (jumps to $080D)
 *   .byte $0C,$08,$00,$00,$9E,$32,$30,$36,$31,$00,$00,$00
 *
 *   start:             ; $080D
 *     lda screen_001   ; border -> $D020
 *     sta $D020
 *     lda screen_001+1 ; bg    -> $D021
 *     sta $D021
 *     lda #$15         ; charset ($15=upper / $17=lower) -> $D018
 *     sta $D018
 *     ldx #$00
 *   loop:              ; $0820  (256 iterations, X wraps 0->FF->0)
 *     lda screen_001+$002,x  sta $0400,x  ; screen page 0
 *     lda screen_001+$3EA,x  sta $D800,x  ; color  page 0
 *     lda screen_001+$102,x  sta $0500,x  ; screen page 1
 *     lda screen_001+$4EA,x  sta $D900,x  ; color  page 1
 *     lda screen_001+$202,x  sta $0600,x  ; screen page 2
 *     lda screen_001+$5EA,x  sta $DA00,x  ; color  page 2
 *     lda screen_001+$2EA,x  sta $06E8,x  ; screen page 3 (last 256)
 *     lda screen_001+$6D2,x  sta $DAE8,x  ; color  page 3
 *     inx
 *     bne loop
 *     jmp *
 *
 *   screen_001:        ; $0856
 *     .byte borderColor, backgroundColor
 *     .byte <1000 screen codes, row-major>
 *     .byte <1000 color  codes, row-major>
 *
 * Only standard 40x25 screens are supported.
 */

#include <stdio.h>
#include "petscii_export.h"
#include "petscii_types.h"
#include "petscii_screen.h"

/*
 * Part 1 of the binary header: PRG load address + BASIC stub + machine code
 * up to and including the LDA-immediate opcode ($A9) for the charset register.
 * screen_001 is at $0856.
 * Bytes 0-1:   PRG load address $0801 (little-endian)
 * Bytes 2-13:  BASIC stub at $0801 (12 bytes)
 * Bytes 14-26: machine code $080D .. $0819 (13 bytes, ends at $A9 opcode)
 */
static const unsigned char s_prg_part1[] = {
    /* PRG load address: $0801 */
    0x01, 0x08,

    /* BASIC stub at $0801 (12 bytes): line 0 - SYS 2061
     * $0801: $0C $08  next-line ptr -> $080C
     * $0803: $00 $00  line number 0
     * $0805: $9E      SYS token
     * $0806: "2061"   jump target in ASCII ($080D = 2061 decimal)
     * $080A: $00      end of line
     * $080B: $00 $00  end of BASIC (null next-line ptr) */
    0x0C, 0x08,
    0x00, 0x00,
    0x9E,
    0x32, 0x30, 0x36, 0x31,
    0x00,
    0x00, 0x00,

    /* $080D: lda $0856  (screen_001: border color) */
    0xAD, 0x56, 0x08,
    /* $0810: sta $D020  (border color register) */
    0x8D, 0x20, 0xD0,
    /* $0813: lda $0857  (screen_001+1: background color) */
    0xAD, 0x57, 0x08,
    /* $0816: sta $D021  (background color register) */
    0x8D, 0x21, 0xD0,
    /* $0819: lda #??    charset register value -- byte follows dynamically */
    0xA9
};

/*
 * Part 2: remainder of machine code after the charset immediate byte.
 * Starts at $081B (sta $D018) and ends with jmp * at $0853.
 */
static const unsigned char s_prg_part2[] = {
    /* $081B: sta $D018  (charset / memory map register) */
    0x8D, 0x18, 0xD0,
    /* $081E: ldx #$00 */
    0xA2, 0x00,

    /* loop: $0820 */

    /* $0820: lda $0858,x  (screen_001+$002,x: screen codes, page 0) */
    0xBD, 0x58, 0x08,
    /* $0823: sta $0400,x */
    0x9D, 0x00, 0x04,
    /* $0826: lda $0C40,x  (screen_001+$3EA,x: color codes, page 0) */
    0xBD, 0x40, 0x0C,
    /* $0829: sta $D800,x */
    0x9D, 0x00, 0xD8,

    /* $082C: lda $0958,x  (screen_001+$102,x: screen codes, page 1) */
    0xBD, 0x58, 0x09,
    /* $082F: sta $0500,x */
    0x9D, 0x00, 0x05,
    /* $0832: lda $0D40,x  (screen_001+$4EA,x: color codes, page 1) */
    0xBD, 0x40, 0x0D,
    /* $0835: sta $D900,x */
    0x9D, 0x00, 0xD9,

    /* $0838: lda $0A58,x  (screen_001+$202,x: screen codes, page 2) */
    0xBD, 0x58, 0x0A,
    /* $083B: sta $0600,x */
    0x9D, 0x00, 0x06,
    /* $083E: lda $0E40,x  (screen_001+$5EA,x: color codes, page 2) */
    0xBD, 0x40, 0x0E,
    /* $0841: sta $DA00,x */
    0x9D, 0x00, 0xDA,

    /* $0844: lda $0B40,x  (screen_001+$2EA,x: screen codes, page 3) */
    0xBD, 0x40, 0x0B,
    /* $0847: sta $06E8,x */
    0x9D, 0xE8, 0x06,
    /* $084A: lda $0F28,x  (screen_001+$6D2,x: color codes, page 3) */
    0xBD, 0x28, 0x0F,
    /* $084D: sta $DAE8,x */
    0x9D, 0xE8, 0xDA,

    /* $0850: inx */
    0xE8,
    /* $0851: bne loop ($0820)
     * offset = $0820 - $0853 = -$33 -> two's complement = $CD */
    0xD0, 0xCD,
    /* $0853: jmp * (jmp $0853 -> infinite loop) */
    0x4C, 0x53, 0x08
};

int PetsciiExport_SavePrgASM(const PetsciiScreen *scr, const char *path)
{
    FILE         *fp;
    int           x, y;
    unsigned char charsetBits;
    unsigned char b;

    if (!scr || !path) return PETSCII_EXPORT_EOPEN;
    /* The 6510 loader is hardwired for the standard 40x25 C64 screen */
    if (scr->width != 40 || scr->height != 25) return PETSCII_EXPORT_EOPEN;

    fp = fopen(path, "wb");
    if (!fp) return PETSCII_EXPORT_EOPEN;

    /* $D018 value: $15 = uppercase charset, $17 = lowercase */
    charsetBits = (scr->charset == PETSCII_CHARSET_LOWER) ? 0x17 : 0x15;

    /* --- binary header --- */
    if (fwrite(s_prg_part1, 1, sizeof(s_prg_part1), fp) != sizeof(s_prg_part1)) {
        fclose(fp); return PETSCII_EXPORT_EWRITE;
    }
    if (fwrite(&charsetBits, 1, 1, fp) != 1) {
        fclose(fp); return PETSCII_EXPORT_EWRITE;
    }
    if (fwrite(s_prg_part2, 1, sizeof(s_prg_part2), fp) != sizeof(s_prg_part2)) {
        fclose(fp); return PETSCII_EXPORT_EWRITE;
    }

    /* --- screen_001 data at $0856 --- */

    /* byte 0: border color, byte 1: background color */
    b = scr->borderColor;
    if (fwrite(&b, 1, 1, fp) != 1) { fclose(fp); return PETSCII_EXPORT_EWRITE; }
    b = scr->backgroundColor;
    if (fwrite(&b, 1, 1, fp) != 1) { fclose(fp); return PETSCII_EXPORT_EWRITE; }

    /* bytes 2..1001: screen codes, row-major */
    for (y = 0; y < 25; y++) {
        for (x = 0; x < 40; x++) {
            b = scr->framebuf[y * scr->width + x].code;
            if (fwrite(&b, 1, 1, fp) != 1) { fclose(fp); return PETSCII_EXPORT_EWRITE; }
        }
    }

    /* bytes 1002..2001: color codes, row-major */
    for (y = 0; y < 25; y++) {
        for (x = 0; x < 40; x++) {
            b = scr->framebuf[y * scr->width + x].color & 0x0F;
            if (fwrite(&b, 1, 1, fp) != 1) { fclose(fp); return PETSCII_EXPORT_EWRITE; }
        }
    }

    fclose(fp);
    return PETSCII_EXPORT_OK;
}
