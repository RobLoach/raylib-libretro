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

#define TMP      "rl_games_test_tmp"
#define CONTENT  TMP "/content"
#define SAVE     TMP "/save"
#define DBPATH   SAVE "/raylib-libretro.db"

static int g_failures = 0;
static void check(bool cond, const char* msg) {
    printf("  [%s] %s\n", cond ? "ok" : "FAIL", msg);
    if (!cond) g_failures++;
}

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
static void deleteGame(const char* rel) { remove(TextFormat("%s/%s", CONTENT, rel)); }

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
    TextCopy(LIBRETRO.saveDirectory, SAVE);
    LibretroGamesSetCallbacks(&TestCoreAvailable, &TestZipInnerExt);

    // ---- system detection (pure functions, no scan) ----
    printf("system detection\n");
    check(TextIsEqual(LibretroGamesDetectSystem("snes"), "super_nes"), "folder 'snes' -> super_nes");
    check(TextIsEqual(LibretroGamesDetectSystem("Nintendo - Super Nintendo Entertainment System"), "super_nes"), "full SNES name -> super_nes");
    check(TextIsEqual(LibretroGamesDetectSystem("Sega - Mega Drive - Genesis"), "mega_drive"), "partial 'Mega Drive - Genesis' -> mega_drive");
    check(TextIsEqual(LibretroGamesDetectSystem("nes"), "nes"), "folder 'nes' -> nes (not super_nes)");
    check(TextIsEqual(LibretroGamesDetectSystem("a totally unknown box"), ""), "unknown folder -> \"\"");
    check(TextIsEqual(LibretroGamesSystemForExtension("sfc"), "super_nes"), "ext 'sfc' -> super_nes");
    check(TextIsEqual(LibretroGamesSystemForExtension("iso"), ""), "ambiguous ext 'iso' -> \"\"");
    check(TextIsEqual(LibretroGamesSystemFamily("game_boy"), "nintendo_handheld"), "game_boy family");
    check(TextIsEqual(LibretroGamesSystemFamily("master_system"), "sega_8_16"), "master_system family");

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

    check(InitLibretroGames(), "InitLibretroGames");
    rescan();
    check(LibretroGamesTotalCount() == 4, TextFormat("total == 4 (got %d)", LibretroGamesTotalCount()));
    check(indexed("Contra.nes"), "root Contra.nes indexed");
    check(indexed("snes/Super Mario World.sfc"), "snes game indexed");
    check(indexed("psx/FF7.m3u"), "m3u playlist indexed");
    check(!indexed("psx/FF7 (Disc 1).cue"), "disc 1 hidden (m3u-referenced)");
    check(!indexed("psx/FF7 (Disc 2).cue"), "disc 2 hidden (m3u-referenced)");
    check(!indexed("GameCube/Metroid Prime.iso"), "gamecube game gated out (no core)");
    check(systemGameCount("super_nes") == 2, TextFormat("super_nes count == 2 (got %d)", systemGameCount("super_nes")));
    check(TextIsEqual(titleOf("Contra.nes"), "Contra"), "title has no extension");

    // ---- unchanged rescan writes nothing ----
    printf("incremental rescan\n");
    CloseLibretroGames();
    check(InitLibretroGames(), "reopen");
    long mtimeBefore = GetFileModTime(DBPATH);
    rescan();
    long mtimeAfter = GetFileModTime(DBPATH);
    check(mtimeBefore == mtimeAfter, "unchanged rescan does not rewrite the DB");
    check(LibretroGamesTotalCount() == 4, "still 4 after no-op rescan");

    // ---- prune a deleted file ----
    printf("prune\n");
    deleteGame("Contra.nes");
    rescan();
    check(LibretroGamesTotalCount() == 3, TextFormat("total == 3 after delete (got %d)", LibretroGamesTotalCount()));
    check(!indexed("Contra.nes"), "deleted file pruned");

    // ---- corruption recovery ----
    printf("corruption recovery\n");
    CloseLibretroGames();
    SaveFileText(DBPATH, (char*)"this is not a sqlite database, just garbage");
    remove(DBPATH "-wal"); remove(DBPATH "-shm");
    check(InitLibretroGames(), "reopen after corruption (rebuilds)");
    check(LibretroGamesTotalCount() == 0, "corrupt DB loads empty");
    rescan();
    check(LibretroGamesTotalCount() == 3, "re-indexed from disk after rebuild");

    // ---- schema version gate ----
    printf("schema version gate\n");
    CloseLibretroGames();
    setUserVersion(99);
    check(InitLibretroGames(), "reopen with stale schema version");
    check(LibretroGamesTotalCount() == 0, "stale-version games table dropped (loads empty)");
    check(readUserVersion() == LIBRETRO_GAMES_SCHEMA_VERSION, TextFormat("user_version reset to %d", LIBRETRO_GAMES_SCHEMA_VERSION));
    rescan();
    check(LibretroGamesTotalCount() == 3, "re-indexed after version-bump rebuild");

    CloseLibretroGames();
    system("rm -rf " TMP);

    printf("\n%s (%d failure%s)\n", g_failures ? "FAILED" : "PASSED", g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
