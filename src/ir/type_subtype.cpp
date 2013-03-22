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

TypePtr Subtype::getMeet(Type &_other) {
    SideType meet = _getMeet(_other);
    if (meet.first || meet.second || _other.getKind() == FRESH)
        return meet.first;

    if (_other.getKind() == FRESH)
        return NULL;

    const Subtype &other = (const Subtype &)_other;

    SubtypePtr pType = new Subtype();
    NamedValuePtr pParam = getParam();

    if (*other.getParam()->getType() != *getParam()->getType()) {
        pParam = new NamedValue(L"", getParam()->getType()->getMeet(*other.getParam()->getType()));
        if (!pParam->getType())
            return NULL;
    }

    Cloner cloner;
    NamedValuePtr pNewParam = cloner.get(pParam);
    cloner.alias(pParam, getParam());
    cloner.alias(pParam, other.getParam());

    ExpressionPtr
        pExprThis = cloner.get(getExpression()),
        pExprOther = cloner.get(other.getExpression());

    ExpressionPtr pExpr;
    if (Expression::implies(pExprThis, pExprOther))
        pExpr = pExprThis;
    else if (Expression::implies(pExprOther, pExprThis))
        pExpr = pExprOther;
    else
        pExpr = new Binary(Binary::BOOL_AND, pExprThis, pExprOther);

    pType->setParam(pNewParam);
    pType->setExpression(pExpr);
    return pType;
}

TypePtr Subtype::getJoin(Type &_other) {
    SideType join = _getJoin(_other);
    if (join.first || join.second || _other.getKind() == FRESH)
        return join.first;

    if (_other.getKind() == FRESH)
        return NULL;

    const Subtype &other = (const Subtype &)_other;

    SubtypePtr pType = new Subtype();
    NamedValuePtr pParam = getParam();

    if (*other.getParam()->getType() != *getParam()->getType()) {
        pParam = new NamedValue(L"", getParam()->getType()->getJoin(*other.getParam()->getType()));
        if (!pParam->getType())
            return NULL;
    }

    Cloner cloner;
    NamedValuePtr pNewParam = cloner.get(pParam);
    cloner.alias(pParam, getParam());
    cloner.alias(pParam, other.getParam());

    ExpressionPtr
        pExprThis = cloner.get(getExpression()),
        pExprOther = cloner.get(other.getExpression());

    ExpressionPtr pExpr;
    if (Expression::implies(pExprThis, pExprOther))
        pExpr = pExprOther;
    else if (Expression::implies(pExprOther, pExprThis))
        pExpr = pExprThis;
    else
        pExpr = new Binary(Binary::BOOL_OR, pExprThis, pExprOther);

    pType->setParam(pNewParam);
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
        const Subtype& other= (const Subtype&)_other;
        switch (getParam()->getType()->compare(*other.getParam()->getType())) {
            case ORD_EQUALS:
                if (Expression::matches(getExpression(), other.getExpression()))
                    return ORD_EQUALS;
                if (Expression::implies(getExpression(), other.getExpression()))
                    return ORD_SUB;
                if (Expression::implies(other.getExpression(), getExpression()))
                    return ORD_SUPER;
                return ORD_UNKNOWN;

            case ORD_SUB:      return Expression::implies(getExpression(), other.getExpression()) ? ORD_SUB : ORD_UNKNOWN;
            case ORD_SUPER:    return Expression::implies(other.getExpression(), getExpression()) ? ORD_SUPER : ORD_UNKNOWN;
            case ORD_UNKNOWN:  return ORD_UNKNOWN;
            case ORD_NONE:     return ORD_NONE;
        }
    }


    switch (getParam()->getType()->compare(_other)) {
        case ORD_NONE:
            return ORD_NONE;
        case ORD_SUPER:
        case ORD_UNKNOWN:
            return ORD_UNKNOWN;
        case ORD_SUB:
        case ORD_EQUALS:
            return ORD_SUB;
    }
}

int Subtype::getMonotonicity(const Type &_var) const {
    return getParam()->getType()->getMonotonicity(_var);
}
