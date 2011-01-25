/// \file expressions.h
/// Internal structures representing expressions.
///

#ifndef BUILTINS_H_
#define BUILTINS_H_

#include "statements.h"

namespace ir {

/// Builtin predicate holder.
class Builtins {
public:
    static Builtins & instance();

    Predicate * find(const std::wstring & _name) const;

private:
    Collection<Predicate> m_predicates;

    /// Default constructor.
    Builtins();
};

} // namespace ir

#endif /* BUILTINS_H_ */
