# Makefile for PetMate - Amiga native GCC 2.95
# Usage: make          (defaults to 68020)
#        make MCPU=m68030
# Place this file in the src/ directory and run make from there.
# Requires GNU make and a native GCC 2.95 installation on the Amiga.
#
# Note: giflib sources use stdbool.h / stdint.h (C99 headers).
# If GCC 2.95 lacks those headers, provide compatibility shims
# in ../giflib or point INCLUDES at a directory that has them.

MCPU     ?= m68020
CC        = gcc
BUILDDIR  = build-gcc-$(MCPU)

# 	-DUSE_DEBUG_BDBPRINT=1

DEFINES = \
	-DPETSCIICANVAS_STATICLINK=1 \
	-DCHARSELECTOR_STATICLINK=1 \
	-DCOLORPICKER_STATICLINK=1


INCLUDES = \
	-I. \
	-IPetsciiCanvas \
	-ICharSelector \
	-IColorPicker \
	-IScreenCarousel \
	-I../giflib

CFLAGS = -$(MCPU) -O2 -noixemul $(INCLUDES) $(DEFINES)

# Search path for .c source files (VPATH).
# All source filenames across these directories are unique, so a flat
# build directory is safe.
VPATH = .:PetsciiCanvas:CharSelector:ColorPicker:ScreenCarousel:cjson:../giflib

# -----------------------------------------------------------------------
# Object list - mirrors the source groups in CMakeLists.txt
# -----------------------------------------------------------------------

# Core
OBJS = \
	$(BUILDDIR)/petmate.o \
	$(BUILDDIR)/petscii_screen.o \
	$(BUILDDIR)/petscii_project.o \
	$(BUILDDIR)/petscii_palette.o \
	$(BUILDDIR)/petscii_charset.o \
	$(BUILDDIR)/petscii_undo.o \
	$(BUILDDIR)/petscii_brush.o \
	$(BUILDDIR)/petscii_ascii.o \
	$(BUILDDIR)/petscii_chartransform.o \
	$(BUILDDIR)/bdbprintf.o

# Framework
OBJS += \
	$(BUILDDIR)/petscii_style.o \
	$(BUILDDIR)/pmlocale.o \
	$(BUILDDIR)/pmstring.o \
	$(BUILDDIR)/pmaction.o \
	$(BUILDDIR)/pmmenu.o \
	$(BUILDDIR)/boopsimessage.o \
	$(BUILDDIR)/boopsimainwindow.o \
	$(BUILDDIR)/appsettings.o \
	$(BUILDDIR)/pmsettingsview.o \
	$(BUILDDIR)/tooltypepref.o \
	$(BUILDDIR)/class_charlayout.o \
	$(BUILDDIR)/class_colorswatch.o \
	$(BUILDDIR)/class_layoutwithpopup.o

# Screen render buffer + miniature thumbnails
OBJS += \
	$(BUILDDIR)/petscii_screenbuf.o \
	$(BUILDDIR)/petscii_screenmini.o

# JSON parser + .petmate file I/O
OBJS += \
	$(BUILDDIR)/cJSON.o \
	$(BUILDDIR)/petscii_fileio.o

# Export formats
OBJS += \
	$(BUILDDIR)/petscii_export_bas.o \
	$(BUILDDIR)/petscii_export_asm.o \
	$(BUILDDIR)/petscii_export_seq.o \
	$(BUILDDIR)/petscii_export_prg_asm.o \
	$(BUILDDIR)/petscii_export_prg_bas.o \
	$(BUILDDIR)/petscii_export_ilbm.o \
	$(BUILDDIR)/petscii_export_gif.o

# Import formats
OBJS += \
	$(BUILDDIR)/petscii_import_image.o \
	$(BUILDDIR)/petscii_import_prg.o

# PetsciiCanvas BOOPSI gadget class
OBJS += \
	$(BUILDDIR)/class_petsciicanvas_lib.o \
	$(BUILDDIR)/class_petsciicanvas_dispatch.o \
	$(BUILDDIR)/class_petsciicanvas_attribs.o \
	$(BUILDDIR)/class_petsciicanvas_render.o \
	$(BUILDDIR)/class_petsciicanvas_handleinput.o

# CharSelector BOOPSI gadget class
OBJS += \
	$(BUILDDIR)/class_charselector_lib.o \
	$(BUILDDIR)/class_charselector_dispatch.o \
	$(BUILDDIR)/class_charselector_attribs.o \
	$(BUILDDIR)/class_charselector_render.o \
	$(BUILDDIR)/class_charselector_handleinput.o \
	$(BUILDDIR)/petmate_charset.o

# ColorPicker BOOPSI gadget class
OBJS += \
	$(BUILDDIR)/class_colorpicker_lib.o \
	$(BUILDDIR)/class_colorpicker_dispatch.o \
	$(BUILDDIR)/class_colorpicker_attribs.o \
	$(BUILDDIR)/class_colorpicker_render.o \
	$(BUILDDIR)/class_colorpicker_handleinput.o

# Views / toolbar
OBJS += \
	$(BUILDDIR)/pmtoolbar.o

# ScreenCarousel BOOPSI gadget class
OBJS += \
	$(BUILDDIR)/class_screencarousel_lib.o \
	$(BUILDDIR)/class_screencarousel_dispatch.o \
	$(BUILDDIR)/class_screencarousel_attribs.o \
	$(BUILDDIR)/class_screencarousel_render.o \
	$(BUILDDIR)/class_screencarousel_input.o

# giflib - static GIF encoder (sources live in ../giflib)
OBJS += \
	$(BUILDDIR)/dgif_lib.o \
	$(BUILDDIR)/egif_lib.o \
	$(BUILDDIR)/gif_err.o \
	$(BUILDDIR)/gif_hash.o \
	$(BUILDDIR)/gifalloc.o

# -----------------------------------------------------------------------
# Targets
# -----------------------------------------------------------------------

all: $(BUILDDIR) PetMate

$(BUILDDIR):
	-mkdir -p $(BUILDDIR)

# Single pattern rule - VPATH resolves each .c to the right directory
$(BUILDDIR)/%.o: %.c $(BUILDDIR)
	$(CC) -c -o $@ $< $(CFLAGS)

PetMate: $(OBJS)
	$(CC) -$(MCPU) -noixemul -o PetMate $(OBJS)

clean:
	-delete ALL FORCE $(BUILDDIR)
