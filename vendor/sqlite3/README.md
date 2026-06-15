# sqlite3 (vendored amalgamation)

SQLite 3.53.2 official amalgamation (`sqlite3.c` + `sqlite3.h`) from
https://sqlite.org/download.html. Used by the raylib-libretro games database
(`include/raylib-libretro-games.h`). Not a git submodule.

SQLite is in the **public domain** (https://sqlite.org/copyright.html).

Built with a trimmed feature set (see `CMakeLists.txt`): no loadable extensions,
single-threaded, no deprecated APIs. To update: download a newer amalgamation and
replace `sqlite3.c` / `sqlite3.h`.
