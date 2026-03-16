
#include <stdio.h>
#include <string.h>

#include <proto/exec.h>
#include <proto/dos.h>

#include "appsettings.h"
#include "tooltypepref.h"
#include "petmate.h"
#include <stdio.h>
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
#define TT_TEMPDIR       "TEMPDIR"
#define TT_RECENT        "RECENT"        /* RECENT0, RECENT1, ... RECENT7 */
#define TT_USE_WORKBENCH "FSUSEWBMODEID" /* "1" or "0" */
#define TT_SCREENMODEID  "SCREENMODEID"  /* 8 hex digits, e.g. "00029000" */
#define TT_USEONECLORBG  "USEONECLORBG"  /* "1" or "0" */
#define TT_BGIMAGE       "BGIMAGE"       /* absolute image file path */

void AppSettings_Load(AppSettings *as)
{
    int i;
    const char *val;
    char key[16]; /* "RECENT0" .. "RECENT7" */

    if(!as) return;

    /* ToolTypePrefs_Init is to be done at ebgining of main. */

    /* Load temp directory */
    val = ToolTypePrefs_Get(TT_TEMPDIR);
    if(val && val[0] != '\0') {
        as->tempDir = StrDup(val);
    }

    /* Load screen mode settings */
    val = ToolTypePrefs_Get(TT_USE_WORKBENCH);
    as->screenModeIdLikeWorkbench = (!val || val[0] != '0') ? TRUE : FALSE; /* default TRUE */

    as->screenModeId = (ULONG)~0L; /* INVALID_ID default */
    val = ToolTypePrefs_Get(TT_SCREENMODEID);
    if (val && val[0] != '\0') {
        unsigned long parsed = 0;
        sscanf(val, "%lX", &parsed);
        as->screenModeId = (ULONG)parsed;
    }

    /* Load UI background settings */
    val = ToolTypePrefs_Get(TT_USEONECLORBG);
    if(val) as->useOneColorBg = 1;
    else  as->useOneColorBg = 0;

    if(as->bgImagePath) FreeVec(as->bgImagePath);
    as->bgImagePath = NULL;
    val = ToolTypePrefs_Get(TT_BGIMAGE);
    if (val && val[0] != '\0') {
        as->bgImagePath = StrDup(val);
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
    /* fullscreen and window position */
    {
        const char *p = ToolTypePrefs_Get("FULLSCREEN");
        app->mainwindow.fullscreen = (p != NULL);
        p = ToolTypePrefs_Get("WINDOW");
        if(p)
        {
            app->mainwindow.left = 0;
            app->mainwindow.top = 0;
            app->mainwindow.width = 0;
            app->mainwindow.height = 0;

             sscanf(p,"%d:%d:%d:%d",&app->mainwindow.left,&app->mainwindow.top,
                            &app->mainwindow.width,&app->mainwindow.height);
        }

    }

}

void AppSettings_Save(AppSettings *as)
{
    int i;
    char key[32];

    if(!as) return;

    /* Save screen mode settings */
    ToolTypePrefs_Set(TT_USE_WORKBENCH, as->screenModeIdLikeWorkbench ? "1" : "0");
    {
        char hexbuf[16];
        sprintf(hexbuf, "%08lX", (unsigned long)as->screenModeId);
        ToolTypePrefs_Set(TT_SCREENMODEID, hexbuf);
    }


    /* Save UI background settings */
    if( as->useOneColorBg)
    {
        ToolTypePrefs_Set(TT_USEONECLORBG,NULL);
    } else ToolTypePrefs_Remove(TT_USEONECLORBG);

    if (as->bgImagePath && as->bgImagePath[0] != '\0') {
        ToolTypePrefs_Set(TT_BGIMAGE, as->bgImagePath);
    } else {
        ToolTypePrefs_Remove(TT_BGIMAGE);
    }

    /* Save temp directory */
    if(as->tempDir) {
        ToolTypePrefs_Set(TT_TEMPDIR, as->tempDir);
    }

    if(app->mainwindow.fullscreen)
    {
        ToolTypePrefs_Set("FULLSCREEN",NULL);
    }
    else
    {
        char buf[32];
        ToolTypePrefs_Remove("FULLSCREEN");
        BMainWindow_GetWindowPos(&app->mainwindow,app->window_obj);
        snprintf(buf,31,"%d:%d:%d:%d",app->mainwindow.left,app->mainwindow.top,
                           app->mainwindow.width,app->mainwindow.height );
         ToolTypePrefs_Set("WINDOW",buf);
    }

    /* Save recent files */
    for(i = 0; i < APPSETTINGS_MAX_RECENT; i++) {
        snprintf(key,31, "%s%d", TT_RECENT, i);
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

    FreeVec(as->bgImagePath);
    as->bgImagePath = NULL;

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

void AppSettings_SetBgImagePath(AppSettings *as, const char *path)
{
    if (!as) return;
    FreeVec(as->bgImagePath);
    as->bgImagePath = path ? StrDup(path) : NULL;
}

