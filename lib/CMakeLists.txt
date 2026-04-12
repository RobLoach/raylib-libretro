
# raylib-libretro-static static library
add_library(raylib-libretro-static STATIC
    raylib-libretro-static.c
    ../vendor/libretro-common/compat/compat_strl.c
    ../vendor/libretro-common/compat/compat_posix_string.c
    ../vendor/libretro-common/string/stdstring.c
    ../vendor/libretro-common/encodings/encoding_utf.c
    ../vendor/libretro-common/file/file_path.c
    ../vendor/libretro-common/features/features_cpu.c
)
target_link_libraries(raylib-libretro-static PUBLIC
    raylib
    raylib-libretro-h
    wasmtime_c_api
    pthread
    dl
    m
)
target_include_directories(raylib-libretro-static PUBLIC
    ../vendor/libretro-common/include
)
# Ensure the Cargo build of Wasmtime completes before this library is linked.
add_dependencies(raylib-libretro-static wasmtime_cargo_build)
install(TARGETS raylib-libretro-static DESTINATION lib)
install(FILES ../include/raylib-libretro.h DESTINATION include)
