/// \file numbers.cpp
///

#include "ir/numbers.h"
#include "utils.h"

#include <iostream>

/// Default constructor initializes number as integer zero.
Number::Number()
    : m_kind(INTEGER), m_nValue(0)
{
    _update();
}

/// Initialize with string. Appropriate kind will be automatically determined.
Number::Number(const Number & _other) {
    (* this) = _other;
}

/// Initialize with floating point value.
Number::Number(long double _f)
    : m_kind(QUAD), m_fValue(_f)
{
    _update();
}

/// Initialize with integer.
Number::Number(int64_t _n)
    : m_kind(INTEGER), m_nValue(_n)
{
    _update();
}

/// Initialize with string.
Number::Number(const std::wstring & _s) {
    m_kind = -1;

    if (_s == L"+inf" || _s == L"inf") {
        m_fValue = INFINITY;
        m_kind = SINGLE;
    } else if (_s == L"-inf") {
        m_fValue = -INFINITY;
        m_kind = SINGLE;
    } else if (_s != L"nan" && swscanf(_s.c_str(), L"%llf", & m_fValue) == 1) {
        wchar_t c;
        //if ((long double) (int64_t (m_fValue)) == m_fValue) {
        if (swscanf(_s.c_str(), L"%lld%c", & m_nValue, & c) == 1) {
            // Treat it as an integer.
            m_nValue = int64_t (m_fValue);
            m_kind = INTEGER;
        } else if (finitel(m_fValue)) {
            m_kind = QUAD;
        } else {
            // Let's hope that lexer took care of the decimal point and the exponent,
            const std::string str = strNarrow(_s.c_str());
            m_qValue.set_str(str.c_str(), 10);
            m_kind = GENERIC;
        }
    }

    if (m_kind == -1) {
        m_fValue = NAN;
        m_kind = SINGLE;
        m_nBits = sizeof(float);
    }

    _update();
}

Number & Number::operator =(const Number & _other) {
    m_kind = _other.m_kind;
    m_nBits = _other.m_nBits;
    m_fValue = _other.m_fValue;
    m_nValue = _other.m_nValue;
    m_qValue = _other.m_qValue;

    return * this;
}

Number::~Number() {
}

const Number Number::m_nan(L"nan");
const Number Number::m_inf(L"inf");
const Number Number::m_infNeg(L"-inf");

static
int _calcBits(uint64_t _n) {
    int i = sizeof(_n)*8 - 1;
    uint64_t bit = 1UL << i;

    while ((i > 0) && (_n & bit) == 0) {
        -- i;
        bit >>= 1;
    }

    return i + 1;
}

void Number::_update() {
    if (m_kind == GENERIC && mpq_class(m_qValue.get_d()) == m_qValue) {
        m_fValue = m_qValue.get_d();
        m_kind = DOUBLE;
    }

    if (m_kind == SINGLE || m_kind == DOUBLE || m_kind == QUAD) {
        /*if ((long double) (int64_t (m_fValue)) == m_fValue) {
            m_nValue = int64_t (m_fValue);
            m_kind = Integer;
        } else*/ if ((long double) (float (m_fValue)) == m_fValue) {
            m_kind = SINGLE;
        } else if ((long double) (double (m_fValue)) == m_fValue) {
            m_kind = DOUBLE;
        } else {
            m_kind = QUAD;
        }
    } else if (m_kind == INTEGER) {
        const std::string str = intToStr(m_nValue);
        m_fValue = m_nValue;
        m_qValue.set_str(str.c_str(), 10);
    }

    switch (m_kind) {
        case GENERIC: m_nBits = GENERIC; break;
        case SINGLE:  m_nBits = sizeof(float)*8; break;
        case DOUBLE:  m_nBits = sizeof(double)*8; break;
        case QUAD:    m_nBits = sizeof(long double)*8; break;
        case INTEGER: m_nBits = _calcBits(m_nValue < 0 ? (-m_nValue) : m_nValue); break;
    }
}

int Number::getBits(bool _bUnsigned) const {
    if (m_kind != INTEGER)
        return m_nBits;

    return _bUnsigned ? m_nBits : (m_nBits + 1);
}

std::wstring Number::toString() const {
    const size_t sz = 128;
    wchar_t s[sz];

    *s = 0;

    switch (m_kind) {
        case GENERIC: return strWiden(m_qValue.get_str());
        case SINGLE:  swprintf(s, sz, L"%f", (float) m_fValue); break;
        case DOUBLE:  swprintf(s, sz, L"%f", (double) m_fValue); break;
        case QUAD:    swprintf(s, sz, L"%llf", m_fValue); break;
        case INTEGER: swprintf(s, sz, L"%lld", m_nValue); break;
    }

    return s;
}
