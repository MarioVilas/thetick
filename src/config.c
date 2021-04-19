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

#include "config.h"
#include "base64.h"

// Nifty macro trick to expand using quotes.
// https://stackoverflow.com/a/3419392/426293
#define _Q(x) #x
#define QUOTE(x) _Q(x)

#if TICK_CONFIG_USE_ARGV
void get_configuration(Settings *s, int argc, char *argv[])
#else
void get_configuration(Settings *s)
#endif
{
    // Zero out the memory structure for the settings.
    // We need this because not all values will be set by the code below.
    memset(s, 0, sizeof(Settings));

    // Now initialize again to the default values.
    // These can be controlled at compile time from the makefile.
    // Useful for situations where you can't find a way to pass the
    // configuration to the bot, so you just make a custom build.
#ifdef TICK_CONFIG_HOSTNAME
    strncpy(s->hostname, QUOTE(TICK_CONFIG_HOSTNAME), sizeof(s->hostname));
#endif
#ifdef TICK_CONFIG_PORT
    s->port = TICK_CONFIG_PORT;
#endif
#if TICK_FEATURES_CRYPTO
#ifdef TICK_CONFIG_USE_SSL
    s->use_ssl = TICK_CONFIG_USE_SSL;
#endif
#ifdef TICK_CONFIG_SSL_PORT
    s->ssl_port = TICK_CONFIG_SSL_PORT;
#endif
#endif

    // If appended configuration parsing is enabled, do it now.
#if TICK_CONFIG_USE_BIN

    // TODO
#   warning Feature not implemented: TICK_CONFIG_USE_BIN

#endif

    // If configuration file parsing is enabled, do it now.
#if TICK_CONFIG_USE_FILE

    // TODO
#   warning Feature not implemented: TICK_CONFIG_USE_FILE

#endif

    // If environment variable parsing is enabled, do it now.
#if TICK_CONFIG_USE_ENV

    // TODO
#   warning Feature not implemented: TICK_CONFIG_USE_ENV

#endif

    // If argv parsing is enabled, do it now.
#if TICK_CONFIG_USE_ARGV
    if (argc > 1) {
        strncpy(s->hostname, argv[1], sizeof(s->hostname));
    }
    if (argc > 2) {
#if TICK_FEATURES_CRYPTO
        if (s->use_ssl) {
            s->ssl_port = atoi(argv[2]);
        } else {
            s->port = atoi(argv[2]);
        }
#else
        s->port = atoi(argv[2]);
#endif
    }
#endif

}
