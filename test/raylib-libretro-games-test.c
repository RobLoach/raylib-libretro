/**********************************************************************************************
*
*   raylib-libretro-games-test - Headless unit tests for the games database layer.
*
*   The games layer is the one part of raylib-libretro that is fully exercisable without a
*   window or a real libretro core: it is pure logic over raylib file I/O and sqlite3. This
*   test stubs the two frontend hooks, scans a throwaway content tree, and asserts the behaviour
*   the layer guarantees -- system inference, [BIOS]/m3u skipping, the core-availability gate,
*   relative paths, incremental reuse, seen-based pruning, corruption recovery, and the schema
*   version gate. It exits non-zero if any check fails, so CTest/CI fails on a regression.
*
*   No InitWindow(): only raylib's file helpers and sqlite3 are used.
*
**********************************************************************************************/

#include <stdio.h>

#define RAYLIB_LIBRETRO_IMPLEMENTATION
#include "raylib-libretro.h"
#define RAYLIB_LIBRETRO_GAMES_IMPLEMENTATION
#include "raylib-libretro-games.h"
// raylib-libretro-games.h pulls in sqlite3.h, used below to manipulate the DB out-of-band.

#define RAYLIB_ASSERT_LOG LOG_FATAL
#include "raylib-assert.h"

#define TMP      "rl_games_test_tmp"
#define CONTENT  TMP "/content"
#define DBDIR    TMP
#define DBPATH   DBDIR "/raylib-libretro.db"

// --- frontend hooks under test: everything is playable except "gamecube" (no GC core) -----------
static bool TestCoreAvailable(const char* ext, const char* systemId) {
    (void)ext;
    return !(systemId && TextIsEqual(systemId, "gamecube"));
}
static bool TestZipInnerExt(const char* zipPath, char* outExt) { (void)zipPath; TextCopy(outExt, "nes"); return true; }

// --- helpers -------------------------------------------------------------------------------------
static void writeGame(const char* rel, const char* text) {
    char path[1024];
    TextCopy(path, TextFormat("%s/%s", CONTENT, rel));
    char dir[1024];
    TextCopy(dir, GetDirectoryPath(path));
    if (!DirectoryExists(dir)) MakeDirectory(dir);
    SaveFileText(path, (char*)text);
}
static void deleteGame(const char* rel) { FileRemove(TextFormat("%s/%s", CONTENT, rel)); }

static void rescan(void) {
    LibretroGamesStartScan(CONTENT);
    int guard = 0;
    while (UpdateLibretroGamesScan() && guard++ < 1000000) {}
}

static bool indexed(const char* rel) {
    LibretroGamesSetSystemFilter(NULL);
    LibretroGamesRebuildFiltered();
    for (int i = 0; i < LibretroGamesFilteredCount(); i++) {
        LibretroGameEntry* e = LibretroGamesFilteredEntry(i);
        if (e && TextIsEqual(e->relpath, rel)) return true;
    }
    return false;
}
static const char* titleOf(const char* rel) {
    for (int i = 0; i < LibretroGamesFilteredCount(); i++) {
        LibretroGameEntry* e = LibretroGamesFilteredEntry(i);
        if (e && TextIsEqual(e->relpath, rel)) return e->title;
    }
    return "";
}
static int systemGameCount(const char* id) {
    for (int i = 0; i < LibretroGamesSystemCount(); i++) {
        LibretroGameSystem* s = LibretroGamesSystem(i);
        if (s && TextIsEqual(s->id, id)) return s->count;
    }
    return 0;
}

static int readUserVersion(void) {
    sqlite3* db = NULL; int v = -1;
    if (sqlite3_open(DBPATH, &db) == SQLITE_OK && db) {
        sqlite3_stmt* st = NULL;
        if (sqlite3_prepare_v2(db, "PRAGMA user_version;", -1, &st, NULL) == SQLITE_OK) {
            if (sqlite3_step(st) == SQLITE_ROW) v = sqlite3_column_int(st, 0);
            sqlite3_finalize(st);
        }
    }
    if (db) sqlite3_close(db);
    return v;
}
static void setUserVersion(int v) {
    sqlite3* db = NULL;
    if (sqlite3_open(DBPATH, &db) == SQLITE_OK && db) sqlite3_exec(db, TextFormat("PRAGMA user_version=%d;", v), NULL, NULL, NULL);
    if (db) sqlite3_close(db);
}

int main(void) {
    SetTraceLogLevel(LOG_WARNING);
    system("rm -rf " TMP);
    LibretroGamesSetCallbacks(&TestCoreAvailable, &TestZipInnerExt);

    // ---- system detection (pure functions, no scan) ----
    printf("system detection\n");
    AssertStringEqual(LibretroGamesDetectSystem("snes"), "super_nes", "folder 'snes' -> super_nes");
    AssertStringEqual(LibretroGamesDetectSystem("Nintendo - Super Nintendo Entertainment System"), "super_nes", "full SNES name -> super_nes");
    AssertStringEqual(LibretroGamesDetectSystem("Sega - Mega Drive - Genesis"), "mega_drive", "partial 'Mega Drive - Genesis' -> mega_drive");
    AssertStringEqual(LibretroGamesDetectSystem("nes"), "nes", "folder 'nes' -> nes (not super_nes)");
    AssertStringEqual(LibretroGamesDetectSystem("a totally unknown box"), "", "unknown folder -> \"\"");
    AssertStringEqual(LibretroGamesSystemForExtension("sfc"), "super_nes", "ext 'sfc' -> super_nes");
    AssertStringEqual(LibretroGamesSystemForExtension("iso"), "", "ambiguous ext 'iso' -> \"\"");
    AssertStringEqual(LibretroGamesSystemFamily("game_boy"), "nintendo_handheld", "game_boy family");
    AssertStringEqual(LibretroGamesSystemFamily("master_system"), "sega_8_16", "master_system family");

    // ---- first scan ----
    printf("first scan\n");
    writeGame("snes/Super Mario World.sfc", "rom");
    writeGame("Nintendo - Super Nintendo Entertainment System/Legend of Zelda.sfc", "rom");
    writeGame("Contra.nes", "rom");                                   // root file -> ext-based system
    writeGame("[BIOS] scph5500.bin", "bios");                        // must be skipped
    writeGame("GameCube/Metroid Prime.iso", "rom");                  // gated out (no gc core)
    writeGame("psx/FF7 (Disc 1).cue", "d1");                         // hidden by the .m3u
    writeGame("psx/FF7 (Disc 2).cue", "d2");                         // hidden by the .m3u
    writeGame("psx/FF7.m3u", "FF7 (Disc 1).cue\nFF7 (Disc 2).cue\n");

    Assert(InitLibretroGames(DBDIR), "InitLibretroGames");
    rescan();
    AssertEqual(LibretroGamesTotalCount(), 4, TextFormat("total == 4 (got %d)", LibretroGamesTotalCount()));
    Assert(indexed("Contra.nes"), "root Contra.nes indexed");
    Assert(indexed("snes/Super Mario World.sfc"), "snes game indexed");
    Assert(indexed("psx/FF7.m3u"), "m3u playlist indexed");
    AssertNot(indexed("psx/FF7 (Disc 1).cue"), "disc 1 hidden (m3u-referenced)");
    AssertNot(indexed("psx/FF7 (Disc 2).cue"), "disc 2 hidden (m3u-referenced)");
    AssertNot(indexed("GameCube/Metroid Prime.iso"), "gamecube game gated out (no core)");
    AssertEqual(systemGameCount("super_nes"), 2, TextFormat("super_nes count == 2 (got %d)", systemGameCount("super_nes")));
    AssertStringEqual(titleOf("Contra.nes"), "Contra", "title has no extension");

    // ---- unchanged rescan writes nothing ----
    printf("incremental rescan\n");
    CloseLibretroGames();
    Assert(InitLibretroGames(DBDIR), "reopen");
    long mtimeBefore = GetFileModTime(DBPATH);
    rescan();
    long mtimeAfter = GetFileModTime(DBPATH);
    AssertEqual(mtimeBefore, mtimeAfter, "unchanged rescan does not rewrite the DB");
    AssertEqual(LibretroGamesTotalCount(), 4, "still 4 after no-op rescan");

    // ---- prune a deleted file ----
    printf("prune\n");
    deleteGame("Contra.nes");
    rescan();
    AssertEqual(LibretroGamesTotalCount(), 3, TextFormat("total == 3 after delete (got %d)", LibretroGamesTotalCount()));
    AssertNot(indexed("Contra.nes"), "deleted file pruned");

    // ---- corruption recovery ----
    printf("corruption recovery\n");
    CloseLibretroGames();
    SaveFileText(DBPATH, (char*)"this is not a sqlite database, just garbage");
    FileRemove(TextFormat("%s-wal", DBPATH));
    FileRemove(TextFormat("%s-shm", DBPATH));
    Assert(InitLibretroGames(DBDIR), "reopen after corruption (rebuilds)");
    AssertEqual(LibretroGamesTotalCount(), 0, "corrupt DB loads empty");
    rescan();
    AssertEqual(LibretroGamesTotalCount(), 3, "re-indexed from disk after rebuild");

    // ---- schema version gate ----
    printf("schema version gate\n");
    CloseLibretroGames();
    setUserVersion(99);
    Assert(InitLibretroGames(DBDIR), "reopen with stale schema version");
    AssertEqual(LibretroGamesTotalCount(), 0, "stale-version games table dropped (loads empty)");
    AssertEqual(readUserVersion(), LIBRETRO_GAMES_SCHEMA_VERSION, TextFormat("user_version reset to %d", LIBRETRO_GAMES_SCHEMA_VERSION));
    rescan();
    AssertEqual(LibretroGamesTotalCount(), 3, "re-indexed after version-bump rebuild");

    CloseLibretroGames();
    system("rm -rf " TMP);

    printf("\nPASSED\n");
    return 0;
}
