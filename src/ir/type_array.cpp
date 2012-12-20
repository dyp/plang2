/// \file type_subtype.cpp
///

#include "ir/base.h"
#include "ir/types.h"
#include "ir/declarations.h"
#include "ir/visitor.h"
#include "typecheck.h"

using namespace ir;

// Array types.

bool ArrayType::less(const Type &_other) const {
    assert(_other.getKind() == ARRAY);
    const ArrayType& other = (const ArrayType&)_other;
    if (!_equals(getBaseType(), other.getBaseType()))
        return _less(getBaseType(), other.getBaseType());
    return _less(getDimensionType(), other.getDimensionType());
}

TypePtr ArrayType::getMeet(Type &_other) {
    SideType meet = _getMeet(_other);
    if (meet.first || meet.second || _other.getKind() == FRESH)
        return meet.first;

    const ArrayType& other = (const ArrayType&)_other;

    TypePtr
        pBaseMeet = getBaseType()->getMeet(*other.getBaseType()),
        pDimensionJoin = getDimensionType()->getJoin(*other.getDimensionType());

    if (!pBaseMeet || !pDimensionJoin)
        return NULL;

    return new ArrayType(pBaseMeet, pDimensionJoin);
}

TypePtr ArrayType::getJoin(Type &_other) {
    SideType join = _getJoin(_other);
    if (join.first || join.second || _other.getKind() == FRESH)
        return join.first;

    const ArrayType& other = (const ArrayType&)_other;

    TypePtr
        pBaseJoin = getBaseType()->getJoin(*other.getBaseType()),
        pDimensionMeet = getDimensionType()->getMeet(*other.getDimensionType());

    if (!pBaseJoin || !pDimensionMeet)
        return NULL;

    return new ArrayType(pBaseJoin, pDimensionMeet);
}

bool ArrayType::rewrite(const TypePtr &_pOld, const TypePtr &_pNew, bool _bRewriteFlags) {
    bool result = false;
    TypePtr pType = getBaseType();
    if (tc::rewriteType(pType, _pOld, _pNew, _bRewriteFlags)) {
        setBaseType(pType);
        result = true;
    }
    pType = getDimensionType();
    if (tc::rewriteType(pType, _pOld, _pNew, _bRewriteFlags)) {
        setDimensionType(pType);
        result = true;
    }
    return result;
}

int ArrayType::compare(const Type &_other) const {
    if (_other.getKind() == FRESH)
        return ORD_UNKNOWN;
    if (_other.getKind() == TOP)
        return ORD_SUB;
    if (_other.getKind() == BOTTOM)
        return ORD_SUPER;
    if (_other.getKind() != ARRAY)
        return ORD_NONE;

    const ArrayType& other = (const ArrayType&)_other;

    const size_t
        szBaseOrd = getBaseType()->compare(*other.getBaseType()),
        szDimOrd = getDimensionType()->compare(*other.getDimensionType());

    if (szDimOrd == ORD_NONE || szBaseOrd == ORD_NONE)
        return ORD_NONE;
    if (szDimOrd == ORD_UNKNOWN || szBaseOrd == ORD_UNKNOWN)
        return ORD_UNKNOWN;
    if (szBaseOrd == ORD_EQUALS)
        return inverse(szDimOrd);
    if (szDimOrd == ORD_EQUALS || szBaseOrd == inverse(szDimOrd))
        return szBaseOrd;

    return ORD_NONE;
}

int ArrayType::getMonotonicity(const Type &_var) const {
    return getBaseType()->getMonotonicity(_var);
}
