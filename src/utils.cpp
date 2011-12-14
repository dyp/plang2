/// \file utils.cpp
///

#include <locale>
#include <iostream>

#include <string.h>
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>

#include "utils.h"

std::string intToStr(int64_t _n) {
    char buf[64];
    snprintf(buf, 64, "%lld", (long long int) _n);
    return buf;
}

std::string natToStr(uint64_t _n) {
    char buf[64];
    snprintf(buf, 64, "%llu", (long long int) _n);
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
    char *mbcBuf = (char *)alloca(bufLen);

    char *pNextOut = mbcBuf;
    const wchar_t *pNextIn = _s.c_str();

    memset((void *)&state, 0, sizeof(state));
    memset((void *)mbcBuf, 0, sizeof(bufLen));

    result = facet.out(state,
            _s.c_str(), _s.c_str() + _s.length() + 1, pNextIn,
            (char *)mbcBuf, (char *)mbcBuf + bufLen, pNextOut);

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
    wchar_t *wcBuf = (wchar_t *)alloca(bufLen*sizeof(wchar_t));

    const char * pNextOut = _s.c_str();
    wchar_t * pNextIn = wcBuf;

    memset((void *) & state, 0, sizeof(state));
    memset((void *) wcBuf, 0, sizeof(wchar_t)*bufLen);

    result = facet.in(state,
            _s.c_str(), _s.c_str() + _s.length() + 1, pNextOut,
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

static
bool _parseOption(const std::string &_strValue, Option &_opt, void *_pParam) {
    if (_opt.argParser != NULL) {
        const char *s = _strValue.c_str();

        for (const char *p = s; s < _strValue.c_str() + _strValue.size(); ++p) {
            if (*p != ',' && *p != 0)
                continue;

            std::string strArg(s, p);
            s = p + 1;

            if ((*_opt.argParser)(strArg, _pParam))
                continue;

            if (*_opt.strName != 0)
                std::cerr << "Invalid argument of option '--" << _opt.strName << "': " << strArg << std::endl;
            else
                std::cerr << "Invalid argument of option '-" << _opt.chNameShort << "': " << strArg << std::endl;

            return false;
        }
    }

    if (_opt.pstrValue != NULL)
        *_opt.pstrValue = _strValue;

    return true;
}

bool parseOptions(size_t _cArgs, const char **_pArgs, Option *_pOptions, void *_pParam,
        ArgHandler _notAnOptionHandler)
{
    const char **pArg = _pArgs;
    const char **pEnd = _pArgs + _cArgs;

    while (pArg != pEnd) {
        if ((*pArg)[0] != '-' || (*pArg)[1] == 0) {
            // Not an option.
            if (_notAnOptionHandler)
                (*_notAnOptionHandler)(*pArg, _pParam);
            ++pArg;
            continue;
        }

        int nOpt;

        if ((*pArg)[1] != '-') {
            // Beginning of short options list.
            const char *pchOpt = *pArg + 1;

            while (*pchOpt != 0) {
                for (nOpt = 0; _pOptions[nOpt].strName != NULL; ++nOpt)
                    if (_pOptions[nOpt].chNameShort == *pchOpt)
                        break;

                if (_pOptions[nOpt].strName == NULL) {
                    std::cerr << "Unknown option '-" << *pchOpt << "'" << std::endl;
                    return false;
                }

                Option &opt = _pOptions[nOpt];

                ++pchOpt;

                if (opt.pbFlag != NULL)
                    *opt.pbFlag = true;

                if (opt.argParser == NULL && opt.pstrValue == NULL)
                    continue; // No argument required.

                const char *strValue = NULL;

                if (*pchOpt == 0) {
                    // Next arg is this option's value.
                    ++pArg;
                    if (pArg == pEnd) {
                        std::cerr << "Option '-" << opt.chNameShort << "' requires an argument" << std::endl;
                        return false;
                    }

                    strValue = *pArg;
                } else
                    strValue = pchOpt;

                if (!_parseOption(strValue, opt, _pParam))
                    return false;

                break;
            }

            ++pArg;
            continue;
        }

        const char *strValue;

        for (nOpt = 0; _pOptions[nOpt].strName != NULL; ++nOpt) {
            const std::string &strOpt = _pOptions[nOpt].strName;

            if (strlen(*pArg + 2) < strOpt.size() || memcmp(*pArg + 2, strOpt.c_str(), strOpt.size()) != 0)
                continue;

            strValue = *pArg + 2 + strOpt.size();

            if (*strValue == 0 || *strValue == '=')
                break;
        }

        Option &opt = _pOptions[nOpt];

        if (opt.strName == NULL) {
            std::cerr << "Unknown option '" << *pArg << "'" << std::endl;
            return false;
        }

        const bool bHasOption = opt.argParser != NULL || opt.pstrValue != NULL;

        if (*strValue == '=' && !bHasOption) {
            std::cerr << "Unexpected argument of option '" << *pArg << "'" << std::endl;
            return false;
        }

        if (opt.pbFlag != NULL)
            *opt.pbFlag = true;

        if (bHasOption) {
            if (*strValue == 0) {
                std::cerr << "Option '" << *pArg << "' requires an argument" << std::endl;
                return false;
            }

            if (!_parseOption(strValue + 1, opt, _pParam))
                return false;
        }

        ++pArg;
    }

    return true;
}

std::wstring cyrillicToASCII(const std::wstring &_str) {
    std::wstring sRussia = L"абвгдеёжзийклмнопрстуфцчшщъыьэюя";
    std::wstring sTranslit[] = { L"a", L"b", L"v", L"g", L"d", L"e", L"jo", L"zh", L"z", L"i", L"j",
                                 L"k", L"l", L"m", L"n", L"o", L"p", L"r", L"s", L"t", L"u", L"f",
                                 L"x", L"cz", L"ch", L"sh", L"shh", L"\"", L"y'", L"'", L"e'", L"ju", L"ja" };
    std::wstring sResult = L"";

    for (int i=0; i<_str.size(); ++i) {
        const wchar_t symbol = towlower(_str[i]);
        const wchar_t *c = std::lower_bound(sRussia.c_str(), sRussia.c_str() + sRussia.size(), symbol);
        if (*c != symbol) {
            sResult += _str[i];
            continue;
        }
        const int ind  = c - sRussia.c_str();
        if (_str[i] != symbol) {
            std::wstring tmp = sTranslit[ind];
            transform(tmp.begin(), tmp.end(), tmp.begin(), towupper);
            sResult += tmp;
        }
        else
            sResult += sTranslit[ind];
    }

    return sResult;
}
