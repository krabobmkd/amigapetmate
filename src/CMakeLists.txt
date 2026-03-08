cmake_minimum_required(VERSION 3.10)
project(PetmateAmiga C)

# C89 standard for maximum Amiga compiler compatibility
set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(AMIGA_USE_FLOAT "use any float or double" OFF)
option(AMIGA_USE_HARDFLOAT "compile using direct 68882 FPU" OFF)
option(AMIGA_USE_68030 "use 68030" ON)
option(AMIGA_USE_68040 "use 68040" OFF)
option(AMIGA_USE_68060 "use 68060" OFF)

# Assembler output for debug
list(APPEND AMIGA_COMPILER_OPTIONS -save-temps)

if(AMIGA_USE_68030)
    list(APPEND AMIGA_COMPILER_OPTIONS -m68030)
elseif(AMIGA_USE_68040)
    list(APPEND AMIGA_COMPILER_OPTIONS -m68040)
elseif(AMIGA_USE_68060)
    list(APPEND AMIGA_COMPILER_OPTIONS -m68060)
else()
    list(APPEND AMIGA_COMPILER_OPTIONS -m68020)
endif()

if(AMIGA_USE_FLOAT)
    if(AMIGA_USE_HARDFLOAT)
        list(APPEND AMIGA_COMPILER_OPTIONS -mhard-float -ffast-math)
    else()
        list(APPEND AMIGA_COMPILER_OPTIONS -msoft-float)
    endif()
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/PetsciiCanvas
    ${CMAKE_CURRENT_SOURCE_DIR}/CharSelector
    ${CMAKE_CURRENT_SOURCE_DIR}/ColorPicker
)

# Phase 1: Core sources
# Phase 8: petscii_undo.c added
# Phase 10: petscii_brush.c added
# Phase 11: petscii_ascii.c added
set(PETMATE_SOURCES
    petmate.c
    petscii_screen.c
    petscii_project.c
    petscii_palette.c
    petscii_charset.c
    petscii_undo.c
    petscii_brush.c
    petscii_ascii.c
    petscii_chartransform.c
    bdbprintf.c
    bdbprintf.h

    petmate_todo.txt

)

# Phase 2: Framework sources
set(PETMATE_FRAMEWORK_SOURCES
    petscii_style.c
    pmlocale.c
    pmstring.c
    pmaction.c
    pmmenu.c
    boopsimessage.c
    boopsimainwindow.c
    appsettings.c
    pmsettingsview.c
    tooltypepref.c
    class_charlayout.c
    class_colorswatch.c
    class_layoutwithpopup.c
)

# Phase 3: Screen render buffer
set(PETMATE_SCREENBUF_SOURCES
    petscii_screenbuf.c
)

# Phase 9: JSON parser (cJSON, Amiga fork) + .petmate file I/O
set(PETMATE_FILEIO_SOURCES
    cjson/cJSON.c
    petscii_fileio.c
)

# Phase 11: Export formats (BAS, ASM, SEQ, IFF ILBM)
set(PETMATE_EXPORT_SOURCES
    petscii_export_bas.c
    petscii_export_asm.c
    petscii_export_seq.c
    petscii_export_ilbm.c
)

# Phase 4+6: PetsciiCanvas BOOPSI gadget class
set(PETMATE_CANVAS_SOURCES
    PetsciiCanvas/class_petsciicanvas_lib.c
    PetsciiCanvas/class_petsciicanvas_dispatch.c
    PetsciiCanvas/class_petsciicanvas_attribs.c
    PetsciiCanvas/class_petsciicanvas_render.c
    PetsciiCanvas/class_petsciicanvas_handleinput.c
)

# Phase 5: CharSelector BOOPSI gadget class
set(PETMATE_CHARSELECTOR_SOURCES
    CharSelector/class_charselector_lib.c
    CharSelector/class_charselector_dispatch.c
    CharSelector/class_charselector_attribs.c
    CharSelector/class_charselector_render.c
    CharSelector/class_charselector_handleinput.c

    CharSelector/petmate_charset.c

)

# Phase 5: ColorPicker BOOPSI gadget class
set(PETMATE_COLORPICKER_SOURCES
    ColorPicker/class_colorpicker_lib.c
    ColorPicker/class_colorpicker_dispatch.c
    ColorPicker/class_colorpicker_attribs.c
    ColorPicker/class_colorpicker_render.c
    ColorPicker/class_colorpicker_handleinput.c
)

# Phase 7: Views and tools
# Phase 10: class_charlayout.c added
set(PETMATE_VIEW_SOURCES
    pmtoolbar.c
    pmscreentabs.c
)

if(AMIGA)
    add_executable(petmate
        ${PETMATE_SOURCES}
        ${PETMATE_FRAMEWORK_SOURCES}
        ${PETMATE_SCREENBUF_SOURCES}
        ${PETMATE_CANVAS_SOURCES}
        ${PETMATE_CHARSELECTOR_SOURCES}
        ${PETMATE_COLORPICKER_SOURCES}
        ${PETMATE_VIEW_SOURCES}
        ${PETMATE_FILEIO_SOURCES}
        ${PETMATE_EXPORT_SOURCES}
        # ${PETMATE_EXTRA_SOURCES}
    )

    install(TARGETS petmate
        ARCHIVE DESTINATION bin
    )

    target_compile_definitions(petmate PRIVATE
        PETSCIICANVAS_STATICLINK=1
        CHARSELECTOR_STATICLINK=1
        COLORPICKER_STATICLINK=1
        USE_DEBUG_BDBPRINT=1
    )

    target_compile_options(petmate PRIVATE
        $<$<CONFIG:RELEASE>:-O2 -Wno-narrowing -DNDEBUG>
        ${AMIGA_COMPILER_OPTIONS}
    )

    # Development tool: generate petscii_chartransform.c/.h
    # Run on Amiga (or emulator), redirect output to project root.
    add_executable(findtables
        findtables.c
        petscii_charset.c
    )
    target_compile_options(findtables PRIVATE ${AMIGA_COMPILER_OPTIONS})
endif()
