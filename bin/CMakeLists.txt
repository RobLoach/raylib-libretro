# raylib-libretro binary
add_executable(raylib-libretro
    raylib-libretro.c
)
target_link_libraries(raylib-libretro PUBLIC
    raylib-libretro-static
    raylib_zip
)
target_include_directories(raylib-libretro PUBLIC
    ../vendor/raygui/src
    ../vendor/raylib-app
    ../vendor/raylib-zip
)
install(TARGETS raylib-libretro DESTINATION bin)
install(FILES ../README.md DESTINATION share/raylib-libretro)
install(FILES ../LICENSE DESTINATION share/raylib-libretro)
