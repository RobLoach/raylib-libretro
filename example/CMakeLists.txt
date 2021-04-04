add_executable(raylib-libretro-basic
    raylib-libretro-basic.c
)
target_link_libraries(raylib-libretro-basic PUBLIC
    raylib-libretro-static
)
