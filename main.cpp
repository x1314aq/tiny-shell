// main.cpp

#include <string>
#include <algorithm>
#include <vector>
#include <map>

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <clocale>

#include <Windows.h>
#include <processthreadsapi.h>

#include "win_getopt.h"

#define WNULL L'\0'
#define LSFMT L"%-10s%-20s%10s  %-30s\n"

using namespace std;

using handler_t = int (*)(vector<WCHAR *> &);

struct command {
    WCHAR cmd[8];
    handler_t handler;
};

static WCHAR g_config[256];

const static struct option g_long_opts[] = {
    {L"config", required_argument, 0, L'f'},
    {L"help", no_argument, 0, L'h'},
    {L"version", no_argument, 0, L'v'},
    {0, 0, 0, 0},
};

static int do_builtin_cd(vector<WCHAR *> &args);
static int do_builtin_pwd(vector<WCHAR *> &args);
static int do_builtin_ls(vector<WCHAR *> &args);
static int do_builtin_exit(vector<WCHAR *> &args);

const static struct command g_builtin[] = {
    {L"cd", do_builtin_cd},
    {L"pwd", do_builtin_pwd},
    {L"ls", do_builtin_ls},
    {L"exit", do_builtin_exit},
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

static inline const struct command *is_builtin(WCHAR *cmd)
{
    for (int i = 0; i < ARRAYSIZE(g_builtin); i++) {
        if (wcscmp(cmd, g_builtin[i].cmd) == 0) {
            return &g_builtin[i];
        }
    }

    return nullptr;
}

static int do_builtin_cd(vector<WCHAR *> &args)
{
    WCHAR *dest = nullptr;

    if (args.size() == 1) {
        return 0;
    }

    dest = args[1];

    return SetCurrentDirectoryW(dest) == TRUE ? 0 : 1;
}

static int do_builtin_pwd(vector<WCHAR *> &args)
{
    WCHAR cwd[MAX_PATH];
    DWORD ret = GetCurrentDirectoryW(_countof(cwd), cwd);

    cwd[ret] = WNULL;
    wprintf(L"%s\n", cwd);
    return 0;
}

static inline void get_lwt(FILETIME time, WCHAR *buf, size_t len)
{
    SYSTEMTIME utc, local;

    FileTimeToSystemTime(&time, &utc);
    SystemTimeToTzSpecificLocalTime(nullptr, &utc, &local);

    swprintf_s(buf, len, L"%02d/%02d/%d %02d:%02d:%02d", local.wMonth, local.wDay, local.wYear,
        local.wHour, local.wMinute, local.wSecond);
}

static int do_builtin_ls(vector<WCHAR *> &args)
{
    HANDLE find = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATAW data;
    WCHAR dest[MAX_PATH] = L"*";

    if (args.size() >= 2) {
        ZeroMemory(dest, _countof(dest));
        swprintf_s(dest, _countof(dest), L"%s\\*", args[1]);
    }

    find = FindFirstFileW(dest, &data);
    if (find == INVALID_HANDLE_VALUE) {
        wprintf(L"internal error %d\n", GetLastError());
        return 1;
    }

    wprintf(LSFMT, L"Mode", L"Last Write Time", L"Size", L"Name");
    wprintf(LSFMT, L"----", L"---------------", L"----", L"----");
    do {
        WCHAR mode[10], lwt[20], length[20];
        ZeroMemory(lwt, _countof(mode));
        ZeroMemory(length, _countof(mode));

        wcscpy_s(mode, _countof(mode), L"----");
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            mode[0] = L'd';
        } else if (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
            mode[0] = L'l';
        } else if (data.dwFileAttributes & FILE_ATTRIBUTE_NORMAL) {
            mode[0] = L'f';
        }

        get_lwt(data.ftLastWriteTime, lwt, _countof(lwt));
        swprintf_s(length, _countof(length), L"%lu", data.nFileSizeHigh * (MAXDWORD + 1) + data.nFileSizeLow);

        wprintf(LSFMT, mode, lwt, length, data.cFileName);
    } while (FindNextFileW(find, &data) != 0);

    if (GetLastError() != ERROR_NO_MORE_FILES) {
        wprintf(L"internal error %d\n", GetLastError());
        FindClose(find);
        return 1;
    }

    FindClose(find);
    return 0;
}

static int do_builtin_exit(vector<WCHAR *> &args)
{
    exit(0);
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