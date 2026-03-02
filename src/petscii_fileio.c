/*
 * PetsciiFileIO - Load/save .petmate JSON files using cJSON.
 *
 * Save: build a cJSON DOM, render to string, write to file.
 * Load: read file into buffer, parse with cJSON, walk the DOM.
 *
 * Memory strategy:
 *   Save: cJSON_PrintUnformatted allocates; we write then free.
 *   Load: AllocVec the whole file, parse in one call, free DOM + buffer.
 *
 * C89 compatible; no VLAs, no C99 declarations after statements.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <proto/exec.h>

#include "petscii_fileio.h"
#include "petscii_project.h"
#include "petscii_screen.h"
#include "petscii_types.h"
#include "cjson/cJSON.h"

/* ------------------------------------------------------------------ */
/* Error strings                                                        */
/* ------------------------------------------------------------------ */

const char *PetsciiFileIO_ErrorString(int errCode)
{
    switch (errCode) {
        case PETSCII_FILEIO_OK:      return "OK";
        case PETSCII_FILEIO_EOPEN:   return "Cannot open file";
        case PETSCII_FILEIO_EREAD:   return "File read error";
        case PETSCII_FILEIO_EPARSE:  return "JSON parse error";
        case PETSCII_FILEIO_EFORMAT: return "Unexpected file format";
        case PETSCII_FILEIO_EALLOC:  return "Out of memory";
        case PETSCII_FILEIO_EWRITE:  return "File write error";
        default:                      return "Unknown error";
    }
}

/* ------------------------------------------------------------------ */
/* Save helpers                                                         */
/* ------------------------------------------------------------------ */

/* Returns 1 if str ends with suffix, case-insensitive ASCII. C89. */
static int endsWithIgnoreCase(const char *str, const char *suffix)
{
    size_t      slen = strlen(str);
    size_t      xlen = strlen(suffix);
    const char *p;
    const char *q;
    char        a;
    char        b;
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

/* ------------------------------------------------------------------ */
/* Save                                                                 */
/* ------------------------------------------------------------------ */

int PetsciiFileIO_Save(const PetsciiProject *proj, const char *path)
{
    char        extbuf[PETSCII_PATH_LEN];
    const char *actualPath;
    cJSON      *root;
    cJSON      *framebufs;
    FILE       *fp;
    UWORD       i;
    char       *jsonStr;
    int         writeOk;

    if (!proj || !path) return PETSCII_FILEIO_EOPEN;

    /* Ensure .petmate extension (case-insensitive check) */
    if (!endsWithIgnoreCase(path, ".petmate")) {
        size_t plen = strlen(path);
        if (plen + 8 < PETSCII_PATH_LEN) {
            strncpy(extbuf, path, PETSCII_PATH_LEN - 1);
            extbuf[PETSCII_PATH_LEN - 1] = '\0';
            strncat(extbuf, ".petmate", PETSCII_PATH_LEN - 1 - plen);
            actualPath = extbuf;
        } else {
            actualPath = path;
        }
    } else {
        actualPath = path;
    }

    root = cJSON_CreateObject();
    if (!root) return PETSCII_FILEIO_EALLOC;

    cJSON_AddNumberToObjectInt(root, "version", 1);

    framebufs = cJSON_AddArrayToObject(root, "framebufs");
    if (!framebufs) {
        cJSON_Delete(root);
        return PETSCII_FILEIO_EALLOC;
    }

    for (i = 0; i < proj->screenCount; i++) {
        PetsciiScreen *scr = proj->screens[i];
        cJSON         *fb;
        cJSON         *codes;
        cJSON         *colors;
        ULONG          j;
        ULONG          total;

        if (!scr) continue;

        fb = cJSON_CreateObject();
        if (!fb) {
            cJSON_Delete(root);
            return PETSCII_FILEIO_EALLOC;
        }

        cJSON_AddNumberToObjectInt(fb, "width",           (int)scr->width);
        cJSON_AddNumberToObjectInt(fb, "height",          (int)scr->height);
        cJSON_AddNumberToObjectInt(fb, "backgroundColor", (int)scr->backgroundColor);
        cJSON_AddNumberToObjectInt(fb, "borderColor",     (int)scr->borderColor);
        cJSON_AddStringToObject(fb, "charset",
            (scr->charset == PETSCII_CHARSET_LOWER) ? "lower" : "upper");
        cJSON_AddStringToObject(fb, "name", scr->name);

        total  = (ULONG)scr->width * (ULONG)scr->height;
        codes  = cJSON_AddArrayToObject(fb, "screencodes");
        colors = cJSON_AddArrayToObject(fb, "colors");

        if (!codes || !colors) {
            cJSON_Delete(fb);
            cJSON_Delete(root);
            return PETSCII_FILEIO_EALLOC;
        }

        for (j = 0; j < total; j++) {
            cJSON_AddItemToArray(codes,  cJSON_CreateNumberInt((int)scr->framebuf[j].code));
            cJSON_AddItemToArray(colors, cJSON_CreateNumberInt((int)scr->framebuf[j].color));
        }

        cJSON_AddItemToArray(framebufs, fb);
    }

    //jsonStr = cJSON_PrintUnformatted(root);
    jsonStr = cJSON_Print(root);
    cJSON_Delete(root);

    if (!jsonStr) return PETSCII_FILEIO_EALLOC;

    fp = fopen(actualPath, "w");
    if (!fp) {
        free(jsonStr);
        return PETSCII_FILEIO_EOPEN;
    }

    writeOk = (fputs(jsonStr, fp) != EOF);
    fclose(fp);
    free(jsonStr);

    if (!writeOk) return PETSCII_FILEIO_EWRITE;

    /* Update project state */
    {
        PetsciiProject *mproj = (PetsciiProject *)(void *)proj; /* drop const */
        strncpy(mproj->filepath, actualPath, PETSCII_PATH_LEN - 1);
        mproj->filepath[PETSCII_PATH_LEN - 1] = '\0';
        mproj->modified = 0;
    }

    return PETSCII_FILEIO_OK;
}

/* ------------------------------------------------------------------ */
/* Load helpers                                                         */
/* ------------------------------------------------------------------ */

/* Read entire file into an AllocVec'd buffer (NUL-terminated).
 * Caller must FreeVec the buffer.  Returns NULL on failure. */
static char *readFileToBuffer(const char *path)
{
    FILE  *fp;
    long   size;
    char  *buf;

    fp = fopen(path, "r");
    if (!fp) return NULL;

    if (fseek(fp, 0L, SEEK_END) != 0) { fclose(fp); return NULL; }
    size = ftell(fp);
    if (size < 0)                      { fclose(fp); return NULL; }
    rewind(fp);

    buf = (char *)AllocVec((ULONG)(size + 1), MEMF_ANY);
    if (!buf) { fclose(fp); return NULL; }

    if ((long)fread(buf, 1, (size_t)size, fp) != size) {
        FreeVec(buf);
        fclose(fp);
        return NULL;
    }
    buf[size] = '\0';
    fclose(fp);
    return buf;
}

/* ------------------------------------------------------------------ */
/* Load                                                                 */
/* ------------------------------------------------------------------ */

int PetsciiFileIO_Load(PetsciiProject *proj, const char *path)
{
    char       *buf;
    cJSON      *root;
    cJSON      *framebufsArr;
    cJSON      *fb;
    int         result;
    int         fileVersion;
    UWORD       screenIdx;

    if (!proj || !path) return PETSCII_FILEIO_EOPEN;

    buf = readFileToBuffer(path);
    if (!buf) return PETSCII_FILEIO_EOPEN;

    root = cJSON_Parse(buf);
    FreeVec(buf);

    if (!root) return PETSCII_FILEIO_EPARSE;

    /* Check version field - must be 1 or 2 */
    {
        cJSON *ver = cJSON_GetObjectItem(root, "version");
        if (!ver || !cJSON_IsNumber(ver) ||
            (ver->valueint != 1 && ver->valueint != 2)) {
            cJSON_Delete(root);
            return PETSCII_FILEIO_EFORMAT;
        }
        fileVersion = ver->valueint;
    }

    framebufsArr = cJSON_GetObjectItem(root, "framebufs");
    if (!cJSON_IsArray(framebufsArr) || cJSON_GetArraySize(framebufsArr) < 1) {
        cJSON_Delete(root);
        return PETSCII_FILEIO_EFORMAT;
    }

    /* Reset project to blank before loading */
    PetsciiProject_Reset(proj);

    result    = PETSCII_FILEIO_OK;
    screenIdx = 0;

    for (fb = framebufsArr->child; fb != NULL; fb = fb->next) {
        cJSON         *jw;
        cJSON         *jh;
        cJSON         *jbg;
        cJSON         *jborder;
        cJSON         *jcharset;
        cJSON         *jname;
        cJSON         *jcodes;
        cJSON         *jcolors;
        cJSON         *jfb2d;
        UWORD          w;
        UWORD          h;
        PetsciiScreen *scr;
        ULONG          total;
        ULONG          j;

        if (screenIdx >= PETSCII_MAX_SCREENS) break;

        jw      = cJSON_GetObjectItem(fb, "width");
        jh      = cJSON_GetObjectItem(fb, "height");
        jbg     = cJSON_GetObjectItem(fb, "backgroundColor");
        jborder = cJSON_GetObjectItem(fb, "borderColor");
        jcharset= cJSON_GetObjectItem(fb, "charset");
        jname   = cJSON_GetObjectItem(fb, "name");
        jcodes  = cJSON_GetObjectItem(fb, "screencodes");
        jcolors = cJSON_GetObjectItem(fb, "colors");
        jfb2d   = cJSON_GetObjectItem(fb, "framebuf");

        if (!cJSON_IsNumber(jw) || !cJSON_IsNumber(jh)) {
            result = PETSCII_FILEIO_EFORMAT;
            break;
        }
        if (fileVersion == 1) {
            if (!cJSON_IsArray(jcodes) || !cJSON_IsArray(jcolors)) {
                result = PETSCII_FILEIO_EFORMAT;
                break;
            }
        } else {
            if (!cJSON_IsArray(jfb2d)) {
                result = PETSCII_FILEIO_EFORMAT;
                break;
            }
        }

        w = (UWORD)jw->valueint;
        h = (UWORD)jh->valueint;

        if (w == 0 || h == 0 ||
            w > PETSCII_MAX_WIDTH || h > PETSCII_MAX_HEIGHT) {
            result = PETSCII_FILEIO_EFORMAT;
            break;
        }

        total = (ULONG)w * (ULONG)h;

        if (fileVersion == 1) {
            if ((ULONG)cJSON_GetArraySize(jcodes)  < total ||
                (ULONG)cJSON_GetArraySize(jcolors) < total) {
                result = PETSCII_FILEIO_EFORMAT;
                break;
            }
        } else {
            if ((ULONG)cJSON_GetArraySize(jfb2d) < (ULONG)h) {
                result = PETSCII_FILEIO_EFORMAT;
                break;
            }
        }

        /* First framebuf reuses the screen created by Reset(); add new ones */
        if (screenIdx == 0) {
            scr = proj->screens[0];
            /* Recreate if dimensions differ */
            if (scr->width != w || scr->height != h) {
                PetsciiScreen *newScr = PetsciiScreen_Create(w, h);
                if (!newScr) { result = PETSCII_FILEIO_EALLOC; break; }
                PetsciiScreen_Destroy(scr);
                proj->screens[0] = newScr;
                scr = newScr;
            }
        } else {
            int idx = PetsciiProject_AddScreen(proj);
            if (idx < 0) {
                result = PETSCII_FILEIO_EALLOC;
                break;
            }
            /* Replace default-size screen if dimensions differ */
            if (proj->screens[idx]->width != w || proj->screens[idx]->height != h) {
                PetsciiScreen *newScr = PetsciiScreen_Create(w, h);
                if (!newScr) { result = PETSCII_FILEIO_EALLOC; break; }
                PetsciiScreen_Destroy(proj->screens[idx]);
                proj->screens[idx] = newScr;
            }
            scr = proj->screens[idx];
        }

        if (!scr) { result = PETSCII_FILEIO_EALLOC; break; }

        /* Metadata */
        if (cJSON_IsNumber(jbg))
            scr->backgroundColor = (UBYTE)(jbg->valueint & 0x0F);
        if (cJSON_IsNumber(jborder))
            scr->borderColor = (UBYTE)(jborder->valueint & 0x0F);
        if (cJSON_IsString(jcharset) && jcharset->valuestring)
            scr->charset = (strcmp(jcharset->valuestring, "lower") == 0)
                           ? PETSCII_CHARSET_LOWER : PETSCII_CHARSET_UPPER;
        if (cJSON_IsString(jname) && jname->valuestring) {
            strncpy(scr->name, jname->valuestring, PETSCII_NAME_LEN - 1);
            scr->name[PETSCII_NAME_LEN - 1] = '\0';
        }

        /* Pixel data */
        if (fileVersion == 1) {
            for (j = 0; j < total; j++) {
                cJSON *code  = cJSON_GetArrayItem(jcodes,  (int)j);
                cJSON *color = cJSON_GetArrayItem(jcolors, (int)j);
                scr->framebuf[j].code  = code  ? (UBYTE)(code->valueint  & 0xFF) : 0;
                scr->framebuf[j].color = color ? (UBYTE)(color->valueint & 0x0F) : 0;
            }
        } else {
            /* Version 2: framebuf is a 2D array: rows of {code, color} objects */
            ULONG row;
            ULONG col;
            for (row = 0; row < (ULONG)h; row++) {
                cJSON *jrow = cJSON_GetArrayItem(jfb2d, (int)row);
                for (col = 0; col < (ULONG)w; col++) {
                    cJSON *jcell  = jrow ? cJSON_GetArrayItem(jrow,  (int)col)    : NULL;
                    cJSON *jcode  = jcell ? cJSON_GetObjectItem(jcell, "code")    : NULL;
                    cJSON *jcolor = jcell ? cJSON_GetObjectItem(jcell, "color")   : NULL;
                    scr->framebuf[row * (ULONG)w + col].code  = jcode  ? (UBYTE)(jcode->valueint  & 0xFF) : 0;
                    scr->framebuf[row * (ULONG)w + col].color = jcolor ? (UBYTE)(jcolor->valueint & 0x0F) : 0;
                }
            }
        }

        screenIdx++;
    }

    cJSON_Delete(root);

    if (result == PETSCII_FILEIO_OK) {
        strncpy(proj->filepath, path, PETSCII_PATH_LEN - 1);
        proj->filepath[PETSCII_PATH_LEN - 1] = '\0';
        proj->modified = 0;
        proj->currentScreen = 0;
    }

    return result;
}
