/// \file sort_module.cpp
///

#include "node_analysis.h"
#include "term_rewriting.h"

#include <list>

using namespace ir;
using namespace tr;

namespace na {

typedef std::map<NodePtr, std::set<NodePtr>> Graph;

class CollectDeclarations : public Visitor {
public:
    CollectDeclarations(Graph& _container) : m_container(_container) {}

    template <class T>
    bool _traverseDeclaration(T & _decl) {
        for(auto i: m_path)
            if (i.pNode && i.pNode->getNodeKind() == Node::MODULE)
                m_container[i.pNode].insert(_decl);
        return true;
    }

    bool traverseModule(const ModulePtr& _node) override {
        return _traverseDeclaration(_node) &&
            Visitor::traverseModule(_node);
    }

    bool traverseVariableDeclaration(const VariableDeclarationPtr& _node) override {
        return _traverseDeclaration(_node);
    }

    bool traverseTypeDeclaration(const TypeDeclarationPtr& _node) override {
        return _traverseDeclaration(_node);
    }

    bool traversePredicate(const PredicatePtr& _node) override {
        return _traverseDeclaration(_node);
    }

    bool traverseFormulaDeclaration(const FormulaDeclarationPtr& _node) override {
        return _traverseDeclaration(_node);
    }

    void run(const NodePtr& _pNode) {
        m_container.clear();
        if (_pNode)
            traverseNode(_pNode);
    }

private:
    Graph& m_container;
};

class CollectDependencies : public Visitor {
public:
    CollectDependencies(const Graph& _decls, Graph& _container) :
        m_decls(_decls), m_container(_container)
    {}

    bool visitNamedReferenceType(const NamedReferenceTypePtr& _type) override {
        return visitDependence(_type->getDeclaration());
    }

    bool visitVariableReference(const VariableReferencePtr& _var) override {
        if (_var->getTarget()->getKind() != NamedValue::LOCAL &&
            _var->getTarget()->getKind() != NamedValue::GLOBAL)
            return true;
        if (!_var->getTarget())
            return true;
        return visitDependence(_var->getTarget()->as<Variable>()->getDeclaration());
    }

    bool visitFormulaCall(const FormulaCallPtr& _call) override {
        return visitDependence(_call->getTarget());
    }

    bool visitCall(const CallPtr& _call) {
        if (_call->getPredicate() &&
            _call->getPredicate()->getKind() != Expression::PREDICATE)
            return true;
        return visitDependence(_call->getPredicate()->as<PredicateReference>()->getTarget());
    }

    bool visitFunctionCall(const FunctionCallPtr& _call) override {
        if (_call->getPredicate() &&
            _call->getPredicate()->getKind() != Expression::PREDICATE)
            return true;
        return visitDependence(_call->getPredicate()->as<PredicateReference>()->getTarget());
    }

    void run(const NodePtr& _pNode) {
        m_container.clear();
        if (_pNode)
            traverseNode(_pNode);
    }

private:
    bool visitDependence(const NodePtr& _pNode) {
        if (!_pNode)
            return true;

        for(auto& i: m_path) {
            if (i.pNode == nullptr)
                continue;
            if (i.pNode == _pNode)
                break;
            if (i.pNode->getNodeKind() != Node::MODULE &&
                i.pNode->getNodeKind() != Node::STATEMENT)
                continue;
            if (i.pNode->getNodeKind() == Node::STATEMENT &&
                i.pNode->as<Statement>()->getKind() != Statement::VARIABLE_DECLARATION &&
                i.pNode->as<Statement>()->getKind() != Statement::TYPE_DECLARATION &&
                i.pNode->as<Statement>()->getKind() != Statement::FORMULA_DECLARATION &&
                i.pNode->as<Statement>()->getKind() != Statement::PREDICATE_DECLARATION)
                continue;

            const auto iVertex = m_decls.find(i.pNode);
            if (iVertex != m_decls.end() &&
                iVertex->second.find(_pNode) != iVertex->second.end())
                continue;

            m_container[i.pNode].insert(_pNode);
        }

        return true;
    }

    const Graph& m_decls;
    Graph& m_container;
};

static bool _depends(const NodePtr& pLow, const NodePtr& pHigh, const Graph& _deps, const Graph& _decls) {
    auto iLowDeps = _deps.find(pLow);
    if (iLowDeps == _deps.end())
        return false;

    if (iLowDeps->second.find(pHigh) != iLowDeps->second.end())
        return true;

    auto iHighDecls = _decls.find(pHigh);
    if (iHighDecls == _decls.end())
        return false;

    for (auto i: iLowDeps->second)
        if (iHighDecls->second.find(i) != iHighDecls->second.end())
            return true;

    return false;
}

static void _buildDeclarations(const ModulePtr & _module, Nodes& _declarations) {
    _declarations.assign(_module->getPredicates());
    _declarations.append(_module->getFormulas());
    _declarations.append(_module->getTypes());
    _declarations.append(_module->getVariables());
    _declarations.append(_module->getModules());
}

static void _buildDependencies(const ModulePtr & _module, Graph& _dependencies) {
    Nodes declarations;
    _buildDeclarations(_module, declarations);

    Graph decls, deps;
    CollectDeclarations(decls).run(_module);
    CollectDependencies(decls, deps).run(_module);

    for (auto i: declarations)
        _dependencies.insert({i, std::set<NodePtr>()});

    if (!_module->getPredicates().empty())
        for (size_t i = 1; i < _module->getPredicates().size(); ++i)
            _dependencies[_module->getPredicates().get(i)].insert(_module->getPredicates().get(i - 1));

    for (auto i: declarations) {
        for (auto j: declarations) {
            if (i != j && _depends(i, j, deps, decls))
                _dependencies[i].insert(j);
        }
    }
}

class TopologicalSort {
public:
    TopologicalSort() {}

    void run(const NodePtr& _pDecl, Nodes& _sorted, const Graph& _dependencies) {
        if (std::find(_sorted.begin(), _sorted.end(), _pDecl) != _sorted.end() ||
            std::find(m_traversedNodes.begin(), m_traversedNodes.end(), _pDecl) != m_traversedNodes.end())
            return;

        m_traversedNodes.insert(_pDecl);

        const auto iter = _dependencies.find(_pDecl);
        if (iter != _dependencies.end())
            for (auto i: iter->second)
                run(i, _sorted, _dependencies);

        _sorted.add(_pDecl);
    }

private:
    std::set<NodePtr> m_traversedNodes;
};

void sortModule(const ModulePtr & _module, Nodes & _sorted) {
    Graph dependencies;
    _buildDependencies(_module, dependencies);

    TopologicalSort ts;
    for (auto i: dependencies)
        ts.run(i.first, _sorted, dependencies);
}

}
