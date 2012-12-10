/// \file callgraph.h
///

#ifndef CALLGRAPH_H_
#define CALLGRAPH_H_

#include <set>

#include "ir/declarations.h"
#include "ir/visitor.h"
#include "utils.h"

namespace ir {

// Graph nodes that contains set of predicates and set of their callees.
class CallGraphNode {
public:
    CallGraphNode() {}

    // m_cOrder equals 0 for predicate declarations and natural for lambdas.
    CallGraphNode(AnonymousPredicate *_pNode, const size_t _cOrder = 0) {
        m_bRecursion = false;
        m_bHasCallers = false;
        assert(_pNode != NULL);
        m_nodePredicates[_pNode] = _cOrder;
    }

    CallGraphNode(const CallGraphNode &_pNode) {
        m_bRecursion = _pNode.m_bRecursion;
        m_bHasCallers = _pNode.m_bHasCallers;
        m_nodePredicates.insert(_pNode.m_nodePredicates.begin(), _pNode.m_nodePredicates.end());
        m_callees.insert(_pNode.m_callees.begin(), _pNode.m_callees.end());
    }

    void addCallee(const CallGraphNode *_pNode) const {
        m_callees.insert(_pNode);
    }

    void addPredicate(AnonymousPredicate *_pNode, const size_t _cOrder) {
        m_nodePredicates.insert(std::make_pair(_pNode, _cOrder));
    }

    const std::set<const CallGraphNode *> &getCallees() const {
        return m_callees;
    }

    // Returns first predicate of node (usually used when node has only one predicate).
    AnonymousPredicate *getPredicate() const {
        assert(!m_nodePredicates.empty());
        return m_nodePredicates.begin()->first;
    }

    const std::map<AnonymousPredicate *, size_t> &getPredicates() const {
        return m_nodePredicates;
    }

    const size_t getOrder() const {
        return m_nodePredicates.begin()->second;
    }

    const size_t getOrder(AnonymousPredicate *_pPred) const {
        return m_nodePredicates.find(_pPred)->second;
    }

    bool isRecursive() const {
        return m_bRecursion;
    }

    void setRecursive() const {
        m_bRecursion = true;
    }

    bool isCalled() const {
        return m_bHasCallers;
    }

    void setCalled() const {
        m_bHasCallers = true;
    }

    void deleteCallee(const CallGraphNode *_pNode) const {
        m_callees.erase(_pNode);
    }

    void deletePredicate(AnonymousPredicate *_pNode) {
        m_nodePredicates.erase(_pNode);
    }

    bool operator<(const CallGraphNode &_other) const {
        if (m_nodePredicates.size() < _other.m_nodePredicates.size())
            return true;

        if (m_nodePredicates.size() == _other.m_nodePredicates.size()) {
            auto i = m_nodePredicates.begin();
            auto j = _other.m_nodePredicates.begin();
            for (;i != m_nodePredicates.end() && j != _other.m_nodePredicates.end(); ++i, ++j) {
                if (*i->first != *j->first)
                    return *i->first < *j->first;
                if (i->second != j->second)
                    return i->second < j->second;
            }
        }

        return false;
    }

    bool operator==(const CallGraphNode &_other) const {
        return !(*this < _other) && !(_other < *this);
    }

private:
    // Numbers for lambdas with equal predicates.
    std::map<AnonymousPredicate *, size_t> m_nodePredicates;
    mutable std::set<const CallGraphNode *> m_callees;
    mutable bool m_bRecursion;
    mutable bool m_bHasCallers;
};

// Graph that contains only set of nodes. Each node contains set of predicates and set of their callees.
class CallGraph {
public:
    CallGraph() {}

    CallGraph(const CallGraph &_graph) {
        for (const auto &i: _graph.getNodes())
            addNode(i.getPredicate(), i.getOrder());

        for (const auto &i: _graph.getNodes())
            for (const auto &j: i.getCallees()) {
                m_nodes.find(i)->addCallee(&*m_nodes.find(*j));
                m_nodes.find(*j)->setCalled();
            }
    }

    const std::set<CallGraphNode> &getNodes() const { return m_nodes; }
    std::set<CallGraphNode> &getNodes() { return m_nodes; }

    const CallGraphNode *addNode(const CallGraphNode &_node) {
        return &*m_nodes.insert(_node).first;
    }

    const CallGraphNode *addNode(AnonymousPredicate *_pPred, const size_t _cOrder = 0) {
        return &*m_nodes.insert(CallGraphNode(_pPred, _cOrder)).first;
    }

    void deleteNode(const CallGraphNode &_node) {
        m_nodes.erase(_node);
    }

    void addCall(AnonymousPredicate *_pCaller, AnonymousPredicate *_pCallee) {
        const CallGraphNode *pFirst = nullptr, *pSecond = nullptr;

        for (const auto &i: m_nodes) {
            if (i.getPredicates().find(_pCaller) != i.getPredicates().end())
                pFirst = &i;
            if (i.getPredicates().find(_pCallee) != i.getPredicates().end())
                pSecond = &i;
        }

        if (pFirst && pSecond) {
            pFirst->addCallee(pSecond);
            pSecond->setCalled();
        } else
            throw std::logic_error("Caller or callee not found in graph nodes.");
    }

private:
    std::set<CallGraphNode> m_nodes;
};

struct CGNodePtrCmp {
    bool operator()(const CallGraphNode *_pLhs, const CallGraphNode *_pRhs) const {
        return *_pLhs < *_pRhs;
    }
};

void printCallGraphNode(const CallGraphNode *_pNode, const CallGraph &_graph, std::wostream &_os);

void printCallGraph(const CallGraph &_graph, std::wostream &_os);

}

#endif /* CALLGRAPH_H_ */
