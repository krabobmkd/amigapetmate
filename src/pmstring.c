/*
 * pmstring.c - dynamic string utilities using AllocVec/FreeVec.
 * C89 compatible.
 */

#include "pmstring.h"

#include <string.h>
#include <proto/exec.h>

char *PmStr_Alloc(const char *src)
{
    ULONG  len;
    char  *buf;

    if (!src) return NULL;
    len = (ULONG)strlen(src);
    buf = (char *)AllocVec(len + 1, MEMF_ANY);
    if (!buf) return NULL;
    CopyMem((APTR)src, (APTR)buf, len + 1);
    return buf;
}

void PmStr_Free(char *s)
{
    if (s) FreeVec(s);
}

int PmStr_EndsWithIgnoreCase(const char *str, const char *suffix)
{
    ULONG       slen;
    ULONG       xlen;
    const char *p;
    const char *q;
    char        a;
    char        b;

    if (!str || !suffix) return 0;
    slen = (ULONG)strlen(str);
    xlen = (ULONG)strlen(suffix);
    if (xlen > slen) return 0;
    p = str + slen - xlen;
    q = suffix;
    while (*q) {
        a = (*p >= 'A' && *p <= 'Z') ? (char)(*p + 32) : *p;
        b = (*q >= 'A' && *q <= 'Z') ? (char)(*q + 32) : *q;
        if (a != b) return 0;
        p++; q++;
    }
    return 1;
}

char *PmStr_WithExt(const char *src, const char *ext)
{
    ULONG  slen;
    ULONG  xlen;
    char  *buf;

    if (!src || !ext) return NULL;
    if (PmStr_EndsWithIgnoreCase(src, ext))
        return PmStr_Alloc(src);

    slen = (ULONG)strlen(src);
    xlen = (ULONG)strlen(ext);
    buf  = (char *)AllocVec(slen + xlen + 1, MEMF_ANY);
    if (!buf) return NULL;
    CopyMem((APTR)src, (APTR)buf, slen);
    CopyMem((APTR)ext, (APTR)(buf + slen), xlen + 1);
    return buf;
}
