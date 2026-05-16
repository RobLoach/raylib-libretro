/**********************************************************************************************
*
*   raylib-libretro-physfs - PhysFS-backed game loader for raylib-libretro.
*
*   Wraps LoadLibretroGame so that .zip archives are mounted at /game and the
*   ROM inside is handed to the core (data or path, per RETRO_ENVIRONMENT_SET_
*   CONTENT_INFO_OVERRIDE). Frontends opt in by including this header; the core
*   raylib-libretro.h stays free of PhysFS.
*
*   USAGE:
*       In exactly one source file, before include:
*           #define RAYLIB_LIBRETRO_PHYSFS_IMPLEMENTATION
*           #include "raylib-libretro-physfs.h"
*
*       Then at runtime:
*           InitLibretroPhysFS();              // once, after InitLibretro()
*           LoadLibretroGamePhysFS(gameFile);  // instead of LoadLibretroGame()
*           ...
*           CloseLibretroPhysFS();             // once, before CloseLibretro()
*
*   LICENSE: zlib/libpng
*
**********************************************************************************************/

#ifndef RAYLIB_LIBRETRO_PHYSFS_H
#define RAYLIB_LIBRETRO_PHYSFS_H

#include "raylib.h"
#include "raylib-libretro.h"
#include "raylib-physfs.h"

#if defined(__cplusplus)
extern "C" {
#endif

static bool InitLibretroPhysFS(void);                       // Start PhysFS. Safe to call repeatedly.
static void CloseLibretroPhysFS(void);                      // Tear down PhysFS and any active /game mount.
static bool LoadLibretroGamePhysFS(const char* gameFile);   // Zip-aware replacement for LoadLibretroGame.

#if defined(__cplusplus)
}
#endif

#endif // RAYLIB_LIBRETRO_PHYSFS_H

#ifdef RAYLIB_LIBRETRO_PHYSFS_IMPLEMENTATION
#ifndef RAYLIB_LIBRETRO_PHYSFS_IMPLEMENTATION_ONCE
#define RAYLIB_LIBRETRO_PHYSFS_IMPLEMENTATION_ONCE

#include <stdio.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct rLibretroPhysFS {
    bool ready;
    char mountSource[RAYLIB_LIBRETRO_VFS_MAX_PATH];
} rLibretroPhysFS;

static rLibretroPhysFS LibretroPhysFS = {0};

static bool InitLibretroPhysFS(void) {
    if (LibretroPhysFS.ready) return true;
    if (!InitPhysFS()) {
        TraceLog(LOG_WARNING, "LIBRETRO: Failed to initialize PhysFS");
        return false;
    }
    LibretroPhysFS.ready = true;
    return true;
}

static void CloseLibretroPhysFS(void) {
    if (LibretroPhysFS.mountSource[0] != '\0') {
        UnmountPhysFS(LibretroPhysFS.mountSource);
        LibretroPhysFS.mountSource[0] = '\0';
    }
    if (LibretroPhysFS.ready) {
        ClosePhysFS();
        LibretroPhysFS.ready = false;
    }
}

// Pick the ROM file inside /game after a zip mount. First preference: an entry
// whose basename (minus extension) matches the zip's own basename. Fallback:
// the first entry whose extension is in the core's validExtensions list.
// Subdirectories are searched recursively. Returns NULL when nothing matched;
// the caller MemFree()s the result.
static char* LibretroPhysFSPickFileInZip(const char* zipFile) {
    FilePathList entries = LoadDirectoryFilesFromPhysFSEx("/game", NULL, true);
    if (entries.count == 0) {
        UnloadDirectoryFiles(entries);
        return NULL;
    }

    const char* zipBase = GetFileNameWithoutExt(zipFile);
    char* picked = NULL;

    // Pass 1: basename match (any depth).
    for (unsigned int i = 0; i < entries.count; i++) {
        const char* name = GetFileName(entries.paths[i]);
        if (TextIsEqual(GetFileNameWithoutExt(name), zipBase)) {
            picked = (char*)MemAlloc(RAYLIB_LIBRETRO_VFS_MAX_PATH);
            TextCopy(picked, entries.paths[i]);
            break;
        }
    }

    // Pass 2: first entry whose extension is in validExtensions (any depth).
    const char* exts = GetLibretroValidExtensions();
    if (picked == NULL && exts != NULL && exts[0] != '\0') {
        char pattern[256];
        LibretroBuildExtPattern(exts, pattern, sizeof(pattern));
        for (unsigned int i = 0; i < entries.count; i++) {
            const char* name = GetFileName(entries.paths[i]);
            if (IsFileExtension(name, pattern)) {
                picked = (char*)MemAlloc(RAYLIB_LIBRETRO_VFS_MAX_PATH);
                TextCopy(picked, entries.paths[i]);
                break;
            }
        }
    }

    UnloadDirectoryFiles(entries);
    return picked;
}

static bool LoadLibretroGamePhysFS(const char* gameFile) {
    if (gameFile == NULL) {
        return LoadLibretroGame(NULL);
    }

    if (!FileExists(gameFile)) {
        TraceLog(LOG_ERROR, "LIBRETRO: Given content does not exist: %s", gameFile);
        return false;
    }

    if (!LibretroPhysFS.ready && !InitLibretroPhysFS()) {
        // No PhysFS — fall back to the direct path-based loader.
        return LoadLibretroGame(gameFile);
    }

    // Drop any prior mount before establishing a new one.
    if (LibretroPhysFS.mountSource[0] != '\0') {
        UnmountPhysFS(LibretroPhysFS.mountSource);
        LibretroPhysFS.mountSource[0] = '\0';
    }

    bool isZip = IsFileExtension(gameFile, ".zip");
    bool treatAsArchive = isZip && !IsLibretroBlockExtract();
    const char* mountSource = treatAsArchive ? gameFile : GetDirectoryPath(gameFile);

    if (!MountPhysFS(mountSource, "/game")) {
        TraceLog(LOG_ERROR, "LIBRETRO: Failed to mount %s at /game", mountSource);
        return LoadLibretroGame(gameFile);
    }
    TextCopy(LibretroPhysFS.mountSource, mountSource);

    // Resolve the virtual ROM path inside /game.
    char virtualPath[RAYLIB_LIBRETRO_VFS_MAX_PATH];
    if (treatAsArchive) {
        char* picked = LibretroPhysFSPickFileInZip(gameFile);
        if (picked == NULL) {
            TraceLog(LOG_ERROR, "LIBRETRO: No suitable ROM found inside %s", gameFile);
            UnmountPhysFS(mountSource);
            LibretroPhysFS.mountSource[0] = '\0';
            return false;
        }
        TextCopy(virtualPath, picked);
        MemFree(picked);
    } else {
        snprintf(virtualPath, sizeof(virtualPath), "/game/%s", GetFileName(gameFile));
    }

    // Per-extension content_info_override may flip need_fullpath for the
    // picked ROM (e.g. fceumm declares need_fullpath=false for .nes).
    bool persistent = false;
    bool needFullpath = LibretroResolveNeedFullpath(virtualPath, &persistent);

    if (needFullpath) {
        // Fullpath cores that don't honor the libretro VFS would fopen()
        // /game/* and fail. Hand them the original OS path; archives are
        // rejected (unless block_extract pushed them through the regular
        // file path above).
        if (treatAsArchive) {
            TraceLog(LOG_ERROR, "LIBRETRO: Core requires a real file path; .zip not supported");
            UnmountPhysFS(mountSource);
            LibretroPhysFS.mountSource[0] = '\0';
            return false;
        }
        bool ok = LoadLibretroGame(gameFile);
        if (!ok) {
            UnmountPhysFS(mountSource);
            LibretroPhysFS.mountSource[0] = '\0';
        }
        return ok;
    }

    // Memory path: read through PhysFS, hand to the in-memory loader.
    int size = 0;
    unsigned char* gameData = LoadFileDataFromPhysFS(virtualPath, &size);
    if (gameData == NULL || size == 0) {
        TraceLog(LOG_ERROR, "LIBRETRO: Failed to read game data from %s", virtualPath);
        if (gameData != NULL) UnloadFileData(gameData);
        UnmountPhysFS(mountSource);
        LibretroPhysFS.mountSource[0] = '\0';
        return false;
    }

    bool ok = LoadLibretroGameFromMemoryEx(gameData, size, virtualPath, persistent);
    if (!ok) {
        UnmountPhysFS(mountSource);
        LibretroPhysFS.mountSource[0] = '\0';
    }
    if (!persistent) {
        UnloadFileData(gameData);
    }
    return ok;
}

#if defined(__cplusplus)
}
#endif

#endif // RAYLIB_LIBRETRO_PHYSFS_IMPLEMENTATION_ONCE
#endif // RAYLIB_LIBRETRO_PHYSFS_IMPLEMENTATION
