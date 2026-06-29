# sqlite3 — official amalgamation (public domain), used by the games database.
add_library(sqlite3 STATIC sqlite3.c)

target_include_directories(sqlite3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Trimmed, single-threaded build: the games layer uses one connection from the
# main loop, so no mutexes / loadable extensions / deprecated APIs are needed.
target_compile_definitions(sqlite3 PRIVATE
    SQLITE_THREADSAFE=0
    SQLITE_OMIT_LOAD_EXTENSION=1
    SQLITE_DEFAULT_MEMSTATUS=0
    SQLITE_OMIT_DEPRECATED=1
    SQLITE_DQS=0
    SQLITE_OMIT_SHARED_CACHE=1
)

# Third-party C: silence its warnings and keep this large TU out of LTO so links
# stay fast and robust. MSVC needs /bigobj for the amalgamation.
set_target_properties(sqlite3 PROPERTIES INTERPROCEDURAL_OPTIMIZATION OFF)
if (MSVC)
    target_compile_options(sqlite3 PRIVATE /w /bigobj)
else()
    target_compile_options(sqlite3 PRIVATE -w)
endif()

target_link_libraries(sqlite3 PUBLIC ${CMAKE_DL_LIBS})
if (NOT MSVC)
    target_link_libraries(sqlite3 PUBLIC m)
endif()
