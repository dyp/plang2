/// \file generate_semantics.h
///


#ifndef GENERATE_SEMANTICS_H_
#define GENERATE_SEMANTICS_H_

#include "ir/declarations.h"
#include "ir/visitor.h"
#include "utils.h"

namespace ir{

class CollectPreConditions : public Visitor {
    Module &m_module;

public:
    CollectPreConditions(Module &_module) : m_module(_module) {}

    CollectPreConditions();

    virtual int handlePredicatePreCondition(Node &_node);
    virtual int handlePredicateBranchPreCondition(Node &_node);
    virtual int handlePredicateTypePreCondition(Node &_node);
    virtual int handlePredicateTypeBranchPreCondition(Node &_node);
    virtual int handleProcessBranchPreCondition(Node &_node);
};

Auto<Module> processPreConditions(Module &_module);

};

#endif /* GENERATE_SEMANTICS_H_ */


