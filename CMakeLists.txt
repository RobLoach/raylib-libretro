cmake_minimum_required(VERSION 3.11)
project(raylib-libretro
    DESCRIPTION "raylib-libretro: libretro frontend using raylib."
    HOMEPAGE_URL "https://github.com/robloach/raylib-libretro"
    VERSION 0.0.6
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
    vendor/libretro-common/compat/compat_strl.c
    vendor/libretro-common/compat/compat_posix_string.c
    vendor/libretro-common/string/stdstring.c
    vendor/libretro-common/encodings/encoding_utf.c
    vendor/libretro-common/file/file_path.c
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
install(TARGETS rlibretro DESTINATION lib)
install(FILES include/rlibretro.h DESTINATION include)
install(FILES include/rlibretro-map.h DESTINATION include)

# raylib-libretro binary
add_executable(raylib-libretro
    bin/raylib-libretro.c
)
target_link_libraries(raylib-libretro PUBLIC
    rlibretro
)
target_include_directories(raylib-libretro PUBLIC
    vendor/raygui/src
)
install(TARGETS raylib-libretro DESTINATION bin)
install(FILES README.md DESTINATION share/raylib-libretro)
install(FILES LICENSE DESTINATION share/raylib-libretro)

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

set(CPACK_PACKAGE_VENDOR "Rob Loach")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_GENERATOR "TGZ;ZIP")
set(CPACK_PACKAGE_EXECUTABLES "raylib-libretro" "raylib-libretro")

include(CPack)
