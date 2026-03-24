# Makefile for PetMate installer package
#
# Run this on the Amiga from inside the Installer/ directory.
# Assembles all files needed for distribution into $(DIST)/.
#
# Prerequisites:
#   Catalogs compiled : cd ../catalogs && make all
#   Binary built      : ../src/PetMate exists
#
# Usage:
#   make all    - create the installer package directory
#   make clean  - delete the package directory

DIST    = ram:PetMate
#BINARY  = /src/PetMate
BINARY  = /build-AmigaGcc-Release/PetMate
ICON    = /src/PetMate.info
GUIDE   = /PetMate.guide
CATS    = /catalogs
LICENCE  = /LICENSE
README   = /PetMate.readme
EXAMPLES = /testfiles

all: dist-dirs copy-files
	@echo "Installer package ready in $(DIST)/"

dist-dirs:
	-makedir $(DIST)
	-makedir $(DIST)/catalogs
	-makedir $(DIST)/examples
	-makedir $(DIST)/catalogs/fr
	-makedir $(DIST)/catalogs/de
	-makedir $(DIST)/catalogs/es
	-makedir $(DIST)/catalogs/it
	-makedir $(DIST)/catalogs/da
	-makedir $(DIST)/catalogs/pl

copy-files:
	copy Install                              $(DIST)/Install
	copy Install.info                         $(DIST)/Install.info
	copy $(BINARY)                            $(DIST)/PetMate
	copy $(ICON)                              $(DIST)/PetMate.info
	copy $(GUIDE)                             $(DIST)/PetMate.guide
	copy $(LICENCE)                           $(DIST)/LICENSE
	copy $(README)                            $(DIST)/PetMate.readme
	copy $(README).info						$(DIST)/PetMate.readme.info
	copy $(CATS)/PetMate.cd                   $(DIST)/catalogs/PetMate.cd
	copy $(CATS)/fr/PetMate.catalog           $(DIST)/catalogs/fr/PetMate.catalog
	copy $(CATS)/de/PetMate.catalog           $(DIST)/catalogs/de/PetMate.catalog
	copy $(CATS)/es/PetMate.catalog           $(DIST)/catalogs/es/PetMate.catalog
	copy $(CATS)/it/PetMate.catalog           $(DIST)/catalogs/it/PetMate.catalog
	copy $(CATS)/da/PetMate.catalog           $(DIST)/catalogs/da/PetMate.catalog
	copy $(CATS)/pl/PetMate.catalog           $(DIST)/catalogs/pl/PetMate.catalog
	copy $(CATS)/fr/PetMate.guide             $(DIST)/catalogs/fr/PetMate.guide
	copy $(CATS)/de/PetMate.guide             $(DIST)/catalogs/de/PetMate.guide
	copy $(CATS)/es/PetMate.guide             $(DIST)/catalogs/es/PetMate.guide
	copy $(CATS)/it/PetMate.guide             $(DIST)/catalogs/it/PetMate.guide
	copy $(CATS)/da/PetMate.guide             $(DIST)/catalogs/da/PetMate.guide
	copy $(CATS)/pl/PetMate.guide             $(DIST)/catalogs/pl/PetMate.guide
	copy $(EXAMPLES)/krabob_logo.petmate      $(DIST)/examples/krabob_logo.petmate

clean:
	-delete $(DIST) ALL
