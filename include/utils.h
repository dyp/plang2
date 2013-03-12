/// \file utils.h
/// Miscellaneous utility functions.

#ifndef UTILS_H_
#define UTILS_H_

#include <string>
#include <stdint.h>
#include <assert.h>

// Iterator operations.

template<typename Iterator>
Iterator next(Iterator i) { return ++ i; }

template<typename Iterator>
Iterator prev(Iterator i) { return -- i; }

// String operations.

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

#define _WIDEN(_STR)  L ## _STR
#define WIDEN(_STR)   _WIDEN(_STR)

std::string intToStr(int64_t _n);
std::string natToStr(uint64_t _n);

std::string strNarrow(const std::wstring & _s);
std::wstring strWiden(const std::string & _s);

std::wstring fmtQuote(const std::wstring & _s);
std::wstring fmtAddParens(const std::wstring & _s, bool _bAddLeadingSpace = true);
std::wstring fmtAddDetail(const std::wstring & _s);

std::wstring fmtInt(int64_t _n, const wchar_t * _fmt = L"%lld");
std::wstring intToAlpha(int _n);

std::wstring removeRedundantSymbols(const std::wstring &_wstring, const std::wstring &_redundant);


// Command-line args.

typedef bool (*ArgHandler)(const std::string &_val, void *_p);

struct Option {
	const char *strName;
	char chNameShort;
	ArgHandler argParser;
	bool *pbFlag;
	std::string *pstrValue;
	bool bMultiple;
};

bool parseOptions(size_t _cArgs, const char **_pArgs, Option *_pOptions, void *_pParam,
		ArgHandler _notAnOptionHandler);

std::wstring cyrillicToASCII(const std::wstring &_str);

#endif /* UTILS_H_ */
