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

    if (_other.getKind() != getKind())
        return ORD_NONE;

    return getBaseType()->compare(*((const DerivedType &)_other).getBaseType());
}

bool DerivedType::less(const Type & _other) const {
    return *getBaseType() < *((const DerivedType &)_other).getBaseType();
}

// Sets.

Type::Extremum SetType::getMeet(Type & _other) {
    Extremum meet = Type::getMeet(_other);

    if (!meet.second)
        return meet;

    if (_other.getKind() != SET)
        return meet;

    meet = getBaseType()->getMeet(*((const SetType &)_other).getBaseType());

    if (meet.first != NULL)
        meet.first = new SetType(meet.first);

    return meet;
}

Type::Extremum SetType::getJoin(Type & _other) {
    Extremum join = Type::getJoin(_other);

    if (!join.second)
        return join;

    if (_other.getKind() != SET)
        return join;

    join = getBaseType()->getJoin(*((const SetType &)_other).getBaseType());

    if (join.first != NULL)
        join.first = new SetType(join.first);

    return join;
}

// Lists.

Type::Extremum ListType::getMeet(Type & _other) {
    Extremum meet = Type::getMeet(_other);

    if (!meet.second)
        return meet;

    if (_other.getKind() != LIST)
        return meet;

    meet = getBaseType()->getMeet(*((const ListType &)_other).getBaseType());

    if (meet.first != NULL)
        meet.first = new ListType(meet.first);

    return meet;
}

Type::Extremum ListType::getJoin(Type & _other) {
    Extremum join = Type::getJoin(_other);

    if (!join.second)
        return join;

    if (_other.getKind() != LIST)
        return join;

    join = getBaseType()->getJoin(*((const ListType &)_other).getBaseType());

    if (join.first != NULL)
        join.first = new ListType(join.first);

    return join;
}

// Maps.

bool MapType::rewrite(Type *_pOld, Type *_pNew) {
    const bool b = DerivedType::rewrite(_pOld, _pNew);
    return tc::rewriteType(m_pIndexType, _pOld, _pNew) || b;
}

int MapType::compare(const Type &_other) const {
    if (_other.getKind() == FRESH)
        return ORD_UNKNOWN;

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

Type::Extremum MapType::getMeet(Type & _other) {
    Extremum meet = Type::getMeet(_other);

    if (!meet.second)
        return meet;

    if (_other.getKind() != MAP)
        return meet;

    Extremum join = getBaseType()->getJoin(*((const MapType &)_other).getBaseType());

    meet = getIndexType()->getMeet(*((const MapType &)_other).getIndexType());

    if (meet.first == NULL || join.first == NULL) {
        meet.second = meet.second || join.second;
        return meet;
    }

    meet.first = new MapType(meet.first, join.first);

    return meet;
}

Type::Extremum MapType::getJoin(Type & _other) {
    Extremum join = Type::getJoin(_other);

    if (!join.second)
        return join;

    if (_other.getKind() != MAP)
        return join;

    Extremum meet = getBaseType()->getMeet(*((const MapType &)_other).getBaseType());

    join = getIndexType()->getJoin(*((const MapType &)_other).getIndexType());

    if (meet.first == NULL || join.first == NULL) {
        join.second = meet.second || join.second;
        return join;
    }

    join.first = new MapType(join.first, meet.first);

    return join;
}
