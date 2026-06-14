# SNKV — single-header key-value store on SQLite's B-tree engine.
# Vendored amalgamation generated from https://github.com/hash-anu/snkv
# (Apache-2.0) via `sh scripts/gen_snkv_header.sh`. Not a git submodule.
# Vector/usearch support is excluded by the generator; encryption (Monocypher)
# is inlined into the amalgamation but unused.
add_library(snkv STATIC snkv_impl.c)

target_include_directories(snkv PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Third-party SQLite code: silence its warnings (so the frontend's stricter
# build stays clean) and keep this large translation unit out of LTO so links
# remain fast and robust.
set_target_properties(snkv PROPERTIES INTERPROCEDURAL_OPTIMIZATION OFF)
if (MSVC)
    # /bigobj — the amalgamation is a single very large translation unit.
    target_compile_options(snkv PRIVATE /w /bigobj)
else()
    target_compile_options(snkv PRIVATE -w)
endif()

# Threads: SQLite needs pthread on desktop. Web (Emscripten) is single-threaded.
if (NOT "${PLATFORM}" STREQUAL "Web")
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads)
    if (Threads_FOUND)
        target_link_libraries(snkv PUBLIC Threads::Threads)
    endif()
endif()

# Math library + dynamic loader (SQLite's default VFS).
if (NOT MSVC)
    target_link_libraries(snkv PUBLIC m)
endif()
target_link_libraries(snkv PUBLIC ${CMAKE_DL_LIBS})
