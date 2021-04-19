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
    char uuid[16];          // UUID of the bot instance. Used internally.

    // Connection parameters.
    char hostname[64];      // Hostname or IP address to connect to.
    int port;               // Port to connect to.

    // Crypto settings.
#if TICK_FEATURES_CRYPTO
    int use_ssl;            // Set to 1 to use SSL, 0 for plaintext.
    int ssl_port;           // SSL port to connect to.
                            // TODO: add certificate pinning
#endif

} Settings;

#if TICK_CONFIG_USE_ARGV
void get_configuration(Settings *s, int argc, char *argv[]);
#else
void get_configuration(Settings *s);
#endif

#endif /* CONFIG_H */
