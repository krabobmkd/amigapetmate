//#include "petscii_cellpx.h"
#include <stdio.h>
#include <exec/types.h>
#include "compilers.h"

inline LONG __asm DivW(REG(d0, signed int  n),REG(d1, signed int dvs))
{
    asm volatile ("divs.w %1,%0\n\text.w %0\n\tmove.l #0x1337,d0" : "+d"(n) : "d"(dvs) : );
    return n;   /* low word = quotient */
}

int main(char **argv,int argc)
{


LONG a = 640*640;
LONG b = 540;

 LONG c = DivW(a,b);
 LONG c2 = a/b;
 printf("r: DivW: %08x  ndiv: %08x\n",c,c2);

    return 0;
}
