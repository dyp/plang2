/// \file expressions.h
/// Internal structures representing expressions.
///

#ifndef BUILTINS_H_
#define BUILTINS_H_

#include "statements.h"

namespace ir {

/// Builtin predicate holder.
class CBuiltins {
public:
    static CBuiltins & instance();

    CPredicate * find(const std::wstring & _name) const;

private:
    CCollection<CPredicate> m_predicates;

    /// Default constructor.
    CBuiltins();
};

} // namespace ir

#endif /* BUILTINS_H_ */
