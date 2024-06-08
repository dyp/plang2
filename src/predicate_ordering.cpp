/// \file predicate_ordering.cpp
///

#include <iostream>
#include <string>
#include <unordered_map>

#include "predicate_ordering.h"
#include "pp_syntax.h"

using namespace ir;

typedef std::unordered_map<const CallGraphNode *, int> NodeNumber;
typedef std::stack<const CallGraphNode *> NodeStack;
typedef std::list<std::set<const CallGraphNode *> > Components;

// Inner function of tarjan algorithm.
static void _tarjanAlgorithmProcessNode(const CallGraphNode *_pNode, int &_nTime,
        NodeNumber &_lowlink, NodeStack &_stack, Components &_components)
{
    _lowlink[_pNode] = _nTime++;
    _stack.push(_pNode);
    bool bRoot = true;

    for (const auto &pNext: _pNode->getCallees()) {
        if (_lowlink.find(pNext) == _lowlink.end())
            _tarjanAlgorithmProcessNode(pNext, _nTime, _lowlink, _stack, _components);

        // _lowlink[pNext] is normally initialized by the recursive call above.
        if (_lowlink[_pNode] > _lowlink[pNext]) {
            // To show previous nodes that we got recursive component.
            _lowlink[_pNode] = _lowlink[pNext];
            bRoot = false;
        }
    }

    if (bRoot) {
        auto &component = *_components.emplace(_components.end());

        bool bBreak;
        do {
            component.insert(_stack.top());
            _lowlink[_stack.top()] = std::numeric_limits<int>::max();

            if (_stack.top() == _pNode)
                bBreak = true;

            _stack.pop();
        } while (!bBreak);
    }
}

// Find strongly connected components in _graph and push them to _components.
void ir::findSCC(CallGraph &_graph, Components &_components) {
    int nTime = 0;
    NodeNumber lowlink;
    NodeStack stack;

    for (const auto &i: _graph.getNodes())
        if (lowlink.find(&i) == lowlink.end())
            _tarjanAlgorithmProcessNode(&i, nTime, lowlink, stack, _components);
}

static void _mergeComponentToNode(const std::set<const CallGraphNode *> &_component, CallGraphNode &_newNode) {
    for (const auto &i: _component) {
        _newNode.addPredicate(i->getPredicate(), i->getOrder());

        // Add to new node all component's callees, excluding ones from this component.
        for (const auto &j: i->getCallees())
            if (_component.find(j) == _component.end())
                _newNode.addCallee(j);
    }
}

// Retarget to _pNewNode calls of predicates contained in _pNewNode.
static void _refreshCallees(CallGraph &_graph, const CallGraphNode *_pNewNode) {
    for (const auto &node: _graph.getNodes()) {
        bool bHasCalleesFromComponent = false;

        for (auto m = node.getCallees().begin(); m != node.getCallees().end();) {
            std::set<const CallGraphNode *>::iterator mNext = ::next(m);

            if (_pNewNode->getPredicates().find((*m)->getPredicate()) != _pNewNode->getPredicates().end()) {
                node.deleteCallee(*m);
                bHasCalleesFromComponent = true;
            }

            m = mNext;
        }

        if (bHasCalleesFromComponent) {
            node.addCallee(_pNewNode);
            _pNewNode->setCalled();
        }
    }
}

static void _setComponentNodeRecursive(const CallGraphNode *_pNode) {
    if (_pNode->getCallees().find(_pNode) != _pNode->getCallees().end()) {
        _pNode->setRecursive();
        // Delete reflexive call.
        _pNode->deleteCallee(_pNode);
    }
}

// Transform _graph into acyclic graph using collected cycles in _components.
void ir::mergeSCC(CallGraph &_graph, Components &_components) {
    for (const auto &component: _components)
        if (component.size() > 1) {
            CallGraphNode newNode;
            newNode.setRecursive();
            _mergeComponentToNode(component, newNode);

            const CallGraphNode *pNewNode = _graph.addNode(newNode);
            _refreshCallees(_graph, pNewNode);

            // Delete processed component's nodes from graph.
            for (const auto &j: component)
                _graph.deleteNode(*j);
        } else
            _setComponentNodeRecursive(*component.begin());
}

typedef std::map<AnonymousPredicateConstPtr, size_t> PredNumbers;
// In order of their appearances in .dot file.
static void _enumeratePredicates(const std::set<CallGraphNode> &_nodes, PredNumbers &_predNumbers) {
    size_t c = 0;

    for (const auto &i: _nodes) {
        // This node hasn't been printed in .dot.
        if (i.getCallees().size() == 0 && i.isCalled())
            continue;

        // Same order as it was printed in .dot.
        std::set<const CallGraphNode *, CGNodePtrCmp> callees;
        callees.insert(i.getCallees().begin(), i.getCallees().end());

        if (_predNumbers.insert(std::make_pair(i.getPredicate(), c)).second)
            ++c;

        // Here we can enumerate predicates that were screened at the start of the cycle.
        for (const auto &j: callees)
            if (_predNumbers.insert(std::make_pair(j->getPredicate(), c)).second)
                ++c;
    }
}

// Find earliest (predicate of which found earliest in graph edges) component that dont call anything or only itself.
static const CallGraphNode *_findEarliestComponent(const CallGraph &_acyclicGraph,
        const PredNumbers &_predNumbers)
{
    size_t cLowest = std::numeric_limits<size_t>::max();
    const CallGraphNode *pNode = NULL;

    for (const auto &node: _acyclicGraph.getNodes())
        if (node.getCallees().size() == 0)
            for (const auto &j: node.getPredicates())
                if (_predNumbers.find(j.first)->second < cLowest) {
                    cLowest = _predNumbers.find(j.first)->second;
                    pNode = &node;
                }

    assert(pNode != NULL);
    return pNode;
}

static void _deleteComponetFromCallees(const CallGraphNode *_pEarliestComponent, std::set<CallGraphNode> &_nodes) {
    for (const auto &i: _nodes)
        i.deleteCallee(_pEarliestComponent);
}

static AnonymousPredicatePtr _findEarliestPredicate(const CallGraphNode &_componentCopy, const PredNumbers &_predNumbers) {
    size_t cLowest = std::numeric_limits<size_t>::max();
    AnonymousPredicatePtr pPred;

    for (const auto &j: _componentCopy.getPredicates())
        if (_predNumbers.find(j.first)->second < cLowest) {
            cLowest = _predNumbers.find(j.first)->second;
            pPred = j.first;
        }

    assert(pPred != NULL);
    return pPred;
}

static const CallGraphNode *_findEarliestCallee(const CallGraphNode &_node, const PredNumbers &_predNumbers) {
    const CallGraphNode *pNode = NULL;
    size_t cLowest = std::numeric_limits<size_t>::max();

    for (const auto &j: _node.getCallees())
        if (_predNumbers.find(j->getPredicate())->second < cLowest) {
            cLowest = _predNumbers.find(j->getPredicate())->second;
            pNode = j;
        }

    assert(pNode != NULL);
    return pNode;
}

static void _printComponent(const CallGraphNode *_pComponent, CallGraphNode &_componentCopy,
        const PredNumbers &_predNumbers, const CallGraph &_cyclicGraph, const size_t _cCompNumber, std::wostream &_os)
{
    _os << fmtInt(_cCompNumber, L"digraph cluster_%u {\n");

    while (!_componentCopy.getPredicates().empty()) {
        const auto pEarliestPredInComp = _findEarliestPredicate(_componentCopy, _predNumbers);

        // Cyclic graph is used to find initial callees of predicate.
        for (const auto &node: _cyclicGraph.getNodes())
            if (node.getPredicate() == pEarliestPredInComp) {
                while (!node.getCallees().empty()) {
                    const CallGraphNode *pEarliestCallee = _findEarliestCallee(node, _predNumbers);

                    // Print only calls from this component.
                    if (_pComponent->getPredicates().find(pEarliestCallee->getPredicate()) != _pComponent->getPredicates().end()) {
                        _os << "\t";
                        CallGraphNode node1(pEarliestPredInComp, _componentCopy.getOrder(pEarliestPredInComp));
                        printCallGraphNode(&node1, _cyclicGraph, _os);
                        _os << " -> ";
                        CallGraphNode node2(pEarliestCallee->getPredicate(), pEarliestCallee->getOrder());
                        printCallGraphNode(&node2, _cyclicGraph, _os);
                        _os << ";\n";
                    }

                    node.deleteCallee(pEarliestCallee);
                }

                if (_pComponent->getPredicates().size() == 1 && !_pComponent->isRecursive()) {
                    _os << "\t";
                    CallGraphNode node(pEarliestPredInComp, _componentCopy.getOrder());
                    printCallGraphNode(&node, _cyclicGraph, _os);
                    _os << ";\n";
                }

                break;
            }

        _componentCopy.deletePredicate(pEarliestPredInComp);
    }

    _os << "}\n";
}

static void _printComponentsCall(const std::map<size_t, CallGraphNode> &_componentNumbers, std::wostream &_os) {
    _os << "digraph scc_map {\n";

    for (const auto &i: _componentNumbers)
        if (i.second.getCallees().size() != 0) {
            for (const auto &k: _componentNumbers)
                for (const auto &j: i.second.getCallees())
                    if (k.second == *j) {
                        _os << fmtInt(i.first, L"\tcluster_%u") << " -> " << fmtInt(k.first, L"cluster_%u;\n");
                        break;
                    }
        }
        else if (!i.second.isCalled())
            _os << fmtInt(i.first, L"\tcluster_%u;\n");

    _os << "}\n";
}

// Print acyclic graph in order of appearance of predicates in graph edges (scc_map program printing order).
static void _printGraphWithStronglyConnectedComponents(CallGraph &_graph, std::wostream &_os) {
    CallGraph constAcyclicGraph(_graph);
    CallGraph acyclicGraph(_graph);
    CallGraph cyclicGraph(_graph);

    Components components;
    Components components2;
    findSCC(constAcyclicGraph, components);
    findSCC(acyclicGraph, components2);

    mergeSCC(constAcyclicGraph, components);
    mergeSCC(acyclicGraph, components2);

    PredNumbers predNumbers;
    _enumeratePredicates(_graph.getNodes(), predNumbers);

    size_t cCompNumber = 0;
    std::map<size_t, CallGraphNode> componentNumbers;

    while (!acyclicGraph.getNodes().empty()) {
        const CallGraphNode *pEarliestComponent = _findEarliestComponent(acyclicGraph, predNumbers);
        CallGraphNode componentCopy = *pEarliestComponent;

        componentNumbers[cCompNumber] = *constAcyclicGraph.getNodes().find(*pEarliestComponent);

        _deleteComponetFromCallees(pEarliestComponent, acyclicGraph.getNodes());

        _printComponent(pEarliestComponent, componentCopy, predNumbers, cyclicGraph, cCompNumber, _os);
        cCompNumber++;

        acyclicGraph.deleteNode(*pEarliestComponent);
    }

    _printComponentsCall(componentNumbers, _os);
}

// Creates order of predicates from bottom to the top of callgraph (traversing first horizontally then vertically).
void ir::predicateOrdering(CallGraph &_graph, std::list<CallGraphNode> &_predicateOrder) {
    Components components;
    findSCC(_graph, components);
    mergeSCC(_graph, components);

    while (!_graph.getNodes().empty()) {
        bool bChanged = false;

        for (auto iNode = _graph.getNodes().begin(); iNode != _graph.getNodes().end();)
            if (iNode->getCallees().size() == 0) {
                _predicateOrder.push_back(*iNode);

                for (const auto &i: _graph.getNodes())
                    i.deleteCallee(&*iNode);

                _graph.getNodes().erase(iNode++);
                bChanged = true;
            } else
                ++iNode;

        if (!bChanged) {
            throw std::logic_error("Error creating merging order.");
            break;
        }
    }
}

// Creates and prints callgraph and then its acyclic version.
void ir::printModuleSCCCallGraph(const ModulePtr &_module, std::wostream &_os) {
    CallGraph graph;
    generateCallGraph(_module, graph);
    printCallGraph(graph, _os);

    _printGraphWithStronglyConnectedComponents(graph, _os);
}
