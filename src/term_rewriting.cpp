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

} // namespace tr
