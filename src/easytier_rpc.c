// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include "easytier_rpc.h"
#include "node_json.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#include <stdint.h>
#include <wchar.h>
#include <windows.h>
typedef HANDLE cli_process_t;
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
typedef pid_t cli_process_t;
#endif

static char g_rpc_host[128] = "127.0.0.1";
static unsigned short g_rpc_port = 15888;
static char g_cli_path[4096] = "easytier-cli";
static int g_custom_cli_path;
static int g_verbose;

static int nat_allows_announce(const char *type)
{
    return strcmp(type, "Public") == 0 || strcmp(type, "FriendlyNAT") == 0 ||
           strcmp(type, "NoPAT") == 0 || strcmp(type, "OpenInternet") == 0 ||
           strcmp(type, "FullCone") == 0;
}

void easytier_rpc_set_verbose(int verbose)
{
    g_verbose = verbose;
}

int easytier_rpc_init(const char *rpc_host, unsigned short rpc_port,
                      const char *cli_path)
{
    const char *executable = cli_path != NULL ? cli_path : "easytier-cli";

    if(rpc_host == NULL || executable[0] == '\0' ||
       strlen(executable) >= sizeof(g_cli_path))
        return -1;
    strcpy(g_cli_path, executable);
    g_custom_cli_path = cli_path != NULL;
    snprintf(g_rpc_host, sizeof(g_rpc_host), "%s", rpc_host);
    g_rpc_port = rpc_port;
    return 0;
}

static unsigned short listener_port(const char *listener)
{
    const char *p = strrchr(listener, ':');
    if(p == NULL || p[1] == '\0')
        return 0;
    return (unsigned short)strtoul(p + 1, NULL, 10);
}

static int wait_for_cli(cli_process_t pid);

#ifdef _WIN32
static wchar_t *append_argument(wchar_t *out, const wchar_t *argument)
{
    *out++ = L'"';
    for(;;) {
        size_t slashes = 0;
        while(*argument == L'\\') {
            slashes++;
            argument++;
        }
        if(*argument == L'"' || *argument == L'\0')
            slashes *= 2;
        while(slashes > 0) {
            *out++ = L'\\';
            slashes--;
        }
        if(*argument == L'\0')
            break;
        if(*argument == L'"')
            *out++ = L'\\';
        *out++ = *argument++;
    }
    *out++ = L'"';
    *out = L'\0';
    return out;
}

static FILE *spawn_easytier_cli(cli_process_t *pid_out)
{
    wchar_t executable[4096];
    wchar_t target[160];
    wchar_t command[2 * (4096 + 160) + 128];
    wchar_t *next;
    SECURITY_ATTRIBUTES security = {sizeof(security), NULL, TRUE};
    STARTUPINFOEXW startup;
    PROCESS_INFORMATION process;
    HANDLE reader = NULL;
    HANDLE writer = NULL;
    HANDLE null_device = INVALID_HANDLE_VALUE;
    HANDLE inherited[2];
    SIZE_T attributes_size = 0;
    int attributes_initialized = 0;
    int fd;
    FILE *stream = NULL;

    if(MultiByteToWideChar(CP_ACP, 0, g_cli_path, -1,
                           executable, 4096) == 0)
        return NULL;
    {
        char rpc_target[160];
        snprintf(rpc_target, sizeof(rpc_target), "%s:%u", g_rpc_host, g_rpc_port);
        if(MultiByteToWideChar(CP_ACP, 0, rpc_target, -1,
                               target, 160) == 0)
            return NULL;
    }
    next = append_argument(command, executable);
    *next++ = L' ';
    next = append_argument(next, L"-p");
    *next++ = L' ';
    next = append_argument(next, target);
    *next++ = L' ';
    next = append_argument(next, L"--output");
    *next++ = L' ';
    next = append_argument(next, L"json");
    *next++ = L' ';
    append_argument(next, L"node");

    memset(&startup, 0, sizeof(startup));
    memset(&process, 0, sizeof(process));
    if(!CreatePipe(&reader, &writer, &security, 0) ||
       !SetHandleInformation(reader, HANDLE_FLAG_INHERIT, 0))
        goto done;
    null_device = CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, &security,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(null_device == INVALID_HANDLE_VALUE)
        goto done;
    InitializeProcThreadAttributeList(NULL, 1, 0, &attributes_size);
    startup.lpAttributeList = malloc(attributes_size);
    if(startup.lpAttributeList == NULL ||
       !InitializeProcThreadAttributeList(startup.lpAttributeList, 1, 0,
                                           &attributes_size))
        goto done;
    attributes_initialized = 1;
    inherited[0] = writer;
    inherited[1] = null_device;
    if(!UpdateProcThreadAttribute(startup.lpAttributeList, 0,
                                  PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                  inherited, sizeof(inherited), NULL, NULL))
        goto done;
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = null_device;
    startup.StartupInfo.hStdOutput = writer;
    startup.StartupInfo.hStdError = null_device;
    if(!CreateProcessW(g_custom_cli_path ? executable : NULL, command,
                       NULL, NULL, TRUE,
                       EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW,
                       NULL, NULL, &startup.StartupInfo, &process))
        goto done;
    CloseHandle(process.hThread);
    CloseHandle(writer);
    writer = NULL;
    fd = _open_osfhandle((intptr_t)reader, _O_RDONLY | _O_BINARY);
    if(fd >= 0) {
        reader = NULL;
        stream = _fdopen(fd, "rb");
        if(stream == NULL)
            _close(fd);
    }
    if(stream == NULL) {
        if(reader != NULL) {
            CloseHandle(reader);
            reader = NULL;
        }
        wait_for_cli(process.hProcess);
    } else {
        *pid_out = process.hProcess;
    }

done:
    if(attributes_initialized)
        DeleteProcThreadAttributeList(startup.lpAttributeList);
    free(startup.lpAttributeList);
    if(reader != NULL)
        CloseHandle(reader);
    if(writer != NULL)
        CloseHandle(writer);
    if(null_device != INVALID_HANDLE_VALUE)
        CloseHandle(null_device);
    return stream;
}

static int wait_for_cli(cli_process_t pid)
{
    DWORD code = 1;
    int ok = WaitForSingleObject(pid, INFINITE) == WAIT_OBJECT_0 &&
             GetExitCodeProcess(pid, &code) && code == 0;
    CloseHandle(pid);
    return ok ? 0 : -1;
}
#else

static FILE *spawn_easytier_cli(cli_process_t *pid_out)
{
    char rpc_target[160];
    int output_pipe[2];
    pid_t pid;
    FILE *stream;

    snprintf(rpc_target, sizeof(rpc_target), "%s:%u", g_rpc_host, g_rpc_port);
    if(pipe(output_pipe) != 0)
        return NULL;

    pid = fork();
    if(pid < 0) {
        close(output_pipe[0]);
        close(output_pipe[1]);
        return NULL;
    }
    if(pid == 0) {
        char *const argv[] = {
            g_cli_path,
            (char *)"-p",
            rpc_target,
            (char *)"--output",
            (char *)"json",
            (char *)"node",
            NULL,
        };
        int devnull;

        close(output_pipe[0]);
        if(dup2(output_pipe[1], STDOUT_FILENO) < 0)
            _exit(127);
        close(output_pipe[1]);

        devnull = open("/dev/null", O_WRONLY);
        if(devnull >= 0) {
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        if(g_custom_cli_path)
            execv(argv[0], argv);
        else
            execvp(argv[0], argv);
        _exit(127);
    }

    close(output_pipe[1]);
    stream = fdopen(output_pipe[0], "r");
    if(stream == NULL) {
        close(output_pipe[0]);
        wait_for_cli(pid);
        return NULL;
    }
    *pid_out = pid;
    return stream;
}

static int wait_for_cli(cli_process_t pid)
{
    int status;

    while(waitpid(pid, &status, 0) < 0) {
        if(errno != EINTR)
            return -1;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}
#endif

int easytier_rpc_get_endpoint(char *buf, size_t buflen, int *nat_ok)
{
    const size_t output_limit = 1024 * 1024;
    char chunk[4096];
    char *json = NULL;
    size_t length = 0;
    size_t capacity = 0;
    size_t count;
    int read_failed = 0;
    node_json_t node;
    FILE *f;
    cli_process_t pid;
    int cli_status;
    unsigned short port;

    if(nat_ok != NULL)
        *nat_ok = 0;

    f = spawn_easytier_cli(&pid);
    if(f == NULL) {
        if(g_verbose)
            printf("easytier-peerdisc: NAT query failed: cannot start easytier-cli\n");
        return -1;
    }

    while((count = fread(chunk, 1, sizeof(chunk), f)) != 0) {
        char *grown;
        if(read_failed)
            continue;
        if(count > output_limit - length) {
            read_failed = 1;
            continue;
        }
        if(length + count > capacity) {
            size_t next = capacity == 0 ? sizeof(chunk) : capacity * 2;
            grown = realloc(json, next);
            if(grown == NULL) {
                read_failed = 1;
                continue;
            }
            json = grown;
            capacity = next;
        }
        memcpy(json + length, chunk, count);
        length += count;
    }
    read_failed |= ferror(f) != 0;
    fclose(f);
    cli_status = wait_for_cli(pid);
    if(cli_status != 0) {
        free(json);
        if(g_verbose)
            printf("easytier-peerdisc: NAT query failed: easytier-cli exited unsuccessfully\n");
        return -1;
    }
    if(read_failed || length == 0 || node_json_parse(json, length, &node) != 0) {
        free(json);
        if(g_verbose)
            printf("easytier-peerdisc: NAT query failed: invalid, oversized, or unreadable node JSON\n");
        return -1;
    }
    free(json);

    if(g_verbose) {
        printf("easytier-peerdisc: NAT query TCP STUN %s, UDP STUN %s "
               "(CLI does not report per-family NAT types)\n",
               node.tcp_nat[0] != '\0' ? node.tcp_nat : "unavailable",
               node.udp_nat[0] != '\0' ? node.udp_nat : "unavailable");
        printf("easytier-peerdisc: NAT query public ipv4 %s, public ipv6 %s, listener %s\n",
               node.public_ipv4[0] != '\0' ? node.public_ipv4 : "unavailable",
               node.public_ipv6[0] != '\0' ? node.public_ipv6 : "unavailable",
               node.listener[0] != '\0' ? node.listener : "unavailable");
    }

    if(!nat_allows_announce(node.udp_nat))
        return -1;
    if(node.public_ipv4[0] == '\0' && node.public_ipv6[0] == '\0')
        return -1;
    if(node.listener[0] == '\0')
        return -1;
    port = listener_port(node.listener);
    if(port == 0)
        return -1;

    if(node.public_ipv4[0] != '\0')
        snprintf(buf, buflen, "tcp://%s:%u", node.public_ipv4, port);
    else
        snprintf(buf, buflen, "tcp://[%s]:%u", node.public_ipv6, port);

    if(nat_ok != NULL)
        *nat_ok = 1;
    return 0;
}

int easytier_rpc_cleanup(void)
{
    return 0;
}
