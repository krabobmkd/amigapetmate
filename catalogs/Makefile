# Makefile for PetMate locale catalogs
#
# Requires FlexCat: https://github.com/adtools/flexcat
# On Amiga: FlexCat is in C:FlexCat or available from Aminet.
# Cross-compile on Linux: install flexcat via apt or build from source.
#
# Usage:
#   make all          - compile all language catalogs
#   make install      - copy binary .catalog files to LOCALE:Catalogs/
#   make clean        - remove compiled .catalog files
#
# Each compiled catalog must be installed to:
#   LOCALE:Catalogs/<language>/PetMate.catalog
#
# Amiga language directory names:
#   français   (French)
#   deutsch    (German)
#   español    (Spanish)
#   italiano   (Italian)

FLEXCAT = FlexCat
CD      = PetMate.cd

CATALOGS = \
	fr/PetMate.catalog \
	de/PetMate.catalog \
	es/PetMate.catalog \
	it/PetMate.catalog \
	da/PetMate.catalog \
	pl/PetMate.catalog

all: $(CATALOGS)

fr/PetMate.catalog: $(CD) fr/PetMate.ct
	$(FLEXCAT) $(CD) fr/PetMate.ct CATALOG=$@

de/PetMate.catalog: $(CD) de/PetMate.ct
	$(FLEXCAT) $(CD) de/PetMate.ct CATALOG=$@

es/PetMate.catalog: $(CD) es/PetMate.ct
	$(FLEXCAT) $(CD) es/PetMate.ct CATALOG=$@

it/PetMate.catalog: $(CD) it/PetMate.ct
	$(FLEXCAT) $(CD) it/PetMate.ct CATALOG=$@

da/PetMate.catalog: $(CD) da/PetMate.ct
	$(FLEXCAT) $(CD) da/PetMate.ct CATALOG=$@

pl/PetMate.catalog: $(CD) pl/PetMate.ct
	$(FLEXCAT) $(CD) pl/PetMate.ct CATALOG=$@
# Install to Amiga LOCALE: assign (run on Amiga only)
install: all
	copy fr/PetMate.catalog LOCALE:Catalogs/fran\xe7ais/PetMate.catalog
	copy de/PetMate.catalog LOCALE:Catalogs/deutsch/PetMate.catalog
	copy es/PetMate.catalog LOCALE:Catalogs/espa\xf1ol/PetMate.catalog
	copy it/PetMate.catalog LOCALE:Catalogs/italiano/PetMate.catalog

clean:
	-delete fr/PetMate.catalog
	-delete de/PetMate.catalog
	-delete es/PetMate.catalog
	-delete it/PetMate.catalog
	-delete da/PetMate.catalog
	-delete pl/PetMate.catalog	
	
