# LibretroCores.cmake
#
# Downloads pre-built WASM libretro cores from:
#   https://github.com/arianrhodsandlot/retroarch-emscripten-build
#
# Each core is fetched as a ZIP (containing a .js loader + .wasm binary);
# only the .wasm is kept.  Downloads are cached — if the .wasm already
# exists it is not re-fetched.
#
# Variables (all have sensible defaults, override before including):
#
#   LIBRETRO_CORES_VERSION   — git tag of retroarch-emscripten-build  (default v1.22.2)
#   LIBRETRO_CORES           — semicolon-separated list of core names  (default: small curated set)
#   LIBRETRO_CORES_DIR       — directory where .wasm files are placed  (default: <build>/cores)

if(NOT DEFINED LIBRETRO_CORES_VERSION)
    set(LIBRETRO_CORES_VERSION "v1.22.2")
endif()

set(_LIBRETRO_BASE_URL
    "https://cdn.jsdelivr.net/gh/arianrhodsandlot/retroarch-emscripten-build@${LIBRETRO_CORES_VERSION}/retroarch")

# Default curated core list — covers the most common retro platforms.
# Users can override by setting LIBRETRO_CORES before including this file, e.g.:
#   set(LIBRETRO_CORES fceumm snes9x)
#   include(cmake/LibretroCores.cmake)
if(NOT DEFINED LIBRETRO_CORES)
    set(LIBRETRO_CORES
        fceumm           # NES
        snes9x           # SNES
        gambatte         # Game Boy / Game Boy Color
        mgba             # Game Boy Advance
        genesis_plus_gx  # Sega Genesis / Mega Drive / Master System
        picodrive        # Sega 32X / Mega CD
        pcsx_rearmed     # PlayStation
    )
endif()

if(NOT DEFINED LIBRETRO_CORES_DIR)
    set(LIBRETRO_CORES_DIR "${CMAKE_BINARY_DIR}/cores")
endif()

file(MAKE_DIRECTORY "${LIBRETRO_CORES_DIR}")

foreach(_CORE IN LISTS LIBRETRO_CORES)
    set(_WASM "${LIBRETRO_CORES_DIR}/${_CORE}_libretro.wasm")
    set(_ZIP  "${LIBRETRO_CORES_DIR}/${_CORE}_libretro.zip")
    set(_URL  "${_LIBRETRO_BASE_URL}/${_CORE}_libretro.zip")

    if(EXISTS "${_WASM}")
        message(STATUS "Libretro core cached:     ${_CORE}_libretro.wasm")
        continue()
    endif()

    message(STATUS "Downloading libretro core: ${_CORE}  (${_URL})")
    file(DOWNLOAD
        "${_URL}"
        "${_ZIP}"
        STATUS _DL_STATUS
        SHOW_PROGRESS
    )
    list(GET _DL_STATUS 0 _DL_CODE)
    list(GET _DL_STATUS 1 _DL_MSG)

    if(NOT _DL_CODE EQUAL 0)
        message(WARNING "  Failed to download ${_CORE}_libretro.zip: ${_DL_MSG}")
        file(REMOVE "${_ZIP}")
        continue()
    endif()

    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E tar xf "${_ZIP}"
        WORKING_DIRECTORY "${LIBRETRO_CORES_DIR}"
        RESULT_VARIABLE _EX_RESULT
    )

    file(REMOVE "${_ZIP}")

    if(NOT _EX_RESULT EQUAL 0)
        message(WARNING "  Failed to extract ${_CORE}_libretro.zip")
        continue()
    endif()

    # The ZIP also contains a .js Emscripten loader we don't need — remove it.
    file(REMOVE "${LIBRETRO_CORES_DIR}/${_CORE}_libretro.js")

    if(EXISTS "${_WASM}")
        message(STATUS "  OK: ${_WASM}")
    else()
        message(WARNING "  .wasm not found after extraction for core: ${_CORE}")
    endif()
endforeach()

# Install all downloaded cores alongside the binary.
install(
    DIRECTORY "${LIBRETRO_CORES_DIR}/"
    DESTINATION "cores"
    FILES_MATCHING PATTERN "*.wasm"
)

message(STATUS "Libretro WASM cores dir: ${LIBRETRO_CORES_DIR}")
