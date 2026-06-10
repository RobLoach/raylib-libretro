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
*           LoadLibretroGameFromPhysFS(gameFile);  // instead of LoadLibretroGame()
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
static bool LoadLibretroGameFromPhysFS(const char* gameFile);   // Zip-aware replacement for LoadLibretroGame.

#if defined(__cplusplus)
}
#endif

#endif // RAYLIB_LIBRETRO_PHYSFS_H

#ifdef RAYLIB_LIBRETRO_PHYSFS_IMPLEMENTATION
#ifndef RAYLIB_LIBRETRO_PHYSFS_IMPLEMENTATION_ONCE
#define RAYLIB_LIBRETRO_PHYSFS_IMPLEMENTATION_ONCE

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct rLibretroPhysFS {
    bool ready;
    char mountSource[RAYLIB_LIBRETRO_VFS_MAX_PATH];
} rLibretroPhysFS;

static rLibretroPhysFS LibretroPhysFS = {0};

/**
 * PhysFS-backed VFS hook: load file data from the PhysFS search path.
 * Returns NULL silently when the file is not found in any mount.
 */
static unsigned char* LibretroPhysFSVfsLoadFileData(const char* fileName, int* bytesRead) {
    if (!IsPhysFSReady() || !FileExistsInPhysFS(fileName)) {
        *bytesRead = 0;
        return NULL;
    }
    return LoadFileDataFromPhysFS(fileName, bytesRead);
}

/**
 * PhysFS-backed VFS hook: stat a path in the PhysFS search path.
 * Returns RETRO_VFS_STAT flags, or 0 if not found.
 */
static int LibretroPhysFSVfsStat(const char* path, int32_t* size) {
    if (!IsPhysFSReady()) return 0;
    PHYSFS_Stat stat;
    if (PHYSFS_stat(path, &stat) == 0) return 0;

    if (stat.filetype == PHYSFS_FILETYPE_DIRECTORY) {
        return RETRO_VFS_STAT_IS_DIRECTORY | RETRO_VFS_STAT_IS_VALID;
    }
    if (stat.filetype == PHYSFS_FILETYPE_REGULAR) {
        if (size != NULL) *size = (int32_t)stat.filesize;
        return RETRO_VFS_STAT_IS_VALID;
    }
    return 0;
}

/**
 * PhysFS-backed VFS hook: list directory contents from the PhysFS search path.
 * Returns an empty list when the directory is not found.
 */
static FilePathList LibretroPhysFSVfsLoadDirFiles(const char* dirPath) {
    if (IsPhysFSReady() && DirectoryExistsInPhysFS(dirPath)) {
        return LoadDirectoryFilesFromPhysFS(dirPath);
    }
    FilePathList empty = {0};
    return empty;
}

/**
 * Unmounts the game mount.
 */
static void LibretroPhysFSClearMount(void) {
    if (LibretroPhysFS.mountSource[0] != '\0') {
        UnmountPhysFS(LibretroPhysFS.mountSource);
        LibretroPhysFS.mountSource[0] = '\0';
    }
}

/**
 * Initialize PhysFS so LoadLibretroGameFromPhysFS() can mount archives.
 *
 * @return \c true on success, \c false if PhysFS initialization failed.
 */
static bool InitLibretroPhysFS(void) {
    if (LibretroPhysFS.ready) return true;
    if (!InitPhysFS()) {
        TraceLog(LOG_WARNING, "LIBRETRO: Failed to initialize PhysFS");
        return false;
    }
    raylib_libretro_vfs_alt_load_file_data = LibretroPhysFSVfsLoadFileData;
    raylib_libretro_vfs_alt_stat = LibretroPhysFSVfsStat;
    raylib_libretro_vfs_alt_load_dir_files = LibretroPhysFSVfsLoadDirFiles;
    LibretroPhysFS.ready = true;
    return true;
}

/**
 * Tear down PhysFS, unmounting any active /game mount.
 */
static void CloseLibretroPhysFS(void) {
    LibretroPhysFSClearMount();
    if (LibretroPhysFS.ready) {
        raylib_libretro_vfs_alt_load_file_data = NULL;
        raylib_libretro_vfs_alt_stat = NULL;
        raylib_libretro_vfs_alt_load_dir_files = NULL;
        ClosePhysFS();
        LibretroPhysFS.ready = false;
    }
}

/**
 * Pick the ROM file inside /game after the caller has already mounted a zip file.
 *
 * Will find the first entry that matches...
 *   1. The same basename without the extension. For example: mario.zip > mario.nes
 *   2. The first entry whose extension appears in the core's retro_system_info::valid_extensions
 *
 * Subdirectories are searched recursively.
 *
 * @param zipFile Path of the original archive (used to derive the basename).
 * @param outPath Caller-provided buffer (at least RAYLIB_LIBRETRO_VFS_MAX_PATH
 *                bytes) that receives the virtual path on success.
 * @return \c true if a candidate was found and copied into @p outPath.
 */
static bool LibretroPhysFSPickFileInZip(const char* zipFile, char* outPath) {
    FilePathList entries = LoadDirectoryFilesFromPhysFSEx("/game", NULL, true);
    if (entries.count == 0) {
        UnloadDirectoryFiles(entries);
        return false;
    }

    const char* zipBase = GetFileNameWithoutExt(zipFile);
    bool found = false;

    // Pass 1: disc metadata — .m3u playlists, then .cue sheets.
    for (unsigned int i = 0; !found && i < entries.count; i++) {
        if (IsFileExtension(entries.paths[i], ".m3u")) {
            TextCopy(outPath, entries.paths[i]);
            found = true;
        }
    }
    for (unsigned int i = 0; !found && i < entries.count; i++) {
        if (IsFileExtension(entries.paths[i], ".cue")) {
            TextCopy(outPath, entries.paths[i]);
            found = true;
        }
    }

    // Pass 2: basename match (any depth).
    for (unsigned int i = 0; !found && i < entries.count; i++) {
        const char* name = GetFileName(entries.paths[i]);
        if (TextIsEqual(GetFileNameWithoutExt(name), zipBase)) {
            TextCopy(outPath, entries.paths[i]);
            found = true;
        }
    }

    // Pass 3: first entry whose extension is in validExtensions (any depth).
    const char* exts = GetLibretroValidExtensions();
    if (!found && exts != NULL && exts[0] != '\0') {
        char pattern[256];
        GetLibretroFileExtensionPattern(exts, pattern, sizeof(pattern));
        for (unsigned int i = 0; i < entries.count; i++) {
            const char* name = GetFileName(entries.paths[i]);
            if (IsFileExtension(name, pattern)) {
                TextCopy(outPath, entries.paths[i]);
                found = true;
                break;
            }
        }
    }

    UnloadDirectoryFiles(entries);
    return found;
}

/**
 * Zip-aware replacement for LoadLibretroGame().
 *
 * @param gameFile OS path to the ROM or .zip. May be NULL for a
 *                 content-less core load.
 * @return \c true on success, \c false on failure.
 */
static bool LoadLibretroGameFromPhysFS(const char* gameFile) {
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
    LibretroPhysFSClearMount();

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
        if (!LibretroPhysFSPickFileInZip(gameFile, virtualPath)) {
            TraceLog(LOG_ERROR, "LIBRETRO: No suitable ROM found inside %s", gameFile);
            LibretroPhysFSClearMount();
            return false;
        }
    } else {
        TextCopy(virtualPath, TextFormat("/game/%s", GetFileName(gameFile)));
    }

    // Per-extension content_info_override may flip need_fullpath for the
    // picked ROM (e.g. fceumm declares need_fullpath=false for .nes).
    bool persistent = false;
    if (GetLibretroNeedFullpath(virtualPath, &persistent)) {
        // Fullpath cores that don't honor the libretro VFS would fopen()
        // /game/* and fail. Hand them the original OS path; archives are
        // rejected (unless block_extract pushed them through the regular
        // file path above).
        if (treatAsArchive) {
            // Hand the core the PhysFS virtual path. The VFS hooks
            // installed by InitLibretroPhysFS() let the core read
            // /game/* transparently from the mounted archive.
            bool ok = LoadLibretroGame(virtualPath);
            if (!ok) LibretroPhysFSClearMount();
            return ok;
        }
        bool ok = LoadLibretroGame(gameFile);
        if (!ok) LibretroPhysFSClearMount();
        return ok;
    }

    // Memory path: read through PhysFS, hand to the in-memory loader.
    int size = 0;
    unsigned char* gameData = LoadFileDataFromPhysFS(virtualPath, &size);
    if (gameData == NULL || size == 0) {
        TraceLog(LOG_ERROR, "LIBRETRO: Failed to read game data from %s", virtualPath);
        UnloadFileData(gameData);
        LibretroPhysFSClearMount();
        return false;
    }

    bool ok = LoadLibretroGameFromMemoryEx(gameData, size, virtualPath, persistent);
    if (!ok) LibretroPhysFSClearMount();
    if (!ok || !persistent) {
        UnloadFileData(gameData);
    }
    return ok;
}

#if defined(__cplusplus)
}
#endif

#endif // RAYLIB_LIBRETRO_PHYSFS_IMPLEMENTATION_ONCE
#endif // RAYLIB_LIBRETRO_PHYSFS_IMPLEMENTATION
