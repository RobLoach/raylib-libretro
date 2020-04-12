cmake_minimum_required(VERSION 3.11)
project(raylib-libretro
	DESCRIPTION "raylib-libretro: libretro frontend using raylib."
	HOMEPAGE_URL "https://github.com/robloach/raylib-libretro"
	VERSION 0.0.1
	LANGUAGES C)

# raylib
find_package(raylib)
if (NOT raylib_FOUND)
	set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
	set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
	set(BUILD_TESTING  OFF CACHE BOOL "" FORCE)
	add_subdirectory(vendor/raylib)
endif()

# raylib-libretro
add_executable(raylib-libretro
	bin/raylib-libretro.c
	vendor/libretro-common/compat/compat_strl.c
	vendor/libretro-common/compat/compat_posix_string.c
	vendor/libretro-common/encodings/encoding_utf.c
	vendor/libretro-common/dynamic/dylib.c
)
target_link_libraries(raylib-libretro PUBLIC
	raylib
	${CMAKE_DL_LIBS}
)
target_include_directories(raylib-libretro PUBLIC
	vendor/libretro-common/include
)
target_compile_definitions(raylib-libretro PRIVATE
	HAVE_DYNAMIC=1
)