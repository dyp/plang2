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
#include "pp_syntax.h"

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

FormulaCallPtr makeCall(const ir::FormulaDeclarationPtr& _pFormula, const Predicate& _predicate) {
    if (!_pFormula)
        return NULL;

    NamedValues params;
    getPredicateParams(_predicate, params);

    FormulaCallPtr pCall = new FormulaCall(_pFormula);
    for (size_t i = 0; i < _pFormula->getParams().size(); ++i) {
        size_t cIdx = params.findIdx(*_pFormula->getParams().get(i));
        assert(cIdx != -1);
        pCall->getArgs().add(new VariableReference(params.get(cIdx)));
    }

    return pCall;
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

class Instantiate : public Visitor {
public:
    Instantiate(const NamedValues& _params, const Collection<Expression>& _args) :
        m_params(_params), m_args(_args), Visitor(CHILDREN_FIRST)
    {}

    ExpressionPtr getExpression(const NamedValuePtr& _pValue);
    TypePtr getFromType(const TypeDeclarationPtr& _pType);
    TypePtr getFromFreshType(const TypePtr& _pType);

    virtual bool visitVariableReference(VariableReference& _var);
    virtual bool visitNamedReferenceType(NamedReferenceType& _type);
    virtual bool visitType(Type& _type);

    virtual bool traverseNamedValue(NamedValue& _node);

private:
    const NamedValues& m_params;
    const Collection<Expression>& m_args;
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
    Normalizer() : Visitor(CHILDREN_FIRST) {}

    static void extractBinaryOperands(const BinaryPtr& _pBinary, int _nOperator,
            Operands& _container);
    virtual bool traverseExpression(Expression& _expr);
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

bool Normalizer::traverseExpression(Expression& _expr) {
    if (_expr.getKind() != Expression::BINARY || !((const Binary&)_expr).isSymmetrical())
        return Visitor::traverseExpression(_expr);

    BinaryPtr pBin(&_expr);
    Operands operands;

    extractBinaryOperands(pBin, pBin->getOperator(), operands);

    if (!operands.empty()) {
        auto iLast = std::prev(operands.end());

        pBin->setLeftSide(*operands.begin());

        if (iLast != operands.begin()) {
            for (auto i = std::next(operands.begin()); i != iLast; ++i) {
                pBin->setRightSide(new Binary(pBin->getOperator()));
                pBin = pBin->getRightSide().as<Binary>();
                pBin->setLeftSide(*i);
            }

            pBin->setRightSide(*iLast);
        } else
            pBin->setRightSide(nullptr);
    }

    return true;
}

void normalizeExpressions(const NodePtr& _pNode) {
    if (!_pNode)
        return;
    Normalizer().traverseNode(*_pNode);
}

} // namespace tr
