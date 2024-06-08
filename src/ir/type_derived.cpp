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

TypePtr SetType::getMeet(const TypePtr &_other) {
    const auto meet = _getMeet(_other);
    if (meet.first || meet.second || _other->getKind() == FRESH)
        return meet.first;

    const auto pMeet = getBaseType()->getMeet(_other->as<SetType>()->getBaseType());

    return pMeet ? std::make_shared<SetType>(pMeet) : SetTypePtr();
}

TypePtr SetType::getJoin(const TypePtr &_other) {
    const auto join = _getJoin(_other);
    if (join.first || join.second || _other->getKind() == FRESH)
        return join.first;

    const auto pJoin = getBaseType()->getJoin(_other->as<SetType>()->getBaseType());

    return pJoin ? std::make_shared<SetType>(pJoin) : SetTypePtr();
}

NodePtr SetType::clone(Cloner &_cloner) const {
    return NEW_CLONE(this, _cloner, _cloner.get<Type>(getBaseType()));
}

// References.

TypePtr RefType::getMeet(const TypePtr &_other) {
    const auto meet = _getMeet(_other);
    if (meet.first || meet.second || _other->getKind() == FRESH)
        return meet.first;

    const auto pMeet = getBaseType()->getMeet(_other->as<RefType>()->getBaseType());

    return pMeet ? std::make_shared<RefType>(pMeet) : RefTypePtr();
}

TypePtr RefType::getJoin(const TypePtr &_other) {
    const auto join = _getJoin(_other);
    if (join.first || join.second || _other->getKind() == FRESH)
        return join.first;

    const auto pJoin = getBaseType()->getJoin(_other->as<RefType>()->getBaseType());

    return pJoin ? std::make_shared<RefType>(pJoin) : RefTypePtr();
}

NodePtr RefType::clone(Cloner &_cloner) const {
    return NEW_CLONE(this, _cloner, _cloner.get<Type>(getBaseType()));
}

// Lists.

TypePtr ListType::getMeet(const TypePtr &_other) {
    const auto meet = _getMeet(_other);
    if (meet.first || meet.second || _other->getKind() == FRESH)
        return meet.first;

    const auto pMeet = getBaseType()->getMeet(_other->as<ListType>()->getBaseType());

    return pMeet ? std::make_shared<ListType>(pMeet) : ListTypePtr();
}

TypePtr ListType::getJoin(const TypePtr &_other) {
    const auto join = _getJoin(_other);
    if (join.first || join.second || _other->getKind() == FRESH)
        return join.first;

    const auto pJoin = getBaseType()->getJoin(_other->as<ListType>()->getBaseType());

    return pJoin ? std::make_shared<ListType>(pJoin) : ListTypePtr();
}

NodePtr ListType::clone(Cloner &_cloner) const {
    return NEW_CLONE(this, _cloner, _cloner.get<Type>(getBaseType()));
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

    const MapType &other = static_cast<const MapType &>(_other);
    return Order().out(*getBaseType(), *other.getBaseType()).in(
            *getIndexType(), *other.getIndexType());
}

bool MapType::less(const Type &_other) const {
    const MapType &other = (const MapType &)_other;

    if (*getBaseType() < *other.getBaseType())
        return true;

    if (*other.getBaseType() < *getBaseType())
        return false;

    return *getIndexType() < *other.getIndexType();
}

TypePtr MapType::getMeet(const TypePtr &_other) {
    const auto meet = _getMeet(_other);
    if (meet.first || meet.second || _other->getKind() == FRESH)
        return meet.first;

    const auto pIndexJoin = getIndexType()->getJoin(_other->as<MapType>()->getIndexType());
    const auto pBaseMeet = getBaseType()->getMeet(_other->as<MapType>()->getBaseType());

    return (pIndexJoin && pBaseMeet) ? std::make_shared<MapType>(pIndexJoin, pBaseMeet) : MapTypePtr();
}

TypePtr MapType::getJoin(const TypePtr &_other) {
    const auto join = _getJoin(_other);
    if (join.first || join.second || _other->getKind() == FRESH)
        return join.first;

    const auto pBaseJoin = getBaseType()->getJoin(_other->as<MapType>()->getBaseType());
    const auto pIndexMeet = getIndexType()->getMeet(_other->as<MapType>()->getIndexType());

    return (pBaseJoin && pIndexMeet) ? std::make_shared<MapType>(pIndexMeet, pBaseJoin) : MapTypePtr();
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

NodePtr MapType::clone(Cloner &_cloner) const {
    return NEW_CLONE(this, _cloner, _cloner.get<Type>(getIndexType()), _cloner.get<Type>(getBaseType()));
}
