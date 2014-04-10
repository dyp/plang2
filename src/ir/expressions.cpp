/// \file expressions.cpp
///

#include "ir/expressions.h"
#include "ir/types.h"
#include "ir/declarations.h"
#include "ir/statements.h"
#include "ir/visitor.h"
#include "utils.h"
#include "typecheck.h"

using namespace ir;

std::wstring NamedReferenceType::getName() const {
    return m_pDecl ? m_pDecl->getName() : L"";
}

std::wstring FormulaCall::getName() const {
    return m_pTarget ? m_pTarget->getName() : L"";
}

const std::wstring &PredicateReference::getName() const {
    return m_pTarget ? m_pTarget->getName() : m_strName;
}

bool UnionConstructor::isComplete() const {
    return m_decls.empty() && m_pProto && m_pProto->getStructFields()
        && size() == m_pProto->getStructFields()->size();
}

void AnonymousPredicate::updateType() const {
    PredicateTypePtr pType = new PredicateType();

    pType->getInParams().append(m_paramsIn);
    pType->getOutParams().append(m_paramsOut);
    pType->setPreCondition(m_pPreCond);
    pType->setPostCondition(m_pPostCond);

    const_cast<AnonymousPredicate *>(this)->m_pType = pType;
}

std::wstring VariableDeclaration::getName() const {
    return m_pVar ? m_pVar->getName() : L"";
}

static std::map<std::pair<int, int>, int> g_precMap;

#define ADD_PRECEDENCE(_KIND, _PREC, _OPERATOR) \
    g_precMap.insert(std::pair<std::pair<int, int>, int>(std::pair<int, int>(_KIND, _OPERATOR), _PREC));

static std::map<std::pair<int, int>, int>& _getPrecedenceMap() {
    if (!g_precMap.empty())
        return g_precMap;

    int nPrec = 0;

    ADD_PRECEDENCE(Expression::BINARY, nPrec, Binary::IMPLIES);
    ADD_PRECEDENCE(Expression::BINARY, nPrec, Binary::IFF);
    ADD_PRECEDENCE(Expression::TERNARY, ++nPrec, -1);
    ADD_PRECEDENCE(Expression::BINARY, ++nPrec, Binary::BOOL_OR);
    ADD_PRECEDENCE(Expression::BINARY, ++nPrec, Binary::BOOL_XOR);
    ADD_PRECEDENCE(Expression::BINARY, nPrec, Binary::BOOL_AND);
    ADD_PRECEDENCE(Expression::BINARY, ++nPrec, Binary::EQUALS);
    ADD_PRECEDENCE(Expression::BINARY, nPrec, Binary::NOT_EQUALS);
    ADD_PRECEDENCE(Expression::BINARY, ++nPrec, Binary::LESS);
    ADD_PRECEDENCE(Expression::BINARY, nPrec, Binary::LESS_OR_EQUALS);
    ADD_PRECEDENCE(Expression::BINARY, nPrec, Binary::GREATER);
    ADD_PRECEDENCE(Expression::BINARY, nPrec, Binary::GREATER_OR_EQUALS);
    ADD_PRECEDENCE(Expression::BINARY, ++nPrec, Binary::IN);
    ADD_PRECEDENCE(Expression::BINARY, ++nPrec, Binary::SHIFT_LEFT);
    ADD_PRECEDENCE(Expression::BINARY, nPrec, Binary::SHIFT_RIGHT);
    ADD_PRECEDENCE(Expression::BINARY, ++nPrec, Binary::ADD);
    ADD_PRECEDENCE(Expression::UNARY, nPrec, Unary::PLUS);
    ADD_PRECEDENCE(Expression::BINARY, nPrec, Binary::SUBTRACT);
    ADD_PRECEDENCE(Expression::UNARY, nPrec, Unary::MINUS);
    ADD_PRECEDENCE(Expression::UNARY, ++nPrec, Unary::BOOL_NEGATE);
    ADD_PRECEDENCE(Expression::UNARY, nPrec, Unary::BITWISE_NEGATE);
    ADD_PRECEDENCE(Expression::BINARY, ++nPrec, Binary::MULTIPLY);
    ADD_PRECEDENCE(Expression::BINARY, nPrec, Binary::DIVIDE);
    ADD_PRECEDENCE(Expression::BINARY, nPrec, Binary::REMAINDER);
    ADD_PRECEDENCE(Expression::BINARY, ++nPrec, Binary::POWER);

    return g_precMap;
}
#undef ADD_PRECEDENCE


int Unary::getPrecedence(int _operator) {
    return _getPrecedenceMap()[std::pair<int, int>(Expression::UNARY, _operator)];
}

int Binary::getPrecedence(int _operator) {
    return _getPrecedenceMap()[std::pair<int, int>(Expression::BINARY, _operator)];
}

int Ternary::getPrecedence() {
    return _getPrecedenceMap()[std::pair<int, int>(Expression::TERNARY, -1)];
}

bool Binary::isSymmetrical() const {
    switch (getOperator()) {
        case ADD:
        case MULTIPLY:
        case EQUALS:
        case NOT_EQUALS:
        case BOOL_AND:
        case BOOL_OR:
        case BOOL_XOR:
        case BITWISE_AND:
        case BITWISE_OR:
        case BITWISE_XOR:
        case IFF:
            return true;
        default:
            return false;
    }
}

int Binary::getInverseOperator() const {
    if (isSymmetrical())
        return getOperator();
    switch (getOperator()) {
        case LESS:
            return GREATER;
        case LESS_OR_EQUALS:
            return GREATER_OR_EQUALS;
        case GREATER:
            return LESS;
        case GREATER_OR_EQUALS:
            return LESS_OR_EQUALS;
        default:
            return -1;
    }
}

bool Expression::matchNamedValues(const NamedValues& _left, const NamedValues& _right) {
    if (_left.size() != _right.size())
        return false;
    for (size_t i=0; i<_left.size(); ++i) {
        bool bEquals = false;
        for (size_t j=0; j<_left.size(); ++j)
            if (_equals(_left.get(i), _right.get(j))) {
                bEquals = true;
                break;
            }
        if (!bEquals)
            return false;
    }
    return true;
}

bool Expression::matchCollections(const Collection<Expression>& _left, const Collection<Expression>& _right, MatchesPtr _pMatches) {
    if (_left.size() != _right.size())
        return false;
    for (size_t i=0; i<_left.size(); ++i) {
        bool bEquals = false;
        for (size_t j=0; j<_left.size(); ++j)
            if (_matches(_left.get(i), _right.get(j), _pMatches)) {
                bEquals = true;
                break;
            }
        if (!bEquals)
            return false;
    }
    return true;
}

bool Expression::_matches(const ExpressionPtr& _pLeft, const ExpressionPtr& _pRight, MatchesPtr _pMatches) {
    if (!_pLeft || !_pRight)
        return (bool)_pLeft == (bool)_pRight;

    MatchesPtr pNewMatches = !_pMatches ? MatchesPtr(new Matches()) : _pMatches;
    Matches oldMatches(*pNewMatches);

    if (!_pLeft->matches(*_pRight, pNewMatches)) {
        pNewMatches->swap(oldMatches);
        return false;
    }

    return true;
}

bool Expression::matches(const Expression& _other, MatchesPtr _pMatches) const {
    if (!_pMatches)
        return getKind() == WILD || _other.getKind() == WILD || _other.getKind() == getKind();
    if (getKind() == WILD && _other.getKind() == WILD)
        return true;
    if (getKind() != WILD && _other.getKind() != WILD)
        return _other.getKind() == getKind();

    const Wild&
        wild = getKind() == WILD ? (const Wild&)*this : (const Wild&)_other;
    const ExpressionPtr
        pExpr = getKind() != WILD ? this : &_other,
        pPattern = _pMatches->getExpression(wild);

    if (pPattern && !_matches(pExpr, pPattern))
        return false;
    if (!pPattern)
        _pMatches->addExpression(wild, pExpr);

    return true;
}

class Substitute : public Visitor {
public:
    Substitute(Matches& _matches) :
        m_matches(_matches)
    {}
    virtual bool visitWild(ir::Wild& _node) {
        ExpressionPtr pExpr = m_matches.getExpression(_node);
        if (pExpr)
            callSetter(pExpr);
        return true;
    }
    void run(Expression& _expr) {
        traverseNode(_expr);
    }

private:
    Matches& m_matches;
};

void Expression::substitute(ExpressionPtr& _pExpr, Matches& _matches) {
    if (!_pExpr)
        return;
    if (_pExpr->getKind() != ir::Expression::WILD)
        Substitute(_matches).run(*_pExpr);
    else
        _pExpr =  _matches.getExpression((Wild&)*_pExpr);
}

class SubstituteByMask : public Visitor {
public:
    SubstituteByMask(const NodePtr& _pNode, const ExpressionPtr &_pFrom, const ExpressionPtr &_pTo) :
        Visitor(CHILDREN_FIRST), m_pRoot(_pNode), m_pFrom(_pFrom), m_pTo(_pTo)
    {}

    bool visitExpression(ir::Expression &_expr) {
        Matches matches;
        if (!_expr.matches(*m_pFrom, &matches))
            return true;

        ExpressionPtr m_pReplacement = clone(*m_pTo);
        Expression::substitute(m_pReplacement, matches);

        if (m_pRoot.ptr() != &_expr) {
            callSetter(m_pReplacement);
            return true;
        }

        m_pRoot = m_pReplacement;
        return true;
    }

    NodePtr substitute() {
        traverseNode(*m_pRoot);
        return m_pRoot;
    }

private:
    NodePtr m_pRoot;
    ExpressionPtr m_pFrom, m_pTo;

};

NodePtr Expression::substitute(const NodePtr &_pNode, const ExpressionPtr &_pFrom, const ExpressionPtr &_pTo) {
    return SubstituteByMask(_pNode, _pFrom, _pTo).substitute();
}

bool Expression::less(const Node& _other) const {
    if (!Node::equals(_other))
        return Node::less(_other);
    const Expression& other = (const Expression&)_other;
    if (getKind() != other.getKind())
        return getKind() < other.getKind();
    return _less(getType(), other.getType());
}

bool Expression::equals(const Node& _other) const {
    if (!Node::equals(_other))
        return false;
    const Expression& other = (const Expression&)_other;
    //FIXME Enable, when typechecking will works.
    return getKind() == other.getKind();// && _equals(getType(), other.getType());
}

bool Expression::implies(const ExpressionPtr& _pLeft, const ExpressionPtr& _pRight) {
    if (_matches(_pLeft, _pRight))
        return true;
    if (!_pLeft || !_pRight)
        return false;

    if (_pLeft->getKind() == Expression::LITERAL
        && _pLeft.as<Literal>()->getLiteralKind() == Literal::BOOL
        && _pLeft.as<Literal>()->getBool() == false)
        return true;
    if (_pRight->getKind() == Expression::LITERAL
        && _pRight.as<Literal>()->getLiteralKind() == Literal::BOOL
        && _pRight.as<Literal>()->getBool() == true)
        return true;

    if (_pLeft->getKind() == BINARY) {
        const Binary& bin = (const Binary&)*_pLeft;
        if (bin.getOperator() == Binary::BOOL_AND && (implies(bin.getLeftSide(), _pRight) || implies(bin.getRightSide(), _pRight)))
            return true;
        if (bin.getOperator() == Binary::BOOL_OR && implies(bin.getLeftSide(), _pRight) && implies(bin.getRightSide(), _pRight))
            return true;
    }
    if (_pRight->getKind() == BINARY) {
        const Binary& bin = (const Binary&)*_pRight;
        if (bin.getOperator() == Binary::BOOL_AND && implies(_pLeft, bin.getLeftSide()) && implies(_pLeft, bin.getRightSide()))
            return true;
        if (bin.getOperator() == Binary::BOOL_OR && (implies(_pLeft, bin.getLeftSide()) || implies(_pLeft, bin.getRightSide())))
            return true;
    }

    return false;
}

const ExpressionPtr& Expression::_cloneTypeTo(const ExpressionPtr& _pExpr, Cloner &_cloner) const {
    _pExpr->setType(_cloner.get(getType()));
    return _pExpr;
}

bool Wild::less(const Node& _other) const {
    if (!Expression::equals(_other))
        return Expression::less(_other);
    return getName() < ((const Wild&)_other).getName();
}

bool Wild::equals(const Node& _other) const {
    if (!Expression::equals(_other))
        return Expression::less(_other);
    return getName() == ((const Wild&)_other).getName();
}

NodePtr Wild::clone(Cloner &_cloner) const {
    return _cloneTypeTo(NEW_CLONE(this, _cloner, Wild(m_strName)), _cloner);
}

bool Literal::less(const Node& _other) const {
    if (!Expression::equals(_other))
        return Expression::less(_other);

    const Literal& other = (Literal&)_other;
    if (getLiteralKind() != other.getLiteralKind())
        return getLiteralKind() < other.getLiteralKind();

    switch (getLiteralKind()) {
        case UNIT:     return false;
        case BOOL:     return getBool() < other.getBool();
        case CHAR:     return getChar() < other.getChar();
        case STRING:   return getString() < other.getString();
        case NUMBER:   return getNumber().toString() < other.getNumber().toString();
    }

    assert(false && "Unreachable");
    return false;
}

bool Literal::equals(const Node& _other) const {
    if (!Expression::equals(_other))
        return false;
    const Literal& other = (Literal&)_other;
    if (getLiteralKind() != other.getLiteralKind())
        return false;
    switch (getLiteralKind()) {
        case UNIT:     return false;
        case BOOL:     return getBool() == other.getBool();
        case CHAR:     return getChar() == other.getChar();
        case STRING:   return getString() == other.getString();
        case NUMBER:   return getNumber().toString() == other.getNumber().toString();
    }

    assert(false && "Unreachable");
    return false;
}

bool Literal::matches(const Expression& _other, MatchesPtr _pMatches) const {
    if (!Expression::equals(_other))
        return Expression::matches(_other, _pMatches);
    return *this == _other;
}

NodePtr Literal::clone(Cloner &_cloner) const {
    return _cloneTypeTo(NEW_CLONE(this, _cloner, Literal(*this)), _cloner);
}

bool VariableReference::less(const Node& _other) const {
    if (!Expression::equals(_other))
        return Expression::less(_other);
    return _less(getTarget(), ((const VariableReference&)_other).getTarget());
}

bool VariableReference::equals(const Node& _other) const {
    if (!Expression::equals(_other))
        return false;
    return _equals(getTarget(), ((const VariableReference&)_other).getTarget());
}

bool VariableReference::matches(const Expression& _other, MatchesPtr _pMatches) const {
    if (!Expression::equals(_other))
        return Expression::matches(_other, _pMatches);
    return _equals(getTarget(), ((const VariableReference&)_other).getTarget());
}

NodePtr VariableReference::clone(Cloner &_cloner) const {
    return _cloneTypeTo(NEW_CLONE(this, _cloner, VariableReference(m_strName, _cloner.get(m_pTarget, true))), _cloner);
}

bool PredicateReference::less(const Node& _other) const {
    if (!Expression::equals(_other))
        return Expression::less(_other);
    const PredicateReference& other = (const PredicateReference&)_other;
    if (getName() != other.getName())
        return getName() < other.getName();
    return getTarget() < other.getTarget();
}

bool PredicateReference::equals(const Node& _other) const {
    if (!Expression::equals(_other))
        return false;
    const PredicateReference& other = (const PredicateReference&)_other;
    return getName() == other.getName() && getTarget() == other.getTarget();
}

bool PredicateReference::matches(const Expression& _other, MatchesPtr _pMatches) const {
    if (!Expression::equals(_other))
        return Expression::matches(_other, _pMatches);
    return *this == (const PredicateReference&)_other;
}

bool Unary::less(const Node& _other) const {
    if (!Expression::equals(_other))
        return Expression::less(_other);
    const Unary& other = (const Unary&)_other;
    if (getOperator() != other.getOperator())
        return getOperator() < other.getOperator();
    return _less(getExpression(), other.getExpression());
}

bool Unary::equals(const Node& _other) const {
    if (!Expression::equals(_other))
        return false;
    const Unary& other = (const Unary&)_other;
    return getOperator() == other.getOperator() && _equals(getExpression(), other.getExpression());
}

bool Unary::matches(const Expression& _other, MatchesPtr _pMatches) const {
    if (!Expression::equals(_other))
        return Expression::matches(_other, _pMatches);
    const Unary& other = (const Unary&)_other;
    return getOperator() == other.getOperator() && _matches(getExpression(), other.getExpression(), _pMatches);
}

NodePtr Unary::clone(Cloner &_cloner) const {
    return _cloneTypeTo(NEW_CLONE(this, _cloner, Unary(m_operator, _cloner.get(m_pExpression), m_overflow)), _cloner);
}

bool Binary::less(const Node& _other) const {
    if (!Expression::equals(_other))
        return Expression::less(_other);
    const Binary& other = (const Binary&)_other;
    if (getOperator() != other.getOperator())
        return getOperator() < other.getOperator();
    if (!_equals(getLeftSide(), other.getLeftSide()))
        return _less(getLeftSide(), other.getLeftSide());
    return _less(getRightSide(), other.getRightSide());
}

bool Binary::equals(const Node& _other) const {
    if (!Expression::equals(_other))
        return false;
    const Binary& other = (const Binary&)_other;
    return getOperator() == other.getOperator()
        && _equals(getLeftSide(), other.getLeftSide())
        && _equals(getRightSide(), other.getRightSide());
}

bool Binary::matches(const Expression& _other, MatchesPtr _pMatches) const {
    if (!Expression::equals(_other))
        return Expression::matches(_other, _pMatches);
    const Binary& other = (const Binary&)_other;
    if (getOperator() != other.getOperator())
        return false;

    MatchesPtr pOldMatches = _pMatches ? new Matches(*_pMatches) : nullptr;
    if (_matches(getLeftSide(), other.getLeftSide(), _pMatches) && _matches(getRightSide(), other.getRightSide(), _pMatches))
        return true;

    if (_pMatches)
        _pMatches->swap(*pOldMatches);

    return isSymmetrical() && _matches(getLeftSide(), other.getRightSide(), _pMatches) &&
        _matches(getRightSide(), other.getLeftSide(), _pMatches);
}

NodePtr Binary::clone(Cloner &_cloner) const {
    return _cloneTypeTo(NEW_CLONE(this, _cloner, Binary(m_operator, _cloner.get(m_pLeft), _cloner.get(m_pRight), m_overflow)), _cloner);
}

bool Ternary::less(const Node& _other) const {
    if (!Expression::equals(_other))
        return Expression::less(_other);
    const Ternary& other = (const Ternary&)_other;
    if (!_equals(getIf(), other.getIf()))
        return _less(getIf(), other.getIf());
    if (!_equals(getThen(), other.getThen()))
        return _less(getThen(), other.getThen());
    return _less(getElse(), other.getElse());
}

bool Ternary::equals(const Node& _other) const {
    if (!Expression::equals(_other))
        return false;
    const Ternary& other = (const Ternary&)_other;
    return _equals(getIf(), other.getIf())
        && _equals(getThen(), other.getThen())
        && _equals(getElse(), other.getElse());
}

bool Ternary::matches(const Expression& _other, MatchesPtr _pMatches) const {
    if (!Expression::equals(_other))
        return Expression::matches(_other, _pMatches);
    const Ternary& other = (const Ternary&)_other;
    return _matches(getIf(), other.getIf(), _pMatches)
        && _matches(getThen(), other.getThen(), _pMatches)
        && _matches(getElse(), other.getElse(), _pMatches);
}

NodePtr Ternary::clone(Cloner &_cloner) const {
    return _cloneTypeTo(NEW_CLONE(this, _cloner, Ternary(_cloner.get(m_pIf), _cloner.get(m_pThen), _cloner.get(m_pElse))), _cloner);
}

bool TypeExpr::less(const Node& _other) const {
    if (!Expression::equals(_other))
        return Expression::less(_other);
    return _less(getContents(), ((const TypeExpr&)_other).getContents());
}

bool TypeExpr::equals(const Node& _other) const {
    if (!Expression::equals(_other))
        return false;
    return _equals(getContents(), ((const TypeExpr&)_other).getContents());
}

bool TypeExpr::matches(const Expression& _other, MatchesPtr _pMatches) const {
    if (!Expression::equals(_other))
        return Expression::matches(_other, _pMatches);
    const TypeExpr& other = (const TypeExpr&)_other;
    return _equals(getContents(), other.getContents());
}

NodePtr TypeExpr::clone(Cloner &_cloner) const {
    return _cloneTypeTo(NEW_CLONE(this, _cloner, TypeExpr(_cloner.get(m_pContents))), _cloner);
}

bool CastExpr::less(const Node& _other) const {
    if (!Expression::equals(_other))
        return Expression::less(_other);
    const CastExpr& other = (const CastExpr&)_other;
    if (!_equals(getToType(), other.getToType()))
        return _less(getToType(), other.getToType());
    return _less(getExpression(), other.getExpression());
}

bool CastExpr::equals(const Node& _other) const {
    if (!Expression::equals(_other))
        return false;
    const CastExpr& other = (const CastExpr&)_other;
    return _equals(getToType(), other.getToType()) && _equals(getExpression(), other.getExpression());
}

bool CastExpr::matches(const Expression& _other, MatchesPtr _pMatches) const {
    if (!Expression::equals(_other))
        return Expression::matches(_other, _pMatches);
    const CastExpr& other = (const CastExpr&)_other;
    return _matches(getToType(), other.getToType(), _pMatches) && _matches(getExpression(), other.getExpression(), _pMatches);
}

NodePtr CastExpr::clone(Cloner &_cloner) const {
    return _cloneTypeTo(NEW_CLONE(this, _cloner, CastExpr(_cloner.get(m_pExpression), _cloner.get(m_pToType))), _cloner);
}

bool Formula::less(const Node& _other) const {
    if (!Expression::equals(_other))
        return Expression::less(_other);
    const Formula& other = (const Formula&)_other;
    if (getQuantifier() != other.getQuantifier())
        return getQuantifier() < other.getQuantifier();
    if (getBoundVariables() != other.getBoundVariables())
        return getBoundVariables() < other.getBoundVariables();
    return _less(getSubformula(), other.getSubformula());
}

bool Formula::equals(const Node& _other) const {
    if (!Expression::equals(_other))
        return false;
    const Formula& other = (const Formula&)_other;
    return getQuantifier() == other.getQuantifier()
        && getBoundVariables() == other.getBoundVariables()
        && _equals(getSubformula(), other.getSubformula());
}

bool Formula::matches(const Expression& _other, MatchesPtr _pMatches) const {
    if (!Expression::equals(_other))
        return Expression::matches(_other, _pMatches);
    const Formula& other = (const Formula&)_other;
    return getQuantifier() == other.getQuantifier()
        && matchNamedValues(getBoundVariables(), other.getBoundVariables())
        && _matches(getSubformula(), other.getSubformula(), _pMatches);
}

NodePtr Formula::clone(Cloner &_cloner) const {
    FormulaPtr pFormula = NEW_CLONE(this, _cloner, Formula(m_quantifier, _cloner.get(m_pSubformula)));
    pFormula->getBoundVariables().appendClones(getBoundVariables(), _cloner);
    return _cloneTypeTo(pFormula, _cloner);
}

bool Component::less(const Node& _other) const {
    if (!Expression::equals(_other))
        return Expression::less(_other);
    const Component& other = (const Component&)_other;
    if (getComponentKind() != other.getComponentKind())
        return getComponentKind() < other.getComponentKind();
    return _less(getObject(), other.getObject());
}

bool Component::equals(const Node& _other) const {
    if (!Expression::equals(_other))
        return false;
    const Component& other = (const Component&)_other;
    return getComponentKind() == other.getComponentKind() && _equals(getObject(), other.getObject());
}

bool Component::matches(const Expression& _other, MatchesPtr _pMatches) const {
    if (!Expression::equals(_other))
        return Expression::matches(_other, _pMatches);
    const Component& other = (const Component&)_other;
    return getComponentKind() == other.getComponentKind() && _matches(getObject(), other.getObject(), _pMatches);
}

bool ArrayPartExpr::less(const Node& _other) const {
    if (!Component::equals(_other))
        return Component::less(_other);
    return getIndices() < ((const ArrayPartExpr&)_other).getIndices();
}

bool ArrayPartExpr::equals(const Node& _other) const {
    if (!Component::equals(_other))
        return false;
    return getIndices() == ((const ArrayPartExpr&)_other).getIndices();
}

bool ArrayPartExpr::matches(const Expression& _other, MatchesPtr _pMatches) const {
    if (!Component::equals(_other))
        return Component::matches(_other, _pMatches);
    return matchCollections(getIndices(), ((const ArrayPartExpr&)_other).getIndices(), _pMatches);
}

bool ArrayPartExpr::isRestrict() const {
    for (size_t i = 0; i < getIndices().size(); ++i)
        if (getIndices().get(i)->getKind() != Expression::TYPE)
            return false;
    return true;
}

NodePtr ArrayPartExpr::clone(Cloner &_cloner) const {
    ArrayPartExprPtr pExpr = NEW_CLONE(this, _cloner, ArrayPartExpr(_cloner.get(getObject())));
    pExpr->getIndices().appendClones(getIndices(), _cloner);
    return _cloneTypeTo(pExpr, _cloner);
}

bool FieldExpr::less(const Node& _other) const {
    if (!Component::equals(_other))
        return Component::less(_other);
    return getFieldName() < ((const FieldExpr&)_other).getFieldName();
}

bool FieldExpr::equals(const Node& _other) const {
    if (!Component::equals(_other))
        return false;
    return getFieldName() == ((const FieldExpr&)_other).getFieldName();
}

bool FieldExpr::matches(const Expression& _other, MatchesPtr _pMatches) const {
    if (!Component::equals(_other))
        return Component::matches(_other, _pMatches);
    return *this == _other;
}

NodePtr FieldExpr::clone(Cloner &_cloner) const {
    return _cloneTypeTo(NEW_CLONE(this, _cloner, FieldExpr(m_strField, _cloner.get(getObject()))), _cloner);
}

bool MapElementExpr::less(const Node& _other) const {
    if (!Component::equals(_other))
        return Component::less(_other);
    return _less(getIndex(), ((const MapElementExpr&)_other).getIndex());
}

bool MapElementExpr::equals(const Node& _other) const {
    if (!Component::equals(_other))
        return false;
    return _equals(getIndex(), ((const MapElementExpr&)_other).getIndex());
}

bool MapElementExpr::matches(const Expression& _other, MatchesPtr _pMatches) const {
    if (!Component::equals(_other))
        return Component::matches(_other, _pMatches);
    return _matches(getIndex(), ((const MapElementExpr&)_other).getIndex(), _pMatches);
}

NodePtr MapElementExpr::clone(Cloner &_cloner) const {
    return _cloneTypeTo(NEW_CLONE(this, _cloner, MapElementExpr(_cloner.get(getIndex()), _cloner.get(getObject()))), _cloner);
}

bool ListElementExpr::less(const Node& _other) const {
    if (!Component::equals(_other))
        return Component::less(_other);
    return _less(getIndex(), ((const ListElementExpr&)_other).getIndex());
}

bool ListElementExpr::equals(const Node& _other) const {
    if (!Component::equals(_other))
        return Component::less(_other);
    return _equals(getIndex(), ((const ListElementExpr&)_other).getIndex());
}

bool ListElementExpr::matches(const Expression& _other, MatchesPtr _pMatches) const {
    if (!Component::equals(_other))
        return Component::matches(_other, _pMatches);
    return _matches(getIndex(), ((const ListElementExpr&)_other).getIndex(), _pMatches);
}

NodePtr ListElementExpr::clone(Cloner &_cloner) const {
    return _cloneTypeTo(NEW_CLONE(this, _cloner, ListElementExpr(_cloner.get(getIndex()), _cloner.get(getObject()))), _cloner);
}

bool Replacement::less(const Node& _other) const {
    if (!Component::equals(_other))
        return Component::less(_other);
    return _less(getNewValues(), ((const Replacement&)_other).getNewValues());
}

bool Replacement::equals(const Node& _other) const {
    if (!Component::equals(_other))
        return false;
    return _equals(getNewValues(), ((const Replacement&)_other).getNewValues());
}

bool Replacement::matches(const Expression& _other, MatchesPtr _pMatches) const {
    if (!Component::equals(_other))
        return Component::matches(_other, _pMatches);
    return _matches(getNewValues(), ((const Replacement&)_other).getNewValues(), _pMatches);
}

NodePtr Replacement::clone(Cloner &_cloner) const {
    return _cloneTypeTo(NEW_CLONE(this, _cloner, Replacement(_cloner.get(getNewValues()), _cloner.get(getObject()))), _cloner);
}

bool AccessorBase::less(const Node& _other) const {
    if (!Component::equals(_other))
        return Component::less(_other);
    const AccessorBase& other = (const AccessorBase&)_other;
    if (getConstructorName() != other.getConstructorName())
        return getConstructorName() < other.getConstructorName();
    return _less(getConstructor(), other.getConstructor());
}

bool AccessorBase::equals(const Node& _other) const {
    if (!Component::equals(_other))
        return false;
    const AccessorBase& other = (const AccessorBase&)_other;
    return getConstructorName() == other.getConstructorName() &&
        _equals(getConstructor(), other.getConstructor());
}

bool AccessorBase::matches(const Expression& _other, MatchesPtr _pMatches) const {
    if (!Component::equals(_other))
        return Component::matches(_other, _pMatches);
    const AccessorBase& other = (const AccessorBase&)_other;
    if  (getConstructor() && other.getConstructor())
        return _equals(getConstructor(), ((const AccessorBase&)_other).getConstructor());
    return getConstructorName() == other.getConstructorName();
}

NodePtr RecognizerExpr::clone(Cloner &_cloner) const {
    return _cloneTypeTo(NEW_CLONE(this, _cloner, RecognizerExpr(m_pConstructor, _cloner.get(getObject()))), _cloner);
}

NodePtr AccessorExpr::clone(Cloner &_cloner) const {
    return _cloneTypeTo(NEW_CLONE(this, _cloner, AccessorExpr(m_pConstructor, _cloner.get(getObject()))), _cloner);
}

bool FunctionCall::less(const Node& _other) const {
    if (!Expression::equals(_other))
        return Expression::less(_other);
    const FunctionCall& other = (const FunctionCall&)_other;
    if (!_equals(getPredicate(), other.getPredicate()))
        return _less(getPredicate(), other.getPredicate());
    return getArgs() < other.getArgs();
}

bool FunctionCall::equals(const Node& _other) const {
    if (!Expression::equals(_other))
        return false;
    const FunctionCall& other = (const FunctionCall&)_other;
    return _equals(getPredicate(), other.getPredicate()) && getArgs() == other.getArgs();
}

bool FunctionCall::matches(const Expression& _other, MatchesPtr _pMatches) const {
    if (!Expression::equals(_other))
        return Expression::matches(_other, _pMatches);
    const FunctionCall& other = (const FunctionCall&)_other;
    return _matches(getPredicate(), other.getPredicate(), _pMatches)
        && matchCollections(getArgs(), other.getArgs(), _pMatches);
}

NodePtr FunctionCall::clone(Cloner &_cloner) const {
    FunctionCallPtr pExpr = NEW_CLONE(this, _cloner, FunctionCall(_cloner.get(getPredicate())));
    pExpr->getArgs().appendClones(getArgs(), _cloner);
    return _cloneTypeTo(pExpr, _cloner);
}

bool Binder::less(const Node& _other) const {
    if (!Expression::equals(_other))
        return Expression::less(_other);
    const Binder& other = (const Binder&)_other;
    if (!_equals(getPredicate(), other.getPredicate()))
        return _less(getPredicate(), other.getPredicate());
    return getArgs() < other.getArgs();
}

bool Binder::equals(const Node& _other) const {
    if (!Expression::equals(_other))
        return false;
    const Binder& other = (const Binder&)_other;
    return _equals(getPredicate(), other.getPredicate()) && getArgs() < other.getArgs();
}

bool Binder::matches(const Expression& _other, MatchesPtr _pMatches) const {
    if (!Expression::equals(_other))
        return Expression::matches(_other, _pMatches);
    const Binder& other = (const Binder&)_other;
    return _matches(getPredicate(), other.getPredicate(), _pMatches)
        && matchCollections(getArgs(), other.getArgs(), _pMatches);
}

NodePtr Binder::clone(Cloner &_cloner) const {
    BinderPtr pExpr = NEW_CLONE(this, _cloner, Binder(_cloner.get(getPredicate())));
    pExpr->getArgs().appendClones(getArgs(), _cloner);
    return _cloneTypeTo(pExpr, _cloner);
}

bool FormulaCall::less(const Node& _other) const {
    if (!Expression::equals(_other))
        return Expression::less(_other);
    const FormulaCall& other = (const FormulaCall&)_other;
    if (!_equals(getTarget(), other.getTarget()))
        return _less(getTarget(), other.getTarget());
    return getArgs() < other.getArgs();
}

bool FormulaCall::equals(const Node& _other) const {
    if (!Expression::equals(_other))
        return false;
    const FormulaCall& other = (const FormulaCall&)_other;
    return _equals(getTarget(), other.getTarget()) && getArgs() < other.getArgs();
}

bool FormulaCall::matches(const Expression& _other, MatchesPtr _pMatches) const {
    if (!Expression::equals(_other))
        return Expression::matches(_other, _pMatches);
    const FormulaCall& other = (const FormulaCall&)_other;
    return _equals(getTarget(), other.getTarget())
        && matchCollections(getArgs(), other.getArgs(), _pMatches);
}

NodePtr FormulaCall::clone(Cloner &_cloner) const {
    FormulaCallPtr pExpr = NEW_CLONE(this, _cloner, FormulaCall(_cloner.get(getTarget(), true)));
    pExpr->getArgs().appendClones(getArgs(), _cloner);
    return _cloneTypeTo(pExpr, _cloner);
}

bool Branch::less(const Node& _other) const {
    if (!Params::equals(_other))
        return Params::less(_other);
    const Branch& other = (const Branch&)_other;
    if (!_equals(getLabel(), other.getLabel()))
        return _less(getLabel(), other.getLabel());
    if (!_equals(getPreCondition(), other.getPreCondition()))
        return _less(getPreCondition(), other.getPreCondition());
    return _less(getPostCondition(), other.getPostCondition());
}

bool Branch::equals(const Node& _other) const {
    if (!Params::equals(_other))
        return false;
    const Branch& other = (const Branch&)_other;
    return _equals(getLabel(), other.getLabel())
        && _equals(getPreCondition(), other.getPreCondition())
        && _equals(getPostCondition(), other.getPostCondition());
}

bool AnonymousPredicate::less(const Node& _other) const {
    if (!Statement::equals(_other))
        return Statement::less(_other);
    const AnonymousPredicate& other = (const AnonymousPredicate&)_other;
    if (!_equals(getMeasure(), other.getMeasure()))
        return _less(getMeasure(), other.getMeasure());
    if (!_equals(getPreCondition(), other.getPreCondition()))
        return _less(getPreCondition(), other.getPreCondition());
    if (!_equals(getPostCondition(), other.getPostCondition()))
        return _less(getPostCondition(), other.getPostCondition());
    if (!_equals(getBlock(), other.getBlock()))
        return _less(getBlock(), other.getBlock());
    if (getOutParams() != other.getOutParams())
        return getOutParams() < other.getOutParams();
    return getInParams() < other.getInParams();
}

bool AnonymousPredicate::equals(const Node& _other) const {
    if (!Statement::equals(_other))
        return false;
    const AnonymousPredicate& other = (const AnonymousPredicate&)_other;
    return _equals(getMeasure(), other.getMeasure())
        && _equals(getPreCondition(), other.getPreCondition())
        && _equals(getPostCondition(), other.getPostCondition())
        && _equals(getBlock(), other.getBlock())
        && getOutParams() == other.getOutParams()
        && getInParams() == other.getInParams();
}

bool Lambda::less(const Node& _other) const {
    if (!Expression::equals(_other))
        return Expression::less(_other);
    return getPredicate() < ((const Lambda&)_other).getPredicate();
}

bool Lambda::equals(const Node& _other) const {
    if (!Expression::equals(_other))
        return false;
    return getPredicate() == ((const Lambda&)_other).getPredicate();
}

bool Lambda::matches(const Expression& _other, MatchesPtr _pMatches) const {
    if (!Expression::equals(_other))
        return Expression::matches(_other, _pMatches);
    return getPredicate() == ((const Lambda&)_other).getPredicate();
}

NodePtr Lambda::clone(Cloner &_cloner) const {
    LambdaPtr pExpr = NEW_CLONE(this, _cloner, Lambda());
    m_pred.cloneTo(pExpr->m_pred, _cloner);
    return _cloneTypeTo(pExpr, _cloner);
}

bool ElementDefinition::less(const Node& _other) const {
    if (!Node::equals(_other))
        return Node::less(_other);
    const ElementDefinition& other = (const ElementDefinition&)_other;
    if (!_equals(getIndex(), other.getIndex()))
        return _less(getIndex(), other.getIndex());
    return _less(getValue(), other.getValue());
}

bool ElementDefinition::equals(const Node& _other) const {
    if (!Node::equals(_other))
        return false;
    const ElementDefinition& other = (const ElementDefinition&)_other;
    return _equals(getIndex(), other.getIndex()) && _equals(getValue(), other.getValue());
}

bool StructFieldDefinition::less(const Node& _other) const {
    if (!Node::equals(_other))
        return Node::less(_other);
    const StructFieldDefinition& other = (const StructFieldDefinition&)_other;
    if (getName() != other.getName())
        return getName() < other.getName();
    return _less(getValue(), other.getValue());
}

bool StructFieldDefinition::equals(const Node& _other) const {
    if (!Node::equals(_other))
        return false;
    const StructFieldDefinition& other = (const StructFieldDefinition&)_other;
    return getName() == other.getName()
        && _equals(getValue(), other.getValue());
}

NodePtr StructConstructor::clone(Cloner &_cloner) const {
    StructConstructorPtr pCopy = NEW_CLONE(this, _cloner, StructConstructor());
    pCopy->appendClones(*this, _cloner);
    return _cloneTypeTo(pCopy, _cloner);
}

bool UnionConstructor::less(const Node& _other) const {
    if (!StructConstructor::equals(_other))
        return StructConstructor::less(_other);
    const UnionConstructor& other = (const UnionConstructor&)_other;
    if (getName() != other.getName())
        return getName() < other.getName();
    if (getDeclarations() != other.getDeclarations())
        return getDeclarations() < other.getDeclarations();
    return _less(getPrototype(), other.getPrototype());
}

bool UnionConstructor::equals(const Node& _other) const {
    if (!StructConstructor::equals(_other))
        return false;
    const UnionConstructor& other = (const UnionConstructor&)_other;
    return getName() == other.getName()
        && getDeclarations() == other.getDeclarations()
        && _equals(getPrototype(), other.getPrototype());
}

NodePtr UnionConstructor::clone(Cloner &_cloner) const {
    UnionConstructorPtr pCopy = NEW_CLONE(this, _cloner, UnionConstructor(getName(), _cloner.get(getPrototype(), true)));
    pCopy->getDeclarations().appendClones(getDeclarations(), _cloner);
    return _cloneTypeTo(pCopy, _cloner);
}

NodePtr ArrayConstructor::clone(Cloner &_cloner) const {
    ArrayConstructorPtr pCopy = NEW_CLONE(this, _cloner, ArrayConstructor());
    pCopy->appendClones(*this, _cloner);
    return _cloneTypeTo(pCopy, _cloner);
}

NodePtr MapConstructor::clone(Cloner &_cloner) const {
    MapConstructorPtr pCopy = NEW_CLONE(this, _cloner, MapConstructor());
    pCopy->appendClones(*this, _cloner);
    return _cloneTypeTo(pCopy, _cloner);
}

NodePtr SetConstructor::clone(Cloner &_cloner) const {
    SetConstructorPtr pCopy = NEW_CLONE(this, _cloner, SetConstructor());
    pCopy->appendClones(*this, _cloner);
    return _cloneTypeTo(pCopy, _cloner);
}

NodePtr ListConstructor::clone(Cloner &_cloner) const {
    ListConstructorPtr pCopy = NEW_CLONE(this, _cloner, ListConstructor());
    pCopy->appendClones(*this, _cloner);
    return _cloneTypeTo(pCopy, _cloner);
}

bool ArrayPartDefinition::less(const Node& _other) const {
    if (!Node::equals(_other))
        return Node::less(_other);
    const ArrayPartDefinition& other = (const ArrayPartDefinition&)_other;
    if (!_equals(getExpression(), other.getExpression()))
        return _less(getExpression(), other.getExpression());
    return getConditions() < other.getConditions();
}

bool ArrayPartDefinition::equals(const Node& _other) const {
    if (!Node::equals(_other))
        return false;
    const ArrayPartDefinition& other = (const ArrayPartDefinition&)_other;
    return _equals(getExpression(), other.getExpression()) && getConditions() == other.getConditions();
}

bool ArrayIteration::less(const Node& _other) const {
    if (!Base::equals(_other))
        return Base::less(_other);
    const ArrayIteration& other = (const ArrayIteration&)_other;
    if (!_equals(getDefault(), other.getDefault()))
        return _less(getDefault(), other.getDefault());
    return getIterators() < other.getIterators();
}

bool ArrayIteration::equals(const Node& _other) const {
    if (!Base::equals(_other))
        return false;
    const ArrayIteration& other = (const ArrayIteration&)_other;
    return _equals(getDefault(), other.getDefault()) && getIterators() == other.getIterators();
}

NodePtr ArrayIteration::clone(Cloner &_cloner) const {
    ArrayIterationPtr pCopy = NEW_CLONE(this, _cloner, ArrayIteration(_cloner.get(getDefault())));
    pCopy->appendClones(*this, _cloner);
    pCopy->getIterators().appendClones(getIterators(), _cloner);
    return _cloneTypeTo(pCopy, _cloner);
}
