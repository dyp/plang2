/// \file numbers.h
/// Auxiliary class representing numeric literal.
///

#ifndef NUMBERS_H_
#define NUMBERS_H_

#include <float.h>
#include <stdint.h>
#include <math.h>
#include <gmpxx.h>

#include <string>

/// Representation of numeric literals.
///
/// This class internally uses standard finite-precision types (\c int, \c float, etc.)
/// or rational numbers provided by GMP library. When converting from string
/// (most frequently a lexer token) to number needed precision can be determined
/// automatically.
///
/// TODO: Support for arithmetic operations will be added later.
class Number {
public:
    /// Known bitnesses.
    enum {
        /// Generic number that does not fit in other kinds. Stored as rational
        /// number.
        GENERIC = -1,
        /// Abstract native bitness (not used directly).
        NATIVE = 0,
        /// Single precision floating point value.
        SINGLE = 32,
        /// Double precision floating point value.
        DOUBLE = 64,
        /// Quad precision floating point value.
        QUAD = 128,
    };

    /// Default constructor initializes number as integer zero.
    Number();

    /// Initialize with string. Appropriate kind will be automatically determined.
    Number(const Number & _other);

    /// Format for string to number conversion.
    enum Format {
        INTEGER,
        REAL,
    };

    /// Initialize with string.
    Number(const std::wstring &_s, Format _fmt);

    /// Initialize with string.
    Number(const std::string &_s, Format _fmt);

    /// Destructor.
    ~Number();

    /// Assignment operator.
    /// \param _other Right hand side of the assignment.
    /// \return Modified number.
    Number & operator =(const Number & _other);

    /// Get "not a number" special value.
    /// \return \c NaN.
    static const Number & makeNaN() { return m_nan; }

    /// Get "infinity" special value.
    /// \param _bNegative Return \c -Inf  if set.
    /// \return Positive or negative \c Inf.
    static const Number & makeInf(bool _bNegative = false) {
        return _bNegative ? m_infNeg : m_inf;
    }

    static Number makeInt(int64_t _n);
    static Number makeNat(uint64_t _n);
    static Number makeReal(long double _f);

    bool isInt() const;
    bool isNat() const;
    bool isNeg() const;
    bool isReal() const;

    /// Check if value is "not a number".
    /// \return True if value is \c NaN, false otherwise.
    bool isNaN() const { return isnanl(m_fSpecial) != 0; }

    /// Check if value is infinite.
    /// \return True if value is \c Inf (positive or negative), false otherwise.
    bool isInfinite() const { return isinfl(m_fSpecial) != 0; }

    /// Convert to string.
    /// \return String representation of the value.
    std::wstring toString() const;

    long double getFloat() const;
    int64_t getInt() const;
    uint64_t getUInt() const;
    mpq_class getRational() const;

    int countBits(bool _bSigned = false) const;

    void negate();

private:
    mpq_class m_qValue;
    long double m_fSpecial;

    static const Number m_nan;
    static const Number m_inf;
    static const Number m_infNeg;

    void _init(const std::string &_s, Format _fmt);
};

#endif /* NUMBERS_H_ */
