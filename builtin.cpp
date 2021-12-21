#include "builtin.h"

using namespace std;

#define LSFMT L"%-10s%-20s%10s  %-30s\n"

static bool vcmp(const WCHAR * a, const WCHAR * b)
{
    if (a[0] == L'-') {
        if (a[1] != L'-') {
            return true;
        } else {
            if (b[0] == L'-' && b[1] != L'-') {
                return false;
            } else {
                return true;
            }
        }
    }

    return b[0] != L'-' ? wcscmp(a, b) < 0 : false;
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
        DWORD err = GetLastError();
        if (err == ERROR_PATH_NOT_FOUND || err == ERROR_FILE_NOT_FOUND) {
            wprintf(L"%s: No such file or directory\n", args[1]);
        } else {
            wprintf(L"internal error %d\n", err);
        }
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

[[noreturn]] static int do_builtin_exit(vector<WCHAR *> &args)
{
    exit(0);
}

static void recursively_remove(WCHAR *dir)
{
    HANDLE find = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATAW data;
    WCHAR dest[MAX_PATH];
    size_t n = wcslen(dir);

    ZeroMemory(dest, _countof(dest));
    wcscpy_s(dest, dir);
    wcscat_s(dest, L"\\*");

    find = FindFirstFileW(dest, &data);
    if (find == INVALID_HANDLE_VALUE) {
        wprintf(L"internal error %d\n", GetLastError());
        return;
    }

    do {
        wcscpy_s(&dest[n + 1], MAX_PATH - n - 1, data.cFileName);
        if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0) {
            continue;
        }
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            recursively_remove(dest);
        } else {
            DeleteFileW(dest);
        }
    } while (FindNextFileW(find, &data) != 0);

    if (GetLastError() != ERROR_NO_MORE_FILES) { 
        wprintf(L"internal error %d\n", GetLastError());
    }

    FindClose(find);
    RemoveDirectoryW(dir);
}

static int do_builtin_rm(vector<WCHAR *> &args)
{
    size_t n = args.size();
    bool force = false, recurs = false;
    size_t i;

    sort(args.begin() + 1, args.end(), vcmp);

    for (i = 1; i < n; i++) {
        if (args[i][0] != L'-') {
            break;
        }
        if (args[i][0] == L'-') {
            WCHAR *p = args[i] + 1;
            while (*p != WNULL) {
                switch (*p) {
                case L'f':
                    force = true;
                    break;
                case L'r':
                    recurs = true;
                    break;
                default:
                    wprintf(L"unknown option %c\n", *p);
                    return 1;
                }
                p++;
            }
        }
    }

    if (i == n) {
        wprintf(L"rm: missing operands\n");
        return 1;
    }

    for (; i < n; i++) {
        WCHAR *c = args[i];
        DWORD attr = GetFileAttributesW(c);
        if (attr == INVALID_FILE_ATTRIBUTES) {
            if (!force) {
                wprintf(L"rm: cannot remove '%s' (error %d)\n", c, GetLastError());
                return 1;
            }
        }
        if (attr & FILE_ATTRIBUTE_DIRECTORY) {
            if (recurs) {
                recursively_remove(c);
            } else {
                wprintf(L"rm: cannot remove '%s' Is a directory\n", c);
                return 1;
            }
        } else {
            if (DeleteFileW(c) == FALSE && !force) {
                wprintf(L"rm: cannot remove '%s' (error %d)\n", c, GetLastError());
                return 1;
            }
        }
    }

    return 0;
}

static int do_builtin_mkdir(vector<WCHAR *> &args)
{
    size_t n = args.size();

    if (n == 1) {
        wprintf(L"mkdir: missing operand\n");
        return 1;
    }

    for (size_t i = 1; i < n; i++) {
        if (CreateDirectoryW(args[i], nullptr) == FALSE) {
            wprintf(L"mkdir: cannot create %s (error %d)\n", args[i], GetLastError());
            return 1;
        }
    }

    return 0;
}

static inline int do_cat_one(WCHAR *file)
{
    FILE *fp;
    int err = _wfopen_s(&fp, file, L"rtS, ccs=UNICODE");
    WCHAR buf[512];
    int count = 0;

    if (err) {
        return err;
    }

    wprintf(L"%s\n-------\n", file);
    while (fgetws(buf, _countof(buf), fp) != nullptr) {
        wprintf(L"%6d  %s", ++count, buf);
    }
    fputwc(L'\n', stdout);
    return 0;
}

static int do_builtin_cat(vector<WCHAR *> &args)
{
    size_t n = args.size();
    int err;

    if (n == 1) {
        wprintf(L"cat: missing operand\n");
        return 1;
    }

    for (size_t i = 1; i < n; i++) {
        err = do_cat_one(args[i]);
        if (err) {
            wprintf(L"cat: file %s (error %d)\n", args[i], err);
            return 1;
        }
    }

    return 0;
}

static int do_builtin_mv(vector<WCHAR *> &args)
{
    BOOL err;
    DWORD flag = 0;
    size_t n = args.size();
    size_t i;
    WCHAR *src = nullptr;
    WCHAR *dest = nullptr;

    for (i = 1; i < n; i++) {
        if (args[i][0] != L'-') {
            if (src && dest) {
                wprintf(L"mv: more than one destination provided\n");
                return 1;
            }
            if (!src) {
                src = args[i];
            } else {
                dest = args[i];
            }
            continue;
        }
        WCHAR *p = args[i] + 1;
        while (*p != WNULL) {
            switch (*p) {
            case L'f':
                flag |= MOVEFILE_REPLACE_EXISTING;
                break;
            default:
                wprintf(L"unknown option %c\n", *p);
                return 1;
            }
            p++;
        }
    }

    if (!src || !dest) {
        wprintf(L"mv: missing operands\n");
        return 1;
    }

    err = MoveFileExW(src, dest, flag);
    if (err == FALSE) {
        wprintf(L"mv: %s -> %s failed (error %d)\n", src, dest, GetLastError());
        return 1;
    }

    return 0;
}

static int do_builtin_cp(vector<WCHAR *> &args)
{
    BOOL err;
    size_t n = args.size();
    size_t i;
    WCHAR *src = nullptr;
    WCHAR *dest = nullptr;
    BOOL force = FALSE;

    for (i = 1; i < n; i++) {
        if (args[i][0] != L'-') {
            if (src && dest) {
                wprintf(L"cp: more than one destination provided\n");
                return 1;
            }
            if (!src) {
                src = args[i];
            } else {
                dest = args[i];
            }
            continue;
        }
        WCHAR *p = args[i] + 1;
        while (*p != WNULL) {
            switch (*p) {
            case L'f':
                force = TRUE;
                break;
            default:
                wprintf(L"unknown option %c\n", *p);
                return 1;
            }
            p++;
        }
    }

    if (!src || !dest) {
        wprintf(L"cp: missing operands\n");
        return 1;
    }

    err = CopyFileW(src, dest, force);
    if (err == FALSE) {
        wprintf(L"cp: %s -> %s failed (error %d)\n", src, dest, GetLastError());
        return 1;
    }

    return 0;
}

const static struct command g_builtin[] = {
    {L"cd", do_builtin_cd},
    {L"pwd", do_builtin_pwd},
    {L"ls", do_builtin_ls},
    {L"exit", do_builtin_exit},
    {L"rm", do_builtin_rm},
    {L"mkdir", do_builtin_mkdir},
    {L"cat", do_builtin_cat},
    {L"mv", do_builtin_mv},
    {L"cp", do_builtin_cp},
};

const struct command *is_builtin(WCHAR *cmd)
{
    for (int i = 0; i < ARRAYSIZE(g_builtin); i++) {
        if (wcscmp(cmd, g_builtin[i].cmd) == 0) {
            return &g_builtin[i];
        }
    }

    return nullptr;
}