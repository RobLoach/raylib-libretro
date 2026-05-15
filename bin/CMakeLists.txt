# raylib-libretro binary
add_executable(raylib-libretro
    raylib-libretro.c
)
target_link_libraries(raylib-libretro PUBLIC
    raylib-libretro-static
)
target_include_directories(raylib-libretro PUBLIC
    ../vendor/raygui/src
    ../vendor/raylib-app
)

install(TARGETS raylib-libretro DESTINATION .)
install(FILES ../README.md DESTINATION .)
install(FILES ../LICENSE DESTINATION .)
install(DIRECTORY "${CORES_DIR}/" DESTINATION cores)

# Web
if ("${PLATFORM}" STREQUAL "Web")
    install(FILES index.data DESTINATION .)
    install(FILES index.js DESTINATION .)
    install(FILES index.wasm DESTINATION .)
endif()
