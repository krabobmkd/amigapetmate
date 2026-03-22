/*
 * findtables.c - PETSCII character transformation table generator.
 *
 * Computes 5 transformation lookup tables for each of the 2 C64 charsets
 * (upper and lower), giving 10 tables of 256 UBYTE each.
 *
 * For each character index i, table[i] is the index of the character in
 * the same charset that best matches the geometrically transformed glyph.
 * If no character is close enough (error >= TRANSFORM_MAX_ERR), table[i]
 * returns i itself (identity = "no good transform found").
 *
 * Transformations on the 8×8 bitmap (row 0 = top, col 0 = left, MSB = col 0):
 *   FlipX   : horizontal mirror (left ↔ right)
 *   FlipY   : vertical mirror   (top  ↔ bottom)
 *   Rot90   : 90° clockwise
 *   RotN90  : 90° counter-clockwise
 *   Rot180  : 180° rotation
 *
 * Error metric: Hamming distance (number of differing bits, range 0-64).
 * A match is accepted only when error < TRANSFORM_MAX_ERR.
 *
 * Output: petscii_chartransform.c and petscii_chartransform.h, written to
 * the current directory.  Run once, commit the generated files.
 *
 * Compile target: Amiga (same toolchain as petmate).
 * Link with: petscii_charset.c
 */

#include <stdio.h>
#include <string.h>
#include "petscii_charset.h"

/* ------------------------------------------------------------------ */
/* Configuration                                                        */
/* ------------------------------------------------------------------ */

/* Accept a transformed match only if its bit-error is strictly below
 * this value (out of 64 possible bit differences).
 * Lower = stricter.  8 accepts ≤7 differing bits (~11% error rate).  */
#define TRANSFORM_MAX_ERR  8

#define NUM_CHARS   256
#define CHAR_BYTES  8

/* ------------------------------------------------------------------ */
/* Bit-manipulation helpers                                             */
/* ------------------------------------------------------------------ */

static UBYTE reverseBits(UBYTE b)
{
    UBYTE r = 0;
    int   i;
    for (i = 0; i < 8; i++) {
        r = (UBYTE)((r << 1) | (b & 1));
        b >>= 1;
    }
    return r;
}

/*
 * GetPixel: return the pixel at column x (0=left), row y (0=top)
 * in an 8x8 glyph stored MSB-first (bit 7 = col 0).
 */
static int GetPixel(const UBYTE *glyph, int x, int y)
{
    return (glyph[y] >> (7 - x)) & 1;
}

/* legacy alias used by the transform functions */
static int getPixel(const UBYTE *glyph, int row, int col)
{
    return GetPixel(glyph, col, row);
}

/* Print an 8x8 glyph to stdout as rows of '0'/'1' characters. */
static void printGlyph(const char *label, const UBYTE *glyph)
{
    int y, x;
    printf("  %s:\n", label);
    for (y = 0; y < CHAR_BYTES; y++) {
        printf("    ");
        for (x = 0; x < 8; x++)
            printf("%c", GetPixel(glyph, x, y) ? '1' : '0');
        printf("\n");
    }
}

/* ------------------------------------------------------------------ */
/* The 5 geometric transformations                                      */
/* ------------------------------------------------------------------ */

/* FlipX: horizontal mirror. Bit-reverse each row byte. */
static void applyFlipX(const UBYTE *src, UBYTE *dst)
{
    int i;
    for (i = 0; i < CHAR_BYTES; i++)
        dst[i] = reverseBits(src[i]);
}

/* FlipY: vertical mirror. Reverse the row order. */
static void applyFlipY(const UBYTE *src, UBYTE *dst)
{
    int i;
    for (i = 0; i < CHAR_BYTES; i++)
        dst[i] = src[CHAR_BYTES - 1 - i];
}

/* Rot90: 90° clockwise.  result[row][col] = src[7-col][row] */
static void applyRot90(const UBYTE *src, UBYTE *dst)
{
    int   row, col;
    UBYTE b;
    for (row = 0; row < CHAR_BYTES; row++) {
        b = 0;
        for (col = 0; col < 8; col++) {
            if (getPixel(src, 7 - col, row))
                b |= (UBYTE)(1 << (7 - col));
        }
        dst[row] = b;
    }
}

/* RotN90: 90° counter-clockwise.  result[row][col] = src[col][7-row] */
static void applyRotN90(const UBYTE *src, UBYTE *dst)
{
    int   row, col;
    UBYTE b;
    for (row = 0; row < CHAR_BYTES; row++) {
        b = 0;
        for (col = 0; col < 8; col++) {
            if (getPixel(src, col, 7 - row))
                b |= (UBYTE)(1 << (7 - col));
        }
        dst[row] = b;
    }
}

/* Rot180: 180° rotation.  result[row][col] = src[7-row][7-col]
 * Equivalent to FlipX then FlipY (or vice versa). */
static void applyRot180(const UBYTE *src, UBYTE *dst)
{
    int i;
    for (i = 0; i < CHAR_BYTES; i++)
        dst[i] = reverseBits(src[CHAR_BYTES - 1 - i]);
}

/* ------------------------------------------------------------------ */
/* Error and matching                                                   */
/* ------------------------------------------------------------------ */

/* Hamming distance between two 8-byte glyphs (0 = identical, max 64). */
static int glyphError(const UBYTE *a, const UBYTE *b)
{
    int   err = 0;
    int   i;
    UBYTE diff;

    for (i = 0; i < CHAR_BYTES; i++) {
        diff = a[i] ^ b[i];
        /* popcount via loop (C89, no intrinsics needed) */
        while (diff) {
            err += diff & 1;
            diff >>= 1;
        }
    }
    return err;
}

/*
 * Find the charset index whose glyph best matches `transformed`.
 * Returns that index if error < TRANSFORM_MAX_ERR, otherwise selfIdx.
 */
static UBYTE findBestMatch(const UBYTE *transformed,
                            const UBYTE *charset,
                            int          selfIdx)
{
    int bestErr = TRANSFORM_MAX_ERR + 1;  /* accept err <= TRANSFORM_MAX_ERR */
    int bestIdx = selfIdx;            /* default: identity */
    int i;
    int err;

    for (i = 0; i < NUM_CHARS; i++) {
        err = glyphError(transformed, charset + i * CHAR_BYTES);
        if (err < bestErr) {
            bestErr = err;
            bestIdx = i;
        }
    }
    return (UBYTE)bestIdx;
}

/* ------------------------------------------------------------------ */
/* Table computation                                                    */
/* ------------------------------------------------------------------ */

typedef void (*TransformFn)(const UBYTE *src, UBYTE *dst);

static void computeTable(const UBYTE *charset,
                          UBYTE       *table,
                          TransformFn  transform)
{
    UBYTE transformed[CHAR_BYTES];
    int   i;

    for (i = 0; i < NUM_CHARS; i++) {
        transform(charset + i * CHAR_BYTES, transformed);
        table[i] = findBestMatch(transformed, charset, i);
    }
}

/*
 * Same as computeTable but prints each char that maps to itself
 * (no match found): shows original and the transformed glyph we searched for.
 */
static void computeTableVerbose(const UBYTE *charset,
                                 UBYTE       *table,
                                 TransformFn  transform,
                                 const char  *transformName,
                                 const char  *charsetName)
{
    UBYTE transformed[CHAR_BYTES];
    int   i;
    int   missCount = 0;

    for (i = 0; i < NUM_CHARS; i++) {
        transform(charset + i * CHAR_BYTES, transformed);
        table[i] = findBestMatch(transformed, charset, i);

        if (table[i] == (UBYTE)i) {
            /* No match found: print original and what we searched for */
            printf("  [%s/%s] char %3d -> IDENTITY (no match below threshold)\n",
                   charsetName, transformName, i);
            printGlyph("original ", charset + i * CHAR_BYTES);
            printGlyph("want match", transformed);
            missCount++;
        }
    }
    if (missCount == 0)
        printf("  [%s/%s] all chars matched.\n", charsetName, transformName);
}

/* ------------------------------------------------------------------ */
/* Output helpers                                                       */
/* ------------------------------------------------------------------ */

static const char *tableNames[2][5] = {
    {
        "petsciiUpperFlipX",
        "petsciiUpperFlipY",
        "petsciiUpperRot90",
        "petsciiUpperRotN90",
        "petsciiUpperRot180"
    },
    {
        "petsciiLowerFlipX",
        "petsciiLowerFlipY",
        "petsciiLowerRot90",
        "petsciiLowerRotN90",
        "petsciiLowerRot180"
    }
};

static void writeHFile(FILE *f)
{
    int cs, t;

    fprintf(f, "/* Auto-generated by findtables.c — do not edit by hand. */\n");
    fprintf(f, "#ifndef PETSCII_CHARTRANSFORM_H\n");
    fprintf(f, "#define PETSCII_CHARTRANSFORM_H\n\n");
    fprintf(f, "#include <exec/types.h>\n\n");
    fprintf(f, "/*\n");
    fprintf(f, " * Character transformation tables.\n");
    fprintf(f, " * table[charIndex] = index of the char that is the\n");
    fprintf(f, " * transformation of charIndex in the same charset.\n");
    fprintf(f, " * Returns charIndex itself when no good match was found.\n");
    fprintf(f, " */\n\n");

    for (cs = 0; cs < 2; cs++) {
        for (t = 0; t < 5; t++) {
            fprintf(f, "extern const UBYTE %s[256];\n", tableNames[cs][t]);
        }
        fprintf(f, "\n");
    }

    fprintf(f, "#endif /* PETSCII_CHARTRANSFORM_H */\n");
}

static void writeTable(FILE *f, const char *name, const UBYTE *table)
{
    int i;

    fprintf(f, "const UBYTE %s[256] = {\n", name);
    for (i = 0; i < NUM_CHARS; i++) {
        if (i % 16 == 0)
            fprintf(f, "    ");
        fprintf(f, "%3u", (unsigned)table[i]);
        if (i < NUM_CHARS - 1)
            fprintf(f, ",");
        if (i % 16 == 15)
            fprintf(f, "  /* %3d-%3d */\n", i - 15, i);
        else
            fprintf(f, " ");
    }
    fprintf(f, "};\n\n");
}

static void writeCFile(FILE *f, UBYTE tables[2][5][NUM_CHARS])
{
    int cs, t;

    fprintf(f, "/* Auto-generated by findtables.c — do not edit by hand. */\n");
    fprintf(f, "#include \"petscii_chartransform.h\"\n\n");

    for (cs = 0; cs < 2; cs++) {
        for (t = 0; t < 5; t++) {
            writeTable(f, tableNames[cs][t], tables[cs][t]);
        }
    }
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

static TransformFn transformFns[5] = {
    applyFlipX,
    applyFlipY,
    applyRot90,
    applyRotN90,
    applyRot180
};

static const char *transformDesc[5] = {
    "FlipX  (horizontal mirror)",
    "FlipY  (vertical mirror)",
    "Rot90  (90 deg clockwise)",
    "RotN90 (90 deg counter-clockwise)",
    "Rot180 (180 deg rotation)"
};

static const char *charsetName[2] = { "Upper", "Lower" };

int main(void)
{
    UBYTE       tables[2][5][NUM_CHARS];
    const UBYTE *charsets[2];
    FILE        *f;
    int          cs, t, i;
    int          identityCount, totalChanged;

    charsets[0] = c64CharsetUpper;
    charsets[1] = c64CharsetLower;

    /* ---- Compute all 10 tables ------------------------------------ */
    printf("findtables: computing transformation tables (threshold=%d bits)\n",
           TRANSFORM_MAX_ERR);

    for (cs = 0; cs < 2; cs++) {
        for (t = 0; t < 5; t++) {
            printf("\n--- %s / %s ---\n", charsetName[cs], transformDesc[t]);
            computeTableVerbose(charsets[cs], tables[cs][t],
                                transformFns[t],
                                transformDesc[t], charsetName[cs]);

            /* Summary line */
            identityCount = 0;
            for (i = 0; i < NUM_CHARS; i++) {
                if (tables[cs][t][i] == (UBYTE)i)
                    identityCount++;
            }
            totalChanged = NUM_CHARS - identityCount;

            printf("  %s / %s : %d mapped, %d identity\n",
                   charsetName[cs], transformDesc[t],
                   totalChanged, identityCount);
        }
    }

    /* ---- Write petscii_chartransform.h ---------------------------- */
    f = fopen("petscii_chartransform.h", "w");
    if (!f) {
        printf("ERROR: cannot open petscii_chartransform.h for writing\n");
        return 1;
    }
    writeHFile(f);
    fclose(f);
    printf("wrote petscii_chartransform.h\n");

    /* ---- Write petscii_chartransform.c ---------------------------- */
    f = fopen("petscii_chartransform.c", "w");
    if (!f) {
        printf("ERROR: cannot open petscii_chartransform.c for writing\n");
        return 1;
    }
    writeCFile(f, tables);
    fclose(f);
    printf("wrote petscii_chartransform.c\n");

    printf("done.\n");
    return 0;
}
