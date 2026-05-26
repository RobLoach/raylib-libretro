cmake_minimum_required(VERSION 3.22.1)
project(raylib-libretro-android C)

set(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

# Download libretro cores into jniLibs so AGP packages them into lib/<abi>/ in the APK.
set(CORES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/main/jniLibs/${CMAKE_ANDROID_ARCH_ABI}")
file(MAKE_DIRECTORY "${CORES_DIR}")
foreach(URL IN ITEMS
    "https://buildbot.libretro.com/nightly/android/latest/${CMAKE_ANDROID_ARCH_ABI}/fceumm_libretro_android.so.zip"
    "https://buildbot.libretro.com/nightly/android/latest/${CMAKE_ANDROID_ARCH_ABI}/snes9x_libretro_android.so.zip"
    "https://buildbot.libretro.com/nightly/android/latest/${CMAKE_ANDROID_ARCH_ABI}/gambatte_libretro_android.so.zip"
    "https://buildbot.libretro.com/nightly/android/latest/${CMAKE_ANDROID_ARCH_ABI}/mgba_libretro_android.so.zip"
    "https://buildbot.libretro.com/nightly/android/latest/${CMAKE_ANDROID_ARCH_ABI}/picodrive_libretro_android.so.zip"
)
    get_filename_component(ARCHIVE_NAME "${URL}" NAME)
    set(ARCHIVE_PATH "${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}")
    if (NOT EXISTS "${ARCHIVE_PATH}")
        message(STATUS "Downloading ${URL}")
        file(DOWNLOAD "${URL}" "${ARCHIVE_PATH}" STATUS DOWNLOAD_STATUS SHOW_PROGRESS TIMEOUT 60)
        list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
        if (NOT STATUS_CODE EQUAL 0)
            list(GET DOWNLOAD_STATUS 1 STATUS_MSG)
            message(WARNING "Failed to download ${URL}: ${STATUS_MSG}")
            continue()
        endif()
    endif()
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xf "${ARCHIVE_PATH}"
        WORKING_DIRECTORY "${CORES_DIR}"
        RESULT_VARIABLE EXTRACT_RESULT
    )
    if (NOT EXTRACT_RESULT EQUAL 0)
        message(WARNING "Failed to extract ${ARCHIVE_NAME}")
    endif()
endforeach()

# Disable raylib extras
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING  OFF CACHE BOOL "" FORCE)
set(WITH_PIC       ON  CACHE BOOL "" FORCE)

# raylib (vendor)
add_subdirectory(${ROOT}/vendor/raylib ${CMAKE_BINARY_DIR}/vendor/raylib EXCLUDE_FROM_ALL)

# raylib-libretro header interface
add_subdirectory(${ROOT}/include ${CMAKE_BINARY_DIR}/include EXCLUDE_FROM_ALL)

# raylib-libretro-static (wraps raylib + libretro-common)
add_subdirectory(${ROOT}/lib ${CMAKE_BINARY_DIR}/lib EXCLUDE_FROM_ALL)

# libmain.so - loaded by android.app.NativeActivity
add_library(main SHARED
    ${ROOT}/bin/raylib-libretro.c
    ${ROOT}/vendor/physfs/src/physfs.c
    ${ROOT}/vendor/physfs/src/physfs_byteorder.c
    ${ROOT}/vendor/physfs/src/physfs_unicode.c
    ${ROOT}/vendor/physfs/src/physfs_archiver_dir.c
    ${ROOT}/vendor/physfs/src/physfs_archiver_unpacked.c
    ${ROOT}/vendor/physfs/src/physfs_archiver_zip.c
)
target_link_libraries(main PRIVATE raylib-libretro-static)
target_include_directories(main PRIVATE
    ${ROOT}/vendor/raygui/src
    ${ROOT}/vendor/raylib-app
    ${ROOT}/vendor/raylib-physfs
    ${ROOT}/vendor/physfs/src
)
target_compile_definitions(main PRIVATE
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
