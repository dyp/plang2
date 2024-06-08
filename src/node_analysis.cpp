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

    bool traverseFormulaDeclaration(const FormulaDeclarationPtr& _formula) override {
        // FIXME Mutual recursion is not detected.
        m_formulas.insert(_formula);

        const bool bResult = Visitor::traverseFormulaDeclaration(_formula);

        m_formulas.erase(_formula);

        return bResult;
    }

    bool traverseFormulaCall(const FormulaCallPtr& _formulaCall) override {
        if (!_formulaCall->getTarget())
            return true;
        if (m_formulas.find(_formulaCall->getTarget()) != m_formulas.end()) {
            m_bResult = true;
            return false;
        }
        return traverseNode(_formulaCall->getTarget());
    }

    bool visitFormula(const FormulaPtr& _formula) override {
        m_bResult |= (_formula->getQuantifier() == Formula::EXISTENTIAL);
        return !m_bResult;
    }

    bool visitType(const TypePtr& _type) override {
        m_bResult |= (_type->getKind() == Type::FRESH);
        return !m_bResult;
    }

    bool run(const NodePtr& _pNode) {
        traverseNode(_pNode);
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
    FunctionCallFinder(const NodePtr& _node) :
        m_pNode(_node), m_bResult(false)
    {}

    bool traverseFunctionCall(const FunctionCallPtr &_node) override {
        m_bResult = true;
        return false;
    }

    bool run() {
        traverseNode(m_pNode);
        return m_bResult;
    }

private:
    NodePtr m_pNode;
    bool m_bResult;
};

bool containsCall(const NodePtr& _pNode) {
    if (!_pNode)
        return false;
    FunctionCallFinder fcf(_pNode);
    return fcf.run();
}

class ValueCollector : public Visitor {
public:
    ValueCollector(const NodePtr& _pNode, ValuesSet& _container) :
        m_pNode(_pNode), m_container(_container)
    {}

    bool addValue(const NamedValuePtr &_val) {
        if (m_bound.find(_val) != m_bound.end())
            return false;
        m_container.insert(_val);
        return true;
    }

    bool visitVariableReference(const VariableReferencePtr &_node) override {
        addValue(_node->getTarget());
        return false;
    }

    bool visitNamedValue(const NamedValuePtr &_node) override {
        addValue(_node);
        return false;
    }

    bool visitParam(const ParamPtr &_node) override {
        addValue(_node);
        return false;
    }

    bool traverseArrayIteration(const ArrayIterationPtr &_expr) override {
        ValuesSet oldBound = m_bound;

        m_bound.insert(_expr->getIterators().begin(), _expr->getIterators().end());
        const bool bResult = Visitor::traverseArrayIteration(_expr);
        m_bound.swap(oldBound);

        return bResult;
    }

    bool traverseFormula(const FormulaPtr &_node) override {
        VISITOR_ENTER(Formula, _node);

        ValuesSet oldBound = m_bound;

        m_bound.insert(_node->getBoundVariables().begin(), _node->getBoundVariables().end());
        VISITOR_TRAVERSE(Expression, Subformula, _node->getSubformula(), _node, Formula, setSubformula);
        m_bound.swap(oldBound);

        VISITOR_EXIT();
    }

    // Cause we don't need to collect values from lambda body.
    bool traverseLambda(const LambdaPtr& _lambda) override {
        return false;
    }

    ValuesSet& run() {
        traverseNode(m_pNode);
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

    bool traverseNode(const NodePtr& _node) override {
        if (*_node == *m_pNode) {
            m_bResult = true;
            return false;
        }
        return true;
    }

    bool find(const NodePtr& _pNode, const NodePtr& _pPattern) {
        if (!_pNode || !_pPattern)
            return false;

        m_pNode = _pPattern;
        traverseNode(_pNode);

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
            collectValues(_pStatement->as<Assignment>()->getLValue(), _container);
            break;
        case Statement::BLOCK:
        case Statement::PARALLEL_BLOCK: {
            const auto block = _pStatement->as<Block>();
            for (size_t i = 0; i < block->size(); ++i)
                getResults(block->get(i), _container);
        }
            break;
        case Statement::IF: {
            const auto _if = _pStatement->as<If>();
            getResults(_if->getBody(), _container);
            getResults(_if->getElse(), _container);
        }
            break;
        case Statement::CALL: {
            const auto call = _pStatement->as<Call>();
            for (size_t i = 0; i < call->getBranches().size(); ++i)
                for (size_t j = 0; j < call->getBranches().get(i)->size(); ++j)
                    collectValues(call->getBranches().get(i)->get(j), _container);
        }
            break;
    }
}

void getArgs(const StatementPtr& _pStatement, ValuesSet& _container) {
    if (!_pStatement)
        return;

    switch (_pStatement->getKind()) {
        case Statement::ASSIGNMENT:
            collectValues(_pStatement->as<Assignment>()->getExpression(), _container);
            break;
        case Statement::BLOCK: {
            const auto block = _pStatement->as<Block>();
            ValuesSet args, results;
            getResults(_pStatement, results);
            for (size_t i = 0; i < block->size(); ++i)
                getArgs(block->get(i), args);
            std::set_difference(args.begin(), args.end(), results.begin(), results.end(), std::inserter(_container, _container.end()));
            break;
        }
        case Statement::PARALLEL_BLOCK: {
            const auto block = _pStatement->as<ParallelBlock>();
            for (size_t i = 0; i < block->size(); ++i)
                getArgs(block->get(i), _container);
        }
            break;
        case Statement::IF: {
            const auto _if = _pStatement->as<If>();
            collectValues(_if->getArg(), _container);
            getArgs(_if->getBody(), _container);
            getArgs(_if->getElse(), _container);
        }
            break;
        case Statement::CALL:
            collectValues(_pStatement->as<Call>()->getArgs().as<Node>(), _container);
            break;
    }
}

void getParameters(const StatementPtr& _pStatement, ValuesSet& _container) {
    getArgs(_pStatement, _container);
    getResults(_pStatement, _container);
}

void getPredicateParams(const PredicatePtr &_predicate, NamedValues& _params) {
    for (size_t i = 0; i < _predicate->getInParams().size(); ++i)
        _params.add(_predicate->getInParams().get(i));
    for (size_t i = 0; i < _predicate->getOutParams().size(); ++i) {
        const Branch& b = *_predicate->getOutParams().get(i);
        for (size_t j = 0; j < b.size(); ++j)
            _params.add(b.get(j));
    }
}

void getPredicateParams(const PredicateTypePtr &_predicateType, NamedValues& _params) {
    for (size_t i = 0; i < _predicateType->getInParams().size(); ++i)
        _params.add(_predicateType->getInParams().get(i));
    for (size_t i = 0; i < _predicateType->getOutParams().size(); ++i) {
        const Branch& b = *_predicateType->getOutParams().get(i);
        for (size_t j = 0; j < b.size(); ++j)
            _params.add(b.get(j));
    }
}

StatementPtr extractCallArguments(const CallPtr& _pCall) {
    if (!_pCall)
        return NULL;

    const Call& call = *_pCall;
    const auto pMA = std::make_shared<Multiassignment>();

    for (size_t i = 0; i < call.getArgs().size(); ++i) {
        pMA->getLValues().add(std::make_shared<VariableReference>(std::make_shared<NamedValue>(L"",
            call.getArgs().get(i)->getType())));
        pMA->getExpressions().add(call.getArgs().get(i));
    }

    return modifyStatement(pMA);
}

void getArgsMap(const FormulaCallPtr &_call, ArgsMap& _args) {
    for (size_t i = 0; i < _call->getArgs().size(); ++i)
        _args.addExpression(_call->getTarget()->getParams().get(i), _call->getArgs().get(i));
}

void getArgsMap(const FunctionCall &_call, ArgsMap& _args) {
    PredicateTypePtr pCallType = _call.getPredicate()->getType()->as<PredicateType>();
    for (size_t i = 0; i < _call.getArgs().size(); ++i)
        _args.addExpression(pCallType->getInParams().get(i), _call.getArgs().get(i));
}

template <class T>
void getArgsMap(const CallPtr &_call, ArgsMap& _args, const std::shared_ptr<T>& _pred) {
    for (size_t i = 0; i < _call->getArgs().size(); ++i)
        _args.addExpression(_pred->getInParams().get(i), _call->getArgs().get(i));

    for (size_t i = 0; i < _call->getBranches().size(); ++i) {
        CallBranch &br = *_call->getBranches().get(i);
        for (size_t j = 0; j < br.size(); ++j)
            _args.addExpression(_pred->getOutParams().get(i)->get(j), br.get(j));
    }
}

void getArgsMap(const CallPtr &_call, ArgsMap& _args) {
    if (_call->getPredicate()->getKind() == Expression::PREDICATE)
        getArgsMap(_call, _args, _call->getPredicate()->as<PredicateReference>()->getTarget());
    else
        getArgsMap(_call, _args, _call->getPredicate()->getType()->as<PredicateType>());
}

bool isRecursiveCall(const ir::CallPtr& _pCall, const ir::PredicatePtr& _pPred) {
    return _pPred && _pCall && _pCall->getPredicate() && _pCall->getPredicate()->getKind() == Expression::PREDICATE
        && _pCall->getPredicate()->as<PredicateReference>()->getTarget() == _pPred;
}

ir::ExpressionPtr generalize(const ExpressionPtr& _pExpr) {
    ValuesSet freeValues;
    collectValues(_pExpr, freeValues);

    if (freeValues.empty())
        return _pExpr;

    const auto pFormula = std::make_shared<Formula>(Formula::UNIVERSAL, _pExpr);
    pFormula->getBoundVariables().prepend(freeValues.begin(), freeValues.end());
    return pFormula;
}

ir::FormulaPtr setQuantifier(int _quantifier, const ir::ExpressionPtr& _pExpr, const ValuesSet& _bound) {
    ValuesSet freeValues;
    collectValues(_pExpr, freeValues, _bound);

    const auto pFormula = std::make_shared<Formula>(_quantifier, _pExpr);
    pFormula->getBoundVariables().prepend(freeValues.begin(), freeValues.end());
    return pFormula;
}

ExpressionPtr resolveCase(const NamedValuePtr& _index, const ExpressionPtr& _pCase) {
    if (_pCase->getKind() != Expression::TYPE ||
        (_pCase->as<TypeExpr>()->getContents()->getKind() != Type::RANGE &&
        _pCase->as<TypeExpr>()->getContents()->getKind() != Type::SUBTYPE))
        return std::make_shared<Binary>(Binary::EQUALS, std::make_shared<VariableReference>(L"", _index), _pCase);

    const SubtypePtr pContents =
        _pCase->as<TypeExpr>()->getContents()->getKind() != Type::SUBTYPE ?
            _pCase->as<TypeExpr>()->getContents()->as<Range>()->asSubtype() :
            _pCase->as<TypeExpr>()->getContents()->as<Subtype>();

    ExpressionPtr pCase = clone(pContents->getExpression());

    return Expression::substitute(pCase,
        std::make_shared<VariableReference>(L"", pContents->getParam()),
        std::make_shared<VariableReference>(L"", _index))->as<Expression>();
}

ExpressionPtr resolveCase(const NamedValues& _indexes, const ExpressionPtr& _pCase) {
    if (_indexes.empty())
        return std::make_shared<Literal>(true);
    if (_indexes.size() == 1)
        return resolveCase(_indexes.get(0), _pCase);

    if (_pCase->getKind() != Expression::CONSTRUCTOR &&
        _pCase->as<Constructor>()->getConstructorKind() != Constructor::STRUCT_FIELDS)
        return nullptr;

    const auto tuple = _pCase->as<StructConstructor>();

    if (tuple->size() != _indexes.size())
        return nullptr;

    std::list<ExpressionPtr> conds;
    for (size_t i = 0; i < _indexes.size(); ++i)
        conds.push_back(resolveCase(_indexes.get(i), tuple->get(i)->getValue()));

    return std::make_shared<Binary>(Binary::BOOL_AND, conds);
}

ExpressionPtr resolveCase(const NamedValues& _indexes, const Collection<Expression>& _case) {
    if (_case.empty())
        return std::make_shared<Literal>(true);
    if (_case.size() == 1)
        return resolveCase(_indexes, _case.get(0));

    std::list<ExpressionPtr> conds;
    for (size_t i = 0; i < _case.size(); ++i)
        conds.push_back(resolveCase(_indexes, _case.get(i)));

    return std::make_shared<Binary>(Binary::BOOL_OR, conds);
}

FormulaDeclarationPtr declareFormula(const std::wstring &_strName, const ExpressionPtr &_pExpr) {
    NamedValues params;
    collectValues(_pExpr, params);
    return declareFormula(_strName, _pExpr, params);
}

FormulaDeclarationPtr declareFormula(const std::wstring &_strName, const ExpressionPtr &_pExpr, const NamedValues& _params) {
    ExpressionPtr pExpr = _pExpr;
    if (pExpr->getKind() == ir::Expression::FORMULA
        && pExpr->as<Formula>()->getQuantifier() == Formula::NONE)
        pExpr = pExpr->as<Formula>()->getSubformula();

    NamedValues params;
    collectValues(pExpr, params, _params);
    const auto pDecl = std::make_shared<FormulaDeclaration>(_strName, pExpr->getType(), pExpr);
    pDecl->getParams().assign(params);

    return pDecl;
}

FormulaDeclarationPtr declareFormula(const std::wstring &_strName, const PredicatePtr &_predicate, const ExpressionPtr &_expr) {
    NamedValues params;
    getPredicateParams(_predicate, params);
    return declareFormula(_strName, _expr, params);
}

std::list<ModulePtr> getModulePath(const std::list<Visitor::Loc>& _path) {
    std::list<ModulePtr> path;
    for(const auto& i : _path) {
        if (i.pNode && i.pNode->getNodeKind() == Node::MODULE)
            path.push_back(i.pNode->as<Module>());
    }
    return path;
}

} // namespace na
