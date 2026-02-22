#include "tool.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *tool_run(const char *cmd, const char *input, int len, int *out_len) {
    *out_len = 0;

    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    HANDLE hStdinR, hStdinW, hStdoutR, hStdoutW;

    if (!CreatePipe(&hStdinR, &hStdinW, &sa, 0)) return NULL;
    if (!CreatePipe(&hStdoutR, &hStdoutW, &sa, 0)) {
        CloseHandle(hStdinR); CloseHandle(hStdinW);
        return NULL;
    }
    SetHandleInformation(hStdinW, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStdoutR, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = hStdinR;
    si.hStdOutput = hStdoutW;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION pi = {0};
    char *cmd_buf = _strdup(cmd);
    BOOL ok = CreateProcessA(NULL, cmd_buf, NULL, NULL,
                             TRUE, 0, NULL, NULL, &si, &pi);
    free(cmd_buf);
    CloseHandle(hStdinR);
    CloseHandle(hStdoutW);

    if (!ok) {
        CloseHandle(hStdinW);
        CloseHandle(hStdoutR);
        return NULL;
    }

    /* Write input to child's stdin */
    DWORD written;
    WriteFile(hStdinW, input, (DWORD)len, &written, NULL);
    CloseHandle(hStdinW);

    /* Read child's stdout */
    size_t cap = 4096, used = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) {
        CloseHandle(hStdoutR);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return NULL;
    }

    DWORD nread;
    while (ReadFile(hStdoutR, buf + used, (DWORD)(cap - used - 1), &nread, NULL) && nread > 0) {
        used += nread;
        if (used + 1 >= cap) {
            cap *= 2;
            char *tmp = (char *)realloc(buf, cap);
            if (!tmp) {
                free(buf);
                CloseHandle(hStdoutR);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                return NULL;
            }
            buf = tmp;
        }
    }
    buf[used] = '\0';
    CloseHandle(hStdoutR);

    WaitForSingleObject(pi.hProcess, 5000);
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exit_code != 0) {
        free(buf);
        return NULL;
    }

    *out_len = (int)used;
    return buf;
}
