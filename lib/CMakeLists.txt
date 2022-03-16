set(RAYLIB_LIBRETRO_LIB_SOURCES
    raylib-libretro-static.c
    ../vendor/libretro-common/compat/compat_strl.c
    ../vendor/libretro-common/compat/compat_posix_string.c
    ../vendor/libretro-common/string/stdstring.c
    ../vendor/libretro-common/encodings/encoding_utf.c
    ../vendor/libretro-common/file/file_path.c
    ../vendor/libretro-common/dynamic/dylib.c
)

set(RAYLIB_LIBRETRO_LIB_INCLUDES ../vendor/libretro-common/include)
set(RAYLIB_LIBRETRO_LIB_DEFINES HAVE_DYNAMIC=1)

# raylib-libretro-static
add_library(raylib-libretro-static STATIC ${RAYLIB_LIBRETRO_LIB_SOURCES})
target_link_libraries(raylib-libretro-static PUBLIC raylib raylib-libretro-h ${CMAKE_DL_LIBS})
target_include_directories(raylib-libretro-static PUBLIC ${RAYLIB_LIBRETRO_LIB_INCLUDES})
target_compile_definitions(raylib-libretro-static PUBLIC ${RAYLIB_LIBRETRO_LIB_DEFINES})
install(TARGETS raylib-libretro-static DESTINATION lib)
install(FILES ../include/raylib-libretro.h DESTINATION include)

# raylib-libretro-shared
# set(BUILD_SHARED_LIBS TRUE)
# add_library(raylib-libretro-shared SHARED ${RAYLIB_LIBRETRO_LIB_SOURCES})
# set_property(TARGET raylib-libretro-shared PROPERTY POSITION_INDEPENDENT_CODE ON)
# set_property(TARGET raylib-libretro-shared PROPERTY POSITION_INDEPENDENT_CODE ON)
# target_link_libraries(raylib-libretro-shared PUBLIC raylib_static raylib-libretro-h ${CMAKE_DL_LIBS})
# target_include_directories(raylib-libretro-shared PUBLIC ${RAYLIB_LIBRETRO_LIB_INCLUDES})
# target_compile_definitions(raylib-libretro-shared PUBLIC ${RAYLIB_LIBRETRO_LIB_DEFINES})
