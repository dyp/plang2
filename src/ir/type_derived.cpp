/// \file type_derived.cpp
///

#include "ir/base.h"
#include "ir/types.h"
#include "ir/declarations.h"
#include "typecheck.h"

using namespace ir;

// Derived types.

bool DerivedType::rewrite(const TypePtr &_pOld, const TypePtr &_pNew, bool _bRewriteFlags) {
    return tc::rewriteType(m_pBaseType, _pOld, _pNew, _bRewriteFlags);
}

int DerivedType::compare(const Type &_other) const {
    if (_other.getKind() == FRESH)
        return ORD_UNKNOWN;

    if (_other.getKind() == TOP)
        return ORD_SUB;

    if (_other.getKind() == BOTTOM)
        return ORD_SUPER;

    if (_other.getKind() == SUBTYPE)
        return inverse(((const Subtype&)_other).compare(*this));

    if (_other.getKind() != getKind())
        return ORD_NONE;

    return getBaseType()->compare(*((const DerivedType &)_other).getBaseType());
}

bool DerivedType::less(const Type &_other) const {
    return *getBaseType() < *((const DerivedType &)_other).getBaseType();
}

int DerivedType::getMonotonicity(const Type &_var) const {
    return getBaseType()->getMonotonicity(_var);
}

// Sets.

TypePtr SetType::getMeet(Type &_other) {
    SideType meet = _getMeet(_other);
    if (meet.first || meet.second || _other.getKind() == FRESH)
        return meet.first;

    TypePtr pMeet = getBaseType()->getMeet(*((const SetType &)_other).getBaseType());

    return pMeet ? new SetType(pMeet) : NULL;
}

TypePtr SetType::getJoin(Type &_other) {
    SideType join = _getJoin(_other);
    if (join.first || join.second || _other.getKind() == FRESH)
        return join.first;

    TypePtr pJoin = getBaseType()->getJoin(*((const SetType &)_other).getBaseType());

    return pJoin ? new SetType(pJoin) : NULL;
}

// Lists.

TypePtr ListType::getMeet(Type &_other) {
    SideType meet = _getMeet(_other);
    if (meet.first || meet.second || _other.getKind() == FRESH)
        return meet.first;

    TypePtr pMeet = getBaseType()->getMeet(*((const ListType &)_other).getBaseType());

    return pMeet ? new ListType(pMeet) : NULL;
}

TypePtr ListType::getJoin(Type &_other) {
    SideType join = _getJoin(_other);
    if (join.first || join.second || _other.getKind() == FRESH)
        return join.first;

    TypePtr pJoin = getBaseType()->getJoin(*((const ListType &)_other).getBaseType());

    return pJoin ? new ListType(pJoin) : NULL;
}

// Maps.

bool MapType::rewrite(const TypePtr &_pOld, const TypePtr &_pNew, bool _bRewriteFlags) {
    const bool b = DerivedType::rewrite(_pOld, _pNew, _bRewriteFlags);
    return tc::rewriteType(m_pIndexType, _pOld, _pNew, _bRewriteFlags) || b;
}

int MapType::compare(const Type &_other) const {
    if (_other.getKind() == FRESH)
        return ORD_UNKNOWN;

    if (_other.getKind() == TOP)
        return ORD_SUB;

    if (_other.getKind() == BOTTOM)
        return ORD_SUPER;

    if (_other.getKind() == SUBTYPE)
        return inverse(((const Subtype&)_other).compare(*this));

    if (_other.getKind() != getKind())
        return ORD_NONE;

    switch (m_pIndexType->compare(*((const MapType &)_other).getIndexType())) {
        case ORD_UNKNOWN:
            return ORD_UNKNOWN;
        case ORD_NONE:
            return ORD_NONE;
        case ORD_EQUALS:
            return getBaseType()->compare(*((const MapType &)_other).getBaseType());
        case ORD_SUB:
            switch (getBaseType()->compare(*((const MapType &)_other).getBaseType())) {
                case ORD_UNKNOWN:
                    return ORD_UNKNOWN;
                case ORD_NONE:
                case ORD_SUB:
                    return ORD_NONE;
                case ORD_EQUALS:
                case ORD_SUPER:
                    return ORD_SUPER;
            }
            break;
        case ORD_SUPER:
            switch (getBaseType()->compare(*((const MapType &)_other).getBaseType())) {
                case ORD_UNKNOWN:
                    return ORD_UNKNOWN;
                case ORD_NONE:
                case ORD_SUPER:
                    return ORD_NONE;
                case ORD_EQUALS:
                case ORD_SUB:
                    return ORD_SUB;
            }
    }

    return ORD_NONE;
}

bool MapType::less(const Type &_other) const {
    const MapType &other = (const MapType &)_other;

    if (*getBaseType() < *other.getBaseType())
        return true;

    if (*other.getBaseType() < *getBaseType())
        return false;

    return *getIndexType() < *other.getIndexType();
}

TypePtr MapType::getMeet(Type &_other) {
    SideType meet = _getMeet(_other);
    if (meet.first || meet.second || _other.getKind() == FRESH)
        return meet.first;

    TypePtr pIndexJoin = getIndexType()->getJoin(*((const MapType &)_other).getIndexType().as<Type>());
    TypePtr pBaseMeet = getBaseType()->getMeet(*((const MapType &)_other).getBaseType());

    return (pIndexJoin && pBaseMeet) ? new MapType(pIndexJoin, pBaseMeet) : NULL;
}

TypePtr MapType::getJoin(Type &_other) {
    SideType join = _getJoin(_other);
    if (join.first || join.second || _other.getKind() == FRESH)
        return join.first;

    TypePtr pBaseJoin = getBaseType()->getJoin(*((const MapType &)_other).getBaseType());
    TypePtr pIndexMeet = getIndexType()->getMeet(*((const MapType &)_other).getIndexType().as<Type>());

    return (pBaseJoin && pIndexMeet) ? new MapType(pIndexMeet, pBaseJoin) : NULL;
}

int MapType::getMonotonicity(const Type &_var) const {
    const int mtb = getBaseType()->getMonotonicity(_var);
    const int mti = getIndexType()->getMonotonicity(_var);
    const bool bMonotone = mtb == MT_MONOTONE || mti == MT_ANTITONE;
    const bool bAntitone = mtb == MT_ANTITONE || mti == MT_MONOTONE;

    if ((bMonotone && bAntitone) || mtb == MT_NONE || mti == MT_NONE)
        return MT_NONE;

    return bMonotone ? MT_MONOTONE : (bAntitone ? MT_ANTITONE : MT_CONST);
}
