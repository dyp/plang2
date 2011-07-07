/// \file type_union.cpp
///

#include "ir/base.h"
#include "ir/types.h"
#include "ir/declarations.h"

using namespace ir;

// Unions.

int UnionType::compare(const Type & _other) const {
    if (_other.getKind() == FRESH)
        return ORD_UNKNOWN;

    if (_other.getKind() == TOP)
        return ORD_SUB;

    if (_other.getKind() == BOTTOM)
        return ORD_SUPER;

    if (_other.getKind() != UNION)
        return ORD_NONE;

    const UnionType & other = (const UnionType &) _other;
    size_t cUnmatched = 0, cOtherUnmatched = other.getConstructors().size();

    for (size_t i = 0; i < m_constructors.size(); ++ i) {
        const UnionConstructorDeclaration & cons = * m_constructors.get(i);
        const size_t cOtherConsIdx = other.getConstructors().findByNameIdx(cons.getName());

        if (cOtherConsIdx != (size_t) -1)
            -- cOtherUnmatched;
        else
            ++ cUnmatched;
    }

    // TODO: actually compare alternatives.

    if (cUnmatched == 0)
        return cOtherUnmatched > 0 ? ORD_SUB : ORD_EQUALS;

    return cOtherUnmatched == 0 ? ORD_SUPER : ORD_NONE;
}
