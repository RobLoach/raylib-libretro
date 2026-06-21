# raylib-libretro binary
add_executable(raylib-libretro
    raylib-libretro.c
    # PhysFS core (no native platform — the raylib platform shim from
    # raylib-physfs/physfs_platform_raylib.c is included via raylib-physfs.h
    # in raylib-libretro.c).
    ../vendor/physfs/src/physfs.c
    ../vendor/physfs/src/physfs_byteorder.c
    ../vendor/physfs/src/physfs_unicode.c
    ../vendor/physfs/src/physfs_archiver_dir.c
    ../vendor/physfs/src/physfs_archiver_unpacked.c
    ../vendor/physfs/src/physfs_archiver_zip.c
)
target_link_libraries(raylib-libretro PUBLIC
    raylib-libretro-static
)
target_include_directories(raylib-libretro PUBLIC
    ../vendor/raygui/src
    ../vendor/raylib-app
    ../vendor/raylib-physfs
    ../vendor/physfs/src
)
target_compile_definitions(raylib-libretro PRIVATE
    RAYLIB_LIBRETRO_VERSION="${PROJECT_VERSION}"
    PHYSFS_SUPPORTS_ZIP=1
    PHYSFS_SUPPORTS_7Z=0
    PHYSFS_SUPPORTS_GRP=0
    PHYSFS_SUPPORTS_WAD=0
    PHYSFS_SUPPORTS_CSM=0
    PHYSFS_SUPPORTS_HOG=0
    PHYSFS_SUPPORTS_MVL=0
    PHYSFS_SUPPORTS_QPAK=0
    PHYSFS_SUPPORTS_ROFS=0
    PHYSFS_SUPPORTS_SLB=0
    PHYSFS_SUPPORTS_ISO9660=0
    PHYSFS_SUPPORTS_VDF=0
    PHYSFS_SUPPORTS_LECARCHIVES=0
    PHYSFS_SUPPORTS_POD=0
)

# Directory where cores are extracted
if (NOT DEFINED CORES_DIR)
    set(CORES_DIR "${CMAKE_BINARY_DIR}/cores")
endif()
file(MAKE_DIRECTORY "${CORES_DIR}")

# download_cores(<url> [<url> ...])
# Downloads each zip URL to the build directory and extracts it into CORES_DIR.
# Already-downloaded archives are skipped on subsequent runs.
function(download_cores)
    file(MAKE_DIRECTORY "${CORES_DIR}")
    foreach(URL IN LISTS ARGN)
        get_filename_component(ARCHIVE_NAME "${URL}" NAME)
        set(ARCHIVE_PATH "${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}")

        if (NOT EXISTS "${ARCHIVE_PATH}")
            message(STATUS "Downloading ${URL}")
            file(DOWNLOAD "${URL}" "${ARCHIVE_PATH}"
                STATUS DOWNLOAD_STATUS
                SHOW_PROGRESS
            )
            list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
            list(GET DOWNLOAD_STATUS 1 STATUS_MSG)
            if (NOT STATUS_CODE EQUAL 0)
                message(WARNING "Failed to download ${URL}: ${STATUS_MSG}")
                continue()
            endif()
        endif()

        message(STATUS "Extracting ${ARCHIVE_NAME} to ${CORES_DIR}")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E tar xf "${ARCHIVE_PATH}"
            WORKING_DIRECTORY "${CORES_DIR}"
            RESULT_VARIABLE EXTRACT_RESULT
        )
        if (NOT EXTRACT_RESULT EQUAL 0)
            message(WARNING "Failed to extract ${ARCHIVE_NAME}")
        endif()
    endforeach()
endfunction()

# download_nightly_cores(<base_url> <ext>)
# Downloads the canonical core set (CORES) from a libretro nightly buildbot
# directory. <base_url> is the directory holding the archives; <ext> is the
# library extension as it appears before .zip (.so, .dll, .dylib, _android.so).
# Builds "<base_url>/<core>_libretro<ext>.zip" for each core.
function(download_nightly_cores BASE_URL EXT)
    set(URLS "")
    foreach(CORE IN LISTS CORES)
        list(APPEND URLS "${BASE_URL}/${CORE}_libretro${EXT}.zip")
    endforeach()
    download_cores(${URLS})
endfunction()

# copy_core_info(<core> [<core> ...])
# Copies each core's <core>_libretro.info metadata from the libretro-core-info
# submodule into CORES_DIR.
set(CORE_INFO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../vendor/libretro-core-info")
function(copy_core_info)
    file(MAKE_DIRECTORY "${CORES_DIR}")
    foreach(CORE IN LISTS ARGN)
        set(INFO_SRC "${CORE_INFO_DIR}/${CORE}_libretro.info")
        set(INFO_DST "${CORES_DIR}/${CORE}_libretro.info")
        if (EXISTS "${INFO_SRC}")
            file(COPY_FILE "${INFO_SRC}" "${INFO_DST}" ONLY_IF_DIFFERENT)
        else()
            # A missing .info means the core ships but silently never appears in
            # the menu (ScanLibretroCoreDirectory is .info-driven). Fail loudly
            # for bundled cores rather than discovering it at runtime.
            message(FATAL_ERROR "${CORE}_libretro.info not found in ${CORE_INFO_DIR}. "
                "Run: git submodule update --init -- vendor/libretro-core-info")
        endif()
    endforeach()
endfunction()

# The cores bundled with every build, as their short names. Download URLs,
# .info files and web side-module outputs are all derived as <core>_libretro*.
set(CORES
    fceumm
    snes9x
    mgba
    genesis_plus_gx
    prboom
    mednafen_pce_fast
    mednafen_wswan
    pcsx_rearmed
    mednafen_psx
    mednafen_psx_hw
)

if ("${PLATFORM}" STREQUAL "Web")
    set_target_properties(raylib-libretro PROPERTIES OUTPUT_NAME "index")
    set_target_properties(raylib-libretro PROPERTIES SUFFIX ".html")

    # Download pre-built wasm SIDE_MODULE cores from the nightly release.
    # Each .wasm is a SIDE_MODULE=2 with all libretro API symbols exported,
    # ready to be dlopen'd by the MAIN_MODULE frontend at runtime.
    set(WASM_CORES_BASE_URL "https://github.com/konsumer/libretro-wasm-cores/releases/download/nightly")
    file(MAKE_DIRECTORY "${CORES_DIR}")
    set(WASM_CORE_FILES "")
    foreach(CORE IN LISTS CORES)
        set(WASM_URL "${WASM_CORES_BASE_URL}/${CORE}_libretro.wasm")
        set(WASM_DST "${CORES_DIR}/${CORE}_libretro.wasm")
        if (NOT EXISTS "${WASM_DST}")
            message(STATUS "Downloading wasm core: ${CORE}")
            file(DOWNLOAD "${WASM_URL}" "${WASM_DST}"
                STATUS DL_STATUS SHOW_PROGRESS)
            list(GET DL_STATUS 0 DL_CODE)
            if (NOT DL_CODE EQUAL 0)
                list(GET DL_STATUS 1 DL_MSG)
                message(WARNING "Failed to download ${CORE}: ${DL_MSG} — skipping")
                file(REMOVE "${WASM_DST}")
            else()
                list(APPEND WASM_CORE_FILES "${WASM_DST}")
            endif()
        else()
            list(APPEND WASM_CORE_FILES "${WASM_DST}")
        endif()
    endforeach()

    # Relink whenever any core wasm file changes so the preload archive is rebuilt.
    if (WASM_CORE_FILES)
        set_property(TARGET raylib-libretro APPEND PROPERTY LINK_DEPENDS ${WASM_CORE_FILES})
    endif()

    target_link_options(raylib-libretro PUBLIC
        -sUSE_GLFW=3
        -sMAIN_MODULE=1
        -sUSE_ZLIB=1 # pcsx_rearmed's side module imports zlib symbols at dlopen; provide the port here
        -sGL_ENABLE_GET_PROC_ADDRESS=1 # allow cores to resolve GL procs at runtime
        -sALLOW_MEMORY_GROWTH=1
        -sENVIRONMENT=web # browser-only target. Drop the Node/worker glue we never ship.
        -sEXPORTED_RUNTIME_METHODS=ccall # shell.html ccalls LoadLibretroGameFromJS / ResizeCanvasFromJS
        -lidbfs.js # Emscripten's File System API
        --preload-file "${CORES_DIR}@/cores"
        --shell-file "${CMAKE_CURRENT_SOURCE_DIR}/shell.html"
        # Emscripten links at -O0 even when the sources were compiled -O3, and the
        # link step is where wasm-opt runs, so pass the optimization level to the
        # linker too.
        $<$<CONFIG:Release>:-O3>
    )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if (CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
        download_nightly_cores("https://buildbot.libretro.com/nightly/linux/aarch64/latest" ".so")
    elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        download_nightly_cores("https://buildbot.libretro.com/nightly/linux/x86_64/latest" ".so")
    endif()
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if (CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        download_nightly_cores("https://buildbot.libretro.com/nightly/windows/x86_64/latest" ".dll")
    endif()
elseif (APPLE)
    target_link_libraries(raylib-libretro PUBLIC
        "-framework IOKit"
        "-framework Cocoa"
        "-framework OpenGL"
    )
    if (CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
        download_nightly_cores("https://buildbot.libretro.com/nightly/apple/osx/arm64/latest" ".dylib")
    else()
        download_nightly_cores("https://buildbot.libretro.com/nightly/apple/osx/x86_64/latest" ".dylib")
    endif()
elseif (CMAKE_SYSTEM_NAME STREQUAL "Android")
    download_nightly_cores("https://buildbot.libretro.com/nightly/android/latest/${CMAKE_ANDROID_ARCH_ABI}" "_android.so")
endif()

copy_core_info(${CORES})

install(TARGETS raylib-libretro DESTINATION .)
install(FILES ../README.md DESTINATION .)
install(FILES ../LICENSE DESTINATION .)
install(DIRECTORY "${CORES_DIR}/" DESTINATION cores)
install(CODE "file(MAKE_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/saves\")")
install(CODE "file(MAKE_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/system\")")

# Web
if ("${PLATFORM}" STREQUAL "Web")
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/index.data"
        "${CMAKE_CURRENT_BINARY_DIR}/index.js"
        "${CMAKE_CURRENT_BINARY_DIR}/index.wasm"
        DESTINATION .
    )
    # PWA assets: manifest, service worker and icons (referenced by shell.html)
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/manifest.webmanifest"
        "${CMAKE_CURRENT_SOURCE_DIR}/sw.js"
        "${CMAKE_CURRENT_SOURCE_DIR}/icon-192.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/icon-512.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/apple-touch-icon.png"
        DESTINATION .
    )
endif()
