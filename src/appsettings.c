
#include <stdio.h>
#include <string.h>

#include <proto/exec.h>
#include <proto/dos.h>

#include "appsettings.h"
#include "tooltypepref.h"

static char *StrDup(const char *s)
{
    char *copy;
    ULONG len;
    if(!s) return NULL;
    len = (ULONG)strlen(s) + 1;
    copy = (char *)AllocVec(len, MEMF_ANY);
    if(copy) strcpy(copy, s);
    return copy;
}

/* Tooltype key names */
#define TT_TEMPDIR  "TEMPDIR"
#define TT_RECENT   "RECENT"  /* RECENT0, RECENT1, ... RECENT7 */

void AppSettings_Init(AppSettings *as)
{
    if(!as) return;
    memset(as, 0, sizeof(AppSettings));
}

void AppSettings_Load(AppSettings *as, const char *exename)
{
    int i;
    const char *val;
    char key[16]; /* "RECENT0" .. "RECENT7" */

    if(!as || !exename) return;

    if(!ToolTypePrefs_Init(exename)) {
        printf("AppSettings: Could not load tooltypes for '%s'\n", exename);
        return;
    }

    /* Load temp directory */
    val = ToolTypePrefs_Get(TT_TEMPDIR);
    if(val && val[0] != '\0') {
        as->tempDir = StrDup(val);
    }

    /* Load recent files */
    as->recentCount = 0;
    for(i = 0; i < APPSETTINGS_MAX_RECENT; i++) {
        sprintf(key, "%s%d", TT_RECENT, i);
        val = ToolTypePrefs_Get(key);
        if(val && val[0] != '\0') {
            /* Check if file still exists */
            BPTR lock = Lock((STRPTR)val, ACCESS_READ);
            if(lock) {
                UnLock(lock);
                as->recentFiles[as->recentCount] = StrDup(val);
                as->recentCount++;
            }
            /* else: file gone, skip it */
        }
    }
}

void AppSettings_Save(AppSettings *as)
{
    int i;
    char key[16];

    if(!as) return;

    /* Save temp directory */
    if(as->tempDir) {
        ToolTypePrefs_Set(TT_TEMPDIR, as->tempDir);
    }

    /* Save recent files */
    for(i = 0; i < APPSETTINGS_MAX_RECENT; i++) {
        sprintf(key, "%s%d", TT_RECENT, i);
        if(i < as->recentCount && as->recentFiles[i]) {
            ToolTypePrefs_Set(key, as->recentFiles[i]);
        } else {
            /* Remove stale entries */
            ToolTypePrefs_Remove(key);
        }
    }

    ToolTypePrefs_Save();
}

void AppSettings_Close(AppSettings *as)
{
    int i;
    if(!as) return;

    FreeVec(as->tempDir);
    as->tempDir = NULL;

    for(i = 0; i < APPSETTINGS_MAX_RECENT; i++) {
        FreeVec(as->recentFiles[i]);
        as->recentFiles[i] = NULL;
    }
    as->recentCount = 0;

    ToolTypePrefs_Close();
}

void AppSettings_SetTempDir(AppSettings *as, const char *path)
{
    if(!as) return;

    FreeVec(as->tempDir);
    as->tempDir = path ? StrDup(path) : NULL;
}

const char *AppSettings_GetTempDir(AppSettings *as)
{
    if(!as) return NULL;
    return as->tempDir;
}

void AppSettings_AddRecentFile(AppSettings *as, const char *path)
{
    int i;
    int existingIndex = -1;

    if(!as || !path || path[0] == '\0') return;

    /* Check if already in list */
    for(i = 0; i < as->recentCount; i++) {
        if(as->recentFiles[i] && strcmp(as->recentFiles[i], path) == 0) {
            existingIndex = i;
            break;
        }
    }

    if(existingIndex == 0) {
        /* Already at top, nothing to do */
        return;
    }

    if(existingIndex > 0) {
        /* Move to top: save the string, shift others down */
        char *existing = as->recentFiles[existingIndex];
        for(i = existingIndex; i > 0; i--) {
            as->recentFiles[i] = as->recentFiles[i - 1];
        }
        as->recentFiles[0] = existing;
    } else {
        /* New entry: drop last if full, shift all down, insert at [0] */
        if(as->recentCount >= APPSETTINGS_MAX_RECENT) {
            FreeVec(as->recentFiles[APPSETTINGS_MAX_RECENT - 1]);
        }

        /* Shift down */
        {
            int limit = as->recentCount < APPSETTINGS_MAX_RECENT ?
                        as->recentCount : APPSETTINGS_MAX_RECENT - 1;
            for(i = limit; i > 0; i--) {
                as->recentFiles[i] = as->recentFiles[i - 1];
            }
        }

        as->recentFiles[0] = StrDup(path);

        if(as->recentCount < APPSETTINGS_MAX_RECENT) {
            as->recentCount++;
        }
    }
}

int AppSettings_GetRecentCount(AppSettings *as)
{
    if(!as) return 0;
    return as->recentCount;
}

const char *AppSettings_GetRecentFile(AppSettings *as, int index)
{
    if(!as || index < 0 || index >= as->recentCount) return NULL;
    return as->recentFiles[index];
}
