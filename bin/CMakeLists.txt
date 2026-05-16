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
    function(build_libretro_side_module CORE_NAME REPO MAKEFILE)
        set(SUBDIR "${ARGV3}")
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

        endif()

        add_custom_command(
            OUTPUT "${SO_FILE}"
            COMMAND "${EMMAKE_EXECUTABLE}" make -f "${MAKEFILE}"
                    platform=emscripten fpic=-fPIC -j
            COMMAND ${CMAKE_COMMAND} -E copy "${BC_FILE}" "${A_FILE}"
            COMMAND "${EMCC_EXECUTABLE}" -O2 -sSIDE_MODULE=2
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

    build_libretro_side_module(fceumm    "https://github.com/libretro/libretro-fceumm"   "Makefile.libretro")
    build_libretro_side_module(snes9x    "https://github.com/libretro/snes9x"            "Makefile"           "libretro")
    build_libretro_side_module(gambatte  "https://github.com/libretro/gambatte-libretro" "Makefile.libretro")
    build_libretro_side_module(mgba      "https://github.com/libretro/mgba"              "Makefile.libretro")
    build_libretro_side_module(picodrive "https://github.com/libretro/picodrive"         "Makefile.libretro")

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
        -sALLOW_MEMORY_GROWTH=1
        --preload-file "${CORES_DIR}@/cores"
        --shell-file "${CMAKE_CURRENT_SOURCE_DIR}/shell.html"
    )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if (CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
        download_cores(
            "https://buildbot.libretro.com/nightly/linux/aarch64/latest/fceumm_libretro.so.zip"
            "https://buildbot.libretro.com/nightly/linux/aarch64/latest/snes9x_libretro.so.zip"
            "https://buildbot.libretro.com/nightly/linux/aarch64/latest/gambatte_libretro.so.zip"
            "https://buildbot.libretro.com/nightly/linux/aarch64/latest/mgba_libretro.so.zip"
            "https://buildbot.libretro.com/nightly/linux/aarch64/latest/picodrive_libretro.so.zip"
        )
    elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        download_cores(
            "https://buildbot.libretro.com/nightly/linux/x86_64/latest/fceumm_libretro.so.zip"
            "https://buildbot.libretro.com/nightly/linux/x86_64/latest/snes9x_libretro.so.zip"
            "https://buildbot.libretro.com/nightly/linux/x86_64/latest/gambatte_libretro.so.zip"
            "https://buildbot.libretro.com/nightly/linux/x86_64/latest/mgba_libretro.so.zip"
            "https://buildbot.libretro.com/nightly/linux/x86_64/latest/picodrive_libretro.so.zip"
        )
    endif()
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if (CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        download_cores(
            "https://buildbot.libretro.com/nightly/windows/x86_64/latest/fceumm_libretro.dll.zip"
            "https://buildbot.libretro.com/nightly/windows/x86_64/latest/snes9x_libretro.dll.zip"
            "https://buildbot.libretro.com/nightly/windows/x86_64/latest/gambatte_libretro.dll.zip"
            "https://buildbot.libretro.com/nightly/windows/x86_64/latest/mgba_libretro.dll.zip"
            "https://buildbot.libretro.com/nightly/windows/x86_64/latest/picodrive_libretro.dll.zip"
        )
    endif()
elseif (APPLE)
    target_link_libraries(raylib-libretro PUBLIC
        "-framework IOKit"
        "-framework Cocoa"
        "-framework OpenGL"
    )
    if (CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
        download_cores(
            "https://buildbot.libretro.com/nightly/apple/osx/arm64/latest/fceumm_libretro.dylib.zip"
            "https://buildbot.libretro.com/nightly/apple/osx/arm64/latest/snes9x_libretro.dylib.zip"
            "https://buildbot.libretro.com/nightly/apple/osx/arm64/latest/gambatte_libretro.dylib.zip"
            "https://buildbot.libretro.com/nightly/apple/osx/arm64/latest/mgba_libretro.dylib.zip"
            "https://buildbot.libretro.com/nightly/apple/osx/arm64/latest/picodrive_libretro.dylib.zip"
        )
    else()
        download_cores(
            "https://buildbot.libretro.com/nightly/apple/osx/x86_64/latest/fceumm_libretro.dylib.zip"
            "https://buildbot.libretro.com/nightly/apple/osx/x86_64/latest/snes9x_libretro.dylib.zip"
            "https://buildbot.libretro.com/nightly/apple/osx/x86_64/latest/gambatte_libretro.dylib.zip"
            "https://buildbot.libretro.com/nightly/apple/osx/x86_64/latest/mgba_libretro.dylib.zip"
            "https://buildbot.libretro.com/nightly/apple/osx/x86_64/latest/picodrive_libretro.dylib.zip"
        )
    endif()
elseif (CMAKE_SYSTEM_NAME STREQUAL "Android")
    download_cores(
        "https://buildbot.libretro.com/nightly/android/latest/${CMAKE_ANDROID_ARCH_ABI}/fceumm_libretro_android.so.zip"
        "https://buildbot.libretro.com/nightly/android/latest/${CMAKE_ANDROID_ARCH_ABI}/snes9x_libretro_android.so.zip"
        "https://buildbot.libretro.com/nightly/android/latest/${CMAKE_ANDROID_ARCH_ABI}/gambatte_libretro_android.so.zip"
        "https://buildbot.libretro.com/nightly/android/latest/${CMAKE_ANDROID_ARCH_ABI}/mgba_libretro_android.so.zip"
        "https://buildbot.libretro.com/nightly/android/latest/${CMAKE_ANDROID_ARCH_ABI}/picodrive_libretro_android.so.zip"
    )
endif()

install(TARGETS raylib-libretro DESTINATION .)
install(FILES ../README.md DESTINATION .)
install(FILES ../LICENSE DESTINATION .)
install(DIRECTORY "${CORES_DIR}/" DESTINATION cores)

# Web
if ("${PLATFORM}" STREQUAL "Web")
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/index.data"
        "${CMAKE_CURRENT_BINARY_DIR}/index.js"
        "${CMAKE_CURRENT_BINARY_DIR}/index.wasm"
        DESTINATION .
    )
endif()
