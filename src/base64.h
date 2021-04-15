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

#ifndef BASE64_H
#define BASE64_H

#include <stdint.h>
#include <stddef.h>

size_t base64_decode(const char *input, uint8_t *output, size_t output_size);

#endif
