#pragma once

#include <vector>
#include <algorithm>
#include <string>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <Windows.h>

#define WNULL L'\0'

using handler_t = int (*)(std::vector<WCHAR *> &);

struct command {
    WCHAR cmd[8];
    handler_t handler;
};

const struct command *is_builtin(WCHAR *cmd);