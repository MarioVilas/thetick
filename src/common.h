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

#endif
