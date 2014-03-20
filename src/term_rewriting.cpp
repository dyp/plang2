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

    virtual bool traverseFunctionCall(FunctionCall &_node) {
        m_pCall = &_node;
        if (m_pNewNode)
            callSetter(m_pNewNode);
        return false;
    }

    FunctionCallPtr run() {
        traverseNode(*m_pNode);
        return m_pCall;
    }

private:
    NodePtr m_pNode, m_pNewNode;
    FunctionCallPtr m_pCall;
};

CallPtr getCallFromFunctionCall(const FunctionCall &_fCall, const VariableReference &_var) {
    CallPtr pCall = new ir::Call(_fCall.getPredicate());
    ir::CallBranchPtr pBranch = new ir::CallBranch();

    pCall->getArgs().assign(_fCall.getArgs());
    pBranch->add(&_var);
    pCall->getBranches().add(pBranch);

    return pCall;
}

std::pair<NodePtr, NodePtr> extractFirstCall(const Node& _node) {
    if (!containsCall(&_node))
        return std::make_pair(NodePtr(NULL), &_node);

    const FunctionCallPtr pFunctionCall = ReplaceCall(&_node, NULL).run();
    const VariableReferencePtr pVar = new VariableReference(new NamedValue(L"", pFunctionCall->getType()));
    const CallPtr pCall = getCallFromFunctionCall(*pFunctionCall, *pVar);

    NodePtr pTail;
    if (&_node != pFunctionCall.ptr()) {
        pTail = clone(_node);
        ReplaceCall(pTail, pVar).run();
    } else
        pTail = pVar;

    return std::make_pair(pCall, pTail);
}

StatementPtr modifyStatement(const StatementPtr& _pStatement) {
    st::StmtVertex top(_pStatement);
    top.expand();
    top.modifyForVerification();
    top.simplify();
    return top.mergeForVerification();
}

class ModifyStatements : public Visitor {
public:
    ModifyStatements() {}

    virtual bool _traverseAnonymousPredicate(AnonymousPredicate &_decl) {
        const StatementPtr
            pOldStatement = _decl.getBlock(),
            pNewStatement = modifyStatement(pOldStatement);

        if (!pNewStatement)
            return true;

        _decl.getBlock()->clear();

        if (pNewStatement->getKind() == Statement::BLOCK)
            _decl.getBlock()->assign(*pNewStatement.as<Block>());
        else
            _decl.getBlock()->add(pNewStatement);

        return true;
    }
};

void modifyModule(const ir::ModulePtr& _pModule) {
    ModifyStatements().traverseNode(*_pModule);
}

FormulaCallPtr makeCall(const ir::FormulaDeclarationPtr& _pFormula, ArgsMap& _args) {
    if (!_pFormula)
        return NULL;

    FormulaCallPtr pCall = new FormulaCall(_pFormula);
    for (size_t i = 0; i < _pFormula->getParams().size(); ++i) {
        const ExpressionPtr pArg = _args.getExpression(*_pFormula->getParams().get(i));
        assert(pArg);
        pCall->getArgs().add(pArg);
    }

    return pCall;
}

FormulaCallPtr makeCall(const ir::FormulaDeclarationPtr& _pFormula, const NamedValues& _params) {
    if (!_pFormula)
        return NULL;

    FormulaCallPtr pCall = new FormulaCall(_pFormula);
    for (size_t i = 0; i < _pFormula->getParams().size(); ++i) {
        const size_t cIdx = _params.findIdx(*_pFormula->getParams().get(i));
        assert(cIdx != (size_t)-1);
        pCall->getArgs().add(new VariableReference(_params.get(cIdx)));
    }

    return pCall;
}

FormulaCallPtr makeCall(const ir::FormulaDeclarationPtr& _pFormula, const Predicate& _predicate) {
    NamedValues params;
    getPredicateParams(_predicate, params);
    return makeCall(_pFormula, params);
}

FormulaCallPtr makeCall(const ir::FormulaDeclarationPtr& _pFormula, const FormulaCall &_call) {
    ArgsMap args;
    getArgsMap(_call, args);
    return makeCall(_pFormula, args);
}

FormulaCallPtr makeCall(const ir::FormulaDeclarationPtr& _pFormula, const Call &_call) {
    ArgsMap args;
    getArgsMap(_call, args);
    return makeCall(_pFormula, args);
}

class FormulasCollector : public Visitor {
public:
    FormulasCollector(const ModulePtr& _pModule) :
        m_pModule(_pModule) {}
    virtual bool traverseFormulaCall(FormulaCall& _call);

private:
    ModulePtr m_pModule;
    std::set<FormulaDeclarationPtr> m_pTraversedFormulas;
};

bool FormulasCollector::traverseFormulaCall(FormulaCall& _call) {
    if (!_call.getTarget())
        return true;
    if (!m_pTraversedFormulas.insert(_call.getTarget()).second)
        return true;

    m_pModule->getFormulas().add(_call.getTarget());
    traverseNode(*_call.getTarget());

    return true;
}

void declareLemma(const ModulePtr& _pModule, const ExpressionPtr& _pProposition) {
    FormulasCollector(_pModule).traverseNode(*_pProposition);
    _pModule->getLemmas().add(new LemmaDeclaration(_pProposition));
}

typedef std::map<TypePtr, NamedReferenceTypePtr, PtrLess<Type>> TypesMap;

class MoveOutStructuredTypes : public Visitor {
public:
    MoveOutStructuredTypes(TypesMap& _container) :
        Visitor(CHILDREN_FIRST), m_container(_container)
    {}

    virtual bool traverseModule(Module& _module) {
        TypesMap last;
        last.swap(m_container);

        const bool bResult = Visitor::traverseModule(_module);

        for (auto i: m_container)
            _module.getTypes().add(i.second->getDeclaration());

        m_container.swap(last);
        return bResult;
    }

    virtual bool visitType(Type& _type) {
        if (getParent() && getParent()->getNodeKind() == Node::STATEMENT &&
            ((Statement*)getParent())->getKind() == Statement::TYPE_DECLARATION)
            return true;

        if (_type.getKind() < Type::ENUM
            || _type.getKind() == Type::NAMED_REFERENCE)
            return true;

        NamedReferenceTypePtr pReference;

        auto iReference = m_container.find(&_type);
        if (iReference == m_container.end()) {
            TypeDeclarationPtr pDedclaration = new TypeDeclaration(L"", &_type);
            pReference = new NamedReferenceType(pDedclaration);
            m_container.insert(std::make_pair(&_type, pReference));
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
    MoveOutStructuredTypes(container).traverseNode(*_pModule);
}

class Instantiate : public Visitor {
public:
    Instantiate(const NamedValues& _params, const Collection<Expression>& _args) :
        Visitor(CHILDREN_FIRST), m_params(_params), m_args(_args)
    {}

    ExpressionPtr getExpression(const NamedValuePtr& _pValue);
    TypePtr getFromType(const TypeDeclarationPtr& _pType);
    TypePtr getFromFreshType(const TypePtr& _pType);

    virtual bool visitVariableReference(VariableReference& _var);
    virtual bool visitNamedReferenceType(NamedReferenceType& _type);
    virtual bool visitType(Type& _type);

    virtual bool traverseNamedValue(NamedValue& _node);

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
            && m_params.get(i)->getType().as<TypeType>()->getDeclaration() == _pType)
            return m_args.get(i).as<TypeExpr>()->getContents();
    }
    return NULL;
}

TypePtr Instantiate::getFromFreshType(const TypePtr& _pType) {
    for (size_t i = 0; i < m_params.size(); ++i) {
        if (m_params.get(i)->getType()
            && m_params.get(i)->getType()->getKind() == Type::TYPE
            && m_params.get(i)->getType().as<TypeType>()->getDeclaration()
            && m_params.get(i)->getType().as<TypeType>()->getDeclaration()->getType() == _pType)
            return m_args.get(i).as<TypeExpr>()->getContents();
    }
    return NULL;
}

bool Instantiate::visitVariableReference(VariableReference& _var) {
    if (!_var.getTarget())
        return true;
    const ExpressionPtr pExpr = getExpression(_var.getTarget());
    if (!pExpr)
        return true;
    callSetter(pExpr);
    return true;
}

bool Instantiate::visitNamedReferenceType(NamedReferenceType& _type) {
    if (!_type.getDeclaration())
        return true;
    const TypePtr pType = getFromType(_type.getDeclaration());
    if (!pType)
        return true;
    callSetter(pType);
    return true;
}

bool Instantiate::visitType(Type& _type) {
    if (_type.getKind() != Type::FRESH)
        return true;
    const TypePtr pType = getFromFreshType(&_type);
    if (!pType)
        return true;
    callSetter(pType);
    return true;
}

bool Instantiate::traverseNamedValue(NamedValue& _node) {
    if (getLoc().role != R_ModuleParam)
        return Visitor::traverseNamedValue(_node);
    return true;
}

void instantiateModule(const ModulePtr& _pModule, const Collection<Expression>& _args) {
    if (_pModule->getParams().empty())
        return;
    Instantiate(_pModule->getParams(), _args).traverseNode(*_pModule);
}

class Normalizer : public Visitor {
public:
    typedef std::multiset<ExpressionPtr, PtrLess<Expression> > Operands;
    Normalizer(const NodePtr& _pRoot) :
        Visitor(CHILDREN_FIRST), m_pRoot(_pRoot)
    {}

    static void extractBinaryOperands(const BinaryPtr& _pBinary, int _nOperator,
            Operands& _container);
    virtual bool visitBinary(Binary& _bin);

    NodePtr run();

private:
    NodePtr m_pRoot;
};

void Normalizer::extractBinaryOperands(const BinaryPtr& _pBinary,
        int _nOperator, Operands& _container)
{
    if (!_pBinary)
        return;
    if (_pBinary->getOperator() != _nOperator) {
        _container.insert(_pBinary);
        return;
    }

    if (_pBinary->getLeftSide()) {
        if (_pBinary->getLeftSide()->getKind() == Expression::BINARY)
            extractBinaryOperands(_pBinary->getLeftSide().as<Binary>(), _nOperator, _container);
        else
            _container.insert(_pBinary->getLeftSide());
    }

    if (_pBinary->getRightSide()) {
        if (_pBinary->getRightSide()->getKind() == Expression::BINARY)
            extractBinaryOperands(_pBinary->getRightSide().as<Binary>(), _nOperator, _container);
        else
            _container.insert(_pBinary->getRightSide());
    }
}

bool Normalizer::visitBinary(Binary& _bin) {
    if (!_bin.isSymmetrical())
        return true;

    const Node* pParent = getParent();
    if (pParent &&
        pParent->getNodeKind() == Node::EXPRESSION &&
        ((Expression*)pParent)->getKind() == Expression::BINARY &&
        ((Binary*)pParent)->getOperator() == _bin.getOperator())
        return true;

    BinaryPtr pBin = &_bin;
    Operands operands;

    extractBinaryOperands(pBin, pBin->getOperator(), operands);
    pBin = new Binary(pBin->getOperator(), operands);

    if (m_pRoot.ptr() == &_bin)
        m_pRoot = pBin;
    else
        callSetter(pBin);

    return true;
}

NodePtr Normalizer::run() {
    traverseNode(*m_pRoot);
    return m_pRoot;
}

NodePtr normalizeExpressions(const NodePtr& _pNode) {
    if (!_pNode)
        return nullptr;
    return Normalizer(_pNode).run();
}

ExpressionPtr conjunctiveNormalForm(const ExpressionPtr& _pExpr) {
    if (!_pExpr)
        return ExpressionPtr();

    struct{ ExpressionPtr pFrom, pTo; } rules[] = {
        // Rule 1.1: A -> B  =>  !A or B
        new Binary(Binary::IMPLIES, new Wild(L"a"), new Wild(L"b")),
        new Binary(Binary::BOOL_OR, new Unary(Unary::BOOL_NEGATE, new Wild(L"a")), new Wild(L"b")),

        // Rule 1.2: A <-> B => (!A or B) and (A or !B)
        new Binary(Binary::IFF, new Wild(L"a"), new Wild(L"b")),
        new Binary(Binary::BOOL_AND,
            new Binary(Binary::BOOL_OR, new Unary(Unary::BOOL_NEGATE, new Wild(L"a")), new Wild(L"b")),
            new Binary(Binary::BOOL_OR, new Wild(L"b"), new Unary(Unary::BOOL_NEGATE, new Wild(L"b")))),

        // Rule 2.1: !(A or B) => !A and !B
        new Unary(Unary::BOOL_NEGATE, new Binary(Binary::BOOL_OR, new Wild(L"a"), new Wild(L"b"))),
        new Binary(Binary::BOOL_AND,
            new Unary(Unary::BOOL_NEGATE, new Wild(L"a")),
            new Unary(Unary::BOOL_NEGATE, new Wild(L"b"))),

        // Rule 2.2: !(A and B) => !A or !B
        new Unary(Unary::BOOL_NEGATE, new Binary(Binary::BOOL_AND, new Wild(L"a"), new Wild(L"b"))),
        new Binary(Binary::BOOL_OR,
            new Unary(Unary::BOOL_NEGATE, new Wild(L"a")),
            new Unary(Unary::BOOL_NEGATE, new Wild(L"b"))),

        // Rule 3: !!A => A
        new Unary(Unary::BOOL_NEGATE, new Unary(Unary::BOOL_NEGATE, new Wild(L"a"))),
        new Wild(L"a"),

        // Rule 4.1: A or (B and C) => (A or B) and (A or C)
        new Binary(Binary::BOOL_OR,
            new Wild(L"a"),
            new Binary(Binary::BOOL_AND, new Wild(L"b"), new Wild(L"c"))),
        new Binary(Binary::BOOL_AND,
            new Binary(Binary::BOOL_OR, new Wild(L"a"), new Wild(L"b")),
            new Binary(Binary::BOOL_OR, new Wild(L"a"), new Wild(L"c"))),

        // Rule 4.2: (A and B) or (A and C) => A and (B or C)
        new Binary(Binary::BOOL_OR,
            new Binary(Binary::BOOL_AND, new Wild(L"a"), new Wild(L"b")),
            new Binary(Binary::BOOL_AND, new Wild(L"a"), new Wild(L"c"))),
        new Binary(Binary::BOOL_AND,
            new Wild(L"a"),
            new Binary(Binary::BOOL_OR, new Wild(L"b"), new Wild(L"c"))),

        // End.
        ExpressionPtr(), ExpressionPtr()
    };

    auto mutate = [&] (const ExpressionPtr& _pOrigin) {
        ExpressionPtr pExpr = clone(_pOrigin);
        for (size_t i = 0; rules[i].pFrom; ++i)
            pExpr = Expression::substitute(pExpr, rules[i].pFrom, rules[i].pTo).as<Expression>();
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
