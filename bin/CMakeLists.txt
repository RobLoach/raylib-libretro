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
)

if ("${PLATFORM}" STREQUAL "Web")
    set_target_properties(raylib-libretro PROPERTIES OUTPUT_NAME "index")
    set_target_properties(raylib-libretro PROPERTIES SUFFIX ".html")

    find_program(EMMAKE_EXECUTABLE emmake REQUIRED)
    find_program(EMCC_EXECUTABLE   emcc   REQUIRED)

    # libretro symbols exported by every side-module core so the MAIN_MODULE
    # host can resolve them via dlsym after dlopen.
    set(LIBRETRO_EXPORTS
        _retro_init _retro_deinit _retro_api_version
        _retro_get_system_info _retro_get_system_av_info
        _retro_set_environment _retro_set_video_refresh
        _retro_set_audio_sample _retro_set_audio_sample_batch
        _retro_set_input_poll _retro_set_input_state
        _retro_set_controller_port_device
        _retro_reset _retro_run
        _retro_serialize_size _retro_serialize _retro_unserialize
        _retro_cheat_reset _retro_cheat_set
        _retro_load_game _retro_load_game_special _retro_unload_game
        _retro_get_region _retro_get_memory_data _retro_get_memory_size
    )
    string(REPLACE ";" "," LIBRETRO_EXPORTS_CSV "${LIBRETRO_EXPORTS}")

    file(MAKE_DIRECTORY "${CORES_DIR}")
    set(LIBRETRO_SIDE_MODULES "")

    # Build a libretro core from source as a wasm SIDE_MODULE that the
    # MAIN_MODULE host can dlopen at runtime. Each core's stock Makefile
    # produces a static archive named *_libretro_emscripten.bc; that file is
    # actually an ar archive that we link as a side module with the libretro
    # API symbols exported.
    #
    # Args:
    #   CORE_NAME  short core name; the output is ${CORE_NAME}_libretro.so
    #   REPO       git URL
    #   MAKEFILE   Makefile filename (e.g. Makefile or Makefile.libretro)
    #   SUBDIR     subdirectory of source to run make in (pass "" for root)
    #   ARGN...    extra flags injected via EMCC_CFLAGS for the make step, so
    #              every emcc compile/link picks them up regardless of the core's
    #              Makefile (e.g. -sUSE_ZLIB=1 to give a core zlib's headers/port).
    #              Pair with a matching flag on the main module's link options so
    #              the symbols exist there.
    function(build_libretro_side_module CORE_NAME REPO MAKEFILE SUBDIR)
        # ARGN carries optional flags for EMCC_CFLAGS (see header above).
        set(EXTRA_EMCC_CFLAGS "${ARGN}")
        string(REPLACE ";" " " EXTRA_EMCC_CFLAGS "${EXTRA_EMCC_CFLAGS}")
        set(SRC_DIR "${CMAKE_BINARY_DIR}/cores-src/${CORE_NAME}")
        if (SUBDIR)
            set(BUILD_DIR "${SRC_DIR}/${SUBDIR}")
        else()
            set(BUILD_DIR "${SRC_DIR}")
        endif()
        set(BC_FILE "${BUILD_DIR}/${CORE_NAME}_libretro_emscripten.bc")
        set(A_FILE  "${BUILD_DIR}/${CORE_NAME}_libretro_emscripten.a")
        set(SO_FILE "${CORES_DIR}/${CORE_NAME}_libretro.so")

        if (NOT EXISTS "${SRC_DIR}/.git")
            message(STATUS "Cloning libretro core: ${CORE_NAME}")
            file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/cores-src")
            execute_process(
                COMMAND git clone --depth 1 --recurse-submodules --shallow-submodules
                        "${REPO}" "${SRC_DIR}"
                RESULT_VARIABLE CLONE_RC
            )
            if (NOT CLONE_RC EQUAL 0)
                message(WARNING "Failed to clone ${CORE_NAME}; the web build will not include this core.")
                return()
            endif()

            # Many cores' Makefile.common gates the libretro-common SOURCES_C
            # block behind `ifneq ($(STATIC_LINKING), 1)`, on the assumption
            # that a static-linked core's frontend provides those symbols. As
            # a self-contained side module the core must include them itself,
            # so neutralise the gate to always-true.
            if (EXISTS "${SRC_DIR}/Makefile.common")
                file(READ "${SRC_DIR}/Makefile.common" MK_COMMON)
                string(REPLACE "ifneq (\$(STATIC_LINKING), 1)" "ifeq (1, 1)"
                       MK_COMMON_PATCHED "${MK_COMMON}")
                if (NOT MK_COMMON STREQUAL MK_COMMON_PATCHED)
                    message(STATUS "Patching ${CORE_NAME}/Makefile.common to always include libretro-common")
                    file(WRITE "${SRC_DIR}/Makefile.common" "${MK_COMMON_PATCHED}")
                endif()
            endif()

            # mgba's libretro-build/Makefile.common defines -DHAVE_CRC32 when
            # STATIC_LINKING=1, which makes the core call zlib's crc32 instead
            # of its own internal implementation. We don't provide zlib in the
            # main module, so drop the define and use mgba's internal crc32.
            if (EXISTS "${SRC_DIR}/libretro-build/Makefile.common")
                file(READ "${SRC_DIR}/libretro-build/Makefile.common" MGBA_MK)
                string(REPLACE "RETRODEFS += -DHAVE_CRC32" "# patched: use internal crc32"
                       MGBA_MK_PATCHED "${MGBA_MK}")
                if (NOT MGBA_MK STREQUAL MGBA_MK_PATCHED)
                    message(STATUS "Patching ${CORE_NAME}/libretro-build/Makefile.common to drop -DHAVE_CRC32")
                    file(WRITE "${SRC_DIR}/libretro-build/Makefile.common" "${MGBA_MK_PATCHED}")
                endif()
            endif()
        endif()

        # emcc reads EMCC_CFLAGS and appends it to every compile/link it runs,
        # so this reaches the core's sources without the Makefile cooperating.
        # Force -O3 on every core translation unit for the best emulation
        # performance, regardless of the core Makefile's own default: EMCC_CFLAGS
        # is appended last and the last -O on the command line wins.
        set(CORE_EMCC_CFLAGS "-O3 ${EXTRA_EMCC_CFLAGS}")
        string(STRIP "${CORE_EMCC_CFLAGS}" CORE_EMCC_CFLAGS)
        set(MAKE_ENV ${CMAKE_COMMAND} -E env "EMCC_CFLAGS=${CORE_EMCC_CFLAGS}")

        add_custom_command(
            OUTPUT "${SO_FILE}"
            COMMAND ${MAKE_ENV} "${EMMAKE_EXECUTABLE}" make -f "${MAKEFILE}"
                    platform=emscripten fpic=-fPIC -j
            COMMAND ${CMAKE_COMMAND} -E copy "${BC_FILE}" "${A_FILE}"
            COMMAND "${EMCC_EXECUTABLE}" -O3 -sSIDE_MODULE=2
                    "-sEXPORTED_FUNCTIONS=${LIBRETRO_EXPORTS_CSV}"
                    -Wl,--whole-archive "${A_FILE}" -Wl,--no-whole-archive
                    -o "${SO_FILE}"
            WORKING_DIRECTORY "${BUILD_DIR}"
            COMMENT "Building libretro side module: ${CORE_NAME}"
            VERBATIM
        )

        list(APPEND LIBRETRO_SIDE_MODULES "${SO_FILE}")
        set(LIBRETRO_SIDE_MODULES "${LIBRETRO_SIDE_MODULES}" PARENT_SCOPE)
    endfunction()

    build_libretro_side_module(fceumm    "https://github.com/libretro/libretro-fceumm"   "Makefile.libretro" "")
    build_libretro_side_module(snes9x    "https://github.com/libretro/snes9x"            "Makefile"          "libretro")
    build_libretro_side_module(mgba      "https://github.com/libretro/mgba"              "Makefile.libretro" "")
    build_libretro_side_module(genesis_plus_gx "https://github.com/libretro/Genesis-Plus-GX" "Makefile.libretro" "")
    build_libretro_side_module(prboom        "https://github.com/libretro/libretro-prboom"       "Makefile"          "")
    build_libretro_side_module(mednafen_pce_fast "https://github.com/libretro/beetle-pce-fast-libretro" "Makefile"     "")
    build_libretro_side_module(mednafen_wswan "https://github.com/libretro/beetle-wswan-libretro" "Makefile"          "")
    build_libretro_side_module(pcsx_rearmed  "https://github.com/libretro/pcsx_rearmed"           "Makefile.libretro" "" "-sUSE_ZLIB=1")

    if (LIBRETRO_SIDE_MODULES)
        add_custom_target(libretro-side-modules ALL DEPENDS ${LIBRETRO_SIDE_MODULES})
        add_dependencies(raylib-libretro libretro-side-modules)
        # Make the final web binary relink whenever a preloaded core changes.
        set_property(TARGET raylib-libretro APPEND PROPERTY
            LINK_DEPENDS ${LIBRETRO_SIDE_MODULES})
    endif()

    target_link_options(raylib-libretro PUBLIC
        -sUSE_GLFW=3
        -sMAIN_MODULE=1
        -sUSE_ZLIB=1 # pcsx_rearmed's side module imports zlib symbols at dlopen; provide the port here
        -sALLOW_MEMORY_GROWTH=1
        -sEXPORTED_RUNTIME_METHODS=ccall # shell.html ccalls LoadLibretroGameFromJS / ResizeCanvasFromJS
        -lidbfs.js # Emscripten's File System API
        --preload-file "${CORES_DIR}@/cores"
        --shell-file "${CMAKE_CURRENT_SOURCE_DIR}/shell.html"
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
