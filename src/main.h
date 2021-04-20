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

#ifndef MAIN_H
#define MAIN_H

#include "common.h"

#if TICK_CONFIG_USE_ARGV
int main(int argc, char *argv[]);
#else
int main(void);
#endif

#endif /* MAIN_H */
