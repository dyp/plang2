/// \file type_derived.cpp
///

#include "ir/base.h"
#include "ir/types.h"
#include "ir/declarations.h"
#include "typecheck.h"

using namespace ir;

// Derived types.

bool DerivedType::rewrite(Type * _pOld, Type * _pNew) {
    return tc::rewriteType(m_pBaseType, _pOld, _pNew);
}

int DerivedType::compare(const Type & _other) const {
    if (_other.getKind() == FRESH)
        return ORD_UNKNOWN;

    if (_other.getKind() == TOP)
        return ORD_SUB;

    if (_other.getKind() == BOTTOM)
        return ORD_SUPER;

    if (_other.getKind() != getKind())
        return ORD_NONE;

    return getBaseType()->compare(*((const DerivedType &)_other).getBaseType());
}

bool DerivedType::less(const Type & _other) const {
    return *getBaseType() < *((const DerivedType &)_other).getBaseType();
}

// Sets.

Type *SetType::getMeet(Type & _other) {
    Type *pMeet = Type::getMeet(_other);

    if (pMeet != NULL || _other.getKind() == FRESH)
        return pMeet;

    pMeet = getBaseType()->getMeet(*((const SetType &)_other).getBaseType());

    return pMeet ? new SetType(pMeet) : NULL;
}

Type *SetType::getJoin(Type & _other) {
    Type *pJoin = Type::getJoin(_other);

    if (pJoin != NULL || _other.getKind() == FRESH)
        return pJoin;

    pJoin = getBaseType()->getJoin(*((const SetType &)_other).getBaseType());

    return pJoin ? new SetType(pJoin) : NULL;
}

// Lists.

Type *ListType::getMeet(Type & _other) {
    Type *pMeet = Type::getMeet(_other);

    if (pMeet != NULL || _other.getKind() == FRESH)
        return pMeet;

    pMeet = getBaseType()->getMeet(*((const ListType &)_other).getBaseType());

    return pMeet ? new ListType(pMeet) : NULL;
}

Type *ListType::getJoin(Type & _other) {
    Type *pJoin = Type::getJoin(_other);

    if (pJoin != NULL || _other.getKind() == FRESH)
        return pJoin;

    pJoin = getBaseType()->getJoin(*((const ListType &)_other).getBaseType());

    return pJoin ? new ListType(pJoin) : NULL;
}

// Maps.

bool MapType::rewrite(Type *_pOld, Type *_pNew) {
    const bool b = DerivedType::rewrite(_pOld, _pNew);
    return tc::rewriteType(m_pIndexType, _pOld, _pNew) || b;
}

int MapType::compare(const Type &_other) const {
    if (_other.getKind() == FRESH)
        return ORD_UNKNOWN;

    if (_other.getKind() == TOP)
        return ORD_SUB;

    if (_other.getKind() == BOTTOM)
        return ORD_SUPER;

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

bool MapType::less(const Type & _other) const {
    const MapType &other = (const MapType &)_other;

    if (*getBaseType() < *other.getBaseType())
        return true;

    if (*other.getBaseType() < *getBaseType())
        return false;

    return *getIndexType() < *other.getIndexType();
}

Type *MapType::getMeet(Type & _other) {
    Type *pMeet = Type::getMeet(_other);

    if (pMeet != NULL || _other.getKind() == FRESH)
        return pMeet;

    Type *pBaseJoin = getBaseType()->getJoin(*((const MapType &)_other).getBaseType());
    Type *pIndexMeet = getIndexType()->getMeet(*((const MapType &)_other).getIndexType());

    return (pBaseJoin && pIndexMeet) ? new MapType(pIndexMeet, pBaseJoin) : NULL;
}

Type *MapType::getJoin(Type & _other) {
    Type *pJoin = Type::getJoin(_other);

    if (pJoin != NULL || _other.getKind() == FRESH)
        return pJoin;

    Type *pIndexJoin = getIndexType()->getJoin(*((const MapType &)_other).getIndexType());
    Type *pBaseMeet = getBaseType()->getMeet(*((const MapType &)_other).getBaseType());

    return (pIndexJoin && pBaseMeet) ? new MapType(pIndexJoin, pBaseMeet) : NULL;
}
