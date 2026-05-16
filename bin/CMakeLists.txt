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
    download_cores(
        "https://cdn.jsdelivr.net/gh/arianrhodsandlot/retroarch-emscripten-build@v1.22.2/retroarch/fceumm_libretro.zip"
        "https://cdn.jsdelivr.net/gh/arianrhodsandlot/retroarch-emscripten-build@v1.22.2/retroarch/snes9x_libretro.zip"
        "https://cdn.jsdelivr.net/gh/arianrhodsandlot/retroarch-emscripten-build@v1.22.2/retroarch/gambatte_libretro.zip"
        "https://cdn.jsdelivr.net/gh/arianrhodsandlot/retroarch-emscripten-build@v1.22.2/retroarch/gpsp_libretro.zip"
        "https://cdn.jsdelivr.net/gh/arianrhodsandlot/retroarch-emscripten-build@v1.22.2/retroarch/picodrive_libretro.zip"
    )
    target_link_options(raylib-libretro PUBLIC
        -sUSE_GLFW=3
        -sMAIN_MODULE=1
        --preload-file "${CORES_DIR}@/cores"
    )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if (CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
        download_cores(
            "https://buildbot.libretro.com/nightly/linux/aarch64/latest/fceumm_libretro.so.zip"
            "https://buildbot.libretro.com/nightly/linux/aarch64/latest/snes9x_libretro.so.zip"
            "https://buildbot.libretro.com/nightly/linux/aarch64/latest/gambatte_libretro.so.zip"
            "https://buildbot.libretro.com/nightly/linux/aarch64/latest/gpsp_libretro.so.zip"
            "https://buildbot.libretro.com/nightly/linux/aarch64/latest/picodrive_libretro.so.zip"
        )
    elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        download_cores(
            "https://buildbot.libretro.com/nightly/linux/x86_64/latest/fceumm_libretro.so.zip"
            "https://buildbot.libretro.com/nightly/linux/x86_64/latest/snes9x_libretro.so.zip"
            "https://buildbot.libretro.com/nightly/linux/x86_64/latest/gambatte_libretro.so.zip"
            "https://buildbot.libretro.com/nightly/linux/x86_64/latest/gpsp_libretro.so.zip"
            "https://buildbot.libretro.com/nightly/linux/x86_64/latest/picodrive_libretro.so.zip"
        )
    endif()
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if (CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        download_cores(
            "https://buildbot.libretro.com/nightly/windows/x86_64/latest/fceumm_libretro.dll.zip"
            "https://buildbot.libretro.com/nightly/windows/x86_64/latest/snes9x_libretro.dll.zip"
            "https://buildbot.libretro.com/nightly/windows/x86_64/latest/gambatte_libretro.dll.zip"
            "https://buildbot.libretro.com/nightly/windows/x86_64/latest/gpsp_libretro.dll.zip"
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
            "https://buildbot.libretro.com/nightly/apple/osx/arm64/latest/gpsp_libretro.dylib.zip"
            "https://buildbot.libretro.com/nightly/apple/osx/arm64/latest/picodrive_libretro.dylib.zip"
        )
    else()
        download_cores(
            "https://buildbot.libretro.com/nightly/apple/osx/x86_64/latest/fceumm_libretro.dylib.zip"
            "https://buildbot.libretro.com/nightly/apple/osx/x86_64/latest/snes9x_libretro.dylib.zip"
            "https://buildbot.libretro.com/nightly/apple/osx/x86_64/latest/gambatte_libretro.dylib.zip"
            "https://buildbot.libretro.com/nightly/apple/osx/x86_64/latest/gpsp_libretro.dylib.zip"
            "https://buildbot.libretro.com/nightly/apple/osx/x86_64/latest/picodrive_libretro.dylib.zip"
        )
    endif()
elseif (CMAKE_SYSTEM_NAME STREQUAL "Android")
    download_cores(
        "https://buildbot.libretro.com/nightly/android/latest/${CMAKE_ANDROID_ARCH_ABI}/fceumm_libretro_android.so.zip"
        "https://buildbot.libretro.com/nightly/android/latest/${CMAKE_ANDROID_ARCH_ABI}/snes9x_libretro_android.so.zip"
        "https://buildbot.libretro.com/nightly/android/latest/${CMAKE_ANDROID_ARCH_ABI}/gambatte_libretro_android.so.zip"
        "https://buildbot.libretro.com/nightly/android/latest/${CMAKE_ANDROID_ARCH_ABI}/gpsp_libretro_android.so.zip"
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
