add_library(test_software_libretro MODULE
    test_software_libretro.c
)

target_include_directories(test_software_libretro PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../vendor/libretro-common/include
)

# Shared library naming: test_software_libretro.so / .dylib / .dll
set_target_properties(test_software_libretro PROPERTIES
    PREFIX ""
    OUTPUT_NAME "test_software_libretro"
)
if (APPLE)
    set_target_properties(test_software_libretro PROPERTIES SUFFIX ".dylib")
elseif (WIN32)
    set_target_properties(test_software_libretro PROPERTIES SUFFIX ".dll")
else()
    set_target_properties(test_software_libretro PROPERTIES SUFFIX ".so")
endif()

# Copy built core next to the other cores so the frontend finds it.
add_custom_command(TARGET test_software_libretro POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/cores"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:test_software_libretro>"
        "${CMAKE_BINARY_DIR}/cores/$<TARGET_FILE_NAME:test_software_libretro>"
    COMMENT "Copying test_software_libretro to cores/"
)
