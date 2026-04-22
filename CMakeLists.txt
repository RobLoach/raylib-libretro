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

# WebAssembly Micro Runtime (WAMR)
set(WAMR_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/wasm-micro-runtime)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(WAMR_BUILD_PLATFORM "windows")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(WAMR_BUILD_PLATFORM "darwin")
else()
    set(WAMR_BUILD_PLATFORM "linux")
endif()

if (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    set(WAMR_BUILD_TARGET "AARCH64")
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "armv7|armhf|arm")
    set(WAMR_BUILD_TARGET "ARM")
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
    set(WAMR_BUILD_TARGET "X86_64")
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86")
    set(WAMR_BUILD_TARGET "X86_32")
else()
    set(WAMR_BUILD_TARGET "X86_64")
endif()

set(WAMR_BUILD_INTERP      1)
set(WAMR_BUILD_FAST_INTERP 1)
set(WAMR_BUILD_AOT         0)
set(WAMR_BUILD_JIT         0)
set(WAMR_BUILD_LIBC_BUILTIN 1)
set(WAMR_BUILD_LIBC_WASI   0)
set(WAMR_BUILD_MULTI_MODULE 0)
set(WAMR_BUILD_SHARED_LIB  0)

include(${WAMR_ROOT_DIR}/build-scripts/runtime_lib.cmake)
add_library(vmlib STATIC ${WAMR_RUNTIME_LIB_SOURCE})
target_include_directories(vmlib PUBLIC ${WAMR_ROOT_DIR}/core/iwasm/include)

add_subdirectory(include)
add_subdirectory(lib)

# Wire WAMR into the interface target
target_include_directories(raylib-libretro-h INTERFACE ${WAMR_ROOT_DIR}/core/iwasm/include)
target_link_libraries(raylib-libretro-h INTERFACE vmlib)

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
