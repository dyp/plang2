/// \file type_union.cpp
///

#include "ir/base.h"
#include "ir/types.h"
#include "ir/declarations.h"
#include "typecheck.h"

using namespace ir;

// Unions.

int UnionType::compare(const Type &_other) const {
    if (_other.getKind() == FRESH)
        return ORD_UNKNOWN;

    if (_other.getKind() == TOP)
        return ORD_SUB;

    if (_other.getKind() == BOTTOM)
        return ORD_SUPER;

    if (_other.getKind() != UNION)
        return ORD_NONE;

    const UnionType &other = (const UnionType &)_other;
    size_t cUnmatched = 0, cOtherUnmatched = other.getConstructors().size();
    size_t cSub = 0, cSuper = 0, cUnknown = 0;

    for (size_t i = 0; i < m_constructors.size(); ++i) {
        const UnionConstructorDeclaration &cons = *m_constructors.get(i);
        const size_t cOtherConsIdx = other.getConstructors().findByNameIdx(cons.getName());

        if (cOtherConsIdx != (size_t)-1) {
            const UnionConstructorDeclaration &otherCons = *other.getConstructors().get(cOtherConsIdx);

            if (otherCons.getFields().size() != cons.getFields().size())
                return ORD_NONE;

            const int cmp = tc::TupleType(&cons.getFields()).compare(tc::TupleType(&otherCons.getFields()));

            if (cmp == ORD_SUB)
                ++cSub;
            else if (cmp == ORD_SUPER)
                ++cSuper;
            else if (cmp == ORD_UNKNOWN)
                ++cUnknown;
            else if (cmp == ORD_NONE)
                return ORD_NONE;

            --cOtherUnmatched;
        } else
            ++cUnmatched;
    }

    if (cUnmatched > 0 && cOtherUnmatched > 0)
        return ORD_NONE;

    if (cSub > 0 && cSuper > 0)
        return ORD_NONE;

    if (cUnknown > 0)
        return ORD_UNKNOWN;

    if (cUnmatched == 0 && cOtherUnmatched == 0) {
        if (cSub > 0)
            return ORD_SUB;
        if (cSuper > 0)
            return ORD_SUPER;
        return ORD_EQUALS;
    }

    if (cUnmatched > 0)
        return cSub > 0 ? ORD_NONE : ORD_SUPER;

    // cOtherUnmatched > 0
    return cSuper > 0 ? ORD_NONE : ORD_SUB;
}

bool UnionType::hasFresh() const {
    for (size_t i = 0; i < m_constructors.size(); ++i)
        for (size_t j = 0; j < m_constructors.get(i)->getFields().size(); ++j)
            if (m_constructors.get(i)->getFields().get(j)->getType()->hasFresh())
                return true;

    return false;
}

bool UnionType::contains(const TypePtr &_pType) const {
    for (size_t i = 0; i < m_constructors.size(); ++i)
        for (size_t j = 0; j < m_constructors.get(i)->getFields().size(); ++j) {
            TypePtr pType = m_constructors.get(i)->getFields().get(j)->getType();
            if (*pType == *_pType || pType->contains(_pType))
                return true;
        }

    return false;
}

bool UnionType::rewrite(const TypePtr &_pOld, const TypePtr &_pNew, bool _bRewriteFlags) {
    bool bResult = false;

    for (size_t i = 0; i < m_constructors.size(); ++i)
        for (size_t j = 0; j < m_constructors.get(i)->getFields().size(); ++j) {
            NamedValue &field = *m_constructors.get(i)->getFields().get(j);
            Auto<ir::Type> pType = field.getType();

            if (tc::rewriteType(pType, _pOld, _pNew, _bRewriteFlags)) {
                bResult = true;
                field.setType(pType);
            }
        }

    return bResult;
}

TypePtr UnionType::getMeet(ir::Type &_other) {
    TypePtr pMeet = Type::getMeet(_other);

    if (pMeet || _other.getKind() == FRESH)
        return pMeet;

    const UnionType &other = (const UnionType &)_other;
    UnionTypePtr pUnion = new UnionType();

    for (size_t i = 0; i < m_constructors.size(); ++i) {
        UnionConstructorDeclaration &cons = *m_constructors.get(i);
        const size_t cOtherConsIdx = other.getConstructors().findByNameIdx(cons.getName());

        if (cOtherConsIdx != (size_t)-1) {
            const UnionConstructorDeclaration &otherCons = *other.getConstructors().get(cOtherConsIdx);

            if (otherCons.getFields().size() != cons.getFields().size())
                continue;

            tc::TupleType otherFields = tc::TupleType(&otherCons.getFields());

            pMeet = tc::TupleType(&cons.getFields()).getMeet(otherFields);

            if (!pMeet)
                return NULL;

            UnionConstructorDeclarationPtr pCons = new UnionConstructorDeclaration(cons.getName());

            pCons->getFields().append(pMeet.as<tc::TupleType>()->getFields());
            pUnion->getConstructors().add(pCons);
        }
    }

    if (pUnion->getConstructors().empty())
        return new Type(BOTTOM);

    return pUnion;
}

TypePtr UnionType::getJoin(ir::Type &_other) {
    TypePtr pJoin = Type::getMeet(_other);

    if (pJoin || _other.getKind() == FRESH)
        return pJoin;

    const UnionType &other = (const UnionType &)_other;
    size_t cOtherUnmatched = other.getConstructors().size();
    UnionTypePtr pUnion = new UnionType();

    for (size_t i = 0; i < m_constructors.size(); ++i) {
        UnionConstructorDeclaration &cons = *m_constructors.get(i);
        const size_t cOtherConsIdx = other.getConstructors().findByNameIdx(cons.getName());

        if (cOtherConsIdx != (size_t) -1) {
            const UnionConstructorDeclaration &otherCons = *other.getConstructors().get(cOtherConsIdx);

            if (otherCons.getFields().size() != cons.getFields().size())
                return new Type(TOP);

            tc::TupleType otherFields = tc::TupleType(&otherCons.getFields());

            pJoin = tc::TupleType(&cons.getFields()).getJoin(otherFields);

            if (!pJoin)
                return NULL;

            UnionConstructorDeclarationPtr pCons = new UnionConstructorDeclaration(cons.getName());

            pCons->getFields().append(pJoin.as<tc::TupleType>()->getFields());
            pUnion->getConstructors().add(pCons);
            --cOtherUnmatched;
        } else {
            UnionConstructorDeclarationPtr pCons = new UnionConstructorDeclaration(cons.getName());

            pCons->getFields().append(cons.getFields());
            pUnion->getConstructors().add(pCons);
        }
    }

    for (size_t i = 0; cOtherUnmatched > 0 && i < other.getConstructors().size(); ++i) {
        const UnionConstructorDeclaration &otherCons = *other.getConstructors().get(i);
        const size_t cConsIdx = getConstructors().findByNameIdx(otherCons.getName());

        if (cConsIdx != (size_t)-1)
            continue;

        UnionConstructorDeclarationPtr pCons = new UnionConstructorDeclaration(otherCons.getName());

        --cOtherUnmatched;
        pCons->getFields().append(otherCons.getFields());
        pUnion->getConstructors().add(pCons);

    }

    return pUnion;
}

bool UnionType::less(const Type &_other) const {
    assert(_other.getKind() == UNION);

    const UnionType &other = (const UnionType &)_other;

    if (getConstructors().size() != other.getConstructors().size())
        return getConstructors().size() < other.getConstructors().size();

    typedef std::map<std::wstring, std::pair<UnionConstructorDeclarationPtr, UnionConstructorDeclarationPtr> > NameMap;
    NameMap conses;

    for (size_t i = 0; i < getConstructors().size(); ++i) {
        conses[getConstructors().get(i)->getName()].first = getConstructors().get(i);
        conses[other.getConstructors().get(i)->getName()].second = other.getConstructors().get(i);
    }

    for (NameMap::iterator i = conses.begin(); i != conses.end(); ++i) {
        UnionConstructorDeclarationPtr pCons = i->second.first;
        UnionConstructorDeclarationPtr pOtherCons = i->second.second;

        if (!pCons)
            return false;

        if (!pOtherCons)
            return true;

        if (tc::TupleType(&pCons->getFields()) < tc::TupleType(&pOtherCons->getFields()))
            return true;

        if (tc::TupleType(&pOtherCons->getFields()) < tc::TupleType(&pCons->getFields()))
            return false;
    }

    return false;
}

int UnionType::getMonotonicity(const Type &_var) const {
    bool bMonotone = false, bAntitone = false;

    for (size_t i = 0; i < getConstructors().size(); ++i)
        for (size_t j = 0; j < getConstructors().get(i)->getFields().size(); ++j) {
            TypePtr pType = getConstructors().get(i)->getFields().get(j)->getType();
            const int mt = pType->getMonotonicity(_var);

            bMonotone |= mt == MT_MONOTONE;
            bAntitone |= mt == MT_ANTITONE;

            if ((bMonotone && bAntitone) || mt == MT_NONE)
                return MT_NONE;
        }

    return bMonotone ? MT_MONOTONE : (bAntitone ? MT_ANTITONE : MT_CONST);
}
