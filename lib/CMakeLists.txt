
# raylib-libretro-static static library
add_library(raylib-libretro-static STATIC
    raylib-libretro-static.c
    ../vendor/libretro-common/compat/compat_strl.c
    ../vendor/libretro-common/compat/compat_posix_string.c
    ../vendor/libretro-common/string/stdstring.c
    ../vendor/libretro-common/encodings/encoding_utf.c
    ../vendor/libretro-common/file/file_path.c
    ../vendor/libretro-common/dynamic/dylib.c
)
target_link_libraries(raylib-libretro-static PUBLIC
    raylib
    raylib-libretro-h
    ${CMAKE_DL_LIBS}
)
target_include_directories(raylib-libretro-static PUBLIC
    ../vendor/libretro-common/include
)
target_compile_definitions(raylib-libretro-static PUBLIC
    HAVE_DYNAMIC=1
)
install(TARGETS raylib-libretro-static DESTINATION lib)
install(FILES ../include/raylib-libretro.h DESTINATION include)
