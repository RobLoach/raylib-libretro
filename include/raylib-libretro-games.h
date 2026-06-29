/**********************************************************************************************
*
*   raylib-libretro-games - Games database: scan a content directory, infer each game's
*                           system from its folder name, and index everything into a
*                           persistent SQLite database for browsing by system.
*
*   LAYER
*
*       Opt-in, core-agnostic frontend layer. Depends on raylib-libretro.h (paths) and the
*       vendored SQLite amalgamation (vendor/sqlite3); it does NOT depend on the menu or on
*       any libretro core. The menu layer renders the data this layer indexes and owns the
*       system->core launch decision (it has the core list). Define
*       RAYLIB_LIBRETRO_GAMES_IMPLEMENTATION in exactly one translation unit (the menu's
*       implementation enables it automatically).
*
*   DATA MODEL (sqlite3)
*
*       games(relpath TEXT PRIMARY KEY, system TEXT, title TEXT)
*       meta(key TEXT PRIMARY KEY, value TEXT)            -- "contentdir" = last-scanned root
*
*       relpath is RELATIVE to the content directory (never absolute). The in-memory entry
*       list is both the source of truth the UI reads and the incremental-scan cache; the
*       database is loaded once at startup and rewritten only when the index actually changes.
*       A scan reuses any entry whose relpath still exists on disk -- no per-file stat -- so an
*       unchanged library re-scans with zero file opens and zero database writes. Replacing a
*       file in place (same name) is not re-detected until a content-dir change or rescan.
*
*   LICENSE: GPL-3.0-or-later
*
**********************************************************************************************/

#ifndef RAYLIB_LIBRETRO_GAMES_H
#define RAYLIB_LIBRETRO_GAMES_H

#include "raylib.h"
#include "raylib-libretro.h"   // GetLibretroDirectory, RETRO_ENVIRONMENT_*, RAYLIB_LIBRETRO_VFS_MAX_PATH
#include "../vendor/c-vector/cvector.h"

#ifndef RAYLIB_LIBRETRO_GAMES_MAX_PATH
#define RAYLIB_LIBRETRO_GAMES_MAX_PATH 1024
#endif

#ifndef RAYLIB_LIBRETRO_GAMES_SCAN_BATCH
#define RAYLIB_LIBRETRO_GAMES_SCAN_BATCH 96    // files processed per Update() while scanning
#endif

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * One indexed game. `relpath`/`title` are heap strings sized to fit; `systemId` is an interned
 * pointer into the static system table (the canonical libretro .info `systemid`), or "" when
 * unknown. `seen` is internal scan bookkeeping.
 */
typedef struct LibretroGameEntry {
    char* relpath;          // relative to the content directory
    char* title;            // display title (file name, no extension)
    const char* systemId;   // interned canonical system id, or ""
    bool seen;              // marked during a scan; survivors are kept, the rest pruned
} LibretroGameEntry;

/** A distinct system discovered while indexing, with how many games it holds. */
typedef struct LibretroGameSystem {
    char id[48];
    char display[64];
    int count;
} LibretroGameSystem;

typedef enum LibretroGamesScanState {
    LIBRETRO_GAMES_IDLE = 0,
    LIBRETRO_GAMES_SCANNING
} LibretroGamesScanState;

// Lifecycle
bool InitLibretroGames(const char* dbDir);                  // open the database in dbDir, load the index
void CloseLibretroGames(void);                             // free memory, close the database

// Scanning (incremental, driven from the app's Update())
void LibretroGamesStartScan(const char* contentDir);       // (re)index a content directory
bool UpdateLibretroGamesScan(void);                        // process a batch; true while scanning
bool IsLibretroGamesScanning(void);
float GetLibretroGamesScanProgress(void);                  // 0..1
void DrawLibretroGamesScanProgress(void);                  // translucent progress overlay (inside a frame)
bool LibretroGamesScanJustFinished(void);                  // one-shot: true once after a scan completes
void LibretroGamesSetFont(Font font);                      // optional font for the overlay

// System detection / naming
const char* LibretroGamesDetectSystem(const char* folderName);     // canonical id from a folder name
const char* LibretroGamesSystemForExtension(const char* extLower); // canonical id from a file extension
const char* LibretroGamesSystemDisplayName(const char* systemId);  // human-readable name
const char* LibretroGamesSystemFamily(const char* systemId);       // family id (a core covering one member covers all)

/**
 * Frontend hooks (nullable). The menu installs these via LibretroGamesSetCallbacks so the
 * core-agnostic games layer can ask the loaded core list whether content is actually playable,
 * and peek inside archives.
 *
 *  - coreAvailable: true when an installed core can run @p extLower for @p systemId. Games with
 *    no available associated core are NOT indexed (e.g. GameCube games with no GameCube core).
 *    When NULL, nothing is indexed (the layer has no other way to know what is playable).
 *  - zipInnerExt: writes the lower-case extension (no dot) of an archive's primary inner file
 *    into @p outExt (>= 16 bytes); false if it cannot be determined.
 */
typedef bool (*LibretroGamesCoreAvailableFn)(const char* extLower, const char* systemId);
typedef bool (*LibretroGamesZipInnerExtFn)(const char* zipPath, char* outExt);
void LibretroGamesSetCallbacks(LibretroGamesCoreAvailableFn coreAvailable, LibretroGamesZipInnerExtFn zipInnerExt);

// Views (filtered by system)
void LibretroGamesSetSystemFilter(const char* systemId);   // NULL/"" = all systems
void LibretroGamesRebuildFiltered(void);
int LibretroGamesFilteredCount(void);
const char* LibretroGamesFilteredLabel(int row);           // game title (stable pointer)
LibretroGameEntry* LibretroGamesFilteredEntry(int row);

// Whole-library queries
const char* LibretroGamesContentDir(void);
int LibretroGamesTotalCount(void);
int LibretroGamesSystemCount(void);
LibretroGameSystem* LibretroGamesSystem(int index);

#if defined(__cplusplus)
}
#endif

#endif  // RAYLIB_LIBRETRO_GAMES_H

/**********************************************************************************************
*   Implementation
**********************************************************************************************/

#ifdef RAYLIB_LIBRETRO_GAMES_IMPLEMENTATION
#ifndef RAYLIB_LIBRETRO_GAMES_IMPLEMENTATION_ONCE
#define RAYLIB_LIBRETRO_GAMES_IMPLEMENTATION_ONCE

#include <stdlib.h>   // qsort
#include "sqlite3.h"
#if defined(PLATFORM_WEB) || defined(__EMSCRIPTEN__)
#include <emscripten.h>   // EM_ASM: flush SQLite writes from MEMFS to IDBFS
#endif

typedef struct LibretroGames {
    sqlite3* db;
    LibretroGamesCoreAvailableFn coreAvailable;   // hook: can an installed core run this content?
    LibretroGamesZipInnerExtFn   zipInnerExt;     // hook: peek an archive's primary inner extension
    char contentDir[RAYLIB_LIBRETRO_GAMES_MAX_PATH];
    cvector(LibretroGameEntry*) entries;    // every indexed game (in memory); the persistent cache
    cvector(LibretroGameSystem*) systems;   // distinct systems, with counts
    cvector(int) filtered;                  // row -> entries index (current view)
    cvector(char*) m3uRefs;                 // disc files referenced by .m3u playlists (hidden from the list)
    char systemFilter[48];                  // "" = all
    LibretroGamesScanState state;
    FilePathList scanFiles;
    bool scanFilesLoaded;
    int scanIndex;
    int scanTotal;
    int oldCount;                           // entries present at scan start (the sorted bsearch range)
    bool dirty;                             // an entry was added or pruned this scan -> rewrite the DB
    bool justFinished;
    Font font;
} LibretroGames;

static LibretroGames GAMES = {0};

void LibretroGamesSetCallbacks(LibretroGamesCoreAvailableFn coreAvailable, LibretroGamesZipInnerExtFn zipInnerExt) {
    GAMES.coreAvailable = coreAvailable;
    GAMES.zipInnerExt = zipInnerExt;
}

// On the web, SQLite commits land in the in-memory MEMFS and must be pushed to IDBFS to survive a
// page reload; a no-op on every other platform. Mirrors the menu's LibretroFlushPersistentStorage.
static void LibretroGamesFlushWeb(void) {
#if defined(PLATFORM_WEB) || defined(__EMSCRIPTEN__)
    EM_ASM({ FS.syncfs(false, function(err) { if (err) console.error("GAMES: IDBFS sync failed:", err); }); });
#endif
}

// ---- canonical system table (id aligned with libretro .info `systemid` where known) ----
// `family` groups systems that a single multi-system core handles (e.g. a Genesis core also runs
// Master System / Game Gear), so core-availability matching tolerates the loaded core's systemid.
// Folder match rule: normalize the folder name (lower-case, alphanumeric only), then pick the
// LONGEST alias (across all systems) that is contained in it. Aliases of <=3 chars must match the
// whole normalized name exactly, so short tokens like "gb"/"nes" do not match inside longer names.
typedef struct {
    const char* id;
    const char* display;
    const char* family;      // shared core family; "" means it is its own family (use id)
    const char* aliases;     // pipe-separated, normalized (lower-case, alphanumeric only)
} LibretroGamesSystemAlias;

static const LibretroGamesSystemAlias LIBRETRO_GAMES_SYSTEMS[] = {
    { "super_nes",          "Super Nintendo",                "",                  "snes|supernintendo|supernes|superfamicom|sfc|nintendosupernintendoentertainmentsystem" },
    { "nes",                "Nintendo Entertainment System", "",                  "nes|nintendoentertainmentsystem|famicom|nintendofamicom" },
    { "nintendo64",         "Nintendo 64",                   "",                  "n64|nintendo64" },
    { "gamecube",           "Nintendo GameCube",             "",                  "gamecube|nintendogamecube" },
    { "wii",                "Nintendo Wii",                  "",                  "wii|nintendowii" },
    { "game_boy_advance",   "Game Boy Advance",              "nintendo_handheld", "gba|gameboyadvance|nintendogameboyadvance" },
    { "game_boy_color",     "Game Boy Color",                "nintendo_handheld", "gbc|gameboycolor|nintendogameboycolor" },
    { "game_boy",           "Game Boy",                      "nintendo_handheld", "gb|gameboy|nintendogameboy" },
    { "nintendo_ds",        "Nintendo DS",                   "",                  "nds|nintendods" },
    { "virtual_boy",        "Virtual Boy",                   "",                  "vb|virtualboy|nintendovirtualboy" },
    { "mega_drive",         "Sega Genesis / Mega Drive",     "sega_8_16",         "genesis|megadrive|segagenesis|segamegadrive|segamegadrivegenesis" },
    { "sega_cd",            "Sega CD / Mega-CD",             "sega_8_16",         "segacd|megacd|segamegacd|segamegacdsegacd" },
    { "master_system",      "Sega Master System",            "sega_8_16",         "sms|mastersystem|segamastersystem|segamastersystemmarkiii" },
    { "game_gear",          "Sega Game Gear",                "sega_8_16",         "gg|gamegear|segagamegear" },
    { "sg1000",             "Sega SG-1000",                  "sega_8_16",         "sg1000|segasg1000" },
    { "saturn",             "Sega Saturn",                   "",                  "saturn|segasaturn" },
    { "dreamcast",          "Sega Dreamcast",                "",                  "dreamcast|segadreamcast" },
    { "playstation",        "Sony PlayStation",              "",                  "psx|ps1|playstation|sonyplaystation" },
    { "playstation2",       "Sony PlayStation 2",            "",                  "ps2|playstation2|sonyplaystation2" },
    { "psp",                "Sony PSP",                      "",                  "psp|playstationportable|sonypsp" },
    { "pc_engine",          "PC Engine / TurboGrafx-16",     "",                  "pcengine|turbografx|turbografx16|necpcengine|necpcenginesupergrafx" },
    { "neo_geo",            "Neo Geo",                       "",                  "neogeo|snkneogeo" },
    { "neo_geo_pocket",     "Neo Geo Pocket",                "",                  "neogeopocket|ngp|ngpc|snkneogeopocket" },
    { "wonderswan",         "WonderSwan",                    "",                  "wonderswan|bandaiwonderswan" },
    { "atari_2600",         "Atari 2600",                    "",                  "atari2600|vcs" },
    { "atari_7800",         "Atari 7800",                    "",                  "atari7800" },
    { "atari_lynx",         "Atari Lynx",                    "",                  "atarilynx|lynx" },
    { "atari_jaguar",       "Atari Jaguar",                  "",                  "atarijaguar|jaguar" },
    { "colecovision",       "ColecoVision",                  "",                  "colecovision|coleco" },
    { "intellivision",      "Intellivision",                 "",                  "intellivision" },
    { "vectrex",            "Vectrex",                       "",                  "vectrex" },
    { "msx",                "MSX",                           "",                  "msx|msx2|microsoftmsx" },
    { "commodore_64",       "Commodore 64",                  "",                  "c64|commodore64" },
    { "amiga",              "Amiga",                         "",                  "amiga|commodoreamiga" },
    { "zx_spectrum",        "ZX Spectrum",                   "",                  "zxspectrum|spectrum|sinclairzxspectrum" },
    { "amstrad_cpc",        "Amstrad CPC",                   "",                  "amstradcpc|amstrad|cpc" },
    { "3do",                "3DO",                           "",                  "3do|panasonic3do" },
    { "arcade",             "Arcade",                        "",                  "arcade|mame|fbneo|fba|finalburn|finalburnneo" },
    { "dos",                "DOS",                           "",                  "dos|msdos|pcdos" },
    { "scummvm",            "ScummVM",                       "",                  "scummvm" },
};
#define LIBRETRO_GAMES_SYSTEM_ALIAS_COUNT ((int)(sizeof(LIBRETRO_GAMES_SYSTEMS)/sizeof(LIBRETRO_GAMES_SYSTEMS[0])))

// Extension -> canonical system, for content sitting in the content root (no system folder).
// Only unambiguous extensions are mapped; shared ones (bin/iso/cue/chd/img/zip/...) stay "" (Other).
typedef struct { const char* ext; const char* id; } LibretroGamesExtSystem;
static const LibretroGamesExtSystem LIBRETRO_GAMES_EXT_SYSTEMS[] = {
    { "nes","nes" }, { "fds","nes" }, { "unf","nes" }, { "unif","nes" },
    { "sfc","super_nes" }, { "smc","super_nes" }, { "swc","super_nes" }, { "fig","super_nes" }, { "bs","super_nes" },
    { "gb","game_boy" }, { "gbc","game_boy_color" }, { "gba","game_boy_advance" },
    { "n64","nintendo64" }, { "z64","nintendo64" }, { "v64","nintendo64" },
    { "nds","nintendo_ds" }, { "gcm","gamecube" }, { "rvz","gamecube" }, { "wbfs","gamecube" },
    { "md","mega_drive" }, { "smd","mega_drive" }, { "gen","mega_drive" },
    { "sms","master_system" }, { "gg","game_gear" }, { "sg","sg1000" },
    { "pce","pc_engine" }, { "sgx","pc_engine" },
    { "ws","wonderswan" }, { "wsc","wonderswan" },
    { "ngp","neo_geo_pocket" }, { "ngc","neo_geo_pocket" }, { "ngpc","neo_geo_pocket" },
    { "a26","atari_2600" }, { "a78","atari_7800" }, { "lnx","atari_lynx" }, { "lyx","atari_lynx" },
    { "j64","atari_jaguar" }, { "jag","atari_jaguar" },
    { "vec","vectrex" }, { "col","colecovision" }, { "int","intellivision" }, { "vb","virtual_boy" },
    { "d64","commodore_64" }, { "t64","commodore_64" }, { "crt","commodore_64" }, { "adf","amiga" },
    { "msx","msx" }, { "mx1","msx" }, { "mx2","msx" },
};
#define LIBRETRO_GAMES_EXT_SYSTEM_COUNT ((int)(sizeof(LIBRETRO_GAMES_EXT_SYSTEMS)/sizeof(LIBRETRO_GAMES_EXT_SYSTEMS[0])))

// ---------------------------------------------------------------------------------------------
// Small string helpers (kept dependency-free, raylib-flavoured)
// ---------------------------------------------------------------------------------------------

static char LibretroGamesLowerChar(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

/** Normalize @p src into @p dst (size @p dstSize): lower-case, alphanumeric only. */
static void LibretroGamesNormalize(const char* src, char* dst, int dstSize) {
    int n = 0;
    for (const char* p = src; *p && n < dstSize - 1; p++) {
        char c = LibretroGamesLowerChar(*p);
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) dst[n++] = c;
    }
    dst[n] = '\0';
}

/** True when @p s begins with @p prefix. */
static bool LibretroGamesHasPrefix(const char* s, const char* prefix) {
    while (*prefix) {
        if (*s != *prefix) return false;
        s++; prefix++;
    }
    return true;
}

/** Case-sensitive ordering (for sorting/binary-searching relpaths). */
static int LibretroGamesPathCmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

static int LibretroGamesStrCaseCmp(const char* a, const char* b) {
    while (*a && *b) {
        char ca = LibretroGamesLowerChar(*a), cb = LibretroGamesLowerChar(*b);
        if (ca != cb) return (int)ca - (int)cb;
        a++; b++;
    }
    return (int)LibretroGamesLowerChar(*a) - (int)LibretroGamesLowerChar(*b);
}

/** A heap copy of @p s sized to exactly fit it (raylib MemAlloc; free with MemFree). */
static char* LibretroGamesDup(const char* s) {
    if (!s) s = "";
    int n = (int)TextLength(s);
    char* p = (char*)MemAlloc((unsigned)(n + 1));
    TextCopy(p, s);
    return p;
}

// ---------------------------------------------------------------------------------------------
// System detection / naming
// ---------------------------------------------------------------------------------------------

/** True when an alias of length @p len is contained in @p normFolder per the match rule. */
static bool LibretroGamesAliasMatches(const char* normFolder, const char* alias, int len) {
    if (len <= 3) return TextIsEqual(normFolder, alias);     // short token: exact-only
    for (const char* h = normFolder; *h; h++) {
        if (LibretroGamesHasPrefix(h, alias)) return true;
    }
    return false;
}

const char* LibretroGamesDetectSystem(const char* folderName) {
    if (!folderName || !folderName[0]) return "";
    char norm[128];
    LibretroGamesNormalize(folderName, norm, sizeof(norm));
    if (!norm[0]) return "";

    const char* bestId = "";
    int bestLen = 0;
    for (int i = 0; i < LIBRETRO_GAMES_SYSTEM_ALIAS_COUNT; i++) {
        const char* aliases = LIBRETRO_GAMES_SYSTEMS[i].aliases;
        int start = 0;
        int total = (int)TextLength(aliases);
        for (int j = 0; j <= total; j++) {
            if (aliases[j] == '|' || aliases[j] == '\0') {
                int len = j - start;
                if (len > 0 && len > bestLen) {
                    char alias[64];
                    if (len < (int)sizeof(alias)) {
                        for (int k = 0; k < len; k++) alias[k] = aliases[start + k];
                        alias[len] = '\0';
                        if (LibretroGamesAliasMatches(norm, alias, len)) {
                            bestLen = len;
                            bestId = LIBRETRO_GAMES_SYSTEMS[i].id;
                        }
                    }
                }
                start = j + 1;
            }
        }
    }
    return bestId;
}

const char* LibretroGamesSystemDisplayName(const char* systemId) {
    if (!systemId || !systemId[0]) return "Other";
    for (int i = 0; i < LIBRETRO_GAMES_SYSTEM_ALIAS_COUNT; i++) {
        if (TextIsEqual(systemId, LIBRETRO_GAMES_SYSTEMS[i].id)) return LIBRETRO_GAMES_SYSTEMS[i].display;
    }
    return systemId;
}

const char* LibretroGamesSystemFamily(const char* systemId) {
    if (!systemId || !systemId[0]) return "";
    for (int i = 0; i < LIBRETRO_GAMES_SYSTEM_ALIAS_COUNT; i++) {
        if (TextIsEqual(systemId, LIBRETRO_GAMES_SYSTEMS[i].id)) {
            return LIBRETRO_GAMES_SYSTEMS[i].family[0] ? LIBRETRO_GAMES_SYSTEMS[i].family : LIBRETRO_GAMES_SYSTEMS[i].id;
        }
    }
    return systemId;   // unknown id is its own family
}

const char* LibretroGamesSystemForExtension(const char* extLower) {
    if (!extLower || !extLower[0]) return "";
    for (int i = 0; i < LIBRETRO_GAMES_EXT_SYSTEM_COUNT; i++) {
        if (TextIsEqual(extLower, LIBRETRO_GAMES_EXT_SYSTEMS[i].ext)) return LIBRETRO_GAMES_EXT_SYSTEMS[i].id;
    }
    return "";   // ambiguous or unknown extension -> Other
}

/** Resolve a system id to its stable pointer in the static table ("" when unknown), so entries
 *  store an interned id instead of a per-entry copy. */
static const char* LibretroGamesInternSystemId(const char* id) {
    if (!id || !id[0]) return "";
    for (int i = 0; i < LIBRETRO_GAMES_SYSTEM_ALIAS_COUNT; i++) {
        if (TextIsEqual(id, LIBRETRO_GAMES_SYSTEMS[i].id)) return LIBRETRO_GAMES_SYSTEMS[i].id;
    }
    return "";
}

// ---------------------------------------------------------------------------------------------
// Entry helpers
// ---------------------------------------------------------------------------------------------

/** Display title: the file name with its extension removed (no directory, no ".ext"). */
static void LibretroGamesTitle(const char* relpath, char* out, int outSize) {
    const char* name = GetFileName(relpath);
    int n = 0, dot = -1;
    for (; name[n] && n < outSize - 1; n++) { out[n] = name[n]; if (name[n] == '.') dot = n; }
    out[n] = '\0';
    if (dot > 0) out[dot] = '\0';   // strip extension (keep leading-dot names intact)
}

/** True when a file is a BIOS image (No-Intro "[BIOS]" filename prefix) -- never indexed. */
static bool LibretroGamesIsBios(const char* relpath) {
    const char* name = GetFileName(relpath);
    const char* bios = "[bios]";
    for (int i = 0; bios[i]; i++) { if (LibretroGamesLowerChar(name[i]) != bios[i]) return false; }
    return true;
}

// ---------------------------------------------------------------------------------------------
// In-memory list management
// ---------------------------------------------------------------------------------------------

static void LibretroGamesFreeEntry(LibretroGameEntry* e) {
    if (!e) return;
    MemFree(e->relpath);
    MemFree(e->title);
    MemFree(e);                 // systemId is interned (static) -- not freed
}

static void LibretroGamesFreeEntries(void) {
    for (size_t i = 0; i < cvector_size(GAMES.entries); i++) LibretroGamesFreeEntry(GAMES.entries[i]);
    cvector_free(GAMES.entries);
    GAMES.entries = NULL;
}

static void LibretroGamesFreeSystems(void) {
    for (size_t i = 0; i < cvector_size(GAMES.systems); i++) MemFree(GAMES.systems[i]);
    cvector_free(GAMES.systems);
    GAMES.systems = NULL;
}

/** Append an entry. @p title may be NULL/"" to derive it from @p relpath. Marked seen. */
static LibretroGameEntry* LibretroGamesPushEntry(const char* relpath, const char* system, const char* title) {
    LibretroGameEntry* e = (LibretroGameEntry*)MemAlloc(sizeof(LibretroGameEntry));
    e->relpath = LibretroGamesDup(relpath);
    if (title && title[0]) {
        e->title = LibretroGamesDup(title);
    } else {
        char t[256];
        LibretroGamesTitle(relpath, t, sizeof(t));
        e->title = LibretroGamesDup(t);
    }
    e->systemId = LibretroGamesInternSystemId(system);
    e->seen = true;
    cvector_push_back(GAMES.entries, e);
    return e;
}

static void LibretroGamesRebuildSystems(void) {
    LibretroGamesFreeSystems();
    for (size_t i = 0; i < cvector_size(GAMES.entries); i++) {
        const char* id = GAMES.entries[i]->systemId;
        LibretroGameSystem* found = NULL;
        for (size_t s = 0; s < cvector_size(GAMES.systems); s++) {
            if (TextIsEqual(GAMES.systems[s]->id, id)) { found = GAMES.systems[s]; break; }
        }
        if (found) { found->count++; continue; }
        LibretroGameSystem* sys = (LibretroGameSystem*)MemAlloc(sizeof(LibretroGameSystem));
        TextCopy(sys->id, id);
        TextCopy(sys->display, LibretroGamesSystemDisplayName(id));
        sys->count = 1;
        cvector_push_back(GAMES.systems, sys);
    }
}

static int LibretroGamesSystemSort(const void* a, const void* b) {
    const LibretroGameSystem* sa = *(const LibretroGameSystem* const*)a;
    const LibretroGameSystem* sb = *(const LibretroGameSystem* const*)b;
    return LibretroGamesStrCaseCmp(sa->display, sb->display);
}

static int LibretroGamesFilteredSort(const void* a, const void* b) {
    int ia = *(const int*)a, ib = *(const int*)b;
    return LibretroGamesStrCaseCmp(GAMES.entries[ia]->title, GAMES.entries[ib]->title);
}

static int LibretroGamesPathSort(const void* a, const void* b) {
    const LibretroGameEntry* ea = *(const LibretroGameEntry* const*)a;
    const LibretroGameEntry* eb = *(const LibretroGameEntry* const*)b;
    return LibretroGamesPathCmp(ea->relpath, eb->relpath);
}

/** Binary-search the (relpath-sorted) cache range [0, oldCount) for @p relpath. */
static LibretroGameEntry* LibretroGamesFindCached(const char* relpath) {
    int lo = 0, hi = GAMES.oldCount - 1;
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        int c = LibretroGamesPathCmp(GAMES.entries[mid]->relpath, relpath);
        if (c == 0) return GAMES.entries[mid];
        if (c < 0) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

void LibretroGamesRebuildFiltered(void) {
    cvector_free(GAMES.filtered);
    GAMES.filtered = NULL;

    for (size_t i = 0; i < cvector_size(GAMES.entries); i++) {
        LibretroGameEntry* e = GAMES.entries[i];
        if (GAMES.systemFilter[0] && !TextIsEqual(e->systemId, GAMES.systemFilter)) continue;
        cvector_push_back(GAMES.filtered, (int)i);
    }

    if (cvector_size(GAMES.filtered) > 1) {
        qsort(GAMES.filtered, cvector_size(GAMES.filtered), sizeof(int), LibretroGamesFilteredSort);
    }
}

// ---------------------------------------------------------------------------------------------
// Database (sqlite3): loaded once at startup, rewritten only when the index changes
// ---------------------------------------------------------------------------------------------

// Bump on any `games` column change. The table is a disposable cache (the disk is the source of
// truth), so a version mismatch just drops `games` and lets the next scan rebuild it -- no ALTERs.
#define LIBRETRO_GAMES_SCHEMA_VERSION 1

static const char* LIBRETRO_GAMES_SCHEMA =
    "CREATE TABLE IF NOT EXISTS games ("
    " relpath TEXT PRIMARY KEY,"
    " system  TEXT NOT NULL DEFAULT '',"
    " title   TEXT NOT NULL DEFAULT '');"
    "CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value TEXT);";

static void LibretroGamesExec(const char* sql) {
    if (GAMES.db) sqlite3_exec(GAMES.db, sql, NULL, NULL, NULL);
}

/** Read a meta value into @p out (size @p outSize); out[0]='\0' when absent. */
static void LibretroGamesMetaGet(const char* key, char* out, int outSize) {
    out[0] = '\0';
    if (!GAMES.db) return;
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(GAMES.db, "SELECT value FROM meta WHERE key=?;", -1, &st, NULL) != SQLITE_OK) return;
    sqlite3_bind_text(st, 1, key, -1, SQLITE_STATIC);
    if (sqlite3_step(st) == SQLITE_ROW) {
        const char* v = (const char*)sqlite3_column_text(st, 0);
        if (v) { int n = 0; for (; v[n] && n < outSize - 1; n++) out[n] = v[n]; out[n] = '\0'; }
    }
    sqlite3_finalize(st);
}

static void LibretroGamesMetaSet(const char* key, const char* value) {
    if (!GAMES.db) return;
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(GAMES.db, "INSERT OR REPLACE INTO meta(key,value) VALUES(?,?);", -1, &st, NULL) != SQLITE_OK) return;
    sqlite3_bind_text(st, 1, key, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 2, value, -1, SQLITE_STATIC);
    sqlite3_step(st);
    sqlite3_finalize(st);
}

static void LibretroGamesLoadFromDB(void) {
    LibretroGamesFreeEntries();
    if (!GAMES.db) return;
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(GAMES.db, "SELECT relpath,system,title FROM games;", -1, &st, NULL) != SQLITE_OK) return;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char* relpath = (const char*)sqlite3_column_text(st, 0);
        if (!relpath || !relpath[0]) continue;
        LibretroGamesPushEntry(relpath,
            (const char*)sqlite3_column_text(st, 1),
            (const char*)sqlite3_column_text(st, 2));
    }
    sqlite3_finalize(st);
}

/** Replace the whole games table with the current in-memory list, in one transaction. */
static void LibretroGamesSaveToDB(void) {
    if (!GAMES.db) return;
    LibretroGamesExec("BEGIN; DELETE FROM games;");
    sqlite3_stmt* ins = NULL;
    bool ok = (sqlite3_prepare_v2(GAMES.db, "INSERT INTO games(relpath,system,title) VALUES(?,?,?);", -1, &ins, NULL) == SQLITE_OK);
    if (ok) {
        for (size_t i = 0; i < cvector_size(GAMES.entries); i++) {
            LibretroGameEntry* e = GAMES.entries[i];
            sqlite3_reset(ins);
            sqlite3_bind_text(ins, 1, e->relpath, -1, SQLITE_STATIC);
            sqlite3_bind_text(ins, 2, e->systemId, -1, SQLITE_STATIC);
            sqlite3_bind_text(ins, 3, e->title, -1, SQLITE_STATIC);
            if (sqlite3_step(ins) != SQLITE_DONE) { ok = false; break; }
        }
        sqlite3_finalize(ins);
    }
    // Never COMMIT a failed rewrite: committing the bare DELETE would wipe the whole library.
    if (ok) {
        LibretroGamesExec("COMMIT;");
        LibretroGamesFlushWeb();
    } else {
        LibretroGamesExec("ROLLBACK;");
        GAMES.dirty = true;   // leave the index dirty so a later scan retries the write
        TraceLog(LOG_ERROR, "GAMES: Failed to persist index (%s)", sqlite3_errmsg(GAMES.db));
    }
}

#if defined(PLATFORM_WEB) || defined(__EMSCRIPTEN__)
#define LIBRETRO_GAMES_JOURNAL "DELETE"   // WAL shared memory is unavailable over IDBFS
#else
#define LIBRETRO_GAMES_JOURNAL "WAL"
#endif

static void LibretroGamesApplyPragmas(void) {
    LibretroGamesExec("PRAGMA journal_mode=" LIBRETRO_GAMES_JOURNAL ";");
    LibretroGamesExec("PRAGMA synchronous=NORMAL;");
    LibretroGamesExec("PRAGMA temp_store=MEMORY;");
}

/** PRAGMA quick_check: true when the first result row is "ok" (a fresh/empty DB passes). */
static bool LibretroGamesQuickCheck(sqlite3* db) {
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(db, "PRAGMA quick_check;", -1, &st, NULL) != SQLITE_OK) return false;
    bool ok = false;
    if (sqlite3_step(st) == SQLITE_ROW) {
        const char* v = (const char*)sqlite3_column_text(st, 0);
        ok = (v != NULL) && TextIsEqual(v, "ok");
    }
    sqlite3_finalize(st);
    return ok;
}

static int LibretroGamesUserVersion(sqlite3* db) {
    sqlite3_stmt* st = NULL;
    int v = 0;
    if (sqlite3_prepare_v2(db, "PRAGMA user_version;", -1, &st, NULL) == SQLITE_OK) {
        if (sqlite3_step(st) == SQLITE_ROW) v = sqlite3_column_int(st, 0);
        sqlite3_finalize(st);
    }
    return v;
}

/** Remove the database file and its journal sidecars (used to rebuild a corrupt cache). */
static void LibretroGamesDeleteDatabaseFiles(const char* dbPath) {
    static const char* suffix[] = { "", "-wal", "-shm", "-journal" };
    for (int i = 0; i < 4; i++) {
        const char* p = (i == 0) ? dbPath : TextFormat("%s%s", dbPath, suffix[i]);
        if (FileExists(p)) FileRemove(p);
    }
}

// ---------------------------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------------------------

bool InitLibretroGames(const char* dbDir) {
    const char* dir = (dbDir && dbDir[0]) ? dbDir : ".";
    if (!DirectoryExists(dir)) MakeDirectory(dir);
    char dbPath[RAYLIB_LIBRETRO_GAMES_MAX_PATH];
    TextCopy(dbPath, TextFormat("%s/raylib-libretro.db", dir));

    if (sqlite3_open(dbPath, &GAMES.db) != SQLITE_OK || !GAMES.db) {
        TraceLog(LOG_ERROR, "GAMES: Failed to open database %s (%s)", dbPath, GAMES.db ? sqlite3_errmsg(GAMES.db) : "?");
        if (GAMES.db) { sqlite3_close(GAMES.db); GAMES.db = NULL; }
        return false;
    }
    LibretroGamesApplyPragmas();

    // sqlite3_open is lazy, so a truncated/corrupt file opens "OK". A corrupt page cannot be
    // repaired in place: close it, delete it (and its journal sidecars), reopen fresh. The
    // always-on startup scan then re-indexes from disk.
    if (!LibretroGamesQuickCheck(GAMES.db)) {
        TraceLog(LOG_WARNING, "GAMES: Database corrupt; rebuilding %s", dbPath);
        sqlite3_close(GAMES.db); GAMES.db = NULL;
        LibretroGamesDeleteDatabaseFiles(dbPath);
        if (sqlite3_open(dbPath, &GAMES.db) != SQLITE_OK || !GAMES.db) {
            TraceLog(LOG_ERROR, "GAMES: Failed to reopen database %s", dbPath);
            if (GAMES.db) { sqlite3_close(GAMES.db); GAMES.db = NULL; }
            return false;
        }
        LibretroGamesApplyPragmas();
    }

    // Drop a stale-schema games table so future columns reach existing databases; meta (the
    // content dir) is preserved, and the next scan rebuilds the disposable index from disk.
    if (LibretroGamesUserVersion(GAMES.db) != LIBRETRO_GAMES_SCHEMA_VERSION) {
        LibretroGamesExec("DROP TABLE IF EXISTS games;");
        LibretroGamesExec(TextFormat("PRAGMA user_version=%d;", LIBRETRO_GAMES_SCHEMA_VERSION));
    }
    LibretroGamesExec(LIBRETRO_GAMES_SCHEMA);

    TraceLog(LOG_INFO, "GAMES: Database ready at %s", dbPath);

    LibretroGamesLoadFromDB();
    LibretroGamesRebuildSystems();
    if (cvector_size(GAMES.systems) > 1) qsort(GAMES.systems, cvector_size(GAMES.systems), sizeof(LibretroGameSystem*), LibretroGamesSystemSort);
    LibretroGamesRebuildFiltered();
    GAMES.justFinished = true;   // prompt the menu to build its Games page from the cached index
    return true;
}

static void LibretroGamesFreeM3uRefs(void) {
    for (size_t i = 0; i < cvector_size(GAMES.m3uRefs); i++) MemFree(GAMES.m3uRefs[i]);
    cvector_free(GAMES.m3uRefs);
    GAMES.m3uRefs = NULL;
}

void CloseLibretroGames(void) {
    if (GAMES.scanFilesLoaded) { UnloadDirectoryFiles(GAMES.scanFiles); GAMES.scanFilesLoaded = false; }
    LibretroGamesFreeEntries();
    LibretroGamesFreeSystems();
    LibretroGamesFreeM3uRefs();
    cvector_free(GAMES.filtered);
    GAMES.filtered = NULL;
    if (GAMES.db) {
#if !defined(PLATFORM_WEB) && !defined(__EMSCRIPTEN__)
        LibretroGamesExec("PRAGMA wal_checkpoint(TRUNCATE);");   // fold the WAL back so it doesn't grow across runs
#endif
        sqlite3_close(GAMES.db);
        GAMES.db = NULL;
    }
}

// ---------------------------------------------------------------------------------------------
// Scanning
// ---------------------------------------------------------------------------------------------

/** Lower-cased extension (no dot) of @p path into @p out (>= 16 bytes); false if none. */
static bool LibretroGamesExtLower(const char* path, char* out, int outSize) {
    const char* ext = GetFileExtension(path);
    if (!ext || !ext[0]) return false;
    if (ext[0] == '.') ext++;
    int n = 0;
    for (const char* p = ext; *p && n < outSize - 1; p++) out[n++] = LibretroGamesLowerChar(*p);
    out[n] = '\0';
    return n > 0;
}

/** Path of @p fullPath relative to the (slash-stripped) content dir, copied into @p out (size
 *  @p outSize) with separators normalized to '/' so the stored key is identical on every platform
 *  (Windows scans yield '\\'). Falls back to the bare file name when @p fullPath is out of tree.
 *  The bounded copy also prevents overflow for very deep paths. */
static void LibretroGamesRelPath(const char* fullPath, char* out, int outSize) {
    int baseLen = (int)TextLength(GAMES.contentDir);
    const char* rel;
    if (baseLen > 0 && (int)TextLength(fullPath) > baseLen && LibretroGamesHasPrefix(fullPath, GAMES.contentDir)) {
        rel = fullPath + baseLen;
        while (*rel == '/' || *rel == '\\') rel++;
    } else {
        rel = GetFileName(fullPath);
    }
    int n = 0;
    for (; rel[n] && n < outSize - 1; n++) out[n] = (rel[n] == '\\') ? '/' : rel[n];
    out[n] = '\0';
}

/** Collect the disc files referenced by every .m3u playlist (relative to the playlist's folder),
 *  so multi-disc games show only as the single .m3u rather than one entry per disc. */
static void LibretroGamesCollectM3uRefs(void) {
    LibretroGamesFreeM3uRefs();
    for (unsigned int i = 0; i < GAMES.scanFiles.count; i++) {
        const char* path = GAMES.scanFiles.paths[i];
        char ext[16];
        if (!LibretroGamesExtLower(path, ext, sizeof(ext)) || !TextIsEqual(ext, "m3u")) continue;

        // Directory portion of the playlist's relative path (bounded copy, '/'-normalized).
        char dir[RAYLIB_LIBRETRO_GAMES_MAX_PATH];
        LibretroGamesRelPath(path, dir, sizeof(dir));
        int slash = -1;
        for (int k = 0; dir[k]; k++) { if (dir[k] == '/' || dir[k] == '\\') slash = k; }
        if (slash >= 0) dir[slash] = '\0'; else dir[0] = '\0';

        char* text = LoadFileText(path);
        if (!text) continue;
        int start = 0;
        for (int k = 0; ; k++) {
            char c = text[k];
            if (c != '\n' && c != '\0') continue;
            char line[RAYLIB_LIBRETRO_GAMES_MAX_PATH];
            int li = 0;
            for (int j = start; j < k && li < (int)sizeof(line) - 1; j++) {
                if (text[j] != '\r') line[li++] = text[j];
            }
            line[li] = '\0';
            char* l = line;
            while (*l == ' ' || *l == '\t') l++;
            for (char* q = l; *q; q++) if (*q == '\\') *q = '/';   // canonical separators
            if (*l && *l != '#') {
                const char* ref = dir[0] ? TextFormat("%s/%s", dir, l) : l;
                cvector_push_back(GAMES.m3uRefs, LibretroGamesDup(ref));
            }
            start = k + 1;
            if (c == '\0') break;
        }
        UnloadFileText(text);
    }
}

static bool LibretroGamesIsM3uReferenced(const char* relpath) {
    for (size_t i = 0; i < cvector_size(GAMES.m3uRefs); i++) {
        if (TextIsEqual(GAMES.m3uRefs[i], relpath)) return true;
    }
    return false;
}

void LibretroGamesStartScan(const char* contentDir) {
    if (GAMES.scanFilesLoaded) { UnloadDirectoryFiles(GAMES.scanFiles); GAMES.scanFilesLoaded = false; }

    if (!contentDir || !contentDir[0] || !DirectoryExists(contentDir)) {
        GAMES.contentDir[0] = '\0';
        LibretroGamesFreeEntries();
        GAMES.state = LIBRETRO_GAMES_IDLE;
        LibretroGamesRebuildSystems();
        LibretroGamesRebuildFiltered();
        GAMES.justFinished = true;
        return;
    }

    TextCopy(GAMES.contentDir, contentDir);
    // Strip any trailing separator so relpath stripping is exact.
    int len = (int)TextLength(GAMES.contentDir);
    while (len > 1 && (GAMES.contentDir[len - 1] == '/' || GAMES.contentDir[len - 1] == '\\')) GAMES.contentDir[--len] = '\0';

    GAMES.scanFiles = LoadDirectoryFilesEx(GAMES.contentDir, NULL, true);
    GAMES.scanFilesLoaded = true;
    GAMES.dirty = false;

    // If the content root changed, drop the whole index and rebuild; otherwise keep the loaded
    // entries as the cache (a matching relpath skips re-indexing).
    char prev[RAYLIB_LIBRETRO_GAMES_MAX_PATH];
    LibretroGamesMetaGet("contentdir", prev, sizeof(prev));
    if (!TextIsEqual(prev, GAMES.contentDir)) {
        LibretroGamesFreeEntries();
        LibretroGamesMetaSet("contentdir", GAMES.contentDir);
        LibretroGamesFlushWeb();
        GAMES.dirty = true;
    }

    // Mark every cached entry stale and sort the cache for binary-search lookup during the scan.
    for (size_t i = 0; i < cvector_size(GAMES.entries); i++) GAMES.entries[i]->seen = false;
    if (cvector_size(GAMES.entries) > 1)
        qsort(GAMES.entries, cvector_size(GAMES.entries), sizeof(LibretroGameEntry*), LibretroGamesPathSort);
    GAMES.oldCount = (int)cvector_size(GAMES.entries);

    // Hide individual discs that belong to an .m3u playlist.
    LibretroGamesCollectM3uRefs();

    GAMES.scanTotal = (int)GAMES.scanFiles.count;
    GAMES.scanIndex = 0;
    GAMES.state = LIBRETRO_GAMES_SCANNING;
    TraceLog(LOG_INFO, "GAMES: Scanning %d files in %s", GAMES.scanTotal, GAMES.contentDir);
}

static void LibretroGamesFinishScan(void) {
    // Prune cached entries whose files are gone (not seen this scan); keep survivors + new finds.
    if (GAMES.oldCount > 0) {
        cvector(LibretroGameEntry*) kept = NULL;
        for (size_t i = 0; i < cvector_size(GAMES.entries); i++) {
            LibretroGameEntry* e = GAMES.entries[i];
            if (e->seen) cvector_push_back(kept, e);
            else { LibretroGamesFreeEntry(e); GAMES.dirty = true; }
        }
        cvector_free(GAMES.entries);
        GAMES.entries = kept;
    }

    if (GAMES.dirty) LibretroGamesSaveToDB();   // only touch the disk when the index actually changed

    LibretroGamesRebuildSystems();
    if (cvector_size(GAMES.systems) > 1) qsort(GAMES.systems, cvector_size(GAMES.systems), sizeof(LibretroGameSystem*), LibretroGamesSystemSort);
    GAMES.systemFilter[0] = '\0';
    LibretroGamesRebuildFiltered();

    if (GAMES.scanFilesLoaded) { UnloadDirectoryFiles(GAMES.scanFiles); GAMES.scanFilesLoaded = false; }
    LibretroGamesFreeM3uRefs();
    GAMES.scanTotal = 0;
    GAMES.scanIndex = 0;
    GAMES.oldCount = 0;
    GAMES.state = LIBRETRO_GAMES_IDLE;
    GAMES.justFinished = true;
    TraceLog(LOG_INFO, "GAMES: Indexed %d games across %d systems",
             (int)cvector_size(GAMES.entries), (int)cvector_size(GAMES.systems));
}

bool UpdateLibretroGamesScan(void) {
    if (GAMES.state != LIBRETRO_GAMES_SCANNING) return false;

    int processed = 0;
    while (processed < RAYLIB_LIBRETRO_GAMES_SCAN_BATCH && GAMES.scanIndex < GAMES.scanTotal) {
        const char* path = GAMES.scanFiles.paths[GAMES.scanIndex];
        GAMES.scanIndex++;
        processed++;

        char ext[16];
        if (!LibretroGamesExtLower(path, ext, sizeof(ext))) continue;

        char relpath[RAYLIB_LIBRETRO_GAMES_MAX_PATH];
        LibretroGamesRelPath(path, relpath, sizeof(relpath));  // '/'-normalized, bounded
        if (LibretroGamesIsBios(relpath)) continue;            // skip "[BIOS] ..." images
        if (LibretroGamesIsM3uReferenced(relpath)) continue;   // hidden disc of an .m3u playlist

        // Already indexed: reuse the cached entry (no archive peek, no core-availability check).
        LibretroGameEntry* cached = LibretroGamesFindCached(relpath);
        if (cached) { cached->seen = true; continue; }

        // New file: resolve the effective (inner, for archives) extension.
        char effExt[16];
        TextCopy(effExt, ext);
        if (TextIsEqual(ext, "zip")) {
            if (GAMES.zipInnerExt == NULL) continue;
            char inner[32];
            if (!GAMES.zipInnerExt(path, inner)) continue;
            TextCopy(effExt, inner);
        }

        // System: from the first path component (folder) when nested, else from the extension.
        const char* system;
        bool hasSubdir = false;
        for (const char* p = relpath; *p; p++) { if (*p == '/' || *p == '\\') { hasSubdir = true; break; } }
        if (hasSubdir) {
            char folder[128];
            int fn = 0;
            for (const char* p = relpath; *p && *p != '/' && *p != '\\' && fn < (int)sizeof(folder) - 1; p++) folder[fn++] = *p;
            folder[fn] = '\0';
            system = LibretroGamesDetectSystem(folder);
        } else {
            system = LibretroGamesSystemForExtension(effExt);
        }

        // Only index content an installed core can actually run (skips e.g. GameCube games when
        // no GameCube core is present). With no hook there is nothing to gate against -> skip.
        if (GAMES.coreAvailable == NULL ||
            !GAMES.coreAvailable(effExt, system)) continue;

        LibretroGamesPushEntry(relpath, system, NULL);   // title derived from the file name
        GAMES.dirty = true;
    }

    if (GAMES.scanIndex >= GAMES.scanTotal) {
        LibretroGamesFinishScan();
        return false;
    }
    return true;
}

bool IsLibretroGamesScanning(void) { return GAMES.state == LIBRETRO_GAMES_SCANNING; }

float GetLibretroGamesScanProgress(void) {
    if (GAMES.scanTotal <= 0) return 0.0f;
    float p = (float)GAMES.scanIndex / (float)GAMES.scanTotal;
    return (p < 0.0f) ? 0.0f : (p > 1.0f ? 1.0f : p);
}

bool LibretroGamesScanJustFinished(void) {
    bool v = GAMES.justFinished;
    GAMES.justFinished = false;
    return v;
}

void LibretroGamesSetFont(Font font) { GAMES.font = font; }

void DrawLibretroGamesScanProgress(void) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, (Color){ 17, 17, 27, 200 });

    Font font = (GAMES.font.texture.id != 0) ? GAMES.font : GetFontDefault();
    float fontSize = (float)((font.baseSize > 0) ? font.baseSize : 20);
    Color fg = (Color){ 245, 224, 220, 255 };

    const char* text = TextFormat("Indexing games...  %d / %d", GAMES.scanIndex, GAMES.scanTotal);
    Vector2 ts = MeasureTextEx(font, text, fontSize, 1);
    float tx = (sw - ts.x) / 2.0f;
    float ty = sh / 2.0f - fontSize - 12;
    DrawTextEx(font, text, (Vector2){ tx, ty }, fontSize, 1, fg);

    // Progress bar.
    int barW = (sw > 480) ? 400 : (sw - 80);
    int barH = 18;
    int bx = (sw - barW) / 2;
    int by = (int)(sh / 2.0f + 6);
    DrawRectangle(bx, by, barW, barH, (Color){ 49, 50, 68, 255 });
    DrawRectangle(bx, by, (int)(barW * GetLibretroGamesScanProgress()), barH, (Color){ 203, 166, 247, 255 });
    DrawRectangleLines(bx, by, barW, barH, fg);
}

// ---------------------------------------------------------------------------------------------
// Views & queries
// ---------------------------------------------------------------------------------------------

void LibretroGamesSetSystemFilter(const char* systemId) {
    TextCopy(GAMES.systemFilter, (systemId && systemId[0]) ? systemId : "");
}

int LibretroGamesFilteredCount(void) { return (int)cvector_size(GAMES.filtered); }

const char* LibretroGamesFilteredLabel(int row) {
    if (row < 0 || row >= (int)cvector_size(GAMES.filtered)) return "";
    return GAMES.entries[GAMES.filtered[row]]->title;
}

LibretroGameEntry* LibretroGamesFilteredEntry(int row) {
    if (row < 0 || row >= (int)cvector_size(GAMES.filtered)) return NULL;
    return GAMES.entries[GAMES.filtered[row]];
}

const char* LibretroGamesContentDir(void) { return GAMES.contentDir; }
int LibretroGamesTotalCount(void) { return (int)cvector_size(GAMES.entries); }
int LibretroGamesSystemCount(void) { return (int)cvector_size(GAMES.systems); }

LibretroGameSystem* LibretroGamesSystem(int index) {
    if (index < 0 || index >= (int)cvector_size(GAMES.systems)) return NULL;
    return GAMES.systems[index];
}

#endif  // RAYLIB_LIBRETRO_GAMES_IMPLEMENTATION_ONCE
#endif  // RAYLIB_LIBRETRO_GAMES_IMPLEMENTATION
