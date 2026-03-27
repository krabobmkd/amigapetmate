#ifndef APPSETTINGS_H
#define APPSETTINGS_H

/*
 * AppSettings - Application-level settings persisted via icon tooltypes
 *
 * Manages temp directory path and a stack of recent project files (max 8).
 * Uses AllocVec/FreeVec for all path allocation (no fixed-size buffers).
 * Loaded/saved through tooltypepref API.
 */

#define APPSETTINGS_MAX_RECENT 4

typedef struct AppSettings {
    char *recentFiles[APPSETTINGS_MAX_RECENT];  /* [0] = most recent */
    int   recentCount;

    /* values that can be changed in SettingsView window: */
    /* Screen mode for fullscreen: */
    int  screenModeIdLikeWorkbench;   /* TRUE = use WB screen mode (SA_LikeWorkbench)     */

    ULONG screenModeId;   /* display ID for SA_DisplayID; 0xFFFFFFFF=invalid  */

    /* UI background style: */
    int   useOneColorBg;  /* TRUE = use solid UI background color, no image   */
    char *bgImagePath;    /* AllocVec'd path to background image, NULL = none */

    ULONG currentPalette;
} AppSettings;

/* Load settings from icon tooltypes. Calls ToolTypePrefs_Init internally. */
void AppSettings_Load(AppSettings *as);

/* Save settings to icon tooltypes, then ToolTypePrefs_Save. */
void AppSettings_Save(AppSettings *as);

/* Free all allocated strings and call ToolTypePrefs_Close. */
void AppSettings_Close(AppSettings *as);

/* Set background image path (duplicates string; NULL clears it). */
void AppSettings_SetBgImagePath(AppSettings *as, const char *path);

/* Add a file path to top of recent list. Moves to top if already present. */
void AppSettings_AddRecentFile(AppSettings *as, const char *path);

/* Get number of recent files currently stored. */
int AppSettings_GetRecentCount(AppSettings *as);

/* Get recent file path by index (0 = most recent). Returns NULL if out of range. */
const char *AppSettings_GetRecentFile(AppSettings *as, int index);

#endif /* APPSETTINGS_H */
