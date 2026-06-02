cmake_minimum_required(VERSION 3.11)
project(raylib-libretro
    DESCRIPTION "raylib-libretro: libretro frontend using raylib."
    HOMEPAGE_URL "https://github.com/robloach/raylib-libretro"
    VERSION 0.0.24
    LANGUAGES C
)

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
    add_subdirectory(vendor/raylib vendor/raylib EXCLUDE_FROM_ALL)
endif()

add_subdirectory(include)
add_subdirectory(lib)

# Example
option(BUILD_RAYLIB_LIBRETRO_EXAMPLE "Build Examples" ON)
if (BUILD_RAYLIB_LIBRETRO_EXAMPLE)
    add_subdirectory(example)
endif()

# raylib-libretro
option(BUILD_RAYLIB_LIBRETRO "raylib-libretro" ON)
if(BUILD_RAYLIB_LIBRETRO)
    add_subdirectory(bin)
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

