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

    if (_other.getKind() == SUBTYPE)
        return inverse(((const Subtype&)_other).compare(*this));

    if (_other.getKind() != UNION)
        return ORD_NONE;

    const UnionType &other = (const UnionType &)_other;
    size_t cUnmatched = 0, cOtherUnmatched = other.getConstructors().size();
    Order order(ORD_EQUALS);

    for (size_t i = 0; i < m_constructors.size(); ++i) {
        const UnionConstructorDeclaration &cons = *m_constructors.get(i);
        const size_t cOtherConsIdx = other.getConstructors().findByNameIdx(cons.getName());

        if (cOtherConsIdx != (size_t)-1) {
            const UnionConstructorDeclaration &otherCons = *other.getConstructors().get(cOtherConsIdx);

            if (!cons.getFields() || !otherCons.getFields())
                return ORD_NONE;

            if (order.out(*cons.getFields(), *otherCons.getFields()) == ORD_NONE)
                return ORD_NONE;

            --cOtherUnmatched;
        } else
            ++cUnmatched;
    }

    if (cUnmatched > 0)
        order.out(ORD_SUPER);

    if (cOtherUnmatched > 0)
        order.out(ORD_SUB);

    return order;
}

bool UnionType::hasFresh() const {
    for (size_t i = 0; i < m_constructors.size(); ++i)
        if (m_constructors.get(i)->getFields() &&
            m_constructors.get(i)->getFields()->hasFresh())
            return true;
    return false;
}

bool UnionType::contains(const TypePtr &_pType) const {
    for (size_t i = 0; i < m_constructors.size(); ++i)
        if (m_constructors.get(i)->getFields() &&
            m_constructors.get(i)->getFields()->contains(_pType))
            return true;
    return false;
}

bool UnionType::rewrite(const TypePtr &_pOld, const TypePtr &_pNew, bool _bRewriteFlags) {
    bool bResult = false;

    for (size_t i = 0; i < m_constructors.size(); ++i) {
        TypePtr pField = m_constructors.get(i)->getFields();

        if (!pField)
            continue;

        if (tc::rewriteType(pField, _pOld, _pNew, _bRewriteFlags)) {
            bResult = true;
            m_constructors.get(i)->setFields(pField);
        }
    }

    return bResult;
}

TypePtr UnionType::getMeet(ir::Type &_other) {
    SideType meet = _getMeet(_other);
    if (meet.first || meet.second || _other.getKind() == FRESH)
        return meet.first;

    const UnionType &other = (const UnionType &)_other;
    UnionTypePtr pUnion = new UnionType();

    for (size_t i = 0; i < m_constructors.size(); ++i) {
        UnionConstructorDeclaration &cons = *m_constructors.get(i);
        const size_t cOtherConsIdx = other.getConstructors().findByNameIdx(cons.getName());

        if (cOtherConsIdx != (size_t)-1) {
            const UnionConstructorDeclaration &otherCons = *other.getConstructors().get(cOtherConsIdx);

            TypePtr pMeet = cons.getFields()->getMeet(*otherCons.getFields());

            if (!pMeet)
                return NULL;

            UnionConstructorDeclarationPtr pCons = new UnionConstructorDeclaration(cons.getName());

            pCons->setFields(pMeet);
            pUnion->getConstructors().add(pCons);
        }
    }

    if (pUnion->getConstructors().empty())
        return new Type(BOTTOM);

    return pUnion;
}

TypePtr UnionType::getJoin(ir::Type &_other) {
    SideType join = _getJoin(_other);
    if (join.first || join.second || _other.getKind() == FRESH)
        return join.first;

    const UnionType &other = (const UnionType &)_other;
    size_t cOtherUnmatched = other.getConstructors().size();
    UnionTypePtr pUnion = new UnionType();

    for (size_t i = 0; i < m_constructors.size(); ++i) {
        UnionConstructorDeclaration &cons = *m_constructors.get(i);
        const size_t cOtherConsIdx = other.getConstructors().findByNameIdx(cons.getName());

        if (cOtherConsIdx != (size_t) -1) {
            const UnionConstructorDeclaration &otherCons = *other.getConstructors().get(cOtherConsIdx);

            TypePtr pJoin = cons.getFields()->getJoin(*otherCons.getFields());

            if (!pJoin)
                return NULL;

            UnionConstructorDeclarationPtr pCons = new UnionConstructorDeclaration(cons.getName());

            pCons->setFields(pJoin);
            pUnion->getConstructors().add(pCons);
            --cOtherUnmatched;
        } else {
            UnionConstructorDeclarationPtr pCons = new UnionConstructorDeclaration(cons.getName());

            pCons->setFields(cons.getFields());
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
        pCons->setFields(otherCons.getFields());
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

    for (NameMap::iterator i = conses.begin(); i != conses.end(); ++i)
        if (!_equals(i->second.first, i->second.second))
            return _less(i->second.first, i->second.second);

    return false;
}

int UnionType::getMonotonicity(const Type &_var) const {
    bool bMonotone = false, bAntitone = false;

    for (size_t i = 0; i < getConstructors().size(); ++i) {
        const int mt = getConstructors().get(i)->getFields()->getMonotonicity(_var);

        bMonotone |= mt == MT_MONOTONE;
        bAntitone |= mt == MT_ANTITONE;

        if ((bMonotone && bAntitone) || mt == MT_NONE)
            return MT_NONE;
    }

    return bMonotone ? MT_MONOTONE : (bAntitone ? MT_ANTITONE : MT_CONST);
}
