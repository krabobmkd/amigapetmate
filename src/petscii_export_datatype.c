/*
 * petscii_export_datatype.c - Export PETSCII screen as images
 */

#include <stdio.h>
#include <proto/exec.h>

#include <datatypes/datatypes.h>
#include <datatypes/pictureclass.h>
#include <proto/datatypes.h>
#include <proto/utility.h>

#include "petscii_export.h"
#include "petscii_types.h"
#include "petscii_screen.h"
/* tehse manage C64 data to chunky bitmap data: */
#include "petscii_screenbuf.h"
#include "petscii_style.h"
#include "petmate.h"

extern struct Library *DataTypesBase;
/*
 * #define	DTF_TYPE_MASK	0x000F
#define	DTF_BINARY	0x0000
#define	DTF_ASCII	0x0001
#define	DTF_IFF		0x0002
#define	DTF_MISC	0x0003
GIF: The DTF value for GIF is DTF_TYPE_GIF (defined as 0x00000002). This flag indicates the file is a GIF image.

PNG: The DTF value for PNG is DTF_TYPE_PNG (defined as 0x00000004).
*/

/*
APTR exportIndexedImageToPNG(const UBYTE *pixels,
UWORD width,
UWORD height,
UWORD depth,
const UBYTE *palette,
const char *filename,
BOOL isPNG)
*/
int PetsciiExport_SaveDatatype(const PetsciiScreen *scr, const char *filename, int exportDTFenum)
{
    PetsciiScreenBuf *screenbuf;
    struct pdtBlitPixelArray bpa;
    struct BitMapHeader *bmh;
    struct BitMap *bm;
    Object *dto = NULL;
    ULONG result;
    struct dtObject *dtobj;
    struct TagItem tags[10];
   // UBYTE *imageData;
    UWORD bytesPerLine;
    UWORD totalSize;
    UWORD i;

    UWORD width,height;
    const UBYTE *pixels;
    UWORD depth = 4;
    struct ColorRegister colors[C64_COLOR_COUNT];

    if(!DataTypesBase)
    {
        return PETSCII_EXPORT_ENOMEM;
    }

    if(!scr) return PETSCII_EXPORT_ENOMEM;

    screenbuf = PetsciiScreenBuf_Create(scr->width, scr->height);
    if(!screenbuf) return PETSCII_EXPORT_ENOMEM;

    pixels = screenbuf->chunky;
    width = screenbuf->pixW;
    height = screenbuf->pixH;

    /* get the current selected palette */
    for (i = 0; i < C64_COLOR_COUNT; i++) {
        ULONG rgb = app->style.c64pens[i].rgbcolor;
        colors[i].red   = (rgb>>16) & 0x0ff;
       colors[i].green  = (rgb>>8)& 0x0ff;
       colors[i].blue   =  rgb & 0x0ff;;
    }

//DTF_IFF
    // Create a new Datatype object for output
    dto = NewDTObject(filename,
              DTA_SourceType,DTST_FILE,
                DTA_GroupID,GID_PICTURE,
                      // DTA_SourceType, DTST_FILE,
                      // PDTA_SourceMode, PMODE_V42,
                       PDTA_DestMode, PMODE_V42,

                    PDTA_ColorRegisters,(ULONG)&colors[0],
                    PDTA_NumColors,16,

                      TAG_DONE);

printf("dto %08x\n",(int)dto);
    if (!dto) {
        PetsciiScreenBuf_Destroy(screenbuf);
        return PETSCII_EXPORT_ENOMEM; // Failed to create Datatype object
    }

    // Prepare the pixel array write operation
    bpa.MethodID = PDTM_WRITEPIXELARRAY;
    bpa.pbpa_PixelData = (APTR)pixels;
    bpa.pbpa_PixelFormat = PBPAFMT_LUT8; // (depth == 32) ? PBPAFMT_RGBA : PBPAFMT_RGB;
    bpa.pbpa_PixelArrayMod =width; //  width * depth / 8;
    bpa.pbpa_Left = 0;
    bpa.pbpa_Top = 0;
    bpa.pbpa_Width = width;
    bpa.pbpa_Height = height;

    // Perform the write operation
    result = DoMethod(dto, (Msg)&bpa);
printf("result %08x\n",(int)result);
    // Clean up
    DisposeDTObject(dto);

    PetsciiScreenBuf_Destroy(screenbuf);

    return ((result)?PETSCII_EXPORT_OK:PETSCII_EXPORT_EWRITE);
    // bytesPerLine = ((width + 15) / 16) * 2; // Word-aligned width
    // totalSize = bytesPerLine * height;

//     // Allocate and initialize bitmap data
//    // imageData = AllocMem(totalSize, MEMF_CLEAR);
//  //   if (!imageData) return NULL;

//     // Copy pixel data (assuming 1-byte per pixel, indexed)
//     // for (i = 0; i < height; i++) {
//     //     memcpy(&imageData[i * bytesPerLine], &pixels[i * width], width);
//     // }

//     // Create BitMapHeader
//     bmh = AllocMem(sizeof(struct BitMapHeader), MEMF_CLEAR);
//     if (!bmh) goto error;

//     bmh->bmh_Width = width;
//     bmh->bmh_Height = height;
//     bmh->bmh_Depth = depth;
//     bmh->bmh_Planes = depth;
//     bmh->bmh_Colors = 1 << depth;
//     bmh->bmh_XOffset = 0;
//     bmh->bmh_YOffset = 0;

//     // Allocate BitMap
//      bm = AllocBitMap(width, height, depth, BMF_CLEAR, NULL);
//      if (!bm) goto error;

//     // Copy pixel data to BitMap
// // /    CopyMem(imageData, bm->Planes[0], totalSize);

//     // Prepare tags for Datatype object
//     tags[0].ti_Tag = DTA_SourceType;
//     tags[0].ti_Data = DTST_MEMORY;
//     tags[1].ti_Tag = DTA_GroupID;
//     tags[1].ti_Data = GID_PICTURE;
//     tags[2].ti_Tag = PDTA_DestMode;
//     tags[2].ti_Data = PMODE_V43;
//     tags[3].ti_Tag = PDTA_BitMap;
//     tags[3].ti_Data = (ULONG)bm;
//     tags[4].ti_Tag = PDTA_BitMapHeader;
//     tags[4].ti_Data = (ULONG)bmh;

//     tags[7].ti_Tag = PDTA_FileType;
//     // tags[7].ti_Data = exportDTFenum;  //isPNG ? DTFT_PNG : DTFT_GIF;


//     // tags[5].ti_Tag = PDTA_Palette;
//     // tags[5].ti_Data = (ULONG)palette;
//     // tags[6].ti_Tag = PDTA_SourceFile;
//     // tags[6].ti_Data = (ULONG)filename;
//     // tags[7].ti_Tag = PDTA_FileType;
//     // tags[7].ti_Data = exportDTFenum;  //isPNG ? DTFT_PNG : DTFT_GIF;
//     tags[8].ti_Tag = TAG_DONE;
// // /*

// /* PDTM_WRITEPIXELARRAY, PDTM_READPIXELARRAY */
// // struct pdtBlitPixelArray
// // {
// // 	ULONG	MethodID;
// // 	APTR	pbpa_PixelData;		/* The pixel data to transfer to/from */
// // 	ULONG	pbpa_PixelFormat;	/* Format of the pixel data (see "Pixel Formats" below) */
// // 	ULONG	pbpa_PixelArrayMod;	/* Number of bytes per row */
// // 	ULONG	pbpa_Left;		/* Left edge of the rectangle to transfer pixels to/from */
// // 	ULONG	pbpa_Top;		/* Top edge of the rectangle to transfer pixels to/from */
// // 	ULONG	pbpa_Width;		/* Width of the rectangle to transfer pixels to/from */
// // 	ULONG	pbpa_Height;		/* Height of the rectangle to transfer pixels to/from */
// // };

//     // Create Datatype object
//     dto = NewDTObject(filename,tags);
//     if (!dto) goto error;

//     // Save the image
//     if (IDoMethod(dto, PDTM_WRITEPIXELARRAY,
//              NULL,
//               0,
//               0,
//                0,
//                0,
//                 0) != 0) {
//         // Save successful
//         DisposeDTObject(dto);
//         FreeMem(imageData, totalSize);
//         FreeBitMap(bm);
//         FreeMem(bmh, sizeof(struct BitMapHeader));
//         if(screenbuf) PetsciiScreenBuf_Destroy(screenbuf);
//         return (APTR)1; // Success
//     }

//error:


//     return NULL;
  

}
