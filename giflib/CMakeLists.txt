cmake_minimum_required(VERSION 3.10)
project(giflib C)

# giflib 4.2.x - static library for AmigaOS 68000 cross-compilation.
#
# The sources use <stdbool.h> and <stdint.h> which require at least C99.
# config.h is not #included by the library sources, but is provided here
# as a hand-crafted file for the m68k-amigaos (big-endian, ILP32) target
# so that any future patches or extensions can rely on it.

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# --- config.h for the Amiga/68000 target -----------------------------------
# Copy the hand-crafted config.h.amiga into the build directory as config.h
# so it is available on the include path without polluting the source tree.
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config.h.amiga
    ${CMAKE_CURRENT_BINARY_DIR}/config.h
    COPYONLY
)

set(GIFLIB_SOURCES
    dgif_lib.c
    egif_lib.c
    gif_err.c
#    gif_font.c
    gif_hash.c
    gifalloc.c
)

add_library(gif STATIC ${GIFLIB_SOURCES})

target_include_directories(gif PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
)

# Make the generated config.h visible to giflib sources and consumers
target_include_directories(gif PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Inherit Amiga compiler options from parent if set
if(AMIGA_COMPILER_OPTIONS)
    target_compile_options(gif PRIVATE ${AMIGA_COMPILER_OPTIONS})
endif()
