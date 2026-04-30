set(libsrc
    raylib-libretro-static.c
    ../vendor/libretro-common/compat/compat_strl.c
    ../vendor/libretro-common/compat/compat_posix_string.c
    ../vendor/libretro-common/string/stdstring.c
    ../vendor/libretro-common/encodings/encoding_utf.c
    ../vendor/libretro-common/file/file_path.c
    ../vendor/libretro-common/dynamic/dylib.c
    ../vendor/libretro-common/time/rtime.c
    ../vendor/libretro-common/features/features_cpu.c
)

add_library(objlib OBJECT ${libsrc})
set_property(TARGET objlib PROPERTY POSITION_INDEPENDENT_CODE 1)
target_compile_definitions(objlib PUBLIC HAVE_DYNAMIC=1)

target_include_directories(objlib PUBLIC
    ../vendor/libretro-common/include
    ../include
    ../vendor/raylib/src
    ../vendor/raylib/external
)

target_link_libraries(objlib PUBLIC
    raylib
    raylib-libretro-h
    ${CMAKE_DL_LIBS}
)
