set(libsrc
    raylib-libretro-static.c
    ../vendor/libretro-common/compat/compat_strl.c
    ../vendor/libretro-common/compat/compat_posix_string.c
    ../vendor/libretro-common/string/stdstring.c
    ../vendor/libretro-common/encodings/encoding_utf.c
    ../vendor/libretro-common/file/file_path.c
    ../vendor/libretro-common/dynamic/dylib.c
    ../vendor/libretro-common/time/rtime.c
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

# add_library(raylib-libretro-static STATIC $<TARGET_OBJECTS:objlib>)

# install(TARGETS raylib-libretro-static DESTINATION lib)
# install(FILES ../include/raylib-libretro.h DESTINATION include)


# # raylib-libretro-static static library
# add_library(raylib-libretro-static STATIC ${libsrc})
# target_link_libraries(raylib-libretro-static PUBLIC
#     raylib
#     raylib-libretro-h
#     ${CMAKE_DL_LIBS}
# )
# target_include_directories(raylib-libretro-static PUBLIC
#     ../vendor/libretro-common/include
# )
# target_compile_definitions(raylib-libretro-static PUBLIC
#     HAVE_DYNAMIC=1
# )
# install(TARGETS raylib-libretro-static DESTINATION lib)
# install(FILES ../include/raylib-libretro.h DESTINATION include)

# add_library(raylib-libretro-shared SHARED ${libsrc})
# target_link_libraries(raylib-libretro-shared PUBLIC
#     raylib
#     raylib-libretro-h
#     ${CMAKE_DL_LIBS}
# )
# target_include_directories(raylib-libretro-shared PUBLIC
#     ../vendor/libretro-common/include
# )
# target_compile_definitions(raylib-libretro-shared PUBLIC
#     HAVE_DYNAMIC=1
# )
# install(TARGETS raylib-libretro-shared DESTINATION lib)
