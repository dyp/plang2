/// \file type_predicate.cpp
///

#include "ir/base.h"
#include "ir/types.h"
#include "ir/declarations.h"
#include "typecheck.h"

using namespace ir;

// Predicate types.

bool PredicateType::hasFresh() const {
    for (size_t i = 0; i < m_paramsIn.size(); ++i)
        if (m_paramsIn.get(i)->getType()->hasFresh())
            return true;

    for (size_t j = 0; j < m_paramsOut.size(); ++j) {
        Branch & branch = *m_paramsOut.get(j);
        for (size_t i = 0; i < branch.size(); ++i)
            if (branch.get(i)->getType()->hasFresh())
                return true;
    }

    return false;
}

bool PredicateType::contains(const TypePtr &_pType) const {
    for (size_t i = 0; i < m_paramsIn.size(); ++i)
        if (*m_paramsIn.get(i)->getType() == *_pType || m_paramsIn.get(i)->getType()->contains(_pType))
            return true;

    for (size_t j = 0; j < m_paramsOut.size(); ++j) {
        Branch & branch = *m_paramsOut.get(j);
        for (size_t i = 0; i < branch.size(); ++i)
            if (*branch.get(i)->getType() == *_pType || branch.get(i)->getType()->contains(_pType))
                return true;
    }

    return false;
}

bool PredicateType::less(const Type &_other) const {
    assert(_other.getKind() == PREDICATE);

    const PredicateType &other = (const PredicateType &)_other;

    if (m_paramsIn.size() != other.m_paramsIn.size())
        return m_paramsIn.size() < other.m_paramsIn.size();

    if (m_paramsOut.size() != other.m_paramsOut.size())
        return m_paramsOut.size() < other.m_paramsOut.size();

    for (size_t i = 0; i < getOutParams().size(); ++i) {
        Branch &branch = *getOutParams().get(i);
        Branch &branchOther = *other.getOutParams().get(i);

        if (branch.size() != branchOther.size())
            return branch.size() < branchOther.size();
    }

    for (size_t i = 0; i < getInParams().size(); ++i) {
        const Param &p = *getInParams().get(i);
        const Param &q = *other.getInParams().get(i);

        if (*p.getType() < *q.getType())
            return true;

        if (*q.getType() < *p.getType())
            return false;
    }

    for (size_t i = 0; i < getOutParams().size(); ++i) {
        Branch &branch = *getOutParams().get(i);
        Branch &branchOther = *other.getOutParams().get(i);

        for (size_t j = 0; j < branch.size(); ++j) {
            const Param &p = *branch.get(j);
            const Param &q = *branchOther.get(j);

            if (*p.getType() < *q.getType())
                return true;

            if (*q.getType() < *p.getType())
                return false;
        }
    }

    return false;
}

bool PredicateType::rewrite(const TypePtr &_pOld, const TypePtr &_pNew, bool _bRewriteFlags) {
    bool bResult = false;

    for (size_t i = 0; i < m_paramsIn.size(); ++i) {
        TypePtr p = m_paramsIn.get(i)->getType();
        if (tc::rewriteType(p, _pOld, _pNew, _bRewriteFlags)) {
            bResult = true;
            m_paramsIn.get(i)->setType(p);
        }
    }

    for (size_t j = 0; j < m_paramsOut.size(); ++j) {
        Branch &branch = *m_paramsOut.get(j);
        for (size_t i = 0; i < branch.size(); ++i) {
            TypePtr p = branch.get(i)->getType();
            if (tc::rewriteType(p, _pOld, _pNew, _bRewriteFlags)) {
                bResult = true;
                branch.get(i)->setType(p);
            }
        }
    }

    return bResult;
}

int PredicateType::compare(const Type &_other) const {
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

    const PredicateType &other = (const PredicateType &)_other;

    if (getInParams().size() != other.getInParams().size())
        return ORD_NONE;

    if (getOutParams().size() != other.getOutParams().size())
        return ORD_NONE;

    bool bSub = false, bSuper = false;

    for (size_t i = 0; i < getInParams().size(); ++i) {
        const Param &p = *getInParams().get(i);
        const Param &q = *other.getInParams().get(i);

        switch (p.getType()->compare(*q.getType())) {
            case ORD_UNKNOWN:
                return ORD_UNKNOWN;
            case ORD_NONE:
                return ORD_NONE;
            case ORD_SUPER:
                if (bSuper)
                    return ORD_NONE;
                bSub = true;
                break;
            case ORD_SUB:
                if (bSub)
                    return ORD_NONE;
                bSuper = true;
                break;
        }
    }

    for (size_t j = 0; j < getOutParams().size(); ++j) {
        const Branch &b = *getOutParams().get(j);
        const Branch &c = *other.getOutParams().get(j);

        if (b.size() != c.size())
            return ORD_NONE;

        for (size_t i = 0; i < b.size(); ++ i) {
            const Param &p = *b.get(i);
            const Param &q = *c.get(i);

            // Sub/Super is inverted for output parameters.
            switch (p.getType()->compare(*q.getType())) {
                case ORD_UNKNOWN:
                    return ORD_UNKNOWN;
                case ORD_NONE:
                    return ORD_NONE;
                case ORD_SUB:
                    if (bSuper)
                        return ORD_NONE;
                    bSub = true;
                    break;
                case ORD_SUPER:
                    if (bSub)
                        return ORD_NONE;
                    bSuper = true;
                    break;
            }
        }
    }

    return bSub ? ORD_SUB : (bSuper ? ORD_SUPER : ORD_EQUALS);
}

TypePtr PredicateType::getMeet(ir::Type &_other) {
    SideType meet = _getMeet(_other);
    if (meet.first || meet.second || _other.getKind() == FRESH)
        return meet.first;

    if (_other.getKind() == FRESH)
        return NULL;

    const PredicateType &other = (const PredicateType &)_other;

    if (getInParams().size() != other.getInParams().size())
        return new Type(BOTTOM);

    if (getOutParams().size() != other.getOutParams().size())
        return new Type(BOTTOM);

    for (size_t j = 0; j < getOutParams().size(); ++j) {
        const Branch &b = *getOutParams().get(j);
        const Branch &c = *other.getOutParams().get(j);

        if (b.size() != c.size())
            return new Type(BOTTOM);
    }

    PredicateTypePtr pType = new PredicateType();

    for (size_t i = 0; i < getInParams().size(); ++i) {
        const Param &p = *getInParams().get(i);
        const Param &q = *other.getInParams().get(i);

        if (TypePtr pJoin = p.getType()->getJoin(*q.getType()))
            pType->getInParams().add(new Param(L"", pJoin, false));
        else
            return NULL;
    }

    for (size_t j = 0; j < getOutParams().size(); ++j) {
        const Branch &b = *getOutParams().get(j);
        const Branch &c = *other.getOutParams().get(j);
        Branch *pBranch = new Branch();

        pType->getOutParams().add(pBranch);

        for (size_t i = 0; i < b.size(); ++ i) {
            const Param &p = *b.get(i);
            const Param &q = *c.get(i);

            if (TypePtr pMeet = p.getType()->getMeet(*q.getType()))
                pBranch->add(new Param(L"", pMeet, true));
            else
                return NULL;
        }
    }

    return pType;
}

TypePtr PredicateType::getJoin(ir::Type &_other) {
    SideType join = _getJoin(_other);
    if (join.first || join.second || _other.getKind() == FRESH)
        return join.first;

    if (_other.getKind() == FRESH)
        return NULL;

    const PredicateType &other = (const PredicateType &)_other;

    if (getInParams().size() != other.getInParams().size())
        return new Type(TOP);

    if (getOutParams().size() != other.getOutParams().size())
        return new Type(TOP);

    for (size_t j = 0; j < getOutParams().size(); ++j) {
        const Branch &b = *getOutParams().get(j);
        const Branch &c = *other.getOutParams().get(j);

        if (b.size() != c.size())
            return new Type(TOP);
    }

    PredicateTypePtr pType = new PredicateType();

    for (size_t i = 0; i < getInParams().size(); ++i) {
        const Param &p = *getInParams().get(i);
        const Param &q = *other.getInParams().get(i);

        if (TypePtr pMeet = p.getType()->getMeet(*q.getType()))
            pType->getInParams().add(new Param(L"", pMeet, false));
        else
            return NULL;
    }

    for (size_t j = 0; j < getOutParams().size(); ++j) {
        const Branch &b = *getOutParams().get(j);
        const Branch &c = *other.getOutParams().get(j);
        Branch *pBranch = new Branch();

        pType->getOutParams().add(pBranch);

        for (size_t i = 0; i < b.size(); ++ i) {
            const Param &p = *b.get(i);
            const Param &q = *c.get(i);

            if (TypePtr pJoin = p.getType()->getJoin(*q.getType()))
                pBranch->add(new Param(L"", pJoin, true));
            else
                return NULL;
        }
    }

    return pType;
}

int PredicateType::getMonotonicity(const Type &_var) const {
    bool bMonotone = false, bAntitone = false;

    for (size_t i = 0; i <  getInParams().size(); ++i) {
        TypePtr pType = getInParams().get(i)->getType();
        const int mt = pType->getMonotonicity(_var);

        // Reversed.
        bMonotone |= mt == MT_ANTITONE;
        bAntitone |= mt == MT_MONOTONE;

        if ((bMonotone && bAntitone) || mt == MT_NONE)
            return MT_NONE;
    }

    for (size_t j = 0; j < m_paramsOut.size(); ++j) {
        Branch &branch = *m_paramsOut.get(j);
        for (size_t i = 0; i < branch.size(); ++i) {
            TypePtr pType = branch.get(i)->getType();
            const int mt = pType->getMonotonicity(_var);

            bMonotone |= mt == MT_MONOTONE;
            bAntitone |= mt == MT_ANTITONE;

            if ((bMonotone && bAntitone) || mt == MT_NONE)
                return MT_NONE;
        }
    }

    return bMonotone ? MT_MONOTONE : (bAntitone ? MT_ANTITONE : MT_CONST);
}
