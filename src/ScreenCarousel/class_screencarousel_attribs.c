/*
 * ScreenCarousel - BOOPSI gadget class for the screen thumbnail strip.
 * Attribute handlers: OM_NEW, OM_DISPOSE, OM_SET, OM_GET.
 */

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include "screen_carousel_private.h"

/* ------------------------------------------------------------------ */
/* OM_NEW                                                               */
/* ------------------------------------------------------------------ */

ULONG ScreenCarousel_OnNew(Class *cl, Object *o, struct opSet *msg)
{
    ScreenCarouselData *inst;
    PetsciiProject     *project = NULL;
    PetsciiStyle       *style   = NULL;
    Object             *newObj;
    struct TagItem     *tag;
    ULONG               i;

    tag = FindTagItem(SCA_Project, msg->ops_AttrList);
    if (tag) project = (PetsciiProject *)tag->ti_Data;

    tag = FindTagItem(SCA_Style, msg->ops_AttrList);
    if (tag) style = (PetsciiStyle *)tag->ti_Data;

    if (!project || !style) return 0;

    newObj = (Object *)DoSuperMethodA(cl, o, (APTR)msg);
    if (!newObj) return 0;

    inst = (ScreenCarouselData *)INST_DATA(cl, newObj);

    inst->project              = project;
    inst->style                = style;
    inst->currentScreen        = 0;
    inst->signalScreen         = 0;
    inst->scrollOffset         = 0;
    inst->miniCount            = 0;
    inst->scrollDragActive     = 0;
    inst->scrollDragStartX     = 0;
    inst->scrollDragStartOffset = 0;
    inst->clipRegion = NewRegion();

    for (i = 0; i < PETSCII_MAX_SCREENS; i++)
        inst->minis[i] = NULL;

    /* Render thumbnails for all existing screens */
    inst->miniCount = (ULONG)project->screenCount;
    for (i = 0; i < inst->miniCount; i++)
        ScreenCarousel_EnsureMini(inst, i);

    return (ULONG)newObj;
}

/* ------------------------------------------------------------------ */
/* OM_DISPOSE                                                           */
/* ------------------------------------------------------------------ */

ULONG ScreenCarousel_OnDispose(Class *cl, Object *o, Msg msg)
{
    ScreenCarouselData *inst = (ScreenCarouselData *)INST_DATA(cl, o);
    ULONG i;

    for (i = 0; i < PETSCII_MAX_SCREENS; i++) {
        if (inst->minis[i]) {
            PetsciiScreenMini_Destroy(inst->minis[i]);
            inst->minis[i] = NULL;
        }
    }
    if (inst->clipRegion) {
        DisposeRegion( inst->clipRegion );
        inst->clipRegion = NULL;
    }


    return DoSuperMethodA(cl, o, (APTR)msg);
}

/* ------------------------------------------------------------------ */
/* OM_SET                                                               */
/* ------------------------------------------------------------------ */

ULONG ScreenCarousel_OnSet(Class *cl, Object *o, struct opSet *msg)
{
    ScreenCarouselData *inst = (ScreenCarouselData *)INST_DATA(cl, o);
    struct TagItem     *tstate = msg->ops_AttrList;
    struct TagItem     *tag;
    ULONG               result = 0;
    ULONG               i;

    while ((tag = NextTagItem(&tstate)) != NULL)
    {
        switch (tag->ti_Tag)
        {
            case SCA_Project:
                inst->project = (PetsciiProject *)tag->ti_Data;
                /* Invalidate all minis; rebuild on next render */
                for (i = 0; i < PETSCII_MAX_SCREENS; i++) {
                    if (inst->minis[i])
                        inst->minis[i]->valid = 0;
                }
                inst->miniCount = inst->project
                                  ? (ULONG)inst->project->screenCount : 0;

                result = 1;
                break;

            case SCA_Style:
                inst->style = (PetsciiStyle *)tag->ti_Data;
                /* Colour change: invalidate all rendered minis */
                for (i = 0; i < PETSCII_MAX_SCREENS; i++) {
                    if (inst->minis[i])
                        inst->minis[i]->valid = 0;
                }
                result = 1;
                break;

            case SCA_CurrentScreen:
                if (tag->ti_Data != inst->currentScreen) {
                    inst->currentScreen = (ULONG)tag->ti_Data;
                    result = 1;
                }
                break;

            case SCA_ScreenModified:
            {
                /* Re-render exactly one thumbnail */
                ULONG idx = (ULONG)tag->ti_Data;
                if (idx < PETSCII_MAX_SCREENS) {
                    /* Sync miniCount with project */
                    if (inst->project)
                        inst->miniCount = (ULONG)inst->project->screenCount;
                    ScreenCarousel_EnsureMini(inst, idx);
                    result = 1;
                }
                break;
            }

            case SCA_AllModified:
                if (tag->ti_Data) {
                    if (inst->project)
                        inst->miniCount = (ULONG)inst->project->screenCount;
                    for (i = 0; i < inst->miniCount; i++)
                        ScreenCarousel_EnsureMini(inst, i);
                    result = 1;
                }
                break;

            default:
                break;
        }
    }

    return result;
}

/* ------------------------------------------------------------------ */
/* OM_GET                                                               */
/* ------------------------------------------------------------------ */

ULONG ScreenCarousel_OnGet(Class *cl, Object *o, struct opGet *msg)
{
    ScreenCarouselData *inst = (ScreenCarouselData *)INST_DATA(cl, o);

    switch (msg->opg_AttrID)
    {
        case SCA_CurrentScreen:
            *msg->opg_Storage = inst->currentScreen;
            return 1;

        case SCA_SignalScreen:
            *msg->opg_Storage = inst->signalScreen;
            return 1;

        default:
            return DoSuperMethodA(cl, o, (APTR)msg);
    }
}
