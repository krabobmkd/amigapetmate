# 🌸 Amiga PetMate

*The birds are back. The flowers are waking up. And somewhere, on a Motorola 68020,*
*a little program is drawing tiny pixel characters in sixteen glorious colors.*

---

## What is this?

**Amiga PetMate** is a native Amiga port of [PetMate](https://github.com/nurpax/petmate),
the beloved C64 PETSCII art editor — rewritten from scratch in **C89**, speaking fluent
Amiga APIs, running on any **68020 or better** Amiga with **AmigaOS 3.x**.

No JavaScript. No Electron. No 47-megabyte runtime. Just your Amiga, your imagination,
and 256 characters from the good old Commodore character ROM.

The original TypeScript soul lives on in two places — go thank them:
- https://github.com/nurpax/petmate
- https://github.com/wbochar/petmate9

This fork is a **parallel reimplementation**: same spirit, same file format (`.petmate` JSON),
new body made entirely of C and Amiga magic.

---

## Features

- Draw, colorize, paint with brushes, type text — the full PETSCII experience
- Load and save `.petmate` project files (compatible with the original)
- Export to **ASM**, **BASIC**, **PRG** (standalone C64 executable), **GIF**, **IFF/ILBM**
- Import from images (PNG, GIF, BMP…) — auto-detects borders and character alignment
- Import from ASM-export **PRG** files — reads the 6510 machine code to locate the data
- Localized interface: **English, Français, Deutsch, Español, Italiano, Dansk, Polski**
- Multiple screens per project, undo/redo, full-screen mode, custom palettes
- Runs beautifully on a real Amiga or your favourite emulator

---

## Licence

**MIT.** Do whatever you want with it.
See the `LICENSE` file. It is short. It is friendly. It smells faintly of spring.

---

## For Developers — How to Build

### Cross-compilation on Linux/Mac (recommended for mortals)

The project uses **CMake** with a cross-compilation toolchain for AmigaOS 3.x:

```bash
cd src
cmake -DCMAKE_TOOLCHAIN_FILE=your-m68k-toolchain.cmake .
make
```

`src/CMakeLists.txt` handles all sources, BOOPSI gadget classes, giflib, cJSON,
and the various export/import modules. Point it at your cross-compiler and it does
the rest.

### Native compilation on the Amiga itself

If you are the proud owner of an Amiga with **GCC 2.95** installed — for instance via
the **ADE package on Aminet** — you can compile natively:

```
cd src
make -f makefile
```

`src/makefile` is written for the Amiga GCC 2.95 / ADE environment.
Put on some music. Make a coffee. The Amiga compiles at its own pace, and it is a
dignified pace.

### Building the installer package

Once the binary is built:

```
cd Installer
make
```

This assembles everything into `ram:PetMate/` — binary, icon, guide, catalogs, readme,
licence — ready to be archived and shipped. Double-click `Install` to run the standard
Amiga `Installer` script and deploy to your chosen drawer.

---

## The Philosophical Corner

This project exists for two reasons beyond the obvious:

1. **To explore BOOPSI private gadget classes** — the canvas, the color picker,
   the character selector, the screen carousel are all hand-rolled BOOPSI classes.
   If you ever wondered how to build a non-trivial Amiga application using nothing
   but the system's own object model, here it is, warts and all.

2. **To prove that you can write a real, usable application using only Amiga APIs**,
   no third-party UI toolkit, no cross-platform abstraction layer.
   Just Intuition, GadTools, DataTypes, Locale, AmigaGuide, and a deep breath.

Spring is coming. The trees are going green. Somewhere a C64 is booting.
Everything is going to be alright.

---

## Bugs? Missing features? Opinions?

Yes, please. Open an issue:

👉 **https://github.com/krabobmkd/amigapetmate/issues**

Polite bug reports, wild feature requests, and haiku about PETSCII are all welcome.
