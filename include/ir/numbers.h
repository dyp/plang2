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
    /// Kind of a number.
    enum {
        /// Generic number that does not fit in other kinds. Stored as rational
        /// number.
        GENERIC = -1,
        /// Abstract native bitness (not used directly).
        NATIVE = 0,
        /// Integer value that fits in 64 bits.
        INTEGER,
        /// Single precision floating point value.
        SINGLE,
        /// Double precision floating point value.
        DOUBLE,
        /// Quad precision floating point value.
        QUAD,
    };

    /// Default constructor initializes number as integer zero.
    Number();

    /// Initialize with string. Appropriate kind will be automatically determined.
    Number(const Number & _other);

    /// Initialize with floating point value.
    Number(long double _f);

    /// Initialize with integer.
    Number(int64_t _n);

    /// Initialize with string.
    Number(const std::wstring & _s);

    /// Destructor.
    ~Number();

    /// Assignment operator.
    /// \param _other Right hand side of the assignment.
    /// \return Modified number.
    Number & operator =(const Number & _other);

    /// Get "not a number" special value.
    /// \return \c NaN.
    static const Number & nan() { return m_nan; }

    /// Get "infinity" special value.
    /// \param _bNegative Return \c -Inf  if set.
    /// \return Positive or negative \c Inf.
    static const Number & inf(bool _bNegative = false) {
        return _bNegative ? m_infNeg : m_inf;
    }

    /// Get kind of a number (one of #Generic, #Integer, #Single, #Double or #Quad).
    int getKind() const { return m_kind; }

    /// Get minimal number of bits necessary to encode the number.
    /// \param _bUnsigned Set if unsigned encoding is needed.
    int getBits(bool _bUnsigned = false) const;

    /// Check if value is "not a number".
    /// \return True if value is \c NaN, false otherwise.
    bool isNaN() const { return isnanl(m_fValue) != 0; }

    /// Check if value is infinite.
    /// \return True if value is \c Inf (positive or negative), false otherwise.
    bool isInfinite() const { return isinfl(m_fValue) != 0; }

    /// Convert to string.
    /// \return String representation of the value.
    std::wstring toString() const;

    long double getFloat() const { return m_fValue; }
    int64_t getInt() const { return m_nValue; }
    mpq_class getRational() const { return m_qValue; }

private:
    int m_kind;
    int m_nBits;
    long double m_fValue;
    int64_t m_nValue;
    mpq_class m_qValue;

    static const Number m_nan;
    static const Number m_inf;
    static const Number m_infNeg;

    void _update();
};

#endif /* NUMBERS_H_ */
