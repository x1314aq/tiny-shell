#pragma once

#include <Windows.h>

class tstring
{
    static const unsigned stack_size = 56;
#define max_nr_in_stack (stack_size / type_size)
#define type_size sizeof(WCHAR)

public:
    tstring()
    {
        _size = 0;
        _capacity = max_nr_in_stack;
        ZeroMemory(_dummy, stack_size);
    }

    ~tstring()
    {
        if (!in_stack()) {
            delete[] _mem;
        }
    }

    void append(WCHAR c)
    {
        if (_size + 1 < _capacity) {
            if (in_stack()) {
                _content[_size] = c;
            } else {
                _mem[_size] = c;
            }
        } else {
            unsigned new_cap = _capacity * 2;
            WCHAR *new_mem = new WCHAR[new_cap]();
            if (in_stack()) {
                memmove_s(new_mem, type_size * new_cap, _content, type_size * _size);
            } else {
                memmove_s(new_mem, type_size * new_cap, _mem, type_size * _size);
                delete[] _mem;
            }
            _mem = new_mem;
            _capacity = new_cap;
            _mem[_size] = c;
        }
        _size++;
    }

    void append(const WCHAR *t, size_t n)
    {
        if (_size + n < _capacity) {
            if (in_stack()) {
                memcpy_s(&_content[_size], type_size * (_capacity - 1 - _size), t, type_size * n);
            } else {
                memcpy_s(&_mem[_size], type_size * (_capacity - 1 - _size), t, type_size * n);
            }
        } else {
            unsigned new_cap = (_size + n) * 2;
            WCHAR *new_mem = new WCHAR[new_cap]();
            if (in_stack()) {
                memmove_s(new_mem, type_size * new_cap, _content, type_size * _size);
            } else {
                memmove_s(new_mem, type_size * new_cap, _mem, type_size * _size);
                delete[] _mem;
            }
            _mem = new_mem;
            _capacity = new_cap;
            memcpy_s(&_mem[_size], type_size * (_capacity - 1 - _size), t, type_size * n);
        }
        _size += n;
    }

    void append(const WCHAR *t)
    {
        append(t, wcslen(t));
    }

    void clear()
    {
        _size = 0;
        if (in_stack()) {
            ZeroMemory(_content, _countof(_content));
        } else {
            delete[] _mem;
            _capacity = max_nr_in_stack;
        }
    }

    WCHAR *data()
    {
        return in_stack() ? _content : _mem;
    }

    WCHAR *c_str()
    {
        return data();
    }

    void strip()
    {
        WCHAR *c = in_stack() ? _content : _mem;
        size_t n = _size;
        size_t i = 0, j = n - 1;

        while (i < n && iswspace(c[i])) i++;
        while (j > i && iswspace(c[j])) j--;
        _size = j - i + 1;

        memmove_s(c, _capacity * type_size, &c[i], _size * type_size);
        c[_size] = WNULL;
    }

    unsigned size()
    {
        return _size;
    }

    unsigned capacity()
    {
        return _capacity;
    }

    WCHAR operator[](unsigned i)
    {
        if (in_stack()) {
            return _content[i];
        } else {
            return _mem[i];
        }
    }

protected:
    bool in_stack()
    {
        return _capacity == max_nr_in_stack;
    }

private:
    unsigned int _size;
    unsigned int _capacity;
    union {
        char _dummy[stack_size];
        WCHAR _content[stack_size / type_size];
        struct {
            WCHAR *_mem;
        };
    };
};
