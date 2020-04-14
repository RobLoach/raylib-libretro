cmake_minimum_required(VERSION 3.11)
project(raylib-libretro
    DESCRIPTION "raylib-libretro: libretro frontend using raylib."
    HOMEPAGE_URL "https://github.com/robloach/raylib-libretro"
    VERSION 0.0.1
    LANGUAGES C
)

# raylib
find_package(raylib QUIET)
if (NOT raylib_FOUND)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING  OFF CACHE BOOL "" FORCE)
    add_subdirectory(vendor/raylib)
endif()

# rlibretro static library
add_library(rlibretro STATIC
    include/rlibretro.h
    include/rlibretro-map.h
    lib/rlibretro.c
    vendor/libretro-common/dynamic/dylib.c
)
target_link_libraries(rlibretro PUBLIC
    raylib
    ${CMAKE_DL_LIBS}
)
target_include_directories(rlibretro PUBLIC
    vendor/libretro-common/include
    include
)
target_compile_definitions(rlibretro PUBLIC
    HAVE_DYNAMIC=1
)

# raylib-libretro binary
add_executable(raylib-libretro
    bin/raylib-libretro.c
)
target_link_libraries(raylib-libretro PUBLIC
    rlibretro
)

# Examples
option(BUILD_RLIBRETRO_EXAMPLES "rlibretro Examples" ON)
if (BUILD_RLIBRETRO_EXAMPLES)
    add_executable(rlibretro_basic_window
        examples/rlibretro_basic_window.c
    )
    target_link_libraries(rlibretro_basic_window PUBLIC
        rlibretro
    )
endif()
