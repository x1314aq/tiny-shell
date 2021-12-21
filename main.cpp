// main.cpp

#include <string>
#include <vector>

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <clocale>

#include <Windows.h>
#include <processthreadsapi.h>

#include "win_getopt.h"
#include "builtin.h"

using namespace std;

static WCHAR g_config[256];

const static struct option g_long_opts[] = {
    {L"config", required_argument, 0, L'f'},
    {L"help", no_argument, 0, L'h'},
    {L"version", no_argument, 0, L'v'},
    {0, 0, 0, 0},
};

static inline void create_process(LPWSTR cmdline)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    BOOL err;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    err = CreateProcessW(nullptr,
                         cmdline,
                         nullptr,
                         nullptr,
                         FALSE,
                         CREATE_SUSPENDED,
                         nullptr,
                         nullptr,
                         &si,
                         &pi);

    if (err == FALSE) {
        wprintf(L"%s failed %d\n", cmdline, GetLastError());
        return;
    }

    ResumeThread(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

static inline void create_process(vector<WCHAR *> &v)
{
    WCHAR cmdline[1024];
    size_t i = 0;

    for (size_t j = 0; j < v.size(); j++) {
        wcscpy_s(&cmdline[i], _countof(cmdline), v[j]);
        i += wcslen(v[j]);
        cmdline[i++] = L' ';
    }
    cmdline[--i] = WNULL;

    create_process(cmdline);
}

static inline WCHAR *strip(WCHAR *line)
{
    size_t n = wcslen(line);
    size_t i = 0, j = n - 1;

    while (i < n && iswspace(line[i])) i++;
    while (j > i && iswspace(line[j])) j--;
    line[++j] = WNULL;

    return &line[i];
}

static inline vector<WCHAR *> split(WCHAR *line)
{
    vector<WCHAR *> v;
    size_t n = wcslen(line);
    size_t i = 0, j = n - 1;

    while (i < n) {
        v.push_back(&line[i]);
        while (i < n && !iswspace(line[i])) i++;
        while (i < n && iswspace(line[i])) line[i++] = WNULL;
    }

    return v;
}

static inline void execute(WCHAR *input)
{
    vector<WCHAR *> v = split(input);
    const struct command *cmd = is_builtin(v[0]);

    if (cmd) {
        cmd->handler(v);
        return;
    }

    create_process(v);
}

static void parse_args(int argc, WCHAR *argv[])
{
    int option;
    int opt_index = 0;

    while ((option = getoptW_long(argc, argv, L"f:hv", g_long_opts, &opt_index)) != -1) {
        switch (option) {
        case L'f':
            wcscpy_s(g_config, optarg);
            break;
        case L'h':
        case L'v':
        default:
            break;
        }
    }
}

static void parse_config()
{
    HANDLE fp = CreateFileW(g_config, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_READONLY | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);

    if (fp == INVALID_HANDLE_VALUE) {
        wprintf(L"open config file \"%s\" failed %d\n", g_config, GetLastError());
        exit(1);
    }

    while (true) {
        // TODO
        break;
    }

    CloseHandle(fp);
}

int wmain(int argc, WCHAR *argv[])
{
    WCHAR buf[1024];
    WCHAR *line;

    _wsetlocale(LC_ALL, L"");
    parse_args(argc, argv);
    parse_config();

    while (true) {
        ZeroMemory(buf, sizeof(buf));
        fputws(L"$> ", stdout);
        _getws_s(buf, _countof(buf));

        line = strip(buf);
        if (wcslen(line) == 0) {
            continue;
        }

        execute(line);
    }

    return 0;
}