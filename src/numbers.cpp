/// \file numbers.cpp
///

#include "ir/numbers.h"
#include "utils.h"

#include <iostream>

#include <stdlib.h>
#include <float.h>

#ifdef _MSC_VER
#include <limits>

#define INFINITY std::numeric_limits<float>::infinity()
#define NAN std::numeric_limits<float>::quiet_NaN()
#define isfinite(_X) (_finite(_X))

#define snprintf _snprintf

wchar_t *wcschrnul(wchar_t *_s, wchar_t _wc) {
	wchar_t *s = wcschr(_s, _wc);
	return s ? s : _s + wcslen(_s);
}
#endif

/// Default constructor initializes number as integer zero.
Number::Number()
    : m_fSpecial(0)
{
}

/// Initialize with string. Appropriate kind will be automatically determined.
Number::Number(const Number & _other) {
    (* this) = _other;
}

static
mpq_class _floatToRational(const std::string &_s) {
    mpq_class q;
    const char *s = _s.c_str();

    // '-' k '.' l 'E' '-' m
    // [+-]?[0-9]*(\.[0-9]+)?([eE][+-]?[0-9]+)?

    bool bPositive = true, bExpPositive = true;
    std::string k = "0", l = "0", ld = "1", m = "0";

    if (*s == '+' || *s == '-') {
        bPositive = *s != '-';
        ++s;
    }

    int n = strspn(s, "0123456789");

    if (n > 0) {
        k = std::string(s, n);
        s += n;
    }

    if (*s == '.') {
        ++s;
        n = strspn(s, "0123456789");

        if (n > 0) {
            l = std::string(s, n);
            s += n;

            for (size_t i = 0; i < n; ++i)
                ld += "0";
        }

    }

    if (*s == 'e' || *s == 'E') {
        ++s;

        if (*s == '+' || *s == '-') {
            bExpPositive = *s != '-';
            ++s;
        }

        m = s; // Up to the end.
    }

    // q = k + l/ld     (without the exponent.)
    q = mpq_class(k, 10) + (mpq_class(l, 10)/mpq_class(ld, 10));

    if (!bPositive)
        q *= -1;

    // q *= 10**m       (add exponent.)
    for (int i = 0; i < atoi(m.c_str()); ++i)
        if (bExpPositive)
            q *= 10;
        else
            q /= 10;

    return q;
}

void Number::_init(const std::string & _s, Number::Format _fmt) {
    if (_fmt == REAL) {
        m_fSpecial = 1;
        m_qValue = 0;

        if (_s == "+inf" || _s == "inf")
            m_fSpecial = INFINITY;
        else if (_s == "-inf")
            m_fSpecial = -INFINITY;
        else if (_s == "nan")
            m_fSpecial = NAN;
        else
            m_qValue = _floatToRational(_s);
    } else {
        m_fSpecial = 0;
        m_qValue.set_str(_s.c_str(), 0);
    }
}

/// Initialize with string.
Number::Number(const std::wstring &_s, Number::Format _fmt) {
    _init(strNarrow(_s.c_str()), _fmt);
}

/// Initialize with string.
Number::Number(const std::string &_s, Number::Format _fmt) {
    _init(_s, _fmt);
}

Number & Number::operator =(const Number & _other) {
    m_fSpecial = _other.m_fSpecial;
    m_qValue = _other.m_qValue;
    return * this;
}

Number::~Number() {
}

const Number Number::m_nan(L"nan", Number::REAL);
const Number Number::m_inf(L"inf", Number::REAL);
const Number Number::m_infNeg(L"-inf", Number::REAL);

Number Number::makeInt(int64_t _n) {
    // GMP doesn't like 64bit integers, have to use strings.
    return Number(intToStr(_n), Number::INTEGER);
}

Number Number::makeNat(uint64_t _n) {
    return Number(natToStr(_n), Number::INTEGER);
}

Number Number::makeReal(long double _f) {
    if (!isfinite(_f)) {
        Number n;
        n.m_fSpecial = _f;
        return n;
    }

    char s[128];
    snprintf(s, 128, "%.*llf", LDBL_DIG, _f);
    return Number(s, Number::REAL);
}

bool Number::isInt() const {
    return m_fSpecial == 0 && m_qValue.get_den() == 1;
}

bool Number::isNat() const {
    return m_fSpecial == 0 && m_qValue.get_den() == 1 && m_qValue >= 0;
}

bool Number::isNeg() const {
    return m_fSpecial < 0 || m_qValue < 0;
}

bool Number::isReal() const {
    return m_fSpecial != 0 || (m_qValue != 0 && m_qValue.get_den() != 1);
}

std::wstring Number::toString() const {
    if (isInt())
        return strWiden(m_qValue.get_num().get_str(10));

    const size_t sz = 128;
    wchar_t s[sz];

    *s = 0;

    if (!isfinite(m_fSpecial)) {
        swprintf(s, sz, L"%llf", m_fSpecial);
        return s;
    }

    switch (countBits()) {
        case GENERIC: return strWiden(m_qValue.get_str());
        case SINGLE:  swprintf(s, sz, L"%.*f", FLT_DIG, (float) m_qValue.get_d()); break;
        case DOUBLE:  swprintf(s, sz, L"%.*f", DBL_DIG, (double) m_qValue.get_d()); break;
        case QUAD:    swprintf(s, sz, L"%.*llf", LDBL_DIG, (long double) m_qValue.get_d()); break;
    }

    wchar_t *cs = wcschrnul(s, L'.');

    if (*cs != 0 && cs[1] != 0) {
        // Trim trailing zeroes, if any.
        for (wchar_t *c = cs + wcslen(cs) - 1; *c == L'0' && c - 1 != cs; --c)
            *c = 0;
    }

    return s;
}

long double Number::getFloat() const {
    if (!isfinite(m_fSpecial))
        return m_fSpecial;

    long double num, den;
    std::string strNum = m_qValue.get_num().get_str(10);
    std::string strDen = m_qValue.get_den().get_str(10);
    char buf[128];

    sscanf(strNum.c_str(), "%llf", &num);
    sscanf(strDen.c_str(), "%llf", &den);

    return num/den;
}

int64_t Number::getInt() const {
    int64_t num;
    std::string strNum = m_qValue.get_num().get_str(10);
    char buf[128];

    sscanf(strNum.c_str(), "%lld", &num);

    return num;
}

uint64_t Number::getUInt() const {
    uint64_t num;
    std::string strNum = m_qValue.get_num().get_str(10);
    char buf[128];

    sscanf(strNum.c_str(), "%llu", &num);

    return num;
}

mpq_class Number::getRational() const {
    return m_qValue;
}

int Number::countBits(bool _bSigned) const {
    if (isInfinite() || isNaN())
        return SINGLE;

    if (isReal()) {
        const long double fValue = getFloat();

        if (makeReal(fValue).getRational() != m_qValue)
            return GENERIC;

        if (makeReal((double)fValue).getRational() != m_qValue)
            return QUAD;

        if (makeReal((float)fValue).getRational() != m_qValue)
            return DOUBLE;

        return SINGLE;
    }

    assert(isInt());
    mpz_class zValue = m_qValue.get_num();

    if (zValue < 0)
        zValue = -zValue - 1;

    if (_bSigned)
        zValue = std::max(mpz_class(zValue*2), mpz_class(2));

    const int nBits = mpz_sizeinbase(zValue.get_mpz_t(), 2);

    return nBits > 64 ? GENERIC : nBits;
}

void Number::negate() {
    if (!isNaN()) {
        m_fSpecial = -m_fSpecial;
        m_qValue = -m_qValue;
    }
}
