// main.cpp

#include <string>
#include <vector>

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <clocale>

#include <Windows.h>
#include <processthreadsapi.h>
#include <io.h>
#include <fcntl.h>

#include "win_getopt.h"
#include "builtin.h"
#include "container.h"

using namespace std;

struct execunit {
    tstring str;
    HANDLE h_stdin;
    HANDLE h_stdout;
    HANDLE h_stderr;
    PROCESS_INFORMATION pi;
    bool is_bg_task;
    bool use_std_handles;
    bool is_builtin;

    execunit()
    {
        str = tstring();
        h_stdin = nullptr;
        h_stdout = nullptr;
        h_stderr = nullptr;
        is_bg_task = false;
        use_std_handles = false;
        is_builtin = false;
        ZeroMemory(&pi, sizeof(pi));
    }

    ~execunit()
    {
        if (h_stdin) {
            CloseHandle(h_stdin);
        }
        if (h_stdout) {
            CloseHandle(h_stdout);
        }
        if (h_stderr) {
            CloseHandle(h_stderr);
        }
        is_bg_task = false;
        use_std_handles = false;
        is_builtin = false;
    }

    WCHAR *get_cmdline()
    {
        return str.data();
    }
};

static WCHAR g_config[256];

const static struct option g_long_opts[] = {
    {L"config", required_argument, 0, L'f'},
    {L"help", no_argument, 0, L'h'},
    {L"version", no_argument, 0, L'v'},
    {0, 0, 0, 0},
};

static inline void create_process(execunit &u)
{
    STARTUPINFOW si;
    BOOL err;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    if (u.use_std_handles) {
        si.hStdInput = u.h_stdin ? u.h_stdin : GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = u.h_stdout ? u.h_stdout : GetStdHandle(STD_OUTPUT_HANDLE);
        si.hStdError = u.h_stderr ? u.h_stderr : GetStdHandle(STD_ERROR_HANDLE);
        si.dwFlags |= STARTF_USESTDHANDLES;
    }

    err = CreateProcessW(nullptr,
                         u.str.data(),
                         nullptr,
                         nullptr,
                         u.use_std_handles ? TRUE : FALSE,
                         0,
                         nullptr,
                         nullptr,
                         &si,
                         &u.pi);

    if (u.use_std_handles) {
        if (u.h_stdin) {
            CloseHandle(u.h_stdin);
            u.h_stdin = nullptr;
        }
        if (u.h_stdout) {
            CloseHandle(u.h_stdout);
            u.h_stdout = nullptr;
        }
        if (u.h_stderr) {
            CloseHandle(u.h_stderr);
            u.h_stderr = nullptr;
        }
    }

    if (err == FALSE) {
        wprintf(L"%s failed %d\n", u.str.data(), GetLastError());
        return;
    }
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
    size_t i = 0;

    while (i < n) {
        v.push_back(&line[i]);
        while (i < n && !iswspace(line[i])) i++;
        while (i < n && iswspace(line[i])) line[i++] = WNULL;
    }

    return v;
}

static void set_stdhandles(HANDLE in, HANDLE out, HANDLE err, bool restore)
{
    int fd;
    SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};

    if (in) {
        _close(_fileno(stdin));
        if (restore) {
            in = CreateFileW(L"CONIN$", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
        }
        SetStdHandle(STD_INPUT_HANDLE, in);
        fd = _open_osfhandle((intptr_t)in, _O_RDONLY | _O_WTEXT);
        _dup2(fd, _fileno(stdin));
    }

    if (out) {
        _close(_fileno(stdout));
        if (restore) {
            out = CreateFileW(L"CONOUT$", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
        }
        SetStdHandle(STD_OUTPUT_HANDLE, out);
        fd = _open_osfhandle((intptr_t)out, _O_WRONLY | _O_WTEXT);
        _dup2(fd, _fileno(stdout));
    }

    if (err) {
        _close(_fileno(stderr));
        if (restore) {
            err = CreateFileW(L"CONOUT$", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
        }
        SetStdHandle(STD_ERROR_HANDLE, err);
        fd = _open_osfhandle((intptr_t)err, _O_WRONLY | _O_WTEXT);
        _dup2(fd, _fileno(stderr));
    }
}

static inline void do_execute(execunit &u)
{
    WCHAR *in = u.str.data();
    const struct command *cmd = is_builtin(in);

    if (cmd) {
        vector<WCHAR *> v = split(in);
        u.is_builtin = true;
        if (u.use_std_handles) {
            set_stdhandles(u.h_stdin, u.h_stdout, u.h_stderr, false);
        }
        cmd->handler(v);
        if (u.use_std_handles) {
            set_stdhandles(u.h_stdin, u.h_stdout, u.h_stderr, true);
            // these handles are already close in set_stdhandles()
            u.h_stdin = nullptr;
            u.h_stdout = nullptr;
            u.h_stderr = nullptr;
        }
        return;
    }

    create_process(u);
}

static inline unsigned parse_fname(WCHAR *dest, WCHAR *c)
{
    unsigned i = 0, j = 0;

    while (c[i] != WNULL && iswspace(c[i])) i++;
    while (c[i + j] != WNULL && !iswspace(c[i + j])) j++;

    memcpy_s(dest, MAX_PATH * sizeof(WCHAR), &c[i], j * sizeof(WCHAR));
    dest[j] = WNULL;

    return i + j;
}

static int process_pipe(execunit &p, execunit &c)
{
    SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};

    if (CreatePipe(&c.h_stdin, &p.h_stdout, &sa, 0) == FALSE) {
        wprintf(L"internal error %d\n", GetLastError());
        return -1;
    }

    p.use_std_handles = true;
    c.use_std_handles = true;
    return 0;
}

static void wait_all_process(vector<execunit> &v)
{
    DWORD err;
    size_t n = v.size();
    HANDLE *procs = (HANDLE *)_malloca(n * sizeof(HANDLE));
    DWORD k = 0;

    if (!procs) {
        wprintf(L"_malloca failed\n");
        return;
    }

    for (size_t i = 0; i < n; i++) {
        if (!v[i].is_builtin) {
            procs[k++] = v[i].pi.hProcess;
        }
    }

    if (k == 0) {
        _freea(procs);
        return;
    }

    err = WaitForMultipleObjects(k, procs, TRUE, INFINITE);
    if (err == WAIT_FAILED) {
        wprintf(L"WaitForMultipleObjects failed %d\n", GetLastError());
    }

    for (auto &u : v) {
        CloseHandle(u.pi.hProcess);
        CloseHandle(u.pi.hThread);
    }
    _freea(procs);
}

static void execute(WCHAR *input)
{
    vector<execunit> v(1);
    execunit *unit = &v[0];
    size_t n;
    WCHAR *c = input;
    WCHAR dest[MAX_PATH];
    SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};

    while (*c != WNULL) {
        switch (*c) {
        case L'|':
            v.push_back(execunit());
            n = v.size();
             if (process_pipe(v[n - 2], v[n - 1])) {
                return;
            }
            unit = &v[n - 1];
            c++;
            break;
        case L'\\':
            unit->str.append(*(c + 1));
            c += 2;
            break;
        case L'<':
            c++;
            c += parse_fname(dest, c);
            unit->h_stdin = CreateFileW(dest, GENERIC_READ, FILE_SHARE_READ, &sa, OPEN_EXISTING,
                                       FILE_ATTRIBUTE_READONLY | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
            if (unit->h_stdin == INVALID_HANDLE_VALUE) {
                wprintf(L"cannot open %s (error %d)\n", dest, GetLastError());
                return;
            }
            unit->use_std_handles = true;
            break;
        case L'>':
            c++;
            c += parse_fname(dest, c);
            unit->h_stdout = CreateFileW(dest, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (unit->h_stdout == INVALID_HANDLE_VALUE) {
                wprintf(L"cannot open %s (error %d)\n", dest, GetLastError());
                return;
            }
            unit->use_std_handles = true;
            break;
        case L'2':
            if (*(c + 1) != L'>') {
                unit->str.append(*c);
                c++;
                break;
            }
            c += 2;
            c += parse_fname(dest, c);
            unit->h_stderr = CreateFileW(dest, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (unit->h_stderr == INVALID_HANDLE_VALUE) {
                wprintf(L"cannot open %s (error %d)\n", dest, GetLastError());
                return;
            }
            unit->use_std_handles = true;
            break;
        case L'&':
            unit->is_bg_task = true;
            c++;
            break;
        default:
            unit->str.append(*c);
            c++;
            break;
        }
    }

    for (execunit &u : v) {
        u.str.strip();
        do_execute(u);
    }

    wait_all_process(v);
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

    _wsetlocale(LC_ALL, L".utf8");
    wprintf(L"Console CP is %u\n", GetConsoleCP());
    wprintf(L"Set Console CP to UTF-8 (65001) %d\n", SetConsoleCP(65001));
    wprintf(L"Console CP is %u\n", GetConsoleCP());

    parse_args(argc, argv);
    if (wcslen(g_config)) {
        parse_config();
    }

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