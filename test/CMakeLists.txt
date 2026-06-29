# Headless unit tests for the games database layer (include/raylib-libretro-games.h).
# raylib-libretro-static supplies the libretro-common symbols (dylib, cpu_features, ...) the
# core header references; sqlite3 backs the database. No window or cores are needed at runtime.
add_executable(raylib-libretro-test raylib-libretro-games-test.c)

target_link_libraries(raylib-libretro-test PRIVATE
    raylib-libretro-static
    sqlite3
)
target_include_directories(raylib-libretro-test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/vendor/sqlite3
)

add_test(NAME games COMMAND raylib-libretro-test)
