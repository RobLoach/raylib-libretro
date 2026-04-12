cmake_minimum_required(VERSION 3.11)
project(raylib-libretro
    DESCRIPTION "raylib-libretro: libretro frontend using raylib."
    HOMEPAGE_URL "https://github.com/robloach/raylib-libretro"
    VERSION 0.0.7
    LANGUAGES C
)

# raylib
find_package(raylib QUIET)
if (NOT raylib_FOUND)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING  OFF CACHE BOOL "" FORCE)
    add_subdirectory(vendor/raylib vendor/raylib)
endif()

# Wasmtime C API — platform-aware pre-built binary download
include(FetchContent)
set(WASMTIME_VERSION "28.0.0")

# Map CMake's OS name to Wasmtime's release naming.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(_WT_OS  "linux")
    set(_WT_EXT "tar.xz")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(_WT_OS  "macos")
    set(_WT_EXT "tar.xz")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(_WT_OS  "windows")
    set(_WT_EXT "zip")
else()
    message(FATAL_ERROR "Wasmtime: unsupported OS '${CMAKE_SYSTEM_NAME}'")
endif()

# Map CMake's processor name to Wasmtime's arch string.
# Apple Silicon reports arm64; Wasmtime calls it aarch64.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
    set(_WT_ARCH "x86_64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
    set(_WT_ARCH "aarch64")
else()
    message(FATAL_ERROR "Wasmtime: unsupported arch '${CMAKE_SYSTEM_PROCESSOR}'")
endif()

set(_WT_ARCHIVE "wasmtime-v${WASMTIME_VERSION}-${_WT_ARCH}-${_WT_OS}-c-api.${_WT_EXT}")
message(STATUS "Wasmtime archive: ${_WT_ARCHIVE}")

FetchContent_Declare(wasmtime
    URL "https://github.com/bytecodealliance/wasmtime/releases/download/v${WASMTIME_VERSION}/${_WT_ARCHIVE}"
    DOWNLOAD_EXTRACT_TIMESTAMP ON
)
FetchContent_MakeAvailable(wasmtime)

set(WASMTIME_INCLUDE_DIR "${wasmtime_SOURCE_DIR}/include")
# Static library name differs on Windows MSVC vs everywhere else.
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(WASMTIME_LIB "${wasmtime_SOURCE_DIR}/lib/wasmtime.lib")
else()
    set(WASMTIME_LIB "${wasmtime_SOURCE_DIR}/lib/libwasmtime.a")
endif()

# Interface target that bundles the lib + headers + platform link deps.
# Mirrors the pattern from wasmtime/crates/c-api/CMakeLists.txt.
add_library(wasmtime_c_api INTERFACE)
target_include_directories(wasmtime_c_api INTERFACE "${WASMTIME_INCLUDE_DIR}")
target_link_libraries(wasmtime_c_api INTERFACE "${WASMTIME_LIB}")
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # Windows MSVC static build requires these extra import libs.
    target_compile_definitions(wasmtime_c_api INTERFACE WASM_API_EXTERN= WASI_API_EXTERN=)
    target_link_libraries(wasmtime_c_api INTERFACE
        ws2_32 advapi32 userenv ntdll shell32 ole32 bcrypt)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_link_libraries(wasmtime_c_api INTERFACE "-framework CoreFoundation")
else()
    target_link_libraries(wasmtime_c_api INTERFACE pthread dl m)
endif()

add_subdirectory(include)
add_subdirectory(lib)

# Libretro WASM cores
option(DOWNLOAD_LIBRETRO_CORES
    "Download pre-built WASM libretro cores from retroarch-emscripten-build" ON)
if(DOWNLOAD_LIBRETRO_CORES)
    include(cmake/LibretroCores.cmake)
endif()

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
