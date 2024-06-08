/// \file type_subtype.cpp
///

#include "ir/base.h"
#include "ir/types.h"
#include "ir/declarations.h"
#include "ir/visitor.h"
#include "typecheck.h"

using namespace ir;

// Subtype types.

bool Subtype::less(const Type &_other) const {
    assert(_other.getKind() == SUBTYPE);
    const Subtype &other = (const Subtype &)_other;
    if (!_equals(getParam(), other.getParam()))
        return _less(getParam(), other.getParam());
    return _less(getExpression(), other.getExpression());
}

TypePtr Subtype::getMeet(const TypePtr &_other) {
    SideType meet = _getMeet(_other);
    if (meet.first || meet.second || _other->getKind() == FRESH)
        return meet.first;

    if (_other->getKind() == FRESH)
        return NULL;

    const auto other = _other->as<Subtype>();

    const auto pType = std::make_shared<Subtype>();
    NamedValuePtr pParam;
    Cloner cloner;

    cloner.alias(getParam(), other->getParam());

    if (*other->getParam()->getType() != *getParam()->getType()) {
        if (const auto pMeet = getParam()->getType()->getMeet(other->getParam()->getType())) {
            pParam = std::make_shared<NamedValue>(L"", pMeet);
            cloner.inject(pParam, getParam());
        } else
            return NULL;
    }

    if (!pParam)
        pParam = cloner.get(getParam());

    ExpressionPtr
        pExprThis = cloner.get(getExpression()),
        pExprOther = cloner.get(other->getExpression());

    ExpressionPtr pExpr;
    if (Expression::implies(pExprThis, pExprOther))
        pExpr = pExprThis;
    else if (Expression::implies(pExprOther, pExprThis))
        pExpr = pExprOther;
    else
        pExpr = std::make_shared<Binary>(Binary::BOOL_AND, pExprThis, pExprOther);

    pType->setParam(pParam);
    pType->setExpression(pExpr);
    return pType;
}

TypePtr Subtype::getJoin(const TypePtr &_other) {
    const auto join = _getJoin(_other);
    if (join.first || join.second || _other->getKind() == FRESH)
        return join.first;

    if (_other->getKind() == FRESH)
        return NULL;

    const auto other = _other->as<Subtype>();

    const auto pType = std::make_shared<Subtype>();
    auto pParam = getParam();
    Cloner cloner;

    cloner.alias(getParam(), other->getParam());

    if (*other->getParam()->getType() != *getParam()->getType()) {
        if (const auto pJoin = getParam()->getType()->getJoin(other->getParam()->getType())) {
            pParam = std::make_shared<NamedValue>(L"", pJoin);
            cloner.inject(pParam, getParam());
        } else
            return nullptr;
    }

    if (!pParam)
        pParam = cloner.get(getParam());

    ExpressionPtr
        pExprThis = cloner.get(getExpression()),
        pExprOther = cloner.get(other->getExpression());

    ExpressionPtr pExpr;
    if (Expression::implies(pExprThis, pExprOther))
        pExpr = pExprOther;
    else if (Expression::implies(pExprOther, pExprThis))
        pExpr = pExprThis;
    else
        pExpr = std::make_shared<Binary>(Binary::BOOL_OR, pExprThis, pExprOther);

    pType->setParam(pParam);
    pType->setExpression(pExpr);

    return pType;
}

bool Subtype::rewrite(const TypePtr &_pOld, const TypePtr &_pNew, bool _bRewriteFlags) {
    TypePtr pType = getParam()->getType();
    if (tc::rewriteType(pType, _pOld, _pNew, _bRewriteFlags)) {
        getParam()->setType(pType);
        return true;
    }
    return false;
}

int Subtype::compare(const Type &_other) const {
    if (_other.getKind() == FRESH)
        return ORD_UNKNOWN;
    if (_other.getKind() == TOP)
        return ORD_SUB;
    if (_other.getKind() == BOTTOM)
        return ORD_SUPER;

    if (_other.getKind() == SUBTYPE) {
        const Subtype &other = (const Subtype &)_other;
        const int nOrder = getParam()->getType()->compare(*other.getParam()->getType());
        int nResult = nOrder & (ORD_NONE | ORD_UNKNOWN);

        if (nOrder & ORD_EQUALS) {
            if (Expression::_matches(getExpression(), other.getExpression()))
                nResult |= ORD_EQUALS;
            else if (Expression::implies(getExpression(), other.getExpression()))
                nResult |= ORD_SUB;
            else if (Expression::implies(other.getExpression(), getExpression()))
                nResult |= ORD_SUPER;
            else
                nResult |= ORD_UNKNOWN;
        }

        if (nOrder & ORD_SUB)
            nResult |= Expression::implies(getExpression(),
                    other.getExpression()) ? ORD_SUB : ORD_UNKNOWN;

        if (nOrder & ORD_SUPER)
            nResult |= Expression::implies(other.getExpression(),
                    getExpression()) ? ORD_SUPER : ORD_UNKNOWN;

        return nResult;
    }

    int nOrder = getParam()->getType()->compare(_other);

    if (nOrder & ORD_SUPER)
        nOrder = (nOrder & ~ORD_SUPER) | ORD_UNKNOWN;

    if (nOrder & ORD_EQUALS)
        nOrder = (nOrder & ~ORD_EQUALS) | ORD_SUB;

    return nOrder;
}

int Subtype::getMonotonicity(const Type &_var) const {
    return getParam()->getType()->getMonotonicity(_var);
}
