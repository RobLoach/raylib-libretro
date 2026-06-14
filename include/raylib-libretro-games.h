/**********************************************************************************************
*
*   raylib-libretro-games - Games database: scan a content directory, infer each game's
*                           system from its folder name, and index everything into a
*                           persistent SNKV (SQLite B-tree) key-value store for browsing
*                           and searching.
*
*   LAYER
*
*       Opt-in, core-agnostic frontend layer. Depends on raylib-libretro.h (paths) and the
*       vendored SNKV amalgamation (vendor/snkv/snkv.h); it does NOT depend on the menu or
*       on any libretro core. The menu layer renders the data this layer indexes and owns
*       the system->core launch decision (it has the core list). Define
*       RAYLIB_LIBRETRO_GAMES_IMPLEMENTATION in exactly one translation unit (the menu's
*       implementation enables it automatically).
*
*   DATA MODEL (key-value, single keyspace, 0x1f = unit separator)
*
*       g\x1f<relpath>      -> "system\x1ftitle\x1fext\x1f<size>\x1f<mtime>"
*       m\x1fcontentdir     -> last-scanned content directory
*
*       Paths stored in the database are RELATIVE to the content directory (never absolute).
*       The in-memory entry list mirrors the database and is what the UI reads; the database
*       is the persistent cache that makes re-scans incremental (size+mtime unchanged = skip).
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
 * One indexed game. `relpath` is relative to the content directory; `systemId` is the
 * canonical system identifier (aligned with libretro .info `systemid`), or "" when unknown.
 */
typedef struct LibretroGameEntry {
    char relpath[RAYLIB_LIBRETRO_GAMES_MAX_PATH];
    char title[256];
    char systemId[48];
    char ext[16];
    long size;
    long mtime;
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
bool InitLibretroGames(void);                              // open the database, load the index
void CloseLibretroGames(void);                             // free memory, close the database

// Scanning (incremental, driven from the app's Update())
void LibretroGamesStartScan(const char* contentDir);       // (re)index a content directory
bool UpdateLibretroGamesScan(void);                        // process a batch; true while scanning
bool IsLibretroGamesScanning(void);
float GetLibretroGamesScanProgress(void);                  // 0..1
void DrawLibretroGamesScanProgress(void);                  // translucent progress overlay (inside a frame)
bool LibretroGamesScanJustFinished(void);                  // one-shot: true once after a scan completes
void LibretroGamesSetFont(Font font);                      // optional font for the overlay
void LibretroGamesSetKnownExtensions(const char* pipeSeparatedLower); // optional ext filter override

// System detection / naming
const char* LibretroGamesDetectSystem(const char* folderName);   // canonical id, or "" if unknown
const char* LibretroGamesSystemDisplayName(const char* systemId); // human-readable name

// Views & search (one shared filtered state: systemFilter + search)
void LibretroGamesSetSystemFilter(const char* systemId);   // NULL/"" = all systems
void LibretroGamesSetSearch(const char* text);
char* LibretroGamesSearchBuffer(void);                     // buffer the search box binds to
int LibretroGamesSearchBufferSize(void);
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

#include <stdlib.h>   // qsort, bsearch
#include "snkv.h"     // header-only (declarations); the implementation lives in vendor/snkv/snkv_impl.c

#define RAYLIB_LIBRETRO_GAMES_SEP '\x1f'

typedef struct LibretroGames {
    void* db;                               // KVStore*
    char contentDir[RAYLIB_LIBRETRO_GAMES_MAX_PATH];
    cvector(LibretroGameEntry*) entries;    // every indexed game (in memory)
    cvector(LibretroGameSystem*) systems;   // distinct systems, with counts
    cvector(int) filtered;                  // row -> entries index (current view)
    char searchBuffer[128];
    char systemFilter[48];                  // "" = all
    char knownExts[1024];                   // optional override list, wrapped as "|nes|sfc|"
    LibretroGamesScanState state;
    FilePathList scanFiles;
    bool scanFilesLoaded;
    int scanIndex;
    int scanTotal;
    bool justFinished;
    Font font;
} LibretroGames;

static LibretroGames GAMES = {0};

// Built-in fallback list of content extensions (lower-case, wrapped in '|' for substring match).
static const char* LIBRETRO_GAMES_DEFAULT_EXTS =
    "|nes|fds|unf|unif|sfc|smc|swc|fig|bs|st|gb|gbc|gba|n64|z64|v64|u1|ndd|md|smd|gen|bin|"
    "gg|sms|sg|sgd|68k|gbs|pce|sgx|cue|ccd|chd|iso|img|m3u|gdi|cdi|pbp|cso|nds|dsi|ws|wsc|"
    "ngp|ngc|gcm|rvz|wbfs|a26|a52|a78|j64|jag|lnx|lyx|vec|col|int|vb|min|d64|t64|prg|crt|"
    "adf|ipf|dsk|tap|tzx|cas|rom|mx1|mx2|msx|p|z80|sna|nibl|3do|zip|7z|gz|smc|spc|";

// ---- canonical system alias table (id aligned with libretro .info `systemid` where known) ----
// Match rule: normalize the folder name (lower-case, alphanumeric only), then pick the LONGEST
// alias (across all systems) that is contained in it. Aliases of <=3 chars must match the whole
// normalized name exactly, so short tokens like "gb"/"nes" do not match inside longer names.
typedef struct {
    const char* id;
    const char* display;
    const char* aliases;     // pipe-separated, normalized (lower-case, alphanumeric only)
} LibretroGamesSystemAlias;

static const LibretroGamesSystemAlias LIBRETRO_GAMES_SYSTEMS[] = {
    { "super_nes",          "Super Nintendo",            "snes|supernintendo|supernes|superfamicom|sfc|nintendosupernintendoentertainmentsystem" },
    { "nes",                "Nintendo Entertainment System", "nes|nintendoentertainmentsystem|famicom|nintendofamicom" },
    { "nintendo64",         "Nintendo 64",               "n64|nintendo64" },
    { "gamecube",           "Nintendo GameCube",         "ngc|gamecube|nintendogamecube" },
    { "wii",                "Nintendo Wii",              "wii|nintendowii" },
    { "game_boy_advance",   "Game Boy Advance",          "gba|gameboyadvance|nintendogameboyadvance" },
    { "game_boy_color",     "Game Boy Color",            "gbc|gameboycolor|nintendogameboycolor" },
    { "game_boy",           "Game Boy",                  "gb|gameboy|nintendogameboy" },
    { "nintendo_ds",        "Nintendo DS",               "nds|nintendods" },
    { "virtual_boy",        "Virtual Boy",               "vb|virtualboy|nintendovirtualboy" },
    { "mega_drive",         "Sega Genesis / Mega Drive", "genesis|megadrive|segagenesis|segamegadrive|segamegadrivegenesis" },
    { "sega_cd",            "Sega CD / Mega-CD",         "segacd|megacd|segamegacd" },
    { "saturn",             "Sega Saturn",               "saturn|segasaturn" },
    { "dreamcast",          "Sega Dreamcast",            "dreamcast|segadreamcast" },
    { "master_system",      "Sega Master System",        "sms|mastersystem|segamastersystem" },
    { "game_gear",          "Sega Game Gear",            "gamegear|segagamegear" },
    { "sg1000",             "Sega SG-1000",              "sg1000|segasg1000" },
    { "playstation",        "Sony PlayStation",          "psx|ps1|playstation|sonyplaystation" },
    { "playstation2",       "Sony PlayStation 2",        "ps2|playstation2|sonyplaystation2" },
    { "psp",                "Sony PSP",                  "psp|playstationportable|sonypsp" },
    { "pc_engine",          "PC Engine / TurboGrafx-16", "pcengine|turbografx|turbografx16|necpcengine" },
    { "neo_geo",            "Neo Geo",                   "neogeo|snkneogeo" },
    { "neo_geo_pocket",     "Neo Geo Pocket",            "neogeopocket|ngp|ngpc" },
    { "wonderswan",         "WonderSwan",                "wonderswan|bandaiwonderswan" },
    { "atari_2600",         "Atari 2600",                "atari2600|vcs" },
    { "atari_7800",         "Atari 7800",                "atari7800" },
    { "atari_lynx",         "Atari Lynx",                "atarilynx|lynx" },
    { "atari_jaguar",       "Atari Jaguar",              "atarijaguar|jaguar" },
    { "colecovision",       "ColecoVision",              "colecovision|coleco" },
    { "intellivision",      "Intellivision",             "intellivision" },
    { "vectrex",            "Vectrex",                   "vectrex" },
    { "msx",                "MSX",                       "msx|msx2" },
    { "commodore_64",       "Commodore 64",              "c64|commodore64" },
    { "amiga",              "Amiga",                     "amiga|commodoreamiga" },
    { "zx_spectrum",        "ZX Spectrum",               "zxspectrum|spectrum|sinclairzxspectrum" },
    { "amstrad_cpc",        "Amstrad CPC",               "amstradcpc|amstrad|cpc" },
    { "3do",                "3DO",                        "3do|panasonic3do" },
    { "arcade",             "Arcade",                    "arcade|mame|fbneo|fba|finalburn|finalburnneo" },
    { "dos",                "DOS",                        "dos|msdos|pcdos" },
    { "scummvm",            "ScummVM",                   "scummvm" },
};
#define LIBRETRO_GAMES_SYSTEM_ALIAS_COUNT ((int)(sizeof(LIBRETRO_GAMES_SYSTEMS)/sizeof(LIBRETRO_GAMES_SYSTEMS[0])))

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

/** True when normalized @p needle occurs in @p haystack (both treated case-insensitively). */
static bool LibretroGamesContainsCI(const char* haystack, const char* needleLower) {
    if (!needleLower[0]) return true;
    for (const char* h = haystack; *h; h++) {
        const char* a = h;
        const char* b = needleLower;
        while (*a && *b && LibretroGamesLowerChar(*a) == *b) { a++; b++; }
        if (!*b) return true;
    }
    return false;
}

static int LibretroGamesStrCaseCmp(const char* a, const char* b) {
    while (*a && *b) {
        char ca = LibretroGamesLowerChar(*a), cb = LibretroGamesLowerChar(*b);
        if (ca != cb) return (int)ca - (int)cb;
        a++; b++;
    }
    return (int)LibretroGamesLowerChar(*a) - (int)LibretroGamesLowerChar(*b);
}

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

// ---------------------------------------------------------------------------------------------
// Extension filtering
// ---------------------------------------------------------------------------------------------

void LibretroGamesSetKnownExtensions(const char* pipeSeparatedLower) {
    if (!pipeSeparatedLower || !pipeSeparatedLower[0]) {
        GAMES.knownExts[0] = '\0';
        return;
    }
    // Store wrapped in separators so "|ext|" substring tests are exact.
    TextCopy(GAMES.knownExts, "|");
    int n = 1;
    for (const char* p = pipeSeparatedLower; *p && n < (int)sizeof(GAMES.knownExts) - 2; p++) {
        GAMES.knownExts[n++] = LibretroGamesLowerChar(*p);
    }
    GAMES.knownExts[n++] = '|';
    GAMES.knownExts[n] = '\0';
}

/** True when @p extLower (no dot) looks like indexable game content. */
static bool LibretroGamesIsGameExt(const char* extLower) {
    if (!extLower || !extLower[0]) return false;
    const char* list = GAMES.knownExts[0] ? GAMES.knownExts : LIBRETRO_GAMES_DEFAULT_EXTS;
    char needle[20];
    int n = 0;
    needle[n++] = '|';
    for (const char* p = extLower; *p && n < (int)sizeof(needle) - 2; p++) needle[n++] = *p;
    needle[n++] = '|';
    needle[n] = '\0';
    return TextFindIndex(list, needle) >= 0;     // both list and needle are lower-case
}

// ---------------------------------------------------------------------------------------------
// Database key/value helpers
// ---------------------------------------------------------------------------------------------

/** Build a "g\x1f<relpath>" key into @p out (>= 2 + len(relpath) + 1). Returns key length. */
static int LibretroGamesKey(char* out, const char* relpath) {
    out[0] = 'g';
    out[1] = RAYLIB_LIBRETRO_GAMES_SEP;
    int n = 2;
    for (const char* p = relpath; *p; p++) out[n++] = *p;
    return n;
}

/** Split a mutable, null-terminated value on the separator into up to @p maxF fields. */
static int LibretroGamesSplitValue(char* v, char** fields, int maxF) {
    int n = 0;
    fields[n++] = v;
    for (char* p = v; *p && n < maxF; p++) {
        if (*p == RAYLIB_LIBRETRO_GAMES_SEP) { *p = '\0'; fields[n++] = p + 1; }
    }
    return n;
}

static void LibretroGamesFillEntry(LibretroGameEntry* e, const char* relpath,
                                   const char* system, const char* title, const char* ext,
                                   long size, long mtime) {
    TextCopy(e->relpath, relpath);
    TextCopy(e->systemId, system ? system : "");
    TextCopy(e->title, (title && title[0]) ? title : GetFileNameWithoutExt(relpath));
    TextCopy(e->ext, ext ? ext : "");
    e->size = size;
    e->mtime = mtime;
}

// ---------------------------------------------------------------------------------------------
// In-memory list management
// ---------------------------------------------------------------------------------------------

static void LibretroGamesFreeEntries(void) {
    for (size_t i = 0; i < cvector_size(GAMES.entries); i++) MemFree(GAMES.entries[i]);
    cvector_free(GAMES.entries);
    GAMES.entries = NULL;
}

static void LibretroGamesFreeSystems(void) {
    for (size_t i = 0; i < cvector_size(GAMES.systems); i++) MemFree(GAMES.systems[i]);
    cvector_free(GAMES.systems);
    GAMES.systems = NULL;
}

static LibretroGameEntry* LibretroGamesPushEntry(const char* relpath, const char* system,
                                                 const char* title, const char* ext,
                                                 long size, long mtime) {
    LibretroGameEntry* e = (LibretroGameEntry*)MemAlloc(sizeof(LibretroGameEntry));
    LibretroGamesFillEntry(e, relpath, system, title, ext, size, mtime);
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

void LibretroGamesRebuildFiltered(void) {
    cvector_free(GAMES.filtered);
    GAMES.filtered = NULL;

    // Lower-cased (but not stripped) needle so spaces/punctuation in titles still match.
    char needle[128];
    int nn = 0;
    for (const char* p = GAMES.searchBuffer; *p && nn < (int)sizeof(needle) - 1; p++) needle[nn++] = LibretroGamesLowerChar(*p);
    needle[nn] = '\0';

    for (size_t i = 0; i < cvector_size(GAMES.entries); i++) {
        LibretroGameEntry* e = GAMES.entries[i];
        if (GAMES.systemFilter[0] && !TextIsEqual(e->systemId, GAMES.systemFilter)) continue;
        if (needle[0] && !LibretroGamesContainsCI(e->title, needle)) continue;
        cvector_push_back(GAMES.filtered, (int)i);
    }

    if (cvector_size(GAMES.filtered) > 1) {
        qsort(GAMES.filtered, cvector_size(GAMES.filtered), sizeof(int), LibretroGamesFilteredSort);
    }
}

// ---------------------------------------------------------------------------------------------
// Database load / prune
// ---------------------------------------------------------------------------------------------

static void LibretroGamesLoadFromDB(void) {
    LibretroGamesFreeEntries();
    if (!GAMES.db) return;

    KVIterator* it = NULL;
    const char prefix[2] = { 'g', RAYLIB_LIBRETRO_GAMES_SEP };
    if (kvstore_prefix_iterator_create((KVStore*)GAMES.db, prefix, 2, &it) != KVSTORE_OK || !it) return;

    while (!kvstore_iterator_eof(it)) {
        void* keyData = NULL; int keyLen = 0;
        void* valData = NULL; int valLen = 0;
        if (kvstore_iterator_key(it, &keyData, &keyLen) == KVSTORE_OK &&
            kvstore_iterator_value(it, &valData, &valLen) == KVSTORE_OK &&
            keyLen > 2 && valLen > 0) {

            char relpath[RAYLIB_LIBRETRO_GAMES_MAX_PATH];
            int rl = keyLen - 2;
            if (rl > (int)sizeof(relpath) - 1) rl = (int)sizeof(relpath) - 1;
            for (int i = 0; i < rl; i++) relpath[i] = ((const char*)keyData)[2 + i];
            relpath[rl] = '\0';

            char value[600];
            int vl = valLen;
            if (vl > (int)sizeof(value) - 1) vl = (int)sizeof(value) - 1;
            for (int i = 0; i < vl; i++) value[i] = ((const char*)valData)[i];
            value[vl] = '\0';

            char* f[6] = {0};
            int nf = LibretroGamesSplitValue(value, f, 5);
            const char* system = nf > 0 ? f[0] : "";
            const char* title  = nf > 1 ? f[1] : "";
            const char* ext    = nf > 2 ? f[2] : "";
            long size  = nf > 3 ? (long)TextToInteger(f[3]) : 0;
            long mtime = nf > 4 ? (long)TextToInteger(f[4]) : 0;
            LibretroGamesPushEntry(relpath, system, title, ext, size, mtime);
        }
        kvstore_iterator_next(it);
    }
    kvstore_iterator_close(it);
}

// Case-sensitive byte compare (relpaths are case-sensitive on POSIX filesystems).
static int LibretroGamesStrCmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

static int LibretroGamesRelPathSort(const void* a, const void* b) {
    return LibretroGamesStrCmp(*(const char* const*)a, *(const char* const*)b);
}

// Delete every database record whose relpath is not in the freshly-scanned entry list.
static void LibretroGamesPruneDB(void) {
    if (!GAMES.db) return;

    // Sorted view of the current (on-disk) relpaths for binary search.
    cvector(const char*) names = NULL;
    for (size_t i = 0; i < cvector_size(GAMES.entries); i++) cvector_push_back(names, (const char*)GAMES.entries[i]->relpath);
    if (cvector_size(names) > 1) qsort(names, cvector_size(names), sizeof(const char*), LibretroGamesRelPathSort);

    cvector(char*) toDelete = NULL;
    KVIterator* it = NULL;
    const char prefix[2] = { 'g', RAYLIB_LIBRETRO_GAMES_SEP };
    if (kvstore_prefix_iterator_create((KVStore*)GAMES.db, prefix, 2, &it) == KVSTORE_OK && it) {
        while (!kvstore_iterator_eof(it)) {
            void* keyData = NULL; int keyLen = 0;
            if (kvstore_iterator_key(it, &keyData, &keyLen) == KVSTORE_OK && keyLen > 2) {
                char relpath[RAYLIB_LIBRETRO_GAMES_MAX_PATH];
                int rl = keyLen - 2;
                if (rl > (int)sizeof(relpath) - 1) rl = (int)sizeof(relpath) - 1;
                for (int i = 0; i < rl; i++) relpath[i] = ((const char*)keyData)[2 + i];
                relpath[rl] = '\0';

                const char* key = relpath;
                const char** hit = (cvector_size(names) > 0)
                    ? (const char**)bsearch(&key, names, cvector_size(names), sizeof(const char*), LibretroGamesRelPathSort)
                    : NULL;
                if (!hit) {
                    char* copy = (char*)MemAlloc((unsigned)(rl + 1));
                    TextCopy(copy, relpath);
                    cvector_push_back(toDelete, copy);
                }
            }
            kvstore_iterator_next(it);
        }
        kvstore_iterator_close(it);
    }

    for (size_t i = 0; i < cvector_size(toDelete); i++) {
        char keyBuf[2 + RAYLIB_LIBRETRO_GAMES_MAX_PATH];
        int kl = LibretroGamesKey(keyBuf, toDelete[i]);
        kvstore_delete((KVStore*)GAMES.db, keyBuf, kl);
        MemFree(toDelete[i]);
    }
    cvector_free(toDelete);
    cvector_free(names);
}

// ---------------------------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------------------------

bool InitLibretroGames(void) {
    const char* saveDir = GetLibretroDirectory(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY);
    if (saveDir && saveDir[0] && !DirectoryExists(saveDir)) MakeDirectory(saveDir);

    const char* dbPath = TextFormat("%s/games.db", (saveDir && saveDir[0]) ? saveDir : ".");
#if defined(PLATFORM_WEB) || defined(__EMSCRIPTEN__)
    int journal = KVSTORE_JOURNAL_DELETE;   // WAL shared-memory is unreliable over IDBFS
#else
    int journal = KVSTORE_JOURNAL_WAL;
#endif

    KVStore* db = NULL;
    int rc = kvstore_open(dbPath, &db, journal);
    if (rc != KVSTORE_OK || !db) {
        TraceLog(LOG_ERROR, "GAMES: Failed to open database %s (rc=%d)", dbPath, rc);
        GAMES.db = NULL;
        return false;
    }
    GAMES.db = db;
    TraceLog(LOG_INFO, "GAMES: Database ready at %s", dbPath);

    LibretroGamesLoadFromDB();
    LibretroGamesRebuildSystems();
    if (cvector_size(GAMES.systems) > 1) qsort(GAMES.systems, cvector_size(GAMES.systems), sizeof(LibretroGameSystem*), LibretroGamesSystemSort);
    LibretroGamesRebuildFiltered();
    GAMES.justFinished = true;   // prompt the menu to build its Games page from the cached index
    return true;
}

void CloseLibretroGames(void) {
    if (GAMES.scanFilesLoaded) { UnloadDirectoryFiles(GAMES.scanFiles); GAMES.scanFilesLoaded = false; }
    LibretroGamesFreeEntries();
    LibretroGamesFreeSystems();
    cvector_free(GAMES.filtered);
    GAMES.filtered = NULL;
    if (GAMES.db) { kvstore_close((KVStore*)GAMES.db); GAMES.db = NULL; }
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

/** Path of @p fullPath relative to the (slash-stripped) content dir; falls back to file name. */
static const char* LibretroGamesRelPath(const char* fullPath) {
    int baseLen = (int)TextLength(GAMES.contentDir);
    if (baseLen > 0 && (int)TextLength(fullPath) > baseLen && LibretroGamesHasPrefix(fullPath, GAMES.contentDir)) {
        const char* rel = fullPath + baseLen;
        while (*rel == '/' || *rel == '\\') rel++;
        return rel;
    }
    return GetFileName(fullPath);
}

/** Canonical system id for a relpath, from its first path component (the system folder). */
static const char* LibretroGamesSystemForRelPath(const char* relpath) {
    char folder[128];
    int n = 0;
    for (const char* p = relpath; *p && *p != '/' && *p != '\\' && n < (int)sizeof(folder) - 1; p++) folder[n++] = *p;
    folder[n] = '\0';
    // No separator found means the file sits directly in the content root: unknown system.
    bool hasSubdir = false;
    for (const char* p = relpath; *p; p++) { if (*p == '/' || *p == '\\') { hasSubdir = true; break; } }
    if (!hasSubdir) return "";
    return LibretroGamesDetectSystem(folder);
}

void LibretroGamesStartScan(const char* contentDir) {
    if (GAMES.scanFilesLoaded) { UnloadDirectoryFiles(GAMES.scanFiles); GAMES.scanFilesLoaded = false; }
    LibretroGamesFreeEntries();

    if (!contentDir || !contentDir[0] || !DirectoryExists(contentDir)) {
        GAMES.contentDir[0] = '\0';
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

    // If the content root changed since last time, drop stale records (relpaths differ).
    if (GAMES.db) {
        const char metaKey[] = { 'm', RAYLIB_LIBRETRO_GAMES_SEP, 'c','o','n','t','e','n','t','d','i','r' };
        int dirLen = (int)TextLength(GAMES.contentDir);
        void* prev = NULL; int prevLen = 0;
        bool changed = true;
        if (kvstore_get((KVStore*)GAMES.db, metaKey, (int)sizeof(metaKey), &prev, &prevLen) == KVSTORE_OK && prev) {
            changed = (prevLen != dirLen);
            for (int i = 0; !changed && i < dirLen; i++) {
                if (((const char*)prev)[i] != GAMES.contentDir[i]) changed = true;
            }
            snkv_free(prev);
        }
        if (changed) {
            // Delete all g\x1f records (keys are 'g' + separator + relpath text), then store the new dir.
            cvector(char*) keys = NULL;
            KVIterator* it = NULL;
            const char gp[2] = { 'g', RAYLIB_LIBRETRO_GAMES_SEP };
            if (kvstore_prefix_iterator_create((KVStore*)GAMES.db, gp, 2, &it) == KVSTORE_OK && it) {
                while (!kvstore_iterator_eof(it)) {
                    void* kd = NULL; int kl = 0;
                    if (kvstore_iterator_key(it, &kd, &kl) == KVSTORE_OK && kl > 0) {
                        char* copy = (char*)MemAlloc((unsigned)(kl + 1));
                        for (int i = 0; i < kl; i++) copy[i] = ((const char*)kd)[i];
                        copy[kl] = '\0';
                        cvector_push_back(keys, copy);
                    }
                    kvstore_iterator_next(it);
                }
                kvstore_iterator_close(it);
            }
            for (size_t i = 0; i < cvector_size(keys); i++) {
                kvstore_delete((KVStore*)GAMES.db, keys[i], (int)TextLength(keys[i]));
                MemFree(keys[i]);
            }
            cvector_free(keys);
            kvstore_put((KVStore*)GAMES.db, metaKey, (int)sizeof(metaKey), GAMES.contentDir, dirLen);
        }
    }

    GAMES.scanFiles = LoadDirectoryFilesEx(GAMES.contentDir, NULL, true);
    GAMES.scanFilesLoaded = true;
    GAMES.scanTotal = (int)GAMES.scanFiles.count;
    GAMES.scanIndex = 0;
    GAMES.state = LIBRETRO_GAMES_SCANNING;
    TraceLog(LOG_INFO, "GAMES: Scanning %d files in %s", GAMES.scanTotal, GAMES.contentDir);
}

static void LibretroGamesFinishScan(void) {
    LibretroGamesPruneDB();
    LibretroGamesRebuildSystems();
    if (cvector_size(GAMES.systems) > 1) qsort(GAMES.systems, cvector_size(GAMES.systems), sizeof(LibretroGameSystem*), LibretroGamesSystemSort);
    GAMES.systemFilter[0] = '\0';
    LibretroGamesRebuildFiltered();

    if (GAMES.scanFilesLoaded) { UnloadDirectoryFiles(GAMES.scanFiles); GAMES.scanFilesLoaded = false; }
    GAMES.scanTotal = 0;
    GAMES.scanIndex = 0;
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
        if (!LibretroGamesIsGameExt(ext)) continue;

        const char* relpath = LibretroGamesRelPath(path);
        if ((int)TextLength(relpath) > RAYLIB_LIBRETRO_GAMES_MAX_PATH - 1) continue;

        long size = (long)GetFileLength(path);
        long mtime = GetFileModTime(path);

        char keyBuf[2 + RAYLIB_LIBRETRO_GAMES_MAX_PATH];
        int kl = LibretroGamesKey(keyBuf, relpath);

        bool reused = false;
        if (GAMES.db) {
            void* val = NULL; int vlen = 0;
            if (kvstore_get((KVStore*)GAMES.db, keyBuf, kl, &val, &vlen) == KVSTORE_OK && val) {
                char value[600];
                int vl = vlen; if (vl > (int)sizeof(value) - 1) vl = (int)sizeof(value) - 1;
                for (int i = 0; i < vl; i++) value[i] = ((const char*)val)[i];
                value[vl] = '\0';
                snkv_free(val);

                char* f[6] = {0};
                int nf = LibretroGamesSplitValue(value, f, 5);
                long storedSize  = nf > 3 ? (long)TextToInteger(f[3]) : -1;
                long storedMtime = nf > 4 ? (long)TextToInteger(f[4]) : -1;
                if (storedSize == size && storedMtime == mtime) {
                    LibretroGamesPushEntry(relpath, nf > 0 ? f[0] : "", nf > 1 ? f[1] : "", nf > 2 ? f[2] : "", size, mtime);
                    reused = true;
                }
            }
        }

        if (!reused) {
            const char* system = LibretroGamesSystemForRelPath(relpath);
            const char* title = GetFileNameWithoutExt(relpath);
            LibretroGamesPushEntry(relpath, system, title, ext, size, mtime);
            if (GAMES.db) {
                const char* value = TextFormat("%s%c%s%c%s%c%ld%c%ld",
                    system, RAYLIB_LIBRETRO_GAMES_SEP, title, RAYLIB_LIBRETRO_GAMES_SEP,
                    ext, RAYLIB_LIBRETRO_GAMES_SEP, size, RAYLIB_LIBRETRO_GAMES_SEP, mtime);
                kvstore_put((KVStore*)GAMES.db, keyBuf, kl, value, (int)TextLength(value));
            }
        }
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

void LibretroGamesSetSearch(const char* text) {
    TextCopy(GAMES.searchBuffer, text ? text : "");
}

char* LibretroGamesSearchBuffer(void) { return GAMES.searchBuffer; }
int LibretroGamesSearchBufferSize(void) { return (int)sizeof(GAMES.searchBuffer); }

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
