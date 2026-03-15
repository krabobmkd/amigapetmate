#include "petscii_project.h"
#include "pmstring.h"

#include <proto/exec.h>
#include <string.h>
#include <stdio.h>

extern void refreshUI(void);

PetsciiProject *PetsciiProject_Create(void)
{
    PetsciiProject *proj;

    proj = (PetsciiProject *)AllocVec(sizeof(PetsciiProject), MEMF_CLEAR);
    if (!proj) return NULL;

    /* Start with one default screen */
    proj->screens[0] = PetsciiScreen_CreateDefault();
    if (!proj->screens[0]) {
        FreeVec(proj);
        return NULL;
    }

    snprintf(proj->screens[0]->name, PETSCII_NAME_LEN, "Screen 1");
    proj->screenCount = 1;
    proj->currentScreen = 0;
    proj->filepath = NULL;
    proj->modified = 0;
    proj->currentPalette = 0;

    return proj;
}

void PetsciiProject_Destroy(PetsciiProject *proj)
{
    UWORD i;
    if (!proj) return;

    for (i = 0; i < proj->screenCount; i++) {
        if (proj->screens[i]) {
            PetsciiScreen_Destroy(proj->screens[i]);
            proj->screens[i] = NULL;
        }
    }
    PmStr_Free(proj->filepath);
    FreeVec(proj);
}

int PetsciiProject_AddScreen(PetsciiProject *proj)
{
    PetsciiScreen *newScr;
    UWORD idx;

    if (!proj) return -1;
    if (proj->screenCount >= PETSCII_MAX_SCREENS) return -1;

    newScr = PetsciiScreen_CreateDefault();
    if (!newScr) return -1;

    idx = proj->screenCount;
    snprintf(newScr->name, PETSCII_NAME_LEN, "Screen %d", (int)(idx + 1));

    proj->screens[idx] = newScr;
    proj->screenCount++;
    proj->modified = 1;


    return (int)idx;
}

int PetsciiProject_CloneCurrentScreen(PetsciiProject *proj)
{
    PetsciiScreen *clone;
    UWORD idx;

    if (!proj) return -1;
    if (proj->screenCount >= PETSCII_MAX_SCREENS) return -1;

    clone = PetsciiScreen_Clone(proj->screens[proj->currentScreen]);
    if (!clone) return -1;

    idx = proj->screenCount;
    snprintf(clone->name, PETSCII_NAME_LEN, "%s (copy)",
             proj->screens[proj->currentScreen]->name);

    proj->screens[idx] = clone;
    proj->screenCount++;
    proj->modified = 1;

   // refreshUI();

    return (int)idx;
}

int PetsciiProject_RemoveScreen(PetsciiProject *proj, UWORD index)
{
    UWORD i;

    if (!proj) return 0;
    if (proj->screenCount <= 1) return 0;
    if (index >= proj->screenCount) return 0;

    PetsciiScreen_Destroy(proj->screens[index]);

    /* Shift remaining screens down */
    for (i = index; i < proj->screenCount - 1; i++) {
        proj->screens[i] = proj->screens[i + 1];
    }
    proj->screens[proj->screenCount - 1] = NULL;
    proj->screenCount--;

    /* Adjust current screen if needed */
    if (proj->currentScreen >= proj->screenCount) {
        proj->currentScreen = proj->screenCount - 1;
    }

    proj->modified = 1;

    refreshUI();

    return 1;
}

PetsciiScreen *PetsciiProject_GetCurrentScreen(PetsciiProject *proj)
{
    if (!proj || proj->screenCount == 0) return NULL;
    return proj->screens[proj->currentScreen];
}

void PetsciiProject_SetCurrentScreen(PetsciiProject *proj, UWORD index)
{
    if (!proj) return;
    if (index >= proj->screenCount) index = proj->screenCount - 1;
    proj->currentScreen = index;

    refreshUI();

}

void PetsciiProject_NavigateScreen(PetsciiProject *proj, int delta)
{
    int newIdx;
    if (!proj || proj->screenCount == 0) return;

    newIdx = (int)proj->currentScreen + delta;
    if (newIdx < 0) newIdx = (int)proj->screenCount - 1;
    if (newIdx >= (int)proj->screenCount) newIdx = 0;

    proj->currentScreen = (UWORD)newIdx;

    refreshUI();

}

void PetsciiProject_Reset(PetsciiProject *proj)
{
    UWORD i;
    if (!proj) return;

    /* Destroy all screens except first */
    for (i = 1; i < proj->screenCount; i++) {
        if (proj->screens[i]) {
            PetsciiScreen_Destroy(proj->screens[i]);
            proj->screens[i] = NULL;
        }
    }

    /* Reset first screen */
    if (proj->screens[0]) {
        PetsciiScreen_Clear(proj->screens[0], C64_LIGHTBLUE);
        proj->screens[0]->backgroundColor = C64_BLUE;
        proj->screens[0]->borderColor = C64_LIGHTBLUE;
        proj->screens[0]->charset = PETSCII_CHARSET_UPPER;
        snprintf(proj->screens[0]->name, PETSCII_NAME_LEN, "Screen 1");
    } else {
        proj->screens[0] = PetsciiScreen_CreateDefault();
        snprintf(proj->screens[0]->name, PETSCII_NAME_LEN, "Screen 1");
    }

    proj->screenCount = 1;
    proj->currentScreen = 0;
    PmStr_Free(proj->filepath);
    proj->filepath = NULL;
    proj->modified = 0;

    refreshUI();

}
