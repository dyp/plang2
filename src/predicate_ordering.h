/// \file predicate_ordering.h
///

#ifndef PREDICATE_ORDERING_H_
#define PREDICATE_ORDERING_H_

#include "ir/declarations.h"
#include "ir/visitor.h"
#include "utils.h"
#include "callgraph.h"
#include "generate_callgraph.h"

namespace ir {

void findSCC(CallGraph &_callGraph,
        std::list<std::set<const CallGraphNode *> > &_components);

void mergeSCC(CallGraph &_callGraph,
        std::list<std::set<const CallGraphNode *> > &_components);

void predicateOrdering(CallGraph &_graph, std::list<CallGraphNode> &_mergingOrder);

void printModuleSCCCallGraph(Module &_module, std::wostream &_os);

}


#endif /* PREDICATE_ORDERING_H_ */
