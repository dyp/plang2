/// \file callgraph.cpp
///

#include <iostream>
#include <string>
#include <sstream>

#include "callgraph.h"
#include "pp_syntax.h"

using namespace ir;

void ir::printCallGraphNode(const CallGraphNode *_pNode, const CallGraph &_graph, std::wostream &_os) {
    if (_pNode && _pNode->getPredicate()->getKind() == Statement::ANONYMOUS_PREDICATE) {
        _os << "lambda" << _pNode->getOrder();
        return;
    }

    const auto pPred = _pNode->getPredicate()->as<Predicate>();
    assert(pPred->getKind() == Statement::PREDICATE_DECLARATION);

    bool bSameName = false;
    for (const auto &i: _graph.getNodes())
        if (i.getPredicate()->getKind() == Statement::PREDICATE_DECLARATION &&
            (pPred->getName() == ((Predicate *)i.getPredicate())->getName() &&
            !pPred->getInParams().equals(i.getPredicate()->getInParams())))
        {
            bSameName = true;
            break;
        }

    if (bSameName) {
        const auto pPred1 = std::make_shared<Predicate>(pPred->getName());
        pPred1->getInParams().append(pPred->getInParams());
        pPred1->getOutParams().append(pPred->getOutParams());

        std::wstringstream wstringstream;
        pp::prettyPrintSyntax(pPred1, wstringstream);
        std::wstring wstr = wstringstream.str();
        _os << "\"" << wstr << "\"";
    } else
        _os << pPred->getName();
}

void ir::printCallGraph(const CallGraph &_graph, std::wostream &_os) {
    _os << "digraph G {\n";

    for (const auto &i: _graph.getNodes()) {
        // Sorting callees to print in correct order.
        std::set<const CallGraphNode *, CGNodePtrCmp> callees;
        callees.insert(i.getCallees().begin(), i.getCallees().end());

        for (const auto &j: callees) {
            printCallGraphNode(&i, _graph, _os);
            _os << " -> ";
            printCallGraphNode(j, _graph, _os);
            _os << "\n";
        }

        if (callees.size() == 0 && !i.isCalled()) {
            printCallGraphNode(&i, _graph, _os);
            _os << "\n";
        }
    }

    _os << "}\n";
}
