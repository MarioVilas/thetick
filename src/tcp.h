/*
 * The Tick, a Linux embedded backdoor.
 * 
 * Developed by Mario Vilas, mvilas@gmail.com
 * http://www.github.com/MarioVilas/thetick
 * 
 * Originally released as open source by NCC Group Plc - http://www.nccgroup.com/
 * http://www.github.com/nccgroup/thetick
 * 
 * See the LICENSE file for further details.
*/

#ifndef TCP_H
#define TCP_H

#include <sys/types.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

int create_socket(int family);
int connect_socket(int fd, const struct sockaddr *sa, size_t count);
int connect_to_host(const char *hostname, int port);
int listen_on_port(const char *bind_addr, int *port);
int send_block(int fd, const char *buf, size_t count);
int recv_block(int sock, char *buf, size_t count);
ssize_t consume_extra_data(int fd, size_t count);
void disconnect_tcp(int fd);

#ifdef _WIN32
int copy_socket_stream(int source, int destination, ssize_t count);
#else
#define copy_socket_stream copy_stream
#endif

#endif /* TCP_H */
