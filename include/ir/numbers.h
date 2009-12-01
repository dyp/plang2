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
class CNumber {
public:
    /// Kind of a number.
    enum {
        /// Generic number that does not fit in other kinds. Stored as rational
        /// number.
        Generic = -1,
        /// Abstract native bitness (not used directly).
        Native = 0,
        /// Integer value that fits in 64 bits.
        Integer,
        /// Single precision floating point value.
        Single,
        /// Double precision floating point value.
        Double,
        /// Quad precision floating point value.
        Quad,
    };

    /// Default constructor initializes number as integer zero.
    CNumber();

    /// Initialize with string. Appropriate kind will be automatically determined.
    CNumber(const CNumber & _other);

    /// Initialize with floating point value.
    CNumber(long double _f);

    /// Initialize with integer.
    CNumber(int64_t _n);

    /// Initialize with string.
    CNumber(const std::wstring & _s);

    /// Destructor.
    ~CNumber();

    /// Assignment operator.
    /// \param _other Right hand side of the assignment.
    /// \return Modified number.
    CNumber & operator =(const CNumber & _other);

    /// Get "not a number" special value.
    /// \return \c NaN.
    static const CNumber & nan() { return m_nan; }

    /// Get "infinity" special value.
    /// \param _bNegative Return \c -Inf  if set.
    /// \return Positive or negative \c Inf.
    static const CNumber & inf(bool _bNegative = false) {
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

    static const CNumber m_nan;
    static const CNumber m_inf;
    static const CNumber m_infNeg;

    void _update();
};

#endif /* NUMBERS_H_ */
