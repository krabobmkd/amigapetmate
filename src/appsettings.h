#ifndef APPSETTINGS_H
#define APPSETTINGS_H

/*
 * AppSettings - Application-level settings persisted via icon tooltypes
 *
 * Manages temp directory path and a stack of recent project files (max 8).
 * Uses AllocVec/FreeVec for all path allocation (no fixed-size buffers).
 * Loaded/saved through tooltypepref API.
 */

#define APPSETTINGS_MAX_RECENT 8

typedef struct AppSettings {
    char *tempDir;                              /* AllocVec-managed path */
    char *recentFiles[APPSETTINGS_MAX_RECENT];  /* [0] = most recent */
    int   recentCount;
} AppSettings;

/* Initialize (zeroes struct). Call before Load. */
void AppSettings_Init(AppSettings *as);

/* Load settings from icon tooltypes. Calls ToolTypePrefs_Init internally. */
void AppSettings_Load(AppSettings *as, const char *exename);

/* Save settings to icon tooltypes, then ToolTypePrefs_Save. */
void AppSettings_Save(AppSettings *as);

/* Free all allocated strings and call ToolTypePrefs_Close. */
void AppSettings_Close(AppSettings *as);

/* Set temp directory (duplicates string). */
void AppSettings_SetTempDir(AppSettings *as, const char *path);

/* Get current temp directory (may return NULL). */
const char *AppSettings_GetTempDir(AppSettings *as);

/* Add a file path to top of recent list. Moves to top if already present. */
void AppSettings_AddRecentFile(AppSettings *as, const char *path);

/* Get number of recent files currently stored. */
int AppSettings_GetRecentCount(AppSettings *as);

/* Get recent file path by index (0 = most recent). Returns NULL if out of range. */
const char *AppSettings_GetRecentFile(AppSettings *as, int index);

#endif /* APPSETTINGS_H */
