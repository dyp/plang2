/// \file utils.cpp
///

#include <locale>

#include <string.h>
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils.h"

std::string intToStr(int64_t _n) {
    char buf[64];
    snprintf(buf, 64, "%lld", (long long int) _n);
    return buf;
}

std::wstring fmtInt(int64_t _n, const wchar_t * _fmt) {
    wchar_t buf[128];
    swprintf(buf, 128, _fmt, _n);
    return buf;
}

std::wstring intToWStrHex(int64_t _n) {
    wchar_t buf[64];
    swprintf(buf, 64, L"%llX", _n);
    return buf;
}

std::string strNarrow(const std::wstring & _s) {
    typedef std::codecvt<wchar_t, char, std::mbstate_t> wc2mbcs_t;
    std::locale loc("");
    std::mbstate_t state;
    const wc2mbcs_t & facet = std::use_facet<wc2mbcs_t>(loc);
    wc2mbcs_t::result result;
    int mbcLen = facet.max_length();
    const int bufLen = (_s.length() + 1)*mbcLen;
    char mbcBuf[bufLen];

    char * pNextOut = mbcBuf;
    const wchar_t * pNextIn = _s.c_str();

    memset((void *) & state, 0, sizeof(state));
    memset((void *) mbcBuf, 0, sizeof(bufLen));

    result = facet.out(state,
            _s.c_str(), _s.c_str() + _s.length() + 2, pNextIn,
            (char *) mbcBuf, (char *) mbcBuf + bufLen, pNextOut);

    return mbcBuf;
}

std::wstring strWiden(const std::string & _s) {
    typedef std::codecvt<wchar_t, char, std::mbstate_t> wc2mbcs_t;
    std::locale loc("");
    std::mbstate_t state;
    const wc2mbcs_t & facet = std::use_facet<wc2mbcs_t>(loc);
    wc2mbcs_t::result result;
//    int wcLen = facet.max_length();
    const int bufLen = _s.length() + 1;
    wchar_t wcBuf[bufLen];

    const char * pNextOut = _s.c_str();
    wchar_t * pNextIn = wcBuf;

    memset((void *) & state, 0, sizeof(state));
    memset((void *) wcBuf, 0, sizeof(wchar_t)*bufLen);

    result = facet.in(state,
            _s.c_str(), _s.c_str() + _s.length() + 2, pNextOut,
            (wchar_t *) wcBuf, (wchar_t *) wcBuf + bufLen, pNextIn);

    return wcBuf;
}

std::wstring fmtQuote(const std::wstring & _s) {
    typedef std::ctype<wchar_t> wctype_t;
    std::locale loc("");
    const wctype_t & ctype = std::use_facet<wctype_t>(loc);
    std::wstring res;

    res += '\"';

    for (size_t i = 0; i < _s.length(); ++ i) {
        if (ctype.is(wctype_t::print, _s[i]))
            res += _s[i];
        else
            res += std::wstring(L"\\x") + intToWStrHex(_s[i]);
    }

    res += '\"';

    return res;
}

std::wstring fmtAddParens(const std::wstring & _s, bool _bAddLeadingSpace) {
    if (_s.empty())
        return _s;
    return (_bAddLeadingSpace ? std::wstring(L" (") : std::wstring(L"(")) +
        _s + std::wstring(L")");
}

std::wstring fmtAddDetail(const std::wstring & _s) {
    if (_s.empty())
        return std::wstring(L"\n");
    return std::wstring(L":\n") + _s;
}

/*
class Foo {
public:
    Foo() {
        printf("ctor\n");
    }

    ~Foo() {
        printf("dtor\n");
    }

    void * operator new (size_t _size, int _i) {
        printf("_size = %d, i = %d\n", _size, _i);
        return malloc(_size);
    }

    void * operator new (size_t _size) {
        printf("_size = %d\n", _size);
        return malloc(_size);
    }
};

void bar() {
    Auto<Foo> foo;
    Auto<Foo> foo2 = foo;
    foo = foo2;
    foo = new Foo;
}

int main(int argc, char ** argv) {
    bar();
}

*/
