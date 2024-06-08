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
        const auto branch = m_paramsOut.get(j);
        for (size_t i = 0; i < branch->size(); ++i)
            if (branch->get(i)->getType()->hasFresh())
                return true;
    }

    return false;
}

bool PredicateType::contains(const Type &_type) const {
    for (size_t i = 0; i < m_paramsIn.size(); ++i)
        if (*m_paramsIn.get(i)->getType() == _type || m_paramsIn.get(i)->getType()->contains(_type))
            return true;

    for (size_t j = 0; j < m_paramsOut.size(); ++j) {
        const auto branch = m_paramsOut.get(j);
        for (size_t i = 0; i < branch->size(); ++i)
            if (*branch->get(i)->getType() == _type || branch->get(i)->getType()->contains(_type))
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
        const auto branch = getOutParams().get(i);
        const auto branchOther = other.getOutParams().get(i);

        if (branch->size() != branchOther->size())
            return branch->size() < branchOther->size();
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
        const auto branch = getOutParams().get(i);
        const auto branchOther = other.getOutParams().get(i);

        for (size_t j = 0; j < branch->size(); ++j) {
            const auto p = branch->get(j);
            const auto q = branchOther->get(j);

            if (*p->getType() < *q->getType())
                return true;

            if (*q->getType() < *p->getType())
                return false;
        }
    }

    return false;
}

bool PredicateType::rewrite(const TypePtr &_pOld, const TypePtr &_pNew, bool _bRewriteFlags) {
    bool bResult = false;

    for (size_t i = 0; i < m_paramsIn.size(); ++i) {
        auto p = m_paramsIn.get(i)->getType();
        if (tc::rewriteType(p, _pOld, _pNew, _bRewriteFlags)) {
            bResult = true;
            m_paramsIn.get(i)->setType(p);
        }
    }

    for (size_t j = 0; j < m_paramsOut.size(); ++j) {
        const auto branch = m_paramsOut.get(j);
        for (size_t i = 0; i < branch->size(); ++i) {
            auto p = branch->get(i)->getType();
            if (tc::rewriteType(p, _pOld, _pNew, _bRewriteFlags)) {
                bResult = true;
                branch->get(i)->setType(p);
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

    Order order(ORD_EQUALS);

    for (size_t i = 0; i < getInParams().size(); ++i)
        if (order.in(*getInParams().get(i)->getType(),
                *other.getInParams().get(i)->getType()) == ORD_NONE)
            return ORD_NONE;

    for (size_t j = 0; j < getOutParams().size(); ++j) {
        const Branch &b = *getOutParams().get(j);
        const Branch &c = *other.getOutParams().get(j);

        if (b.size() != c.size())
            return ORD_NONE;

        for (size_t i = 0; i < b.size(); ++ i)
            if (order.out(*b.get(i)->getType(), *c.get(i)->getType()) == ORD_NONE)
                return ORD_NONE;
    }

    return order;
}

TypePtr PredicateType::getMeet(const ir::TypePtr &_other) {
    SideType meet = _getMeet(_other);
    if (meet.first || meet.second || _other->getKind() == FRESH)
        return meet.first;

    if (_other->getKind() == FRESH)
        return NULL;

    const auto other = _other->as<PredicateType>();

    if (getInParams().size() != other->getInParams().size())
        return std::make_shared<Type>(BOTTOM);

    if (getOutParams().size() != other->getOutParams().size())
        return std::make_shared<Type>(BOTTOM);

    for (size_t j = 0; j < getOutParams().size(); ++j) {
        const auto b = getOutParams().get(j);
        const auto c = other->getOutParams().get(j);

        if (b->size() != c->size())
            return std::make_shared<Type>(BOTTOM);
    }

    const auto pType = std::make_shared<PredicateType>();

    for (size_t i = 0; i < getInParams().size(); ++i) {
        const auto p = getInParams().get(i);
        const auto q = other->getInParams().get(i);

        if (const auto pJoin = p->getType()->getJoin(q->getType()))
            pType->getInParams().add(std::make_shared<Param>(L"", pJoin, false));
        else
            return NULL;
    }

    for (size_t j = 0; j < getOutParams().size(); ++j) {
        const auto b = getOutParams().get(j);
        const auto c = other->getOutParams().get(j);
        const auto pBranch = std::make_shared<Branch>();

        pType->getOutParams().add(pBranch);

        for (size_t i = 0; i < b->size(); ++ i) {
            const auto p = b->get(i);
            const auto q = c->get(i);

            if (const auto pMeet = p->getType()->getMeet(q->getType()))
                pBranch->add(std::make_shared<Param>(L"", pMeet, true));
            else
                return nullptr;
        }
    }

    return pType;
}

TypePtr PredicateType::getJoin(const ir::TypePtr &_other) {
    SideType join = _getJoin(_other);
    if (join.first || join.second || _other->getKind() == FRESH)
        return join.first;

    if (_other->getKind() == FRESH)
        return NULL;

    const auto other = _other->as<PredicateType>();

    if (getInParams().size() != other->getInParams().size())
        return std::make_shared<Type>(TOP);

    if (getOutParams().size() != other->getOutParams().size())
        return std::make_shared<Type>(TOP);

    for (size_t j = 0; j < getOutParams().size(); ++j) {
        const auto b = getOutParams().get(j);
        const auto c = other->getOutParams().get(j);

        if (b->size() != c->size())
            return std::make_shared<Type>(TOP);
    }

    const auto pType = std::make_shared<PredicateType>();

    for (size_t i = 0; i < getInParams().size(); ++i) {
        const auto p = getInParams().get(i);
        const auto q = other->getInParams().get(i);

        if (const auto pMeet = p->getType()->getMeet(q->getType()))
            pType->getInParams().add(std::make_shared<Param>(L"", pMeet, false));
        else
            return nullptr;
    }

    for (size_t j = 0; j < getOutParams().size(); ++j) {
        const auto b = getOutParams().get(j);
        const auto c = other->getOutParams().get(j);
        const auto pBranch = std::make_shared<Branch>();

        pType->getOutParams().add(pBranch);

        for (size_t i = 0; i < b->size(); ++ i) {
            const auto p = b->get(i);
            const auto q = c->get(i);

            if (const auto pJoin = p->getType()->getJoin(q->getType()))
                pBranch->add(std::make_shared<Param>(L"", pJoin, true));
            else
                return nullptr;
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
        const auto branch = m_paramsOut.get(j);
        for (size_t i = 0; i < branch->size(); ++i) {
            const TypePtr pType = branch->get(i)->getType();
            const int mt = pType->getMonotonicity(_var);

            bMonotone |= mt == MT_MONOTONE;
            bAntitone |= mt == MT_ANTITONE;

            if ((bMonotone && bAntitone) || mt == MT_NONE)
                return MT_NONE;
        }
    }

    return bMonotone ? MT_MONOTONE : (bAntitone ? MT_ANTITONE : MT_CONST);
}

NodePtr PredicateType::clone(Cloner &_cloner) const {
    const auto pCopy = NEW_CLONE(this, _cloner, _cloner.get<Formula>(getPreCondition()), _cloner.get<Formula>(getPostCondition()));
    pCopy->getInParams().appendClones(getInParams(), _cloner);
    pCopy->getOutParams().appendClones(getOutParams(), _cloner);
    return pCopy;
}
