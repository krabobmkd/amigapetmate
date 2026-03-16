#ifndef PETSCII_CELLPX_H
#define PETSCII_CELLPX_H

/*
 * petscii_cellpx.h - Fast 32/16 signed projection for PETSCII cell coordinates.
 *
 * CellPx(n, cDim, pDim) computes:
 *
 *   result = (LONG)(n) * 8 * (LONG)(cDim) / (WORD)(pDim)
 *
 * mapping a cell-edge index n (0..screenW or 0..screenH) to a content-relative
 * pixel coordinate that aligns with PetsciiScreenBuf_BlitScaled's mapping.
 *
 * WHY THIS FILE EXISTS
 * --------------------
 * The 68000 CPU has a native signed 32-by-16 divide instruction:
 *
 *   divs.w  Dn, Dm
 *
 * which divides the 32-bit signed integer in Dm by the 16-bit signed integer
 * in Dn and stores the 16-bit quotient in the low word of Dm (remainder in
 * the high word).  This is fast (typically ~90-140 cycles on 68000, versus
 * a > 200 cycle math-library call for a 32/32 divide).
 *
 * ANSI C integer promotion rules require that both operands of "/" are
 * promoted to the same type.  Because the dividend is LONG, the divisor
 * is also promoted to LONG, and every C compiler generates a 32/32 divide.
 * The 68000 has no 32/32 divide instruction; the compiler therefore calls a
 * math-library function (e.g. __DIVSLL in amiga.lib / libm).  Starting with
 * the 68020, divs.l handles 32/32 natively so the issue disappears there.
 *
 * COMPILER SELECTION
 * ------------------
 *
 * 1. GCC 2.x ("Amiga GCC 2.95.x") or GCC 6.x ("Bebbo") targeting -m68000
 *    (preprocessor: defined(__GNUC__) && defined(__mc68000__) &&
 *                   !defined(__mc68020__))
 *
 *    Uses extended inline-asm __asm__().  Both GCC versions share the same
 *    68k extended-asm dialect.  The constraint "+d"(num) allocates the
 *    32-bit dividend in a data register (read+write); "d"(pDim) allocates
 *    the 16-bit divisor in a data register.
 *
 *    After  divs.w Dn, Dm:
 *      Dm[15:0]  = quotient   <- the WORD we return
 *      Dm[31:16] = remainder  <- discarded by the (WORD) cast
 *
 * 2. SAS/C 6.x (preprocessor: defined(__SASC))
 *
 *    Uses SAS/C-specific storage-class qualifiers:
 *      register __d0  binds a LONG local to data register D0 (dividend)
 *      register __d1  binds a WORD local to data register D1 (divisor)
 *    followed by a #asm / #endasm block that inserts "divs.w d1,d0" verbatim.
 *    The compiler guarantees the register bindings hold at the asm point.
 *
 * 3. All other compilers / mc68020+ / non-Amiga (fallback)
 *
 *    Plain C.  On 68020+ divs.l is a native instruction the compiler uses
 *    directly; on hosted builds any decent compiler optimises this well.
 *
 * ARGUMENTS
 * ---------
 *   n    : cell-edge index (LONG, signed; negative for brush hot-points)
 *   cDim : content dimension in pixels (LONG, positive: gadget W or H)
 *   pDim : source pixel dimension (WORD, e.g. 320 or 200; must be != 0)
 *
 * RETURNS
 *   WORD : signed pixel offset relative to the content rectangle origin
 *
 * COMPATIBILITY MACRO
 * -------------------
 *   CELL_PX(n, cDim, pDim)  expands to CellPx with explicit casts so all
 *   existing call sites compile without any source changes.
 */

#include <exec/types.h>
#include "compilers.h"  /* INLINE */

/* ------------------------------------------------------------------ */
/* 1. GCC 2.x / GCC 6.x "Bebbo" targeting mc68000 (not mc68020+)     */
/* ------------------------------------------------------------------ */
#if defined(__GNUC__) && defined(__mc68000__)

INLINE WORD CellPx(LONG n, LONG cDim, WORD pDim)
{
    LONG num = n * 8L * cDim;
    /*
     * divs.w Dn, Dm  :  Dm[31:0] (signed 32) / Dn[15:0] (signed 16)
     *   -> Dm[15:0]  = 16-bit signed quotient
     *   -> Dm[31:16] = 16-bit signed remainder  (discarded below)
     *
     * Constraint "+d"(num) : data-register, input AND output.
     * Constraint  "d"(pDim): data-register, 16-bit value for divisor.
     *
     * Note: pDim is WORD; GCC zero-extends it into a 32-bit register but
     * divs.w only reads the low 16 bits as the divisor -- correct behaviour.
     */
    __asm__ ("divs.w %1,%0\n\text.w d0" : "+d"(num) : "d"(pDim));
    return (LONG)num;   /* low word = quotient */
}

/* ------------------------------------------------------------------ */
/* 2. SAS/C 6.x                                                        */
/* ------------------------------------------------------------------ */
#elif 0
// defined(__SASC)

INLINE LONG CellPx(LONG n, LONG cDim, WORD pDim)
{
    /*
     * __d0 / __d1 are SAS/C storage-class qualifiers that bind local
     * variables to specific 68k data registers.  The #asm / #endasm block
     * inserts "divs.w d1,d0" verbatim after the register assignments.
     *
     * After divs.w:  d0[15:0] = quotient, d0[31:16] = remainder.
     * (WORD)_num returns only the low 16-bit quotient.
     */
    register __d0 LONG _num = n * 8L * cDim;
    register __d1 WORD _div = pDim;
#asm
    divs.w  d1,d0
    ext.w d0
#endasm
    (void)_div;         /* suppress SAS/C "unused variable" warning */
    return (LONG)_num;
}

/* ------------------------------------------------------------------ */
/* 3. Fallback: plain C (mc68020+, VBCC, non-Amiga, etc.)             */
/* ------------------------------------------------------------------ */
#else

INLINE LONG CellPx(LONG n, LONG cDim, WORD pDim)
{
    /*
     * 68020+ has divs.l (native 32/32 signed divide); the compiler uses
     * it directly.  On all other targets the compiler picks the best path.
     */
    return (LONG)((n * 8L * cDim) / (LONG)pDim);
}

#endif /* compiler selection — CellPx */

/* ------------------------------------------------------------------ */
/* DivW(n, d)                                                          */
/*                                                                     */
/* Raw signed 32-by-16 divide.  Returns (WORD)((LONG)n / (WORD)d).    */
/*                                                                     */
/* Use this when the dividend is already a pre-formed LONG product and */
/* no fixed *8 scale is needed (e.g. colour-picker cell geometry).     */
/* ------------------------------------------------------------------ */
#if defined(__GNUC__) && defined(__mc68000__)

inline LONG DivW(REG(d0, signed int  n),REG(d1, signed short dvs))
{
    asm volatile ("divs.w %1,%0\n\text.w %0" : "+d"(n) : "d"(dvs) : );
    return n;   /* low word = quotient */
}

INLINE ULONG DivU(ULONG n, UWORD d)
{
    __asm__ ("divu.w %1,%0\n\tswap %0\n\tclr.w %0\n\tswap %0" : "=d"(n) : "d"(d));
    return (ULONG)n;   /* low word = quotient */
}


#elif defined(__SASC)

INLINE LONG DivW(LONG n, WORD d)
{
    register __d0 LONG _num = n;
    register __d1 WORD _div = d;
#asm
    divs.w  d1,d0
    ext.w d0
#endasm
    (void)_div;
    return (LONG)_num;
}

#else

INLINE LONG DivW(LONG n, WORD d)
{
    return (LONG)(n / (LONG)d);
}

#endif /* compiler selection — DivW */

/* ------------------------------------------------------------------ */
/* FillW(dst, val, count)                                              */
/*                                                                     */
/* Fill `count' WORDs at dst[] with val.                               */
/*                                                                     */
/* This function exists mainly to demonstrate inline-asm local labels  */
/* in GCC 68k.  The same technique applies to any loop that needs a   */
/* branch target inside an INLINE function.                            */
/*                                                                     */
/* WHY ORDINARY NAMED LABELS FAIL IN INLINE FUNCTIONS                 */
/* ---------------------------------------------------                 */
/* Writing  "myloop:\n\t dbra %0,myloop\n\t"  works when the function */
/* is emitted exactly once.  But an INLINE function expanded in two    */
/* translation units (or twice in one .c) produces two identical       */
/* "myloop:" labels in the .s -> assembler/linker duplicate-symbol     */
/* error.                                                              */
/*                                                                     */
/* SOLUTION 1 - Numeric local labels (standard GAS, recommended)      */
/*   0:  defines a local label named "0".                              */
/*   0b  references the nearest PRECEDING "0" (b = backward).         */
/*   0f  references the nearest FOLLOWING  "0" (f = forward).         */
/*   GAS rescopes them between consecutive named labels, so each       */
/*   expansion gets its own private copy - no collision possible.      */
/*                                                                     */
/* SOLUTION 2 - %= substitution (GCC extension)                       */
/*   GCC replaces %= with a unique decimal integer for each asm        */
/*   statement it compiles, making "fillw_%=:" become e.g.            */
/*   "fillw_42:" in the .s output.  More readable in disassembly,     */
/*   but requires the same %= token at both the definition and every   */
/*   reference within the same asm string.                             */
/*                                                                     */
/* SAS/C NOTE                                                          */
/*   SAS/C #asm inserts verbatim text; there is no equivalent of %=   */
/*   and numeric labels have global scope.  For a loop in an INLINE    */
/*   function under SAS/C, fall back to C - the compiler generates a  */
/*   dbra loop automatically for a simple counted for-loop when        */
/*   optimisation is enabled.                                          */
/* ------------------------------------------------------------------ */
#if defined(__GNUC__) && defined(__mc68000__) && !defined(__mc68020__)

INLINE void FillW(WORD *dst, WORD val, WORD count)
{
    if (count <= 0) return;
    /*
     * dbra Dn,label : Dn -= 1 ; branch if Dn != -1
     * Loop runs exactly `count' times when seeded with count - 1.
     *
     * Constraints:
     *   "+d"(count)  data register, R/W  (modified by subq.w and dbra)
     *   "+a"(dst)    address register, R/W (post-incremented by move.w)
     *   "d"(val)     data register, read-only
     *   "cc"         clobber: dbra modifies the condition codes
     *
     * Style 1: numeric local labels (active)
     *   "0:" defines the label; "0b" references the nearest preceding one.
     *
     * Style 2: %= labels (shown commented out below)
     *   Uncomment to compare - both produce identical machine code; the
     *   only difference is the symbol name in the disassembly/map file.
     */
    __asm__ volatile (
        "subq.w  #1,%0\n\t"     /* seed loop counter: count - 1          */
        "0:\n\t"                /* local label - GAS numeric style        */
        "move.w  %2,(%1)+\n\t" /* *dst++ = val                           */
        "dbra    %0,0b\n\t"    /* --count >= 0 ? branch back to 0:       */
        : "+d"(count), "+a"(dst)
        : "d"(val)
        : "cc"
    );
    /*
     * Style 2 equivalent - replace the asm above with:
     *
     * __asm__ volatile (
     *     "subq.w  #1,%0\n\t"
     *     "fillw_%=:\n\t"              %= -> unique integer per expansion
     *     "move.w  %2,(%1)+\n\t"
     *     "dbra    %0,fillw_%=\n\t"    same %= -> same unique integer
     *     : "+d"(count), "+a"(dst)
     *     : "d"(val)
     *     : "cc"
     * );
     */
}

#else   /* SAS/C and all other compilers - see SAS/C NOTE above */

INLINE void FillW(WORD *dst, WORD val, WORD count)
{
    WORD i;
    for (i = 0; i < count; i++) dst[i] = val;
}

#endif /* compiler selection - FillW */

/* ------------------------------------------------------------------ */
/* Compatibility macro - keeps all existing call sites unchanged       */
/* ------------------------------------------------------------------ */

/*
 * CELL_PX(n, cDim, pDim)
 *
 * Drop-in replacement for the old macro:
 *   #define CELL_PX(n,cDim,pDim) ((WORD)(((LONG)(n)*8*(LONG)(cDim))/(WORD)(pDim)))
 *
 * All three arguments are cast to the required types before the call so
 * callers passing WORD, int or any other integral type continue to work.
 */
#define CELL_PX(n, cDim, pDim)  CellPx((LONG)(n), (LONG)(cDim), (WORD)(pDim))

#endif /* PETSCII_CELLPX_H */
