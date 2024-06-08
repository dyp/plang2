/// \file term_rewriting.cpp
///

#include <set>

#include "ir/base.h"
#include "ir/declarations.h"
#include "ir/statements.h"
#include "ir/visitor.h"

#include "utils.h"
#include "statement_tree.h"
#include "node_analysis.h"

using namespace ir;
using namespace st;
using namespace na;

namespace tr {

class ReplaceCall : Visitor {
public:
    ReplaceCall(const NodePtr& _pNode, const NodePtr& _pNewNode) :
        Visitor(CHILDREN_FIRST), m_pNode(_pNode), m_pNewNode(_pNewNode), m_pCall(NULL)
    {}

    virtual bool traverseFunctionCall(const FunctionCallPtr &_node) {
        m_pCall = _node;
        if (m_pNewNode)
            callSetter(m_pNewNode);
        return false;
    }

    FunctionCallPtr run() {
        traverseNode(m_pNode);
        return m_pCall;
    }

private:
    NodePtr m_pNode, m_pNewNode;
    FunctionCallPtr m_pCall;
};

CallPtr getCallFromFunctionCall(const FunctionCallPtr &_fCall, const VariableReferencePtr &_var) {
    const auto pCall = std::make_shared<ir::Call>(_fCall->getPredicate());
    const auto pBranch = std::make_shared<ir::CallBranch>();

    pCall->getArgs().assign(_fCall->getArgs());
    pBranch->add(_var);
    pCall->getBranches().add(pBranch);

    return pCall;
}

std::pair<NodePtr, NodePtr> extractFirstCall(const NodePtr& _node) {
    if (!containsCall(_node))
        return std::make_pair(NodePtr(), _node);

    const NodePtr pNode = clone(_node);

    const FunctionCallPtr pFunctionCall = ReplaceCall(pNode, NULL).run();
    const VariableReferencePtr pVar = std::make_shared<VariableReference>(std::make_shared<NamedValue>(L"", pFunctionCall->getType()));
    const CallPtr pCall = getCallFromFunctionCall(pFunctionCall, pVar);

    NodePtr pTail = Expression::substitute(pNode, pFunctionCall, pVar);
    return std::make_pair(pCall, pTail);
}

class ExcludeCasts : public Visitor {
public:
    ExcludeCasts() : Visitor(CHILDREN_FIRST) {}
    bool visitCastExpr(const CastExprPtr& _expr) override {
        callSetter(_expr->getExpression());
        return true;
    }
};

StatementPtr modifyStatement(const StatementPtr& _pStatement) {
    const auto top = std::make_shared<st::StmtVertex>(_pStatement);
    top->expand();
    top->modifyForVerification();
    top->simplify();
    const auto pStatment = top->mergeForVerification();
    ExcludeCasts().traverseNode(pStatment);
    return pStatment;
}

class ModifyStatements : public Visitor {
public:
    ModifyStatements() {}

    bool _traverseAnonymousPredicate(const AnonymousPredicatePtr &_decl) override {
        const StatementPtr
            pOldStatement = _decl->getBlock(),
            pNewStatement = modifyStatement(pOldStatement);

        if (!pNewStatement)
            return true;

        _decl->getBlock()->clear();

        if (pNewStatement->getKind() == Statement::BLOCK)
            _decl->getBlock()->assign(*pNewStatement->as<Block>().get());
        else
            _decl->getBlock()->add(pNewStatement);

        return true;
    }
};

void modifyModule(const ir::ModulePtr& _pModule) {
    ModifyStatements().traverseNode(_pModule);
}

FormulaCallPtr makeCall(const ir::FormulaDeclarationPtr& _pFormula, ArgsMap& _args) {
    if (!_pFormula)
        return NULL;

    FormulaCallPtr pCall = std::make_shared<FormulaCall>(_pFormula);
    for (size_t i = 0; i < _pFormula->getParams().size(); ++i) {
        const auto param = _pFormula->getParams().get(i);
        const ExpressionPtr pArg = _args.getExpression(param);
        assert(pArg);
        pCall->getArgs().add(pArg);
    }

    return pCall;
}

FormulaCallPtr makeCall(const ir::FormulaDeclarationPtr& _pFormula, const NamedValues& _params) {
    if (!_pFormula)
        return NULL;

    FormulaCallPtr pCall = std::make_shared<FormulaCall>(_pFormula);
    for (size_t i = 0; i < _pFormula->getParams().size(); ++i) {
        const size_t cIdx = _params.findIdx(*_pFormula->getParams().get(i));
        assert(cIdx != (size_t)-1);
        pCall->getArgs().add(std::make_shared<VariableReference>(_params.get(cIdx)));
    }

    return pCall;
}

FormulaCallPtr makeCall(const ir::FormulaDeclarationPtr& _pFormula, const PredicatePtr& _predicate) {
    NamedValues params;
    getPredicateParams(_predicate, params);
    return makeCall(_pFormula, params);
}

FormulaCallPtr makeCall(const ir::FormulaDeclarationPtr& _pFormula, const FormulaCallPtr &_call) {
    ArgsMap args;
    getArgsMap(_call, args);
    return makeCall(_pFormula, args);
}

FormulaCallPtr makeCall(const ir::FormulaDeclarationPtr& _pFormula, const CallPtr &_call) {
    ArgsMap args;
    getArgsMap(_call, args);
    return makeCall(_pFormula, args);
}

class FormulasCollector : public Visitor {
public:
    FormulasCollector(std::set<FormulaDeclarationPtr>& _formulas) :
        m_pTraversedFormulas(_formulas) {}
    bool traverseFormulaCall(const FormulaCallPtr& _call) override;

private:
    std::set<FormulaDeclarationPtr>& m_pTraversedFormulas;
};

bool FormulasCollector::traverseFormulaCall(const FormulaCallPtr& _call) {
    if (!_call->getTarget())
        return true;
    if (!m_pTraversedFormulas.insert(_call->getTarget()).second)
        return true;

    traverseNode(_call->getTarget());

    return true;
}

void declareLemma(const ExpressionPtr& _pProposition, std::set<FormulaDeclarationPtr>& _declarations,
    const ModulePtr& _pModule)
{
    FormulasCollector(_declarations).traverseNode(_pProposition);
    _pModule->getLemmas().add(std::make_shared<LemmaDeclaration>(_pProposition, std::make_shared<Label>(L"")));
}

typedef std::map<TypePtr, NamedReferenceTypePtr, PtrLess<Type>> TypesMap;

class MoveOutStructuredTypes : public Visitor {
public:
    MoveOutStructuredTypes(TypesMap& _container) :
        Visitor(CHILDREN_FIRST), m_container(_container)
    {}

    bool traverseModule(const ModulePtr& _module) override {
        TypesMap last;
        last.swap(m_container);

        const bool bResult = Visitor::traverseModule(_module);

        for (auto i: m_container)
            _module->getTypes().add(i.second->getDeclaration());

        m_container.swap(last);
        return bResult;
    }

    bool visitType(const TypePtr& _type) override {
        if (getParent() && getParent()->getNodeKind() == Node::STATEMENT &&
            getParent()->as<Statement>()->getKind() == Statement::TYPE_DECLARATION)
            return true;

        if (_type->getKind() < Type::ENUM
            || _type->getKind() == Type::NAMED_REFERENCE)
            return true;

        if (_type->getKind() == Type::ARRAY && getRole() == R_ArrayBaseType)
            return true;

        NamedReferenceTypePtr pReference;

        auto iReference = m_container.find(_type);
        if (iReference == m_container.end()) {
            TypeDeclarationPtr pDedclaration = std::make_shared<TypeDeclaration>(L"", _type);
            pReference = std::make_shared<NamedReferenceType>(pDedclaration);
            m_container.insert(std::make_pair(_type, pReference));
        } else
            pReference = iReference->second;

        callSetter(pReference);

        return true;
    }

private:
     TypesMap & m_container;
};

void moveOutStructuredTypes(const ir::ModulePtr& _pModule) {
    TypesMap container;
    MoveOutStructuredTypes(container).traverseNode(_pModule);
}

typedef std::map<ExpressionPtr, FormulaDeclarationPtr, PtrLess<Expression>> ExpressionMap;

class MoveOutExpressions : public Visitor {
public:
    MoveOutExpressions(ExpressionMap& _container) :
        Visitor(CHILDREN_FIRST), m_container(_container)
    {}

    bool traverseModule(const ModulePtr& _module) override {
        ExpressionMap last;
        last.swap(m_container);

        const bool bResult = Visitor::traverseModule(_module);

        for (auto i: m_container)
            _module->getFormulas().add(i.second);

        m_container.swap(last);
        return bResult;
    }

    bool traverseVariableDeclaration(const VariableDeclarationPtr& _var) override {
        return true;
    }

    void moveOut(const ExpressionPtr& _expr) {
        NamedValues params;

        // TODO: Should use lexical context instead of collect values.
        na::collectValues(_expr, params);

        auto iReference = m_container.find(_expr);
        const FormulaDeclarationPtr
            pFormula = iReference == m_container.end() ?
                declareFormula(L"", _expr) : iReference->second;

        if (iReference == m_container.end())
            m_container.insert(std::make_pair(_expr, pFormula));

        callSetter(makeCall(pFormula, params));
    }

    bool visitReplacement(const ReplacementPtr& _expr) override {
        if (_expr->getNewValues()->getConstructorKind() == Constructor::ARRAY_ITERATION)
            moveOut(_expr);
        return true;
    }

    bool visitArrayIteration(const ArrayIterationPtr &_expr) override {
        if (getRole() != R_ReplacementValue)
            moveOut(_expr);
        return true;
    }

    bool visitArrayConstructor(const ArrayConstructorPtr &_expr) override {
        moveOut(_expr);
        return true;
    }

private:
     ExpressionMap & m_container;
};

void moveOutExpressions(const ir::ModulePtr& _pModule) {
    ExpressionMap container;
    MoveOutExpressions(container).traverseNode(_pModule);
}

class Instantiate : public Visitor {
public:
    Instantiate(const NamedValues& _params, const Collection<Expression>& _args) :
        Visitor(CHILDREN_FIRST), m_params(_params), m_args(_args)
    {}

    ExpressionPtr getExpression(const NamedValuePtr& _pValue);
    TypePtr getFromType(const TypeDeclarationPtr& _pType);
    TypePtr getFromFreshType(const TypePtr& _pType);

    bool visitVariableReference(const VariableReferencePtr& _var) override;
    bool visitNamedReferenceType(const NamedReferenceTypePtr& _type) override;
    bool visitType(const TypePtr& _type) override;

    bool traverseNamedValue(const NamedValuePtr& _node) override;

private:
    const NamedValues &m_params;
    const Collection<Expression> &m_args;
};

ExpressionPtr Instantiate::getExpression(const NamedValuePtr& _pValue) {
    for (size_t i = 0; i < m_params.size(); ++i)
        if (m_params.get(i) == _pValue)
            return m_args.get(i);
    return NULL;
}

TypePtr Instantiate::getFromType(const TypeDeclarationPtr& _pType) {
    for (size_t i = 0; i < m_params.size(); ++i) {
        if (m_params.get(i)->getType()
            && m_params.get(i)->getType()->getKind() == Type::TYPE
            && m_params.get(i)->getType()->as<TypeType>()->getDeclaration() == _pType)
            return m_args.get(i)->as<TypeExpr>()->getContents();
    }
    return NULL;
}

TypePtr Instantiate::getFromFreshType(const TypePtr& _pType) {
    for (size_t i = 0; i < m_params.size(); ++i) {
        if (m_params.get(i)->getType()
            && m_params.get(i)->getType()->getKind() == Type::TYPE
            && m_params.get(i)->getType()->as<TypeType>()->getDeclaration()
            && m_params.get(i)->getType()->as<TypeType>()->getDeclaration()->getType() == _pType)
            return m_args.get(i)->as<TypeExpr>()->getContents();
    }
    return NULL;
}

bool Instantiate::visitVariableReference(const VariableReferencePtr& _var) {
    if (!_var->getTarget())
        return true;
    const auto pExpr = getExpression(_var->getTarget());
    if (!pExpr)
        return true;
    callSetter(pExpr);
    return true;
}

bool Instantiate::visitNamedReferenceType(const NamedReferenceTypePtr& _type) {
    if (!_type->getDeclaration())
        return true;
    const auto pType = getFromType(_type->getDeclaration());
    if (!pType)
        return true;
    callSetter(pType);
    return true;
}

bool Instantiate::visitType(const TypePtr& _type) {
    if (_type->getKind() != Type::FRESH)
        return true;
    const auto pType = getFromFreshType(_type);
    if (!pType)
        return true;
    callSetter(pType);
    return true;
}

bool Instantiate::traverseNamedValue(const NamedValuePtr& _node) {
    if (getLoc().role != R_ModuleParam)
        return Visitor::traverseNamedValue(_node);
    return true;
}

void instantiateModule(const ModulePtr& _pModule, const Collection<Expression>& _args) {
    if (_pModule->getParams().empty())
        return;
    Instantiate(_pModule->getParams(), _args).traverseNode(_pModule);
}

class Normalizer : public Visitor {
public:
    typedef std::multiset<ExpressionPtr, PtrLess<Expression> > Operands;
    Normalizer(const NodePtr& _pRoot, bool _bIgnoreTypes) :
        Visitor(CHILDREN_FIRST), m_pRoot(_pRoot), m_bIgnoreTypes(_bIgnoreTypes)
    {}

    static void extractBinaryOperands(const BinaryPtr& _pBinary, int _nOperator,
            const TypePtr & _pType, Operands& _container, bool _bIgnoreTypes, bool _bFirst);
    bool visitBinary(const BinaryPtr& _bin) override;

    NodePtr run();

private:
    NodePtr m_pRoot;
    bool m_bIgnoreTypes;

    static bool _canContinueNormalize(int _nPrevOperator, int _nOperator,
            const TypePtr & _pPrevType, const TypePtr & _pLeftType,
            const TypePtr & _pRightType, bool _bIgnoreTypes);
};

void Normalizer::extractBinaryOperands(const BinaryPtr& _pBinary,
        int _nOperator, const TypePtr & _pType, Operands& _container,
        bool _bIgnoreTypes, bool _bFirst)
{
    if (!_pBinary)
        return;

    if (!_bFirst && !_canContinueNormalize(_nOperator, _pBinary->getOperator(), _pType,
            _pBinary->getLeftSide() ? _pBinary->getLeftSide()->getType() : nullptr,
            _pBinary->getRightSide() ? _pBinary->getRightSide()->getType() : nullptr,
            _bIgnoreTypes))
    {
        _container.insert(_pBinary);
        return;
    }

    if (_pBinary->getLeftSide()) {
        if (_pBinary->getLeftSide()->getKind() == Expression::BINARY)
            extractBinaryOperands(_pBinary->getLeftSide()->as<Binary>(),
                    _nOperator, _pType, _container, _bIgnoreTypes, false);
        else
            _container.insert(_pBinary->getLeftSide());
    }

    if (_pBinary->getRightSide()) {
        if (_pBinary->getRightSide()->getKind() == Expression::BINARY)
            extractBinaryOperands(_pBinary->getRightSide()->as<Binary>(),
                    _nOperator, _pType, _container, _bIgnoreTypes, false);
        else
            _container.insert(_pBinary->getRightSide());
    }
}

bool Normalizer::visitBinary(const BinaryPtr& _bin) {
    if (!_canContinueNormalize(_bin->getOperator(), _bin->getOperator(),
            nullptr, nullptr, nullptr, true))
        return true;

    const auto pParent = getParent();

    if (pParent &&
            pParent->getNodeKind() == Node::EXPRESSION &&
            pParent->as<Expression>()->getKind() == Expression::BINARY)
    {
        const auto pPrevBinary = pParent->as<Binary>();
        if (_canContinueNormalize(pPrevBinary->getOperator(), _bin->getOperator(),
                pPrevBinary->getType(),
                _bin->getLeftSide() ? _bin->getLeftSide()->getType() : nullptr,
                _bin->getRightSide() ? _bin->getRightSide()->getType() : nullptr,
                m_bIgnoreTypes))
            return true;
    }

    BinaryPtr pBin = _bin;
    Operands operands;

    extractBinaryOperands(pBin, pBin->getOperator(), pBin->getType(),
            operands, m_bIgnoreTypes, true);
    pBin = std::make_shared<Binary>(pBin->getOperator(), operands);

    if (m_pRoot == _bin)
        m_pRoot = pBin;
    else
        callSetter(pBin);

    return true;
}

bool Normalizer::_canContinueNormalize(int _nPrevOperator, int _nOperator,
            const TypePtr & _pPrevType, const TypePtr & _pLeftType,
            const TypePtr & _pRightType, bool _bIgnoreTypes)
{
    if (_nPrevOperator != _nOperator)
        return false;

    if (_nOperator == Binary::ADD || _nOperator == Binary::MULTIPLY ||
            _nOperator == Binary::BOOL_AND || _nOperator == Binary::BOOL_OR ||
            _nOperator == Binary::BOOL_XOR || _nOperator == Binary::IFF)
        return true;

    if (_nOperator != Binary::EQUALS && _nOperator != Binary::NOT_EQUALS)
        return false;

    if (_bIgnoreTypes)
        return true;

    if (!_pPrevType || !_pLeftType || !_pRightType)
        return false;

    if (*_pPrevType == *_pLeftType && *_pPrevType == *_pRightType)
        return true;

    const auto primitiveType = [](const Type & _type) {
        return _type.getKind() >= Type::BOTTOM &&
                _type.getKind() >= Type::STRING;
    };

    return primitiveType(*_pPrevType) && primitiveType(*_pLeftType) &&
            primitiveType(*_pRightType) && _pPrevType->getKind() == _pLeftType->getKind() &&
            _pPrevType->getKind() == _pRightType->getKind();
}

NodePtr Normalizer::run() {
    traverseNode(m_pRoot);
    return m_pRoot;
}

NodePtr normalizeExpressions(const NodePtr& _pNode, bool _bIgnoreTypes /* = false */) {
    if (!_pNode)
        return nullptr;
    return Normalizer(_pNode, _bIgnoreTypes).run();
}

ExpressionPtr conjunctiveNormalForm(const ExpressionPtr& _pExpr) {
    if (!_pExpr)
        return ExpressionPtr();

    struct{ ExpressionPtr pFrom, pTo; } rules[] = {
        // Rule 1.1: A -> B  =>  !A or B
        std::make_shared<Binary>(Binary::IMPLIES, std::make_shared<Wild>(L"a"), std::make_shared<Wild>(L"b")),
        std::make_shared<Binary>(Binary::BOOL_OR, std::make_shared<Unary>(Unary::BOOL_NEGATE, std::make_shared<Wild>(L"a")), std::make_shared<Wild>(L"b")),

        // Rule 1.2: A <-> B => (!A or B) and (A or !B)
        std::make_shared<Binary>(Binary::IFF, std::make_shared<Wild>(L"a"), std::make_shared<Wild>(L"b")),
        std::make_shared<Binary>(Binary::BOOL_AND,
            std::make_shared<Binary>(Binary::BOOL_OR, std::make_shared<Unary>(Unary::BOOL_NEGATE, std::make_shared<Wild>(L"a")), std::make_shared<Wild>(L"b")),
            std::make_shared<Binary>(Binary::BOOL_OR, std::make_shared<Wild>(L"b"), std::make_shared<Unary>(Unary::BOOL_NEGATE, std::make_shared<Wild>(L"b")))),

        // Rule 2.1: !(A or B) => !A and !B
        std::make_shared<Unary>(Unary::BOOL_NEGATE, std::make_shared<Binary>(Binary::BOOL_OR, std::make_shared<Wild>(L"a"), std::make_shared<Wild>(L"b"))),
        std::make_shared<Binary>(Binary::BOOL_AND,
            std::make_shared<Unary>(Unary::BOOL_NEGATE, std::make_shared<Wild>(L"a")),
            std::make_shared<Unary>(Unary::BOOL_NEGATE, std::make_shared<Wild>(L"b"))),

        // Rule 2.2: !(A and B) => !A or !B
        std::make_shared<Unary>(Unary::BOOL_NEGATE, std::make_shared<Binary>(Binary::BOOL_AND, std::make_shared<Wild>(L"a"), std::make_shared<Wild>(L"b"))),
        std::make_shared<Binary>(Binary::BOOL_OR,
            std::make_shared<Unary>(Unary::BOOL_NEGATE, std::make_shared<Wild>(L"a")),
            std::make_shared<Unary>(Unary::BOOL_NEGATE, std::make_shared<Wild>(L"b"))),

        // Rule 3: !!A => A
        std::make_shared<Unary>(Unary::BOOL_NEGATE, std::make_shared<Unary>(Unary::BOOL_NEGATE, std::make_shared<Wild>(L"a"))),
        std::make_shared<Wild>(L"a"),

        // Rule 4.1: A or (B and C) => (A or B) and (A or C)
        std::make_shared<Binary>(Binary::BOOL_OR,
            std::make_shared<Wild>(L"a"),
            std::make_shared<Binary>(Binary::BOOL_AND, std::make_shared<Wild>(L"b"), std::make_shared<Wild>(L"c"))),
        std::make_shared<Binary>(Binary::BOOL_AND,
            std::make_shared<Binary>(Binary::BOOL_OR, std::make_shared<Wild>(L"a"), std::make_shared<Wild>(L"b")),
            std::make_shared<Binary>(Binary::BOOL_OR, std::make_shared<Wild>(L"a"), std::make_shared<Wild>(L"c"))),

        // Rule 4.2: (A and B) or (A and C) => A and (B or C)
        std::make_shared<Binary>(Binary::BOOL_OR,
            std::make_shared<Binary>(Binary::BOOL_AND, std::make_shared<Wild>(L"a"), std::make_shared<Wild>(L"b")),
            std::make_shared<Binary>(Binary::BOOL_AND, std::make_shared<Wild>(L"a"), std::make_shared<Wild>(L"c"))),
        std::make_shared<Binary>(Binary::BOOL_AND,
            std::make_shared<Wild>(L"a"),
            std::make_shared<Binary>(Binary::BOOL_OR, std::make_shared<Wild>(L"b"), std::make_shared<Wild>(L"c"))),

        // End.
        ExpressionPtr(), ExpressionPtr()
    };

    auto mutate = [&] (const ExpressionPtr& _pOrigin) {
        ExpressionPtr pExpr = clone(_pOrigin);
        for (size_t i = 0; rules[i].pFrom; ++i)
            pExpr = Expression::substitute(pExpr, rules[i].pFrom, rules[i].pTo)->as<Expression>();
        return pExpr;
    };

    ExpressionPtr pCurrent = _pExpr, pLast;

    do {
        pLast = pCurrent;
        pCurrent = mutate(pCurrent);
    } while (*pCurrent != *pLast);

    return pCurrent;

}

} // namespace tr
