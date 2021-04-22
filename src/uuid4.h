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

#ifndef UUID4_H
#define UUID4_H

#include "common.h"

void uuid4(unsigned char *uuid);
int uuid_decode(const char *str, unsigned char *uuid);
int uuid_encode(const unsigned char *uuid, char *str, size_t str_max);

#endif /* UUID4_H */
