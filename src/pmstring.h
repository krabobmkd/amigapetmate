#ifndef PMSTRING_H
#define PMSTRING_H

/*
 * PmStr - dynamic string utilities using AllocVec/FreeVec.
 *
 * All returned strings must be freed with PmStr_Free().
 * C89 compatible.
 */

/* Allocate a copy of src. Returns NULL on failure or if src is NULL. */
char *PmStr_Alloc(const char *src);

/* Free a string allocated by PmStr_Alloc / PmStr_WithExt. Safe with NULL. */
void PmStr_Free(char *s);

/* Returns 1 if str ends with suffix (case-insensitive ASCII). */
int PmStr_EndsWithIgnoreCase(const char *str, const char *suffix);

/*
 * Allocate a new string: src + ext appended, unless src already ends
 * with ext (case-insensitive).  Returns NULL on failure.
 */
char *PmStr_WithExt(const char *src, const char *ext);

#endif /* PMSTRING_H */
