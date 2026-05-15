cmake_minimum_required(VERSION 3.11)
project(raylib-libretro
    DESCRIPTION "raylib-libretro: libretro frontend using raylib."
    HOMEPAGE_URL "https://github.com/robloach/raylib-libretro"
    VERSION 0.0.8
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

add_subdirectory(include)
add_subdirectory(lib)

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

# OSX
if (APPLE)
    target_link_libraries(${PROJECT_NAME} "-framework IOKit")
    target_link_libraries(${PROJECT_NAME} "-framework Cocoa")
    target_link_libraries(${PROJECT_NAME} "-framework OpenGL")
endif()

# Web
if ("${PLATFORM}" STREQUAL "Web")
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "index")
    set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".html")
    download_cores(
        "https://cdn.jsdelivr.net/gh/arianrhodsandlot/retroarch-emscripten-build@v1.22.2/retroarch/fceumm_libretro.zip"
        "https://cdn.jsdelivr.net/gh/arianrhodsandlot/retroarch-emscripten-build@v1.22.2/retroarch/snes9x_libretro.zip"
    )
    target_link_options(${PROJECT_NAME} PUBLIC
        -sUSE_GLFW=3
        -sMAIN_MODULE=1
        --preload-file "${CORES_DIR}@/cores"
    )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if (CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
        download_cores(
            "https://buildbot.libretro.com/nightly/linux/aarch64/latest/fceumm_libretro.so.zip"
            "https://buildbot.libretro.com/nightly/linux/aarch64/latest/snes9x_libretro.so.zip"
        )
    elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        download_cores(
            "https://buildbot.libretro.com/nightly/linux/x86_64/latest/fceumm_libretro.so.zip"
            "https://buildbot.libretro.com/nightly/linux/x86_64/latest/snes9x_libretro.so.zip"
        )
    endif()
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if (CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        download_cores(
            "https://buildbot.libretro.com/nightly/windows/x86_64/latest/fceumm_libretro.dll.zip"
            "https://buildbot.libretro.com/nightly/windows/x86_64/latest/snes9x_libretro.dll.zip"
        )
    endif()
elseif (APPLE)
    if (CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
        download_cores(
            "https://buildbot.libretro.com/nightly/apple/osx/arm64/latest/fceumm_libretro.dylib.zip"
            "https://buildbot.libretro.com/nightly/apple/osx/arm64/latest/snes9x_libretro.dylib.zip"
        )
    else()
        download_cores(
            "https://buildbot.libretro.com/nightly/apple/osx/x86_64/latest/fceumm_libretro.dylib.zip"
            "https://buildbot.libretro.com/nightly/apple/osx/x86_64/latest/snes9x_libretro.dylib.zip"
        )
    endif()
elseif (CMAKE_SYSTEM_NAME STREQUAL "Android")
    download_cores(
        "https://buildbot.libretro.com/nightly/android/latest/${CMAKE_ANDROID_ARCH_ABI}/fceumm_libretro_android.so.zip"
        "https://buildbot.libretro.com/nightly/android/latest/${CMAKE_ANDROID_ARCH_ABI}/snes9x_libretro_android.so.zip"
    )
endif()
