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

#ifndef CONFIG_H
#define CONFIG_H

#include "common.h"

// Bot settings structure.
typedef struct {

    // Connection parameters.
    char uuid[16];          // UUID of the bot instance. Used internally.
    char hostname[64];      // Hostname or IP address to connect to.
    int port;               // Port to connect to. Defaults to 5555.

#ifndef TICK_FEATURES_NO_CRYPTO

    // Crypto settings.
    int use_ssl;            // Set to 1 to use SSL, 0 for plaintext.
                            // TODO: add certificate pinning

#endif

} Settings;

#endif /* CONFIG_H */
