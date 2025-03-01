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

static const char *raylib_libretro_vfs_get_path(struct retro_vfs_file_handle *stream);
struct retro_vfs_file_handle* raylib_libretro_vfs_open(const char *path, unsigned int mode, unsigned int hints);
int raylib_libretro_vfs_close(struct retro_vfs_file_handle *stream);
int64_t raylib_libretro_vfs_size(struct retro_vfs_file_handle *stream);

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
int64_t raylib_libretro_vfs_truncate(struct retro_vfs_file_handle *stream, int64_t length);

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
int64_t raylib_libretro_vfs_tell(struct retro_vfs_file_handle *stream);

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
int64_t raylib_libretro_vfs_seek(struct retro_vfs_file_handle *stream, int64_t offset, int seek_position);

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
int64_t raylib_libretro_vfs_read(struct retro_vfs_file_handle *stream, void *s, uint64_t len);

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
int64_t raylib_libretro_vfs_write(struct retro_vfs_file_handle *stream, const void *s, uint64_t len);

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
int raylib_libretro_vfs_flush(struct retro_vfs_file_handle *stream);

/**
 * Deletes the file at the given path.
 *
 * @param path The path to the file that will be deleted.
 * @return 0 on success, -1 on failure.
 * @see filestream_delete
 * @since VFS API v1
 */
int raylib_libretro_vfs_remove(const char *path);

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
int raylib_libretro_vfs_rename(const char *old_path, const char *new_path);

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
int raylib_libretro_vfs_stat(const char *path, int32_t *size);

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
int raylib_libretro_vfs_mkdir(const char *dir);

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
struct retro_vfs_dir_handle *raylib_libretro_vfs_opendir(const char *dir, bool include_hidden);

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
bool raylib_libretro_vfs_readdir(struct retro_vfs_dir_handle *dirstream);

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
const char *raylib_libretro_vfs_dirent_get_name(struct retro_vfs_dir_handle *dirstream);

/**
 * Checks whether the current dirent names a directory.
 *
 * @param dirstream The directory to read from.
 * @return \c true if \c dirstream's current dirent points to a directory,
 * \c false if not or if there was an error.
 * @see retro_dirent_is_dir
 * @since VFS API v3
 */
bool raylib_libretro_vfs_dirent_is_dir(struct retro_vfs_dir_handle *dirstream);

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
int raylib_libretro_vfs_closedir(struct retro_vfs_dir_handle *dirstream);

#endif

#ifdef RAYLIB_LIBRETRO_VFS_IMPLEMENTATION
#ifndef RAYLIB_LIBRETRO_VFS_IMPLEMENTATION_ONCE
#define RAYLIB_LIBRETRO_VFS_IMPLEMENTATION_ONCE

static const char *raylib_libretro_vfs_get_path(struct retro_vfs_file_handle *stream) {
    if (stream == NULL) {
        return "";
    }

    return stream->path;
}

struct retro_vfs_file_handle* raylib_libretro_vfs_open(const char *path, unsigned int mode, unsigned int hints) {
    if (TextLength(path) >= RAYLIB_LIBRETRO_VFS_MAX_PATH) {
        TraceLog(LOG_ERROR, "LIBRETRO: Path length is too long");
        return NULL;
    }

    struct retro_vfs_file_handle* handle = (struct retro_vfs_file_handle*)MemAlloc(sizeof(struct retro_vfs_file_handle));
    if (handle == NULL) {
        return NULL;
    }

    TextCopy(handle->path, path);
    handle->data = LoadFileData(path, &handle->dataSize);
    if (handle->data == NULL) {
        MemFree(handle);
        TraceLog(LOG_ERROR, "LIBRETRO: File not found: %s", path);
        return NULL;
    }
    handle->position = 0;

    return handle;
}

int raylib_libretro_vfs_close(struct retro_vfs_file_handle *stream) {
    if (stream == NULL) {
        return 0;
    }

    if (stream->data != NULL) {
        UnloadFileData(stream->data);
        stream->data = NULL;
    }

    MemFree(stream);
}

int64_t raylib_libretro_vfs_size(struct retro_vfs_file_handle *stream) {
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
int64_t raylib_libretro_vfs_truncate(struct retro_vfs_file_handle *stream, int64_t length) {
    TraceLog(LOG_ERROR, "LIBRETRO: raylib_libretro_vfs_truncate not implemented");
    return -1;
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
int64_t raylib_libretro_vfs_tell(struct retro_vfs_file_handle *stream) {
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
int64_t raylib_libretro_vfs_seek(struct retro_vfs_file_handle *stream, int64_t offset, int seek_position) {
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
int64_t raylib_libretro_vfs_read(struct retro_vfs_file_handle *stream, void *s, uint64_t len) {
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
int64_t raylib_libretro_vfs_write(struct retro_vfs_file_handle *stream, const void *s, uint64_t len) {
    TraceLog(LOG_ERROR, "LIBRETRO: raylib_libretro_vfs_write not implemented");
    return -1;
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
int raylib_libretro_vfs_flush(struct retro_vfs_file_handle *stream) {
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
int raylib_libretro_vfs_remove(const char *path) {
    TraceLog(LOG_ERROR, "LIBRETRO: raylib_libretro_vfs_remove not implemented");
    return -1;
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
int raylib_libretro_vfs_rename(const char *old_path, const char *new_path) {
    TraceLog(LOG_ERROR, "LIBRETRO: raylib_libretro_vfs_rename not implemented");
    return -1;
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
int raylib_libretro_vfs_stat(const char *path, int32_t *size) {
    int output = 0;

    //TraceLog(LOG_WARNING, "raylib_libretro_vfs_stat('%s')", path);

    bool fileExists = FileExists(path);
    bool directoryExists = DirectoryExists(path);

    if (size != NULL && fileExists && !directoryExists) {
        *size = (int32_t)GetFileLength(path);
    }

    if (fileExists) {
        return RETRO_VFS_STAT_IS_VALID;
    }

    if (directoryExists) {
        return RETRO_VFS_STAT_IS_DIRECTORY;
    }

    return 0;
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
int raylib_libretro_vfs_mkdir(const char *dir) {
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
struct retro_vfs_dir_handle *raylib_libretro_vfs_opendir(const char *dir, bool include_hidden) {
    if (dir == NULL) {
        return NULL;
    }

    if (TextLength(dir) >= RAYLIB_LIBRETRO_VFS_MAX_PATH) {
        TraceLog(LOG_ERROR, "LIBRETRO: Dir path length is too long");
        return NULL;
    }

    if (!DirectoryExists(dir)) {
        return NULL;
    }

    struct retro_vfs_dir_handle * handle = (struct retro_vfs_dir_handle *)MemAlloc(sizeof(struct retro_vfs_dir_handle));
    if (handle == NULL) {
        return NULL;
    }

    TextCopy(handle->path, dir);
    handle->include_hidden = include_hidden;

    handle->directoryFiles = LoadDirectoryFiles(handle->path);
    handle->directoryFilesIndex = -1;
    raylib_libretro_vfs_readdir(handle);

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
bool raylib_libretro_vfs_readdir(struct retro_vfs_dir_handle *dirstream) {
    if (dirstream == NULL) {
        return false;
    }

    bool found = false;
    while (!found) {
        if (++dirstream->directoryFilesIndex >= dirstream->directoryFiles.count) {
            return false;
        }

        if (TextIsEqual(dirstream->directoryFiles.paths[dirstream->directoryFilesIndex], ".")) {
            continue;
        }

        if (TextIsEqual(dirstream->directoryFiles.paths[dirstream->directoryFilesIndex], "..")) {
            continue;
        }

        if (!dirstream->include_hidden) {
            if (dirstream->directoryFiles.paths[dirstream->directoryFilesIndex][0] == '.') {
                continue;
            }
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
const char *raylib_libretro_vfs_dirent_get_name(struct retro_vfs_dir_handle *dirstream) {
    if (dirstream == NULL) {
        return NULL;
    }

    if (dirstream->directoryFilesIndex >= dirstream->directoryFiles.count) {
        return NULL;
    }

    return dirstream->directoryFiles.paths[dirstream->directoryFilesIndex];
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
bool raylib_libretro_vfs_dirent_is_dir(struct retro_vfs_dir_handle *dirstream) {
    if (dirstream == NULL) {
        return false;
    }

    const char* dir = raylib_libretro_vfs_dirent_get_name(dirstream);
    if (dir == NULL) {
        return false;
    }

    return DirectoryExists(dir);
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
int raylib_libretro_vfs_closedir(struct retro_vfs_dir_handle *dirstream) {
    if (dirstream == NULL) {
        return 0;
    }

    UnloadDirectoryFiles(dirstream->directoryFiles);
    MemFree(dirstream);
    return 0;
}

#endif
#endif
