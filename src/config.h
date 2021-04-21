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

    // Time limit settings.
#if TICK_FEATURES_TIME_LIMIT
    time_t start_time;
    time_t end_time;
#endif

    // Crypto settings.
#if TICK_FEATURES_CRYPTO
    int use_ssl;            // Set to 1 to use SSL, 0 for plaintext.
                            // TODO: add certificate pinning
#endif

} Settings;

int find_short_option(char c);
int find_long_option(char *option);
int parse_option(Settings *s, int option_index, char *input);
int parse_command_line(Settings *s, int argc, char *argv[], int skip_first);
int split_command_line(char *cmdline, char *argv[]);
int parse_environment(Settings *s);
int parse_config_file(Settings *s, char *filename);
void show_help(Settings *s, char *execname);

#if TICK_CONFIG_USE_ARGV
void get_configuration(Settings *s, int argc, char *argv[]);
#else
void get_configuration(Settings *s);
#endif

#endif /* CONFIG_H */
