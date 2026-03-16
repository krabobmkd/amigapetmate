#ifndef CHAR_SELECTOR_PRIVATE_H
#define CHAR_SELECTOR_PRIVATE_H

#include <exec/types.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>
#include <intuition/intuition.h>

#include "compilers.h"
#include "char_selector.h"
#include "petscii_style.h"
#include "petscii_charset.h"
#include "petscii_screenbuf.h"  /* for PetsciiScreenBuf (fakeBuf trick) */
#include "petmate_charset.h"   /* Petmate display ordering tables       */

#ifndef G
#define G(o) ((struct Gadget *)(o))
#endif

/*
enum for render type,  renderType
*/
#define RENDT_WRITECHUNKYPIXEL8 0
#define RENDT_CGXRGBCLUT 1
#define RENDT_OCS 2



typedef struct CharSelectorData {
    PetsciiStyle *style;
    struct Region *clipRegion;

    UBYTE         charset;       /* PETSCII_CHARSET_UPPER or _LOWER */
    UBYTE         fgColor;       /* 0-15 */
    UBYTE         bgColor;       /* 0-15 */
    UBYTE         selectedChar;  /* 0-255 */

    UBYTE        *cbuf;          /* 128*128 native render buffer, owned */

    /* Cached scaled buffer (same pattern as PetsciiCanvas) */
    UBYTE        *scaledBuf;

    UWORD         scaledW;
    UWORD         scaledH;

    BOOL          valid;         /* 0 = needs rebuild */
    /* Aspect-ratio preservation */
    BOOL          keepRatio;  /* TRUE = preserve 1:1 ratio with margins   */

    /* Content sub-rectangle within the gadget bounds (gadget-relative).
     * Updated by GM_LAYOUT; safety-recomputed in GM_RENDER.
     * When keepRatio=FALSE: X=0, Y=0, W/H = full gadget size.
     * Read by the input handler to map mouse coords to character cells. */
    WORD          contentX;
    WORD          contentY;
    WORD          contentW;
    WORD          contentH;

    /* draw optimisation */
    BOOL              refreshExtraMarge;

    WORD    renderType;
} CharSelectorData;

ULONG CharSelector_OnNew        (Class *cl, Object *o, struct opSet     *msg);
ULONG CharSelector_OnDispose    (Class *cl, Object *o, Msg               msg);
ULONG CharSelector_OnSet        (Class *cl, Object *o, struct opSet     *msg);
ULONG CharSelector_OnGet        (Class *cl, Object *o, struct opGet     *msg);
ULONG CharSelector_OnDomain     (Class *cl, Object *o, struct gpDomain  *msg);
ULONG CharSelector_OnRender     (Class *cl, Object *o, struct gpRender  *msg);
ULONG CharSelector_OnLayout     (Class *cl, Object *o, struct gpLayout  *msg);
ULONG CharSelector_OnHitTest    (Class *cl, Object *o, struct gpHitTest *msg);
ULONG CharSelector_OnGoActive   (Class *cl, Object *o, struct gpInput   *msg);
ULONG CharSelector_OnHandleInput(Class *cl, Object *o, struct gpInput   *msg);
ULONG CharSelector_OnGoInactive (Class *cl, Object *o, struct gpGoInactive *msg);

ULONG ASM SAVEDS CharSelector_Dispatch(
    REG(a0, Class *cl),
    REG(a2, Object *o),
    REG(a1, Msg msg));

#endif /* CHAR_SELECTOR_PRIVATE_H */
