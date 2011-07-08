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

        if (p.getType() != q.getType())
            return *p.getType() < *q.getType();
    }

    for (size_t i = 0; i < getOutParams().size(); ++i) {
        Branch &branch = *getOutParams().get(i);
        Branch &branchOther = *other.getOutParams().get(i);

        for (size_t j = 0; j < branch.size(); ++j) {
            const Param &p = *branch.get(j);
            const Param &q = *branchOther.get(j);

            if (p.getType() != q.getType())
                return *p.getType() < *q.getType();
        }
    }

    return false;
}

bool PredicateType::rewrite(const TypePtr &_pOld, const TypePtr &_pNew) {
    bool bResult = false;

    for (size_t i = 0; i < m_paramsIn.size(); ++i) {
        TypePtr p = m_paramsIn.get(i)->getType();
        if (tc::rewriteType(p, _pOld, _pNew)) {
            bResult = true;
            m_paramsIn.get(i)->setType(p);
        }
    }

    for (size_t j = 0; j < m_paramsOut.size(); ++j) {
        Branch &branch = *m_paramsOut.get(j);
        for (size_t i = 0; i < branch.size(); ++i) {
            TypePtr p = branch.get(i)->getType();
            if (tc::rewriteType(p, _pOld, _pNew)) {
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
