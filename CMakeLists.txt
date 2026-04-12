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

# ---------------------------------------------------------------------------
# Wasmtime C API — built from source using Cargo
# ---------------------------------------------------------------------------
find_program(CARGO_EXECUTABLE cargo
    HINTS "$ENV{HOME}/.cargo/bin" "$ENV{CARGO_HOME}/bin"
)
if(NOT CARGO_EXECUTABLE)
    message(FATAL_ERROR
        "cargo (Rust toolchain) not found.\n"
        "Install from https://rustup.rs/ then re-run CMake.")
endif()
message(STATUS "Found cargo: ${CARGO_EXECUTABLE}")

set(WASMTIME_VERSION "28.0.0")

include(FetchContent)
include(ExternalProject)

# Clone the source at configure time so headers are available immediately.
# We use FetchContent_Populate (not MakeAvailable) to avoid add_subdirectory
# picking up Wasmtime's own CMakeLists.txt.
FetchContent_Declare(wasmtime_src
    GIT_REPOSITORY "https://github.com/bytecodealliance/wasmtime.git"
    GIT_TAG        "v${WASMTIME_VERSION}"
    GIT_SHALLOW    ON
    GIT_SUBMODULES_RECURSE OFF
)
FetchContent_GetProperties(wasmtime_src)
if(NOT wasmtime_src_POPULATED)
    message(STATUS "Cloning Wasmtime v${WASMTIME_VERSION} source...")
    FetchContent_Populate(wasmtime_src)
endif()

set(WASMTIME_SOURCE_DIR  "${wasmtime_src_SOURCE_DIR}")
set(WASMTIME_INCLUDE_DIR "${WASMTIME_SOURCE_DIR}/crates/c-api/include")
set(WASMTIME_STATIC_LIB  "${WASMTIME_SOURCE_DIR}/target/release/libwasmtime.a")

# Build the C API crate at build time.
# BUILD_BYPRODUCTS tells Ninja/Make about the output file so it knows when
# a rebuild is needed.
ExternalProject_Add(wasmtime_cargo_build
    SOURCE_DIR      "${WASMTIME_SOURCE_DIR}"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND
        "${CARGO_EXECUTABLE}" build
            --release
            --manifest-path "${WASMTIME_SOURCE_DIR}/Cargo.toml"
            -p wasmtime-c-api
    INSTALL_COMMAND ""
    BUILD_IN_SOURCE ON
    BUILD_BYPRODUCTS "${WASMTIME_STATIC_LIB}"
    STAMP_DIR       "${CMAKE_CURRENT_BINARY_DIR}/wasmtime-stamp"
)

# IMPORTED STATIC target — carries both the library path and the include dir,
# so consumers only need a single target_link_libraries() call.
add_library(wasmtime_c_api STATIC IMPORTED GLOBAL)
add_dependencies(wasmtime_c_api wasmtime_cargo_build)
set_target_properties(wasmtime_c_api PROPERTIES
    IMPORTED_LOCATION             "${WASMTIME_STATIC_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${WASMTIME_INCLUDE_DIR}"
)
# ---------------------------------------------------------------------------

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
