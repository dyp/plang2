/// \file numbers.cpp
///

#include "ir/numbers.h"
#include "utils.h"

#include <iostream>

/// Default constructor initializes number as integer zero.
CNumber::CNumber()
    : m_kind(Integer), m_nValue(0)
{
    _update();
}

/// Initialize with string. Appropriate kind will be automatically determined.
CNumber::CNumber(const CNumber & _other) {
    (* this) = _other;
}

/// Initialize with floating point value.
CNumber::CNumber(long double _f)
    : m_kind(Quad), m_fValue(_f)
{
    _update();
}

/// Initialize with integer.
CNumber::CNumber(int64_t _n)
    : m_kind(Integer), m_nValue(_n)
{
    _update();
}

/// Initialize with string.
CNumber::CNumber(const std::wstring & _s) {
    m_kind = -1;

    if (_s == L"+inf" || _s == L"inf") {
        m_fValue = INFINITY;
        m_kind = Single;
    } else if (_s == L"-inf") {
        m_fValue = -INFINITY;
        m_kind = Single;
    } else if (_s != L"nan" && swscanf(_s.c_str(), L"%llf", & m_fValue) == 1) {
        wchar_t c;
        //if ((long double) (int64_t (m_fValue)) == m_fValue) {
        if (swscanf(_s.c_str(), L"%lld%c", & m_nValue, & c) == 1) {
            // Treat it as an integer.
            m_nValue = int64_t (m_fValue);
            m_kind = Integer;
        } else if (finitel(m_fValue)) {
            m_kind = Quad;
        } else {
            // Let's hope that lexer took care of the decimal point and the exponent,
            const std::string str = strNarrow(_s.c_str());
            m_qValue.set_str(str.c_str(), 10);
            m_kind = Generic;
        }
    }

    if (m_kind == -1) {
        m_fValue = NAN;
        m_kind = Single;
        m_nBits = sizeof(float);
    }

    _update();
}

CNumber & CNumber::operator =(const CNumber & _other) {
    m_kind = _other.m_kind;
    m_nBits = _other.m_nBits;
    m_fValue = _other.m_fValue;
    m_nValue = _other.m_nValue;
    m_qValue = _other.m_qValue;

    return * this;
}

CNumber::~CNumber() {
}

const CNumber CNumber::m_nan(L"nan");
const CNumber CNumber::m_inf(L"inf");
const CNumber CNumber::m_infNeg(L"-inf");

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

void CNumber::_update() {
    if (m_kind == Generic && mpq_class(m_qValue.get_d()) == m_qValue) {
        m_fValue = m_qValue.get_d();
        m_kind = Double;
    }

    if (m_kind == Single || m_kind == Double || m_kind == Quad) {
        /*if ((long double) (int64_t (m_fValue)) == m_fValue) {
            m_nValue = int64_t (m_fValue);
            m_kind = Integer;
        } else*/ if ((long double) (float (m_fValue)) == m_fValue) {
            m_kind = Single;
        } else if ((long double) (double (m_fValue)) == m_fValue) {
            m_kind = Double;
        } else {
            m_kind = Quad;
        }
    } else if (m_kind == Integer) {
        const std::string str = intToStr(m_nValue);
        m_fValue = m_nValue;
        m_qValue.set_str(str.c_str(), 10);
    }

    switch (m_kind) {
        case Generic: m_nBits = Generic; break;
        case Single:  m_nBits = sizeof(float)*8; break;
        case Double:  m_nBits = sizeof(double)*8; break;
        case Quad:    m_nBits = sizeof(long double)*8; break;
        case Integer: m_nBits = _calcBits(m_nValue < 0 ? (-m_nValue) : m_nValue); break;
    }
}

int CNumber::getBits(bool _bUnsigned) const {
    if (m_kind != Integer)
        return m_nBits;

    return _bUnsigned ? m_nBits : (m_nBits + 1);
}

std::wstring CNumber::toString() const {
    const size_t sz = 128;
    wchar_t s[sz];

    *s = 0;

    switch (m_kind) {
        case Generic: return strWiden(m_qValue.get_str());
        case Single:  swprintf(s, sz, L"%f", (float) m_fValue); break;
        case Double:  swprintf(s, sz, L"%f", (double) m_fValue); break;
        case Quad:    swprintf(s, sz, L"%llf", m_fValue); break;
        case Integer: swprintf(s, sz, L"%lld", m_nValue); break;
    }

    return s;
}
