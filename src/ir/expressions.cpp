/// \file expressions.cpp
///

#include "ir/expressions.h"
#include "ir/types.h"
#include "ir/declarations.h"
#include "ir/statements.h"
#include "ir/visitor.h"
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
    return m_decls.empty() && m_pProto && size() == m_pProto->getFields().size();
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

UnionFieldIdx UnionType::findField(const std::wstring &_strName) const {
    for (size_t i = 0; i < m_constructors.size(); ++i) {
        size_t cIdx = m_constructors.get(i)->getFields().findByNameIdx(_strName);
        if (cIdx != (size_t) -1)
            return UnionFieldIdx(i, cIdx);
    }

    return UnionFieldIdx((size_t) -1, (size_t) -1);
}

UnionAlternativeExpr::UnionAlternativeExpr(const UnionTypePtr &_pType, const UnionFieldIdx &_idx) :
    m_strName(_pType->getConstructors().get(_idx.first)->getFields().get(_idx.second)->getName()), m_pType(_pType), m_idx(_idx)
{
}

UnionConstructorDeclarationPtr UnionAlternativeExpr::getConstructor() const {
    return getUnionType()->getConstructors().get(m_idx.first);
}

NamedValuePtr UnionAlternativeExpr::getField() const {
    return getConstructor()->getFields().get(m_idx.second);
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

bool Expression::matches(const Expression& _other, MatchesPtr _pMatches) const {
    if (!_pMatches)
        return getKind() == WILD || _other.getKind() == WILD || _other.getKind() == getKind();
    if (getKind() == WILD && _other.getKind() == WILD)
        return true;
    if (getKind() != WILD && _other.getKind() != WILD)
        return _other.getKind() == getKind();

    if (getKind() == WILD)
        _pMatches->addExpression((const Wild&)*this, _other);
    else
        _pMatches->addExpression((const Wild&)_other, *this);
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
    SubstituteByMask(ir::Node &_node, const ir::ExpressionPtr &_pFrom, const ir::ExpressionPtr &_pTo) :
        Visitor(CHILDREN_FIRST), m_pNode(&_node), m_pFrom(_pFrom), m_pTo(_pTo)
    {}

    bool visitExpression(ir::Expression &_expr) {
        Matches matches;
        if (!_expr.matches(*m_pFrom, &matches))
            return true;

        ir::ExpressionPtr pNewNode = clone(*m_pTo);
        Expression::substitute(pNewNode, matches);

        // FIXME If _expr is root node.
        callSetter(pNewNode);

        return true;
    }

    void substitute() {
        traverseNode(*m_pNode);
    }

private:
    ir::NodePtr m_pNode;
    ir::ExpressionPtr m_pFrom, m_pTo;

};

void Expression::substitute(ir::Node &_node, const ir::ExpressionPtr &_pFrom, const ir::ExpressionPtr &_pTo) {
    SubstituteByMask(_node, _pFrom, _pTo).substitute();
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
    return getKind() == other.getKind() && _equals(getType(), other.getType());
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
}

bool Literal::matches(const Expression& _other, MatchesPtr _pMatches) const {
    if (!Expression::equals(_other))
        return Expression::matches(_other, _pMatches);
    return *this == _other;
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
    return *getTarget() == *((const VariableReference&)_other).getTarget();
}

bool PredicateReference::less(const Node& _other) const {
    if (!Expression::equals(_other))
        return Expression::less(_other);
    const PredicateReference& other = (const PredicateReference&)_other;
    if (getName() != other.getName())
        return getName() < other.getName();
    return _less(getTarget(), other.getTarget());
}

bool PredicateReference::equals(const Node& _other) const {
    if (!Expression::equals(_other))
        return false;
    const PredicateReference& other = (const PredicateReference&)_other;
    return getName() == other.getName() && _equals(getTarget(), other.getTarget());
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
    if (_matches(getLeftSide(), other.getLeftSide(), _pMatches) && _matches(getRightSide(), other.getRightSide(), _pMatches))
        return true;
    return getOperator() == other.getInverseOperator()
        ? _matches(getLeftSide(), other.getRightSide(), _pMatches) && _matches(getRightSide(), other.getLeftSide(), _pMatches)
        : false;
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

bool UnionAlternativeExpr::less(const Node& _other) const {
    if (!Component::equals(_other))
        return Component::less(_other);
    const UnionAlternativeExpr& other = (const UnionAlternativeExpr&)_other;
    if (getName() != other.getName())
        return getName() < other.getName();
    return getIdx() < other.getIdx();
}

bool UnionAlternativeExpr::equals(const Node& _other) const {
    if (!Component::equals(_other))
        return false;
    const UnionAlternativeExpr& other = (const UnionAlternativeExpr&)_other;
    return getName() == other.getName()
        && getIdx() == other.getIdx();
}

bool UnionAlternativeExpr::matches(const Expression& _other, MatchesPtr _pMatches) const {
    if (!Component::equals(_other))
        return Component::matches(_other, _pMatches);
    return *this == _other;
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
    if (!Collection::equals(_other))
        return Collection::less(_other);
    const ArrayIteration& other = (const ArrayIteration&)_other;
    if (!_equals(getDefault(), other.getDefault()))
        return _less(getDefault(), other.getDefault());
    return getIterators() < other.getIterators();
}

bool ArrayIteration::equals(const Node& _other) const {
    if (!Collection::equals(_other))
        return false;
    const ArrayIteration& other = (const ArrayIteration&)_other;
    return _equals(getDefault(), other.getDefault()) && getIterators() == other.getIterators();
}
