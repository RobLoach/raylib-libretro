cmake_minimum_required(VERSION 3.11)
project(raylib-libretro
    DESCRIPTION "raylib-libretro: libretro frontend using raylib."
    HOMEPAGE_URL "https://github.com/robloach/raylib-libretro"
    VERSION 0.0.32
    LANGUAGES C
)

# Release performance: enable link-time / interprocedural optimization (LTO).
# Applied to the Release config only — Debug builds stay fast to compile — and
# set before any target is created so it also covers the vendored raylib. Gated
# on check_ipo_supported so toolchains without LTO configure cleanly instead of
# erroring. Skipped on Web: LTO interacts badly with Emscripten's
# MAIN_MODULE/SIDE_MODULE dynamic linking (cores are dlopen'd at runtime).
if (NOT "${PLATFORM}" STREQUAL "Web")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT RAYLIB_LIBRETRO_IPO_SUPPORTED OUTPUT RAYLIB_LIBRETRO_IPO_ERROR LANGUAGES C)
    if (RAYLIB_LIBRETRO_IPO_SUPPORTED)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
        message(STATUS "raylib-libretro: LTO enabled for Release builds")
    else()
        message(STATUS "raylib-libretro: LTO unavailable, building without it: ${RAYLIB_LIBRETRO_IPO_ERROR}")
    endif()
endif()

# raylib
find_package(raylib QUIET)
if (NOT raylib_FOUND)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING  OFF CACHE BOOL "" FORCE)
    set(CUSTOMIZE_BUILD         ON  CACHE BOOL "" FORCE)
    set(SUPPORT_MODULE_RMODELS  OFF CACHE BOOL "" FORCE)
    set(SUPPORT_CAMERA_SYSTEM      OFF CACHE BOOL "" FORCE)
    set(SUPPORT_GESTURES_SYSTEM    ON CACHE BOOL "" FORCE)
    set(SUPPORT_RPRAND_GENERATOR   OFF CACHE BOOL "" FORCE)
    set(SUPPORT_MOUSE_GESTURES     ON CACHE BOOL "" FORCE)
    set(SUPPORT_AUTOMATION_EVENTS  OFF CACHE BOOL "" FORCE)
    set(SUPPORT_CLIPBOARD_IMAGE    OFF CACHE BOOL "" FORCE)
    set(SUPPORT_FILEFORMAT_BMP     OFF CACHE BOOL "" FORCE)
    set(SUPPORT_FILEFORMAT_PNG     ON  CACHE BOOL "" FORCE)  # Required by TakeScreenshot() (saves .png)
    set(SUPPORT_FILEFORMAT_JPG     OFF CACHE BOOL "" FORCE)
    set(SUPPORT_FILEFORMAT_GIF     OFF CACHE BOOL "" FORCE)
    set(SUPPORT_FILEFORMAT_QOI     OFF CACHE BOOL "" FORCE)
    set(SUPPORT_FILEFORMAT_DDS     OFF CACHE BOOL "" FORCE)
    set(SUPPORT_IMAGE_GENERATION   OFF CACHE BOOL "" FORCE)
    set(SUPPORT_FILEFORMAT_FNT     OFF CACHE BOOL "" FORCE)
    set(SUPPORT_FILEFORMAT_OGG     OFF CACHE BOOL "" FORCE)
    set(SUPPORT_FILEFORMAT_MP3     OFF CACHE BOOL "" FORCE)
    set(SUPPORT_FILEFORMAT_QOA     OFF CACHE BOOL "" FORCE)
    set(SUPPORT_FILEFORMAT_XM      OFF CACHE BOOL "" FORCE)
    set(SUPPORT_FILEFORMAT_MOD     OFF CACHE BOOL "" FORCE)
    set(SUPPORT_FILEFORMAT_WAV     OFF CACHE BOOL "" FORCE)  # Core audio is raw PCM, no audio files loaded
    set(SUPPORT_COMPRESSION_API    OFF CACHE BOOL "" FORCE)
    set(SUPPORT_SCREEN_CAPTURE     OFF CACHE BOOL "" FORCE)  # F12 auto-capture; frontend uses its own screenshot key
    set(SUPPORT_SSH_KEYBOARD_RPI   OFF CACHE BOOL "" FORCE)
    # Web: build raylib for WebGL2 / GLES3. The frontend itself needs WebGL2 for
    # glBlitFramebuffer, sampler objects, and pixel-store params used in the HW
    # state-restore path — all unavailable in WebGL1/GLES2. Prebuilt wasm cores
    # only implement GLES2, so GET_PREFERRED_HW_RENDER advertises OPENGLES2.
    # raylib appends -sMIN/MAX_WEBGL_VERSION=2 for this GRAPHICS value
    # (vendor/raylib/src/CMakeLists.txt) and propagates -DGRAPHICS_API_OPENGL_ES3
    # PUBLIC so the frontend compiles its ES3 code paths.
    if ("${PLATFORM}" STREQUAL "Web")
        set(GRAPHICS "GRAPHICS_API_OPENGL_ES3" CACHE STRING "" FORCE)
    endif()
    add_subdirectory(vendor/raylib vendor/raylib EXCLUDE_FROM_ALL)
endif()

add_subdirectory(include)
add_subdirectory(lib)

# sqlite3 — backs the games database (include/raylib-libretro-games.h); shared by the app and tests.
# Built from the vendor/sqlite-amalgamation submodule's sqlite3.c (its own CMakeLists.txt is not
# used: it builds a shared lib + shell and forces CMAKE_BUILD_TYPE).
add_library(sqlite3 STATIC vendor/sqlite-amalgamation/sqlite3.c)
target_include_directories(sqlite3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor/sqlite-amalgamation)
# Trimmed, single-threaded build: the games layer uses one connection from the
# main loop, so no mutexes / loadable extensions / deprecated APIs are needed.
target_compile_definitions(sqlite3 PRIVATE
    SQLITE_THREADSAFE=0
    SQLITE_OMIT_LOAD_EXTENSION=1
    SQLITE_DEFAULT_MEMSTATUS=0
    SQLITE_OMIT_DEPRECATED=1
    SQLITE_DQS=0
    SQLITE_OMIT_SHARED_CACHE=1
)
# Third-party C: silence its warnings and keep this large TU out of LTO so links
# stay fast and robust. MSVC needs /bigobj for the amalgamation.
set_target_properties(sqlite3 PROPERTIES INTERPROCEDURAL_OPTIMIZATION OFF)
if (MSVC)
    target_compile_options(sqlite3 PRIVATE /w /bigobj)
else()
    target_compile_options(sqlite3 PRIVATE -w)
endif()
target_link_libraries(sqlite3 PUBLIC ${CMAKE_DL_LIBS})
if (NOT MSVC)
    target_link_libraries(sqlite3 PUBLIC m)
endif()

# Example
option(BUILD_RAYLIB_LIBRETRO_EXAMPLE "Build Examples" ON)
if (BUILD_RAYLIB_LIBRETRO_EXAMPLE)
    add_subdirectory(example)
endif()

# Test cores
option(BUILD_RAYLIB_LIBRETRO_TEST_CORES "Build test cores" ON)
if (BUILD_RAYLIB_LIBRETRO_TEST_CORES AND NOT "${PLATFORM}" STREQUAL "Web")
    add_subdirectory(tests/test_opengl)
    add_subdirectory(tests/test_software)
endif()

# raylib-libretro
option(BUILD_RAYLIB_LIBRETRO "raylib-libretro" ON)
if(BUILD_RAYLIB_LIBRETRO)
    add_subdirectory(bin)
endif()

# Tests (headless games-database unit tests; no window or cores required)
option(BUILD_RAYLIB_LIBRETRO_TESTS "Build tests" ON)
if (BUILD_RAYLIB_LIBRETRO_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# Documentation
find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_SOURCE_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating API documentation"
        VERBATIM)
endif()

