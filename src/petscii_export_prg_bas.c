/*
 * petscii_export_prg_bas.c - Export PETSCII screen as a C64 BASIC PRG.
 *
 * Produces a binary PRG file (load address $0801) containing a tokenized
 * C64 BASIC 2.0 program equivalent to what petscii_export_bas.c writes as
 * a text source.  The program can be RUN directly on a real C64 or emulator.
 *
 * The generated BASIC program:
 *
 *   10  REM CREATED WITH PETMATE
 *   20  POKE 53280,<border>
 *   30  POKE 53281,<bgcolor>
 *   40  POKE 53272,<charsetbits>   (21=upper / 23=lower)
 *   100 FOR I=1024 TO <1024+cells-1>
 *   110 READ A:POKE I,A:NEXT I
 *   120 FOR I=55296 TO <55296+cells-1>
 *   130 READ A:POKE I,A:NEXT I
 *   140 GOTO 140
 *   200 DATA <screen codes, 16 per line>
 *   ...
 *   <n> DATA <color values, 16 per line>
 *   ...
 *
 * C64 BASIC 2.0 tokenized-line format:
 *   [2 bytes: next-line ptr, little-endian, C64 RAM address]
 *   [2 bytes: line number, little-endian]
 *   [tokenized bytes: keywords -> single token byte, rest -> raw PETSCII]
 *   [$00: end of line]
 * End of program: $00 $00
 *
 * Numbers in the tokenized stream are stored as ASCII digit strings ($30-$39).
 * Relevant tokens: $81=FOR $82=NEXT $83=DATA $87=READ $89=GOTO
 *                  $8F=REM $97=POKE $A4=TO $B2==
 */

#include <stdio.h>
#include <stdlib.h>
#include "petscii_export.h"
#include "petscii_types.h"
#include "petscii_screen.h"

/* C64 BASIC 2.0 tokens used in this program */
#define TOK_FOR  0x81
#define TOK_NEXT 0x82
#define TOK_DATA 0x83
#define TOK_READ 0x87
#define TOK_GOTO 0x89
#define TOK_REM  0x8F
#define TOK_POKE 0x97
#define TOK_TO   0xA4
#define TOK_EQ   0xB2   /* = operator */

/* C64 BASIC always loads at this address */
#define C64_BASIC_START 0x0801U

/*
 * Buffer size for the tokenized BASIC program.
 * 2000 data values at 16 per DATA line = 125 DATA lines.
 * Worst case per DATA line: 5 overhead + 1 token + 16*3 digits + 15 commas = 69 bytes.
 * 125 * 69 = 8625 bytes for DATA + ~300 bytes for the init lines.
 * 12 KB is ample.
 */
#define BAS_PRG_BUFSIZE 12288

/* -----------------------------------------------------------------------
 * Helpers to build tokenized BASIC into a byte buffer.
 * ----------------------------------------------------------------------- */

static void bb_putbyte(unsigned char *buf, int *pos, unsigned char b)
{
    buf[(*pos)++] = b;
}

/* Write an unsigned integer as ASCII decimal digits. */
static void bb_putuint(unsigned char *buf, int *pos, unsigned int v)
{
    char  tmp[6]; /* max 5 digits for 65535 */
    int   len, i;

    if (v == 0) { bb_putbyte(buf, pos, '0'); return; }
    len = 0;
    while (v) { tmp[len++] = (char)('0' + v % 10); v /= 10; }
    for (i = len - 1; i >= 0; i--)
        bb_putbyte(buf, pos, (unsigned char)tmp[i]);
}

/*
 * Begin a BASIC line: reserve 2 bytes for the next-line pointer (filled by
 * bb_endline), then write the 2-byte line number.
 * Returns the buffer offset of the reserved pointer field.
 */
static int bb_beginline(unsigned char *buf, int *pos, unsigned int linenum)
{
    int off = *pos;
    bb_putbyte(buf, pos, 0x00); /* next-line ptr lo - patched later */
    bb_putbyte(buf, pos, 0x00); /* next-line ptr hi - patched later */
    bb_putbyte(buf, pos, (unsigned char)(linenum & 0xFF));
    bb_putbyte(buf, pos, (unsigned char)((linenum >> 8) & 0xFF));
    return off;
}

/*
 * End a BASIC line: write $00 terminator, then patch the next-line pointer
 * that was reserved by bb_beginline to point to the next byte in C64 RAM.
 */
static void bb_endline(unsigned char *buf, int *pos, int ptr_off)
{
    unsigned int next_addr;
    bb_putbyte(buf, pos, 0x00);
    next_addr = C64_BASIC_START + (unsigned int)*pos;
    buf[ptr_off]     = (unsigned char)(next_addr & 0xFF);
    buf[ptr_off + 1] = (unsigned char)((next_addr >> 8) & 0xFF);
}

/* ----------------------------------------------------------------------- */

int PetsciiExport_SavePrgBAS(const PetsciiScreen *scr, const char *path)
{
    unsigned char *buf;
    FILE          *fp;
    int            pos;
    int            ptr_off;
    int            i;
    int            totalCells;
    int            totalValues;
    int            linenum;
    unsigned char  charsetBits;
    unsigned char  prg_hdr[2];
    const char    *rem_text;
    int            j;

    if (!scr || !path) return PETSCII_EXPORT_EOPEN;

    buf = (unsigned char *)malloc(BAS_PRG_BUFSIZE);
    if (!buf) return PETSCII_EXPORT_ENOMEM;

    pos         = 0;
    totalCells  = (int)scr->width * (int)scr->height;
    totalValues = totalCells * 2; /* screen codes + color values */
    charsetBits = (scr->charset == PETSCII_CHARSET_LOWER) ? 23 : 21;
    linenum     = 200;

    /* -- line 10: REM CREATED WITH PETMATE -- */
    rem_text = " CREATED WITH PETMATE";
    ptr_off = bb_beginline(buf, &pos, 10);
    bb_putbyte(buf, &pos, TOK_REM);
    for (j = 0; rem_text[j]; j++)
        bb_putbyte(buf, &pos, (unsigned char)rem_text[j]);
    bb_endline(buf, &pos, ptr_off);

    /* -- line 20: POKE 53280,<border> -- */
    ptr_off = bb_beginline(buf, &pos, 20);
    bb_putbyte(buf, &pos, TOK_POKE);
    bb_putuint(buf, &pos, 53280);
    bb_putbyte(buf, &pos, ',');
    bb_putuint(buf, &pos, (unsigned int)scr->borderColor);
    bb_endline(buf, &pos, ptr_off);

    /* -- line 30: POKE 53281,<bgcolor> -- */
    ptr_off = bb_beginline(buf, &pos, 30);
    bb_putbyte(buf, &pos, TOK_POKE);
    bb_putuint(buf, &pos, 53281);
    bb_putbyte(buf, &pos, ',');
    bb_putuint(buf, &pos, (unsigned int)scr->backgroundColor);
    bb_endline(buf, &pos, ptr_off);

    /* -- line 40: POKE 53272,<charsetbits> -- */
    ptr_off = bb_beginline(buf, &pos, 40);
    bb_putbyte(buf, &pos, TOK_POKE);
    bb_putuint(buf, &pos, 53272);
    bb_putbyte(buf, &pos, ',');
    bb_putuint(buf, &pos, (unsigned int)charsetBits);
    bb_endline(buf, &pos, ptr_off);

    /* -- line 100: FOR I=1024 TO <1024+cells-1> -- */
    ptr_off = bb_beginline(buf, &pos, 100);
    bb_putbyte(buf, &pos, TOK_FOR);
    bb_putbyte(buf, &pos, 'I');
    bb_putbyte(buf, &pos, TOK_EQ);
    bb_putuint(buf, &pos, 1024);
    bb_putbyte(buf, &pos, TOK_TO);
    bb_putuint(buf, &pos, (unsigned int)(1024 + totalCells - 1));
    bb_endline(buf, &pos, ptr_off);

    /* -- line 110: READ A:POKE I,A:NEXT I -- */
    ptr_off = bb_beginline(buf, &pos, 110);
    bb_putbyte(buf, &pos, TOK_READ);
    bb_putbyte(buf, &pos, 'A');
    bb_putbyte(buf, &pos, ':');
    bb_putbyte(buf, &pos, TOK_POKE);
    bb_putbyte(buf, &pos, 'I');
    bb_putbyte(buf, &pos, ',');
    bb_putbyte(buf, &pos, 'A');
    bb_putbyte(buf, &pos, ':');
    bb_putbyte(buf, &pos, TOK_NEXT);
    bb_putbyte(buf, &pos, 'I');
    bb_endline(buf, &pos, ptr_off);

    /* -- line 120: FOR I=55296 TO <55296+cells-1> -- */
    ptr_off = bb_beginline(buf, &pos, 120);
    bb_putbyte(buf, &pos, TOK_FOR);
    bb_putbyte(buf, &pos, 'I');
    bb_putbyte(buf, &pos, TOK_EQ);
    bb_putuint(buf, &pos, 55296);
    bb_putbyte(buf, &pos, TOK_TO);
    bb_putuint(buf, &pos, (unsigned int)(55296 + totalCells - 1));
    bb_endline(buf, &pos, ptr_off);

    /* -- line 130: READ A:POKE I,A:NEXT I -- */
    ptr_off = bb_beginline(buf, &pos, 130);
    bb_putbyte(buf, &pos, TOK_READ);
    bb_putbyte(buf, &pos, 'A');
    bb_putbyte(buf, &pos, ':');
    bb_putbyte(buf, &pos, TOK_POKE);
    bb_putbyte(buf, &pos, 'I');
    bb_putbyte(buf, &pos, ',');
    bb_putbyte(buf, &pos, 'A');
    bb_putbyte(buf, &pos, ':');
    bb_putbyte(buf, &pos, TOK_NEXT);
    bb_putbyte(buf, &pos, 'I');
    bb_endline(buf, &pos, ptr_off);

    /* -- line 140: GOTO 140 -- */
    ptr_off = bb_beginline(buf, &pos, 140);
    bb_putbyte(buf, &pos, TOK_GOTO);
    bb_putuint(buf, &pos, 140);
    bb_endline(buf, &pos, ptr_off);

    /*
     * DATA lines: all screen codes first (framebuf[0..cells-1].code),
     * then all color values (framebuf[0..cells-1].color), 16 values per line.
     * framebuf is row-major so flat indexing matches row-by-row order.
     */
    ptr_off = -1;
    for (i = 0; i < totalValues; i++) {
        UBYTE val;

        val = (i < totalCells)
            ? scr->framebuf[i].code
            : (scr->framebuf[i - totalCells].color & 0x0F);

        if (i % 16 == 0) {
            if (i > 0)
                bb_endline(buf, &pos, ptr_off);
            ptr_off = bb_beginline(buf, &pos, (unsigned int)linenum++);
            bb_putbyte(buf, &pos, TOK_DATA);
        } else {
            bb_putbyte(buf, &pos, ',');
        }
        bb_putuint(buf, &pos, (unsigned int)val);
    }
    if (totalValues > 0)
        bb_endline(buf, &pos, ptr_off);

    /* end of BASIC program */
    bb_putbyte(buf, &pos, 0x00);
    bb_putbyte(buf, &pos, 0x00);

    /* write PRG file: 2-byte load address then the tokenized BASIC */
    fp = fopen(path, "wb");
    if (!fp) { free(buf); return PETSCII_EXPORT_EOPEN; }

    prg_hdr[0] = 0x01;
    prg_hdr[1] = 0x08;
    if (fwrite(prg_hdr, 1, 2, fp) != 2 ||
        fwrite(buf, 1, (size_t)pos, fp) != (size_t)pos) {
        fclose(fp);
        free(buf);
        return PETSCII_EXPORT_EWRITE;
    }

    fclose(fp);
    free(buf);
    return PETSCII_EXPORT_OK;
}
