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

# WebAssembly core loading via wasm-c-api (wasmtime or wasmer required)
option(RAYLIB_LIBRETRO_WASM "Enable loading libretro cores as WebAssembly modules" OFF)
if (RAYLIB_LIBRETRO_WASM)
    find_package(PkgConfig QUIET)
    set(_WASM_RUNTIME_FOUND FALSE)

    if (PkgConfig_FOUND AND NOT _WASM_RUNTIME_FOUND)
        pkg_check_modules(WASMTIME QUIET wasmtime)
        if (WASMTIME_FOUND)
            set(_WASM_RUNTIME_FOUND TRUE)
            set(_WASM_LIBRARIES    ${WASMTIME_LIBRARIES})
            set(_WASM_INCLUDE_DIRS ${WASMTIME_INCLUDE_DIRS})
            set(_WASM_LINK_DIRS    ${WASMTIME_LIBRARY_DIRS})
            message(STATUS "LIBRETRO: WASM runtime: wasmtime (pkg-config)")
        endif()
    endif()

    if (PkgConfig_FOUND AND NOT _WASM_RUNTIME_FOUND)
        pkg_check_modules(WASMER QUIET wasmer)
        if (WASMER_FOUND)
            set(_WASM_RUNTIME_FOUND TRUE)
            set(_WASM_LIBRARIES    ${WASMER_LIBRARIES})
            set(_WASM_INCLUDE_DIRS ${WASMER_INCLUDE_DIRS})
            set(_WASM_LINK_DIRS    ${WASMER_LIBRARY_DIRS})
            message(STATUS "LIBRETRO: WASM runtime: wasmer (pkg-config)")
        endif()
    endif()

    if (NOT _WASM_RUNTIME_FOUND)
        find_library(WASMTIME_LIB NAMES wasmtime)
        if (WASMTIME_LIB)
            set(_WASM_RUNTIME_FOUND TRUE)
            set(_WASM_LIBRARIES ${WASMTIME_LIB})
            message(STATUS "LIBRETRO: WASM runtime: wasmtime (find_library)")
        endif()
    endif()

    if (NOT _WASM_RUNTIME_FOUND)
        message(WARNING "RAYLIB_LIBRETRO_WASM is ON but no wasm-c-api runtime was found. "
                        "Install wasmtime or wasmer and ensure pkg-config can find it.")
    else()
        set(RAYLIB_LIBRETRO_WASM_AVAILABLE TRUE)
    endif()
endif()

add_subdirectory(include)
add_subdirectory(lib)

# Propagate WASM settings to all targets that use the interface header
if (RAYLIB_LIBRETRO_WASM AND RAYLIB_LIBRETRO_WASM_AVAILABLE)
    target_compile_definitions(raylib-libretro-h INTERFACE RAYLIB_LIBRETRO_WASM)
    target_include_directories(raylib-libretro-h INTERFACE vendor/wasm-c-api/include)
    target_link_directories(raylib-libretro-h INTERFACE ${_WASM_LINK_DIRS})
    target_link_libraries(raylib-libretro-h INTERFACE ${_WASM_LIBRARIES})
    if (_WASM_INCLUDE_DIRS)
        target_include_directories(raylib-libretro-h INTERFACE ${_WASM_INCLUDE_DIRS})
    endif()
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
