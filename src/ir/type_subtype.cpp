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
    if (TypePtr pMeet = Type::getMeet(_other))
        return pMeet;
    if (_other.getKind() == FRESH)
        return NULL;

    const Subtype &other = (const Subtype &)_other;

    SubtypePtr pType = new Subtype();
    NamedValuePtr pParam = getParam();

    if (*other.getParam()->getType() != *getParam()->getType())
        pParam = new NamedValue(getParam()->getName(), getParam()->getType()->getMeet(*other.getParam()->getType()));

    Cloner cloner;
    NamedValuePtr pNewParam = cloner.get(pParam);
    cloner.alias(pParam, getParam());
    cloner.alias(pParam, other.getParam());

    ExpressionPtr pExpr;
    if (Expression::implies(getExpression(), other.getExpression()))
        pExpr = cloner.get(getExpression());
    else if (Expression::implies(other.getExpression(), getExpression()))
        pExpr = cloner.get(other.getExpression());
    else
        pExpr = new Binary(Binary::BOOL_AND, cloner.get(getExpression()), cloner.get(other.getExpression()));

    pType->setParam(pParam);
    pType->setExpression(pExpr);
    return pType;
}

TypePtr Subtype::getJoin(Type &_other) {
    if (TypePtr pJoin = Type::getJoin(_other))
        return pJoin;

    if (_other.getKind() == FRESH)
        return NULL;

    const Subtype &other = (const Subtype &)_other;

    SubtypePtr pType = new Subtype();
    NamedValuePtr pParam = getParam();

    if (*other.getParam()->getType() != *getParam()->getType())
        pParam = new NamedValue(getParam()->getName(), getParam()->getType()->getJoin(*other.getParam()->getType()));

    Cloner cloner;
    NamedValuePtr pNewParam = cloner.get(pParam);
    cloner.alias(pParam, getParam());
    cloner.alias(pParam, other.getParam());

    ExpressionPtr pExpr;
    if (Expression::implies(getExpression(), other.getExpression()))
        pExpr = cloner.get(other.getExpression());
    else if (Expression::implies(other.getExpression(), getExpression()))
        pExpr = cloner.get(getExpression());
    else
        pExpr = new Binary(Binary::BOOL_OR, cloner.get(getExpression()), cloner.get(other.getExpression()));

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