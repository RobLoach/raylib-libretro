#ifndef RAYLIB_LIBRETRO_VFS_H__
#define RAYLIB_LIBRETRO_VFS_H__

#include "libretro.h"
#include "raylib.h"

#ifndef RAYLIB_LIBRETRO_VFS_MAX_PATH
#define RAYLIB_LIBRETRO_VFS_MAX_PATH 4096
#endif

#ifndef RAYLIB_LIBRETRO_VFS_SEEK_SET
#define RAYLIB_LIBRETRO_VFS_SEEK_SET  0 /* Seek from beginning of file.  */
#endif
#ifndef RAYLIB_LIBRETRO_VFS_SEEK_CUR
#define RAYLIB_LIBRETRO_VFS_SEEK_CUR  1       /* Seek from current position.  */
#endif
#ifndef RAYLIB_LIBRETRO_VFS_SEEK_END
#define RAYLIB_LIBRETRO_VFS_SEEK_END  2       /* Set file pointer to EOF plus "offset" */
#endif

struct retro_vfs_file_handle {
    unsigned char* data;
    char path[RAYLIB_LIBRETRO_VFS_MAX_PATH];
    int mode;
    unsigned int hints;
    int dataSize;
    int64_t position;
};

struct retro_vfs_dir_handle {
    char path[RAYLIB_LIBRETRO_VFS_MAX_PATH];
    bool include_hidden;
    FilePathList directoryFiles;
    int directoryFilesIndex;
};

#if defined(__cplusplus)
extern "C" {
#endif

static const char* raylib_libretro_vfs_get_path(struct retro_vfs_file_handle* stream);
struct retro_vfs_file_handle* raylib_libretro_vfs_open(const char* path, unsigned int mode, unsigned int hints);
int raylib_libretro_vfs_close(struct retro_vfs_file_handle* stream);
int64_t raylib_libretro_vfs_size(struct retro_vfs_file_handle* stream);
int64_t raylib_libretro_vfs_truncate(struct retro_vfs_file_handle* stream, int64_t length);
int64_t raylib_libretro_vfs_tell(struct retro_vfs_file_handle* stream);
int64_t raylib_libretro_vfs_seek(struct retro_vfs_file_handle* stream, int64_t offset, int seek_position);
int64_t raylib_libretro_vfs_read(struct retro_vfs_file_handle* stream, void *s, uint64_t len);
int64_t raylib_libretro_vfs_write(struct retro_vfs_file_handle* stream, const void *s, uint64_t len);
int raylib_libretro_vfs_flush(struct retro_vfs_file_handle* stream);
int raylib_libretro_vfs_remove(const char* path);
int raylib_libretro_vfs_rename(const char* old_path, const char* new_path);
int raylib_libretro_vfs_stat(const char* path, int32_t *size);
int raylib_libretro_vfs_stat_64(const char* path, int64_t *size);
int raylib_libretro_vfs_mkdir(const char* dir);
struct retro_vfs_dir_handle* raylib_libretro_vfs_opendir(const char* dir, bool include_hidden);
bool raylib_libretro_vfs_readdir(struct retro_vfs_dir_handle* dirstream);
const char* raylib_libretro_vfs_dirent_get_name(struct retro_vfs_dir_handle* dirstream);
bool raylib_libretro_vfs_dirent_is_dir(struct retro_vfs_dir_handle* dirstream);
int raylib_libretro_vfs_closedir(struct retro_vfs_dir_handle* dirstream);

#if defined(__cplusplus)
}
#endif

#endif

#ifdef RAYLIB_LIBRETRO_VFS_IMPLEMENTATION
#ifndef RAYLIB_LIBRETRO_VFS_IMPLEMENTATION_ONCE
#define RAYLIB_LIBRETRO_VFS_IMPLEMENTATION_ONCE

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * Gets the path associated with the given file handle.
 *
 * @param stream The file handle to query.
 * @return The path the file was opened with,
 * or an empty string if \c stream is \c NULL.
 * @see retro_vfs_get_path_t
 * @since VFS API v1
 */
static const char* raylib_libretro_vfs_get_path(struct retro_vfs_file_handle* stream) {
    if (stream == NULL) {
        return "";
    }

    return stream->path;
}

/**
 * Opens the file at the given path.
 *
 * @param path The path of the file to open.
 * @param mode A bitwise combination of \c RETRO_VFS_FILE_ACCESS flags.
 * At least one of \c RETRO_VFS_FILE_ACCESS_READ or \c RETRO_VFS_FILE_ACCESS_WRITE must be set.
 * Adding \c RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING to a write-mode open preserves the file's
 * existing content rather than truncating it; without this flag, write mode starts empty.
 * @param hints A bitwise combination of \c RETRO_VFS_FILE_ACCESS_HINT flags.
 * @return A handle to the opened file,
 * or \c NULL if there was an error.
 * @see retro_vfs_open_t
 * @since VFS API v1
 */
struct retro_vfs_file_handle* raylib_libretro_vfs_open(const char* path, unsigned int mode, unsigned int hints) {
    if (path == NULL || TextLength(path) >= RAYLIB_LIBRETRO_VFS_MAX_PATH) {
        TraceLog(LOG_ERROR, "LIBRETRO: Path length is too long");
        return NULL;
    }

    struct retro_vfs_file_handle* handle = (struct retro_vfs_file_handle*)MemAlloc(sizeof(struct retro_vfs_file_handle));
    if (handle == NULL) {
        return NULL;
    }

    TextCopy(handle->path, path);
    handle->mode = mode;
    handle->hints = hints;
    handle->position = 0;
    handle->dataSize = 0;
    handle->data = NULL;

    if (mode & RETRO_VFS_FILE_ACCESS_READ) {
        handle->data = LoadFileData(path, &handle->dataSize);
        if (handle->data == NULL) {
            MemFree(handle);
            TraceLog(LOG_ERROR, "LIBRETRO: File not found: %s", path);
            return NULL;
        }
    } else if ((mode & RETRO_VFS_FILE_ACCESS_WRITE) && (mode & RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING)) {
        if (FileExists(path)) {
            handle->data = LoadFileData(path, &handle->dataSize);
            if (handle->data == NULL) {
                MemFree(handle);
                return NULL;
            }
        }
    }

    return handle;
}

/**
 * Closes the given file handle and releases its resources.
 *
 * For write-mode handles, any pending data is flushed to disk before closing.
 * Must be called on any handle returned by \c raylib_libretro_vfs_open.
 *
 * @param stream The file handle to close.
 * When this function returns, \c stream will no longer be valid and must not be used.
 * @return 0 on success, -1 on failure.
 * @see retro_vfs_close_t
 * @since VFS API v1
 */
int raylib_libretro_vfs_close(struct retro_vfs_file_handle* stream) {
    if (stream == NULL) {
        return 0;
    }

    if (stream->mode & RETRO_VFS_FILE_ACCESS_WRITE) {
        raylib_libretro_vfs_flush(stream);
        MemFree(stream->data);
    } else if (stream->data != NULL) {
        UnloadFileData(stream->data);
    }

    MemFree(stream);
    return 0;
}

/**
 * Gets the size of the given file in bytes.
 *
 * @param stream The file handle to query.
 * @return The size of the file in bytes,
 * or -1 if there was an error.
 * @see retro_vfs_size_t
 * @since VFS API v1
 */
int64_t raylib_libretro_vfs_size(struct retro_vfs_file_handle* stream) {
    if (stream == NULL) {
        return -1;
    }

    return (int64_t)stream->dataSize;
}

/**
 * Set the file's length.
 *
 * @param stream The file whose length will be adjusted.
 * @param length The new length of the file, in bytes.
 * If shorter than the original length, the extra bytes will be discarded.
 * If longer, the file's padding is unspecified (and likely platform-dependent).
 * @return 0 on success,
 * -1 on failure.
 * @see filestream_truncate
 * @since VFS API v2
 */
int64_t raylib_libretro_vfs_truncate(struct retro_vfs_file_handle* stream, int64_t length) {
    if (stream == NULL || length < 0) {
        return -1;
    }

    if (!(stream->mode & RETRO_VFS_FILE_ACCESS_WRITE)) {
        return -1;
    }

    unsigned char* resized = (unsigned char*)MemRealloc(stream->data, (unsigned int)length);
    if (resized == NULL && length > 0) {
        return -1;
    }

    stream->data = resized;
    stream->dataSize = (int)length;

    if (stream->position > length) {
        stream->position = length;
    }

    return 0;
}

/**
 * Gets the given file's current read/write position.
 * This position is advanced with each call to \c retro_vfs_read_t or \c retro_vfs_write_t.
 *
 * @param stream The file to query the position of.
 * @return The current stream position in bytes
 * or -1 if there was an error.
 * @see filestream_tell
 * @since VFS API v1
 */
int64_t raylib_libretro_vfs_tell(struct retro_vfs_file_handle* stream) {
    if (stream == NULL) {
        return -1;
    }

    return stream->position;
}

/**
 * Sets the given file handle's current read/write position.
 *
 * @param stream The file to set the position of.
 * @param offset The new position, in bytes.
 * @param seek_position The position to seek from.
 * @return The new position,
 * or -1 if there was an error.
 * @since VFS API v1
 * @see File Seek Positions
 * @see filestream_seek
 */
int64_t raylib_libretro_vfs_seek(struct retro_vfs_file_handle* stream, int64_t offset, int seek_position) {
    if (stream == NULL) {
        return -1;
    }

    switch (seek_position) {
        case RAYLIB_LIBRETRO_VFS_SEEK_SET:
            stream->position = offset;
            break;
        case RAYLIB_LIBRETRO_VFS_SEEK_CUR:
            stream->position += offset;
            break;
        case RAYLIB_LIBRETRO_VFS_SEEK_END:
            stream->position = stream->dataSize + offset;
            break;
        default:
            return -1;
    }

    if (stream->position < 0) {
        stream->position = 0;
    }
    else if (stream->position > stream->dataSize) {
        stream->position = stream->dataSize;
    }

    return stream->position;
}

/**
 * Read data from a file, if it was opened for reading.
 *
 * @param stream The file to read from.
 * @param s The buffer to read into.
 * @param len The number of bytes to read.
 * The buffer pointed to by \c s must be this large.
 * @return The number of bytes read,
 * or -1 if there was an error.
 * @since VFS API v1
 * @see filestream_read
 */
int64_t raylib_libretro_vfs_read(struct retro_vfs_file_handle* stream, void *s, uint64_t len) {
    if (stream == NULL || s == NULL || stream->data == NULL) {
        return -1;
    }

    unsigned char* data = stream->data + stream->position;
    unsigned char* output = (unsigned char*)s;
    int64_t bytesRead = 0;
    for (uint64_t i = 0; i < len; i++) {
        if (stream->position + i >= stream->dataSize) {
            break;
        }
        output[i] = data[i];
        bytesRead++;
    }

    stream->position += bytesRead;
    return bytesRead;
}

/**
 * Write data to a file, if it was opened for writing.
 *
 * @param stream The file handle to write to.
 * @param s The buffer to write from.
 * @param len The number of bytes to write.
 * The buffer pointed to by \c s must be this large.
 * @return The number of bytes written,
 * or -1 if there was an error.
 * @since VFS API v1
 * @see filestream_write
 */
int64_t raylib_libretro_vfs_write(struct retro_vfs_file_handle* stream, const void *s, uint64_t len) {
    if (stream == NULL || s == NULL) {
        return -1;
    }

    if (!(stream->mode & RETRO_VFS_FILE_ACCESS_WRITE)) {
        return -1;
    }

    int64_t end = stream->position + (int64_t)len;
    if (end > stream->dataSize) {
        unsigned char* resized = (unsigned char*)MemRealloc(stream->data, (unsigned int)end);
        if (resized == NULL) {
            return -1;
        }
        stream->data = resized;
        stream->dataSize = (int)end;
    }

    unsigned char* dest = stream->data + stream->position;
    const unsigned char* src = (const unsigned char*)s;
    for (uint64_t i = 0; i < len; i++) {
        dest[i] = src[i];
    }

    stream->position = end;
    return (int64_t)len;
}

/**
 * Flush pending writes to the file, if applicable.
 *
 * This does not mean that the changes will be immediately persisted to disk;
 * that may be scheduled for later, depending on the platform.
 *
 * @param stream The file handle to flush.
 * @return 0 on success, -1 on failure.
 * @since VFS API v1
 * @see filestream_flush
 */
int raylib_libretro_vfs_flush(struct retro_vfs_file_handle* stream) {
    if (stream == NULL || !(stream->mode & RETRO_VFS_FILE_ACCESS_WRITE)) {
        return 0;
    }

    if (!SaveFileData(stream->path, stream->data, stream->dataSize)) {
        return -1;
    }

    return 0;
}

/**
 * Deletes the file at the given path.
 *
 * @param path The path to the file that will be deleted.
 * @return 0 on success, -1 on failure.
 * @see filestream_delete
 * @since VFS API v1
 */
int raylib_libretro_vfs_remove(const char* path) {
    return FileRemove(path);
}

/**
 * Rename the specified file.
 *
 * @param old_path Path to an existing file.
 * @param new_path The destination path.
 * Must not name an existing file.
 * @return 0 on success, -1 on failure
 * @see filestream_rename
 * @since VFS API v1
 */
int raylib_libretro_vfs_rename(const char* old_path, const char* new_path) {
    return FileRename(old_path, new_path);
}

/**
 * Gets information about the given file.
 *
 * @param path The path to the file to query.
 * @param[out] size The reported size of the file in bytes.
 * May be \c NULL, in which case this value is ignored.
 * @return A bitmask of \c RETRO_VFS_STAT flags,
 * or 0 if \c path doesn't refer to a valid file.
 * @see path_stat
 * @see path_get_size
 * @see RETRO_VFS_STAT
 * @since VFS API v3
 */
int raylib_libretro_vfs_stat(const char* path, int32_t *size) {
    // DirectoryExists must be checked first, because FileExists uses
    // access() which returns true for directories.
    if (DirectoryExists(path)) {
        return RETRO_VFS_STAT_IS_DIRECTORY | RETRO_VFS_STAT_IS_VALID;
    }

    if (FileExists(path)) {
        int fileLength = GetFileLength(path);
        if (size != NULL) {
            *size = (int32_t)fileLength;
        }
        return RETRO_VFS_STAT_IS_VALID;
    }

    return 0;
}

/**
 * Gets information about the given file using an int64.
 *
 * @param path The path to the file to query.
 * @param[out] size The reported size of the file in bytes.
 * May be \c NULL, in which case this value is ignored.
 * @return A bitmask of \c RETRO_VFS_STAT flags,
 * or 0 if \c path doesn't refer to a valid file.
 * @see path_stat
 * @see path_get_size
 * @see RETRO_VFS_STAT
 * @see raylib_libretro_vfs_stat
 * @since VFS API v4
 */
int raylib_libretro_vfs_stat_64(const char* path, int64_t *size) {
    // TODO: raylib doesn't really have an int64 file size type, so we use stat() instead.
    int32_t size32;
    int output = raylib_libretro_vfs_stat(path, &size32);
    if (size != NULL) {
        *size = (int64_t)size32;
    }
    return output;
}

/**
 * Creates a directory at the given path.
 *
 * @param dir The desired location of the new directory.
 * @return 0 if the directory was created,
 * -2 if the directory already exists,
 * or -1 if some other error occurred.
 * @see path_mkdir
 * @since VFS API v3
 */
int raylib_libretro_vfs_mkdir(const char* dir) {
    if (DirectoryExists(dir)) {
        return -2;
    }

    if (MakeDirectory(dir) == 0) {
        return 0;
    }

    return -1;
}

/**
 * Opens a handle to a directory so its contents can be inspected.
 *
 * @param dir The path to the directory to open.
 * Must be an existing directory.
 * @param include_hidden Whether to include hidden files in the directory listing.
 * The exact semantics of this flag will depend on the platform.
 * @return A handle to the opened directory,
 * or \c NULL if there was an error.
 * @see retro_opendir
 * @since VFS API v3
 */
struct retro_vfs_dir_handle* raylib_libretro_vfs_opendir(const char* dir, bool include_hidden) {
    if (dir == NULL) {
        return NULL;
    }

    if (TextLength(dir) >= RAYLIB_LIBRETRO_VFS_MAX_PATH) {
        TraceLog(LOG_ERROR, "LIBRETRO: Directory path length is too long");
        return NULL;
    }

    if (!DirectoryExists(dir)) {
        return NULL;
    }

    struct retro_vfs_dir_handle* handle = (struct retro_vfs_dir_handle*)MemAlloc(sizeof(struct retro_vfs_dir_handle));
    if (handle == NULL) {
        return NULL;
    }

    TextCopy(handle->path, dir);
    handle->include_hidden = include_hidden;

    handle->directoryFiles = LoadDirectoryFiles(handle->path);
    handle->directoryFilesIndex = -1;

    return handle;
}

/**
 * Gets the next dirent ("directory entry")
 * within the given directory.
 *
 * @param[in,out] dirstream The directory to read from.
 * Updated to point to the next file, directory, or other path.
 * @return \c true when the next dirent was retrieved,
 * \c false if there are no more dirents to read.
 * @note This API iterates over all files and directories within \c dirstream.
 * Remember to check what the type of the current dirent is.
 * @note This function does not recurse,
 * i.e. it does not return the contents of subdirectories.
 * @note This may include "." and ".." on Unix-like platforms.
 * @see retro_readdir
 * @see retro_vfs_dirent_is_dir_t
 * @since VFS API v3
 */
bool raylib_libretro_vfs_readdir(struct retro_vfs_dir_handle* dirstream) {
    if (dirstream == NULL) {
        return false;
    }

    bool found = false;
    while (!found) {
        if (++dirstream->directoryFilesIndex >= dirstream->directoryFiles.count) {
            return false;
        }

        const char* name = GetFileName(dirstream->directoryFiles.paths[dirstream->directoryFilesIndex]);
        if (TextIsEqual(name, ".") || TextIsEqual(name, "..")) {
            continue;
        }

        if (!dirstream->include_hidden && name[0] == '.') {
            continue;
        }

        found = true;
    }

    return true;
}

/**
 * Gets the filename of the current dirent.
 *
 * The returned string pointer is valid
 * until the next call to \c retro_vfs_readdir_t or \c retro_vfs_closedir_t.
 *
 * @param dirstream The directory to read from.
 * @return The current dirent's name,
 * or \c NULL if there was an error.
 * @note This function only returns the file's \em name,
 * not a complete path to it.
 * @see retro_dirent_get_name
 * @since VFS API v3
 */
const char* raylib_libretro_vfs_dirent_get_name(struct retro_vfs_dir_handle* dirstream) {
    if (dirstream == NULL) {
        return NULL;
    }

    if (dirstream->directoryFilesIndex >= dirstream->directoryFiles.count) {
        return NULL;
    }

    return GetFileName(dirstream->directoryFiles.paths[dirstream->directoryFilesIndex]);
}

/**
 * Checks whether the current dirent names a directory.
 *
 * @param dirstream The directory to read from.
 * @return \c true if \c dirstream's current dirent points to a directory,
 * \c false if not or if there was an error.
 * @see retro_dirent_is_dir
 * @since VFS API v3
 */
bool raylib_libretro_vfs_dirent_is_dir(struct retro_vfs_dir_handle* dirstream) {
    if (dirstream == NULL) {
        return false;
    }

    if (dirstream->directoryFilesIndex < 0 || dirstream->directoryFilesIndex >= dirstream->directoryFiles.count) {
        return false;
    }

    return DirectoryExists(dirstream->directoryFiles.paths[dirstream->directoryFilesIndex]);
}

/**
 * Closes the given directory and release its resources.
 *
 * Must be called on any \c retro_vfs_dir_handle returned by \c retro_vfs_open_t.
 *
 * @param dirstream The directory to close.
 * When this function returns (even failure),
 * \c dirstream will no longer be valid and must not be used.
 * @return 0 on success, -1 on failure.
 * @see retro_closedir
 * @since VFS API v3
 */
int raylib_libretro_vfs_closedir(struct retro_vfs_dir_handle* dirstream) {
    if (dirstream == NULL) {
        return 0;
    }

    UnloadDirectoryFiles(dirstream->directoryFiles);
    MemFree(dirstream);
    return 0;
}

#if defined(__cplusplus)
}
#endif

#endif
#endif
