//#include "petscii_cellpx.h"
#include <stdio.h>
#include <exec/types.h>
#include "compilers.h"

static inline int DivsW(int  n, short dvs)
{
    asm volatile ("divs.w  %1,%0\n\text.l %0\n" : "+d"(n) : "d"(dvs) : );
    return n;
}
static inline unsigned int DivuW(unsigned int  n,unsigned short dvs)
{
    asm volatile ("divu.w  %1,%0\n\text.l %0\n" : "+d"(n) : "d"(dvs) : );
    return n;
}



int main(char **argv,int argc)
{


LONG a = 640*640;
LONG b = 540;

 LONG c = DivW(a,b);

 printf("result: %08x\n",c);

    return 0;
}
