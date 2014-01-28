/// \file node_analisys.cpp
///

#include <set>
#include "ir/visitor.h"
#include "ir/expressions.h"
#include "node_analysis.h"
#include "term_rewriting.h"

using namespace ir;
using namespace tr;

namespace na {

class BannedNodesFinder : public Visitor {
public:
    BannedNodesFinder() : m_bResult(false) {}

    virtual bool traverseFormulaDeclaration(FormulaDeclaration& _formula) {
        // FIXME Mutual recursion is not detected.
        m_formulas.insert(&_formula);

        const bool bResult = Visitor::traverseFormulaDeclaration(_formula);

        m_formulas.erase(&_formula);

        return bResult;
    }

    virtual bool traverseFormulaCall(FormulaCall& _formulaCall) {
        if (!_formulaCall.getTarget())
            return true;
        if (m_formulas.find(_formulaCall.getTarget()) != m_formulas.end()) {
            m_bResult = true;
            return false;
        }
        return traverseNode(*_formulaCall.getTarget());
    }

    virtual bool visitFormula(Formula& _formula) {
        m_bResult |= (_formula.getQuantifier() == Formula::EXISTENTIAL);
        return !m_bResult;
    }

    virtual bool visitType(Type& _type) {
        m_bResult |= (_type.getKind() == Type::FRESH);
        return !m_bResult;
    }

    bool run(const NodePtr& _pNode) {
        traverseNode(*_pNode);
        return m_bResult;
    }

private:
    std::set<FormulaDeclarationPtr> m_formulas;
    bool m_bResult;
};

bool containsBannedNodes(const NodePtr& _pNode) {
    return BannedNodesFinder().run(_pNode);
}

class FunctionCallFinder : public Visitor {
public:
    FunctionCallFinder(const Node& _node) :
        m_pNode(&_node), m_bResult(false)
    {}

    virtual bool traverseFunctionCall(FunctionCall &_node) {
        m_bResult = true;
        return false;
    }

    bool run() {
        traverseNode(*m_pNode);
        return m_bResult;
    }

private:
    NodePtr m_pNode;
    bool m_bResult;
};

bool containsCall(NodePtr _pNode) {
    if (!_pNode)
        return false;
    FunctionCallFinder fcf(*_pNode);
    return fcf.run();
}

class ValueCollector : public Visitor {
public:
    ValueCollector(const NodePtr& _pNode, ValuesSet& _container) :
        m_pNode(_pNode), m_container(_container)
    {}

    bool addValue(const NamedValue &_val) {
        if (m_bound.find(&_val) != m_bound.end())
            return false;
        m_container.insert(&_val);
        return true;
    }

    virtual bool visitVariableReference(VariableReference &_node) {
        addValue(*_node.getTarget());
        return false;
    }

    virtual bool visitNamedValue(NamedValue &_node) {
        addValue(_node);
        return false;
    }

    virtual bool visitParam(Param &_node) {
        addValue(_node);
        return false;
    }

    virtual bool traverseFormula(Formula &_node) {
        VISITOR_ENTER(Formula, _node);

        ValuesSet oldBound = m_bound;

        m_bound.insert(_node.getBoundVariables().begin(), _node.getBoundVariables().end());
        VISITOR_TRAVERSE(Expression, Subformula, _node.getSubformula(), _node, Formula, setSubformula);
        m_bound.swap(oldBound);

        VISITOR_EXIT();
    }

    // Cause we don't need to collect values from lambda body.
    virtual bool traverseLambda(Lambda& _lambda) {
        return false;
    }

    ValuesSet& run() {
        traverseNode(*m_pNode);
        return m_container;
    }

private:
    const NodePtr m_pNode;
    ValuesSet& m_container;
    ValuesSet m_bound;
};

void collectValues(const NodePtr& _pNode, ValuesSet& _container) {
    ValueCollector(_pNode, _container).run();
}

void collectValues(const NodePtr& _pNode, ValuesSet& _container, const ValuesSet& _allow) {
    ValuesSet collection;
    collectValues(_pNode, collection);
    std::set_intersection(collection.begin(), collection.end(),
        _allow.begin(), _allow.end(), std::inserter(_container, _container.end()));
}

void collectValues(const NodePtr& _pNode, NamedValues& _container) {
    ValuesSet container;
    collectValues(_pNode, container);
    _container.prepend(container.begin(), container.end());
}

void collectValues(const NodePtr& _pNode, NamedValues& _container, const NamedValues& _allow) {
    ValuesSet container;
    collectValues(_pNode, container);

    _container.assign(_allow);

    size_t i = 0;
    while (i < _container.size()) {
        if (container.find(_container.get(i)) == container.end()) {
            _container.remove(i);
            continue;
        }
        ++i;
    }
}

class NodeFinder : public Visitor {
public:
    NodeFinder() : Visitor(), m_bResult(false) {}

    virtual bool traverseNode(Node& _node) {
        if (_node == *m_pNode) {
            m_bResult = true;
            return false;
        }
        return true;
    }

    bool find(const NodePtr& _pNode, const NodePtr& _pPattern) {
        if (!_pNode || !_pPattern)
            return false;

        m_pNode = _pPattern;
        traverseNode(*_pNode);

        return m_bResult;
    }

private:
    NodePtr m_pNode;
    bool m_bResult;
};

bool nodeExists(const NodePtr& _pNode, const NodePtr& _pPattern) {
    return NodeFinder().find(_pNode, _pPattern);
}

void getResults(const StatementPtr& _pStatement, ValuesSet& _container) {
    if (!_pStatement)
        return;

    switch (_pStatement->getKind()) {
        case Statement::ASSIGNMENT:
            collectValues(_pStatement.as<Assignment>()->getLValue(), _container);
            break;
        case Statement::BLOCK:
        case Statement::PARALLEL_BLOCK:
            for (size_t i = 0; i < _pStatement.as<Block>()->size(); ++i)
                getResults(_pStatement.as<Block>()->get(i), _container);
            break;
        case Statement::IF:
            getResults(_pStatement.as<If>()->getBody(), _container);
            getResults(_pStatement.as<If>()->getElse(), _container);
            break;
        case Statement::CALL:
            for (size_t i = 0; i < _pStatement.as<Call>()->getBranches().size(); ++i)
                for (size_t j = 0; j < _pStatement.as<Call>()->getBranches().get(i)->size(); ++j)
                    collectValues(_pStatement.as<Call>()->getBranches().get(i)->get(j), _container);
            break;
    }
}

void getArgs(const StatementPtr& _pStatement, ValuesSet& _container) {
    if (!_pStatement)
        return;

    switch (_pStatement->getKind()) {
        case Statement::ASSIGNMENT:
            collectValues(_pStatement.as<Assignment>()->getExpression(), _container);
            break;
        case Statement::BLOCK: {
            ValuesSet args, results;
            getResults(_pStatement, results);
            for (size_t i = 0; i < _pStatement.as<Block>()->size(); ++i)
                getArgs(_pStatement.as<Block>()->get(i), args);
            std::set_difference(args.begin(), args.end(), results.begin(), results.end(), std::inserter(_container, _container.end()));
            break;
        }
        case Statement::PARALLEL_BLOCK:
            for (size_t i = 0; i < _pStatement.as<ParallelBlock>()->size(); ++i)
                getArgs(_pStatement.as<ParallelBlock>()->get(i), _container);
            break;
        case Statement::IF:
            collectValues(_pStatement.as<If>()->getArg(), _container);
            getArgs(_pStatement.as<If>()->getBody(), _container);
            getArgs(_pStatement.as<If>()->getElse(), _container);
            break;
        case Statement::CALL:
            collectValues(&_pStatement.as<Call>()->getArgs(), _container);
            break;
    }
}

void getParameters(const StatementPtr& _pStatement, ValuesSet& _container) {
    getArgs(_pStatement, _container);
    getResults(_pStatement, _container);
}

void getPredicateParams(const Predicate &_predicate, NamedValues& _params) {
    for (size_t i = 0; i < _predicate.getInParams().size(); ++i)
        _params.add(_predicate.getInParams().get(i));
    for (size_t i = 0; i < _predicate.getOutParams().size(); ++i) {
        const Branch& b = *_predicate.getOutParams().get(i);
        for (size_t j = 0; j < b.size(); ++j)
            _params.add(b.get(j));
    }
}

void getPredicateParams(const PredicateType &_predicateType, NamedValues& _params) {
    for (size_t i = 0; i < _predicateType.getInParams().size(); ++i)
        _params.add(_predicateType.getInParams().get(i));
    for (size_t i = 0; i < _predicateType.getOutParams().size(); ++i) {
        const Branch& b = *_predicateType.getOutParams().get(i);
        for (size_t j = 0; j < b.size(); ++j)
            _params.add(b.get(j));
    }
}

StatementPtr extractCallArguments(const CallPtr& _pCall) {
    if (!_pCall)
        return NULL;

    const Call& call = *_pCall;
    MultiassignmentPtr pMA = new Multiassignment();

    for (size_t i = 0; i < call.getArgs().size(); ++i) {
        pMA->getLValues().add(new VariableReference(new NamedValue(L"",
            call.getArgs().get(i)->getType())));
        pMA->getExpressions().add(call.getArgs().get(i));
    }

    return modifyStatement(pMA);
}

void getArgsMap(const FormulaCall &_call, ArgsMap& _args) {
    for (size_t i = 0; i < _call.getArgs().size(); ++i)
        _args.addExpression(*_call.getTarget()->getParams().get(i), _call.getArgs().get(i));
}

void getArgsMap(const FunctionCall &_call, ArgsMap& _args) {
    PredicateTypePtr pCallType = _call.getPredicate()->getType().as<PredicateType>();
    for (size_t i = 0; i < _call.getArgs().size(); ++i)
        _args.addExpression(*pCallType->getInParams().get(i), _call.getArgs().get(i));
}

void getArgsMap(const Call &_call, ArgsMap& _args) {
    PredicateTypePtr pCallType = _call.getPredicate()->getType().as<PredicateType>();

    for (size_t i = 0; i < _call.getArgs().size(); ++i)
        _args.addExpression(*pCallType->getInParams().get(i), _call.getArgs().get(i));

    for (size_t i = 0; i < _call.getBranches().size(); ++i) {
        CallBranch &br = *_call.getBranches().get(i);
        for (size_t j = 0; j < br.size(); ++j)
            _args.addExpression(*pCallType->getOutParams().get(i)->get(j), br.get(j));
    }
}

bool isRecursiveCall(const ir::CallPtr& _pCall, const ir::PredicatePtr& _pPred) {
    return _pPred && _pCall && _pCall->getPredicate() && _pCall->getPredicate()->getKind() == Expression::PREDICATE
        && _pCall->getPredicate().as<PredicateReference>()->getTarget() == _pPred;
}

ir::FormulaPtr generalize(const ExpressionPtr& _pExpr) {
    ValuesSet freeValues;
    collectValues(_pExpr, freeValues);

    FormulaPtr pFormula = new Formula(Formula::UNIVERSAL, _pExpr);
    pFormula->getBoundVariables().prepend(freeValues.begin(), freeValues.end());
    return pFormula;
}

ir::FormulaPtr setQuantifier(int _quantifier, const ir::ExpressionPtr& _pExpr, const ValuesSet& _bound) {
    ValuesSet freeValues;
    collectValues(_pExpr, freeValues, _bound);

    FormulaPtr pFormula = new Formula(_quantifier, _pExpr);
    pFormula->getBoundVariables().prepend(freeValues.begin(), freeValues.end());
    return pFormula;
}

FormulaDeclarationPtr declareFormula(const std::wstring &_strName, const ExpressionPtr &_pExpr) {
    NamedValues params;
    collectValues(_pExpr, params);
    return declareFormula(_strName, _pExpr, params);
}

FormulaDeclarationPtr declareFormula(const std::wstring &_strName, const ExpressionPtr &_pExpr, const NamedValues& _params) {
    ExpressionPtr pExpr = _pExpr;
    if (pExpr->getKind() == ir::Expression::FORMULA
        && pExpr.as<Formula>()->getQuantifier() == Formula::NONE)
        pExpr = pExpr.as<Formula>()->getSubformula();

    NamedValues params;
    collectValues(pExpr, params, _params);
    FormulaDeclarationPtr pDecl = new FormulaDeclaration(_strName, pExpr->getType(), pExpr);
    pDecl->getParams().assign(params);

    return pDecl;
}

FormulaDeclarationPtr declareFormula(const std::wstring &_strName, const Predicate &_predicate, const Expression &_expr) {
    NamedValues params;
    getPredicateParams(_predicate, params);
    return declareFormula(_strName, &_expr, params);
}

} // namespace na
