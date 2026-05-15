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

# Directory where downloaded resources are extracted
if (NOT DEFINED RESOURCES_DIR)
    set(RESOURCES_DIR "${CMAKE_BINARY_DIR}/resources")
endif()

# download_resources(<url> [<url> ...])
# Downloads each zip URL to the build directory and extracts it into RESOURCES_DIR.
# Already-downloaded archives are skipped on subsequent runs.
function(download_resources)
    file(MAKE_DIRECTORY "${RESOURCES_DIR}")
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

        message(STATUS "Extracting ${ARCHIVE_NAME} to ${RESOURCES_DIR}")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E tar xf "${ARCHIVE_PATH}"
            WORKING_DIRECTORY "${RESOURCES_DIR}"
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
    file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/cores")
    target_link_options(${PROJECT_NAME} PUBLIC
        -sUSE_GLFW=3
        -sMAIN_MODULE=1
        --preload-file "${CMAKE_SOURCE_DIR}/cores@/cores"
    )

endif()
