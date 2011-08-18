/// \file generate_semantics.h
///


#ifndef GENERATE_SEMANTICS_H_
#define GENERATE_SEMANTICS_H_

#include "ir/declarations.h"
#include "ir/visitor.h"
#include "utils.h"
#include "generate_name.h"

namespace ir{

class CollectPreConditions : public Visitor {
    Module &m_module;
    NameGenerator m_nameGen;
    Predicate *m_pPredicate;
    Process *m_pProcess;
    Auto<Module> m_pNewModule;
    Formula m_pre;
    Collection<Formula> m_branchPre;
    Collection<Formula> m_branch;

public:
    CollectPreConditions(Module &_module) : m_module(_module), m_pNewModule(NULL), m_nameGen() {}

    CollectPreConditions();

    virtual bool visitPredicateType(Node &_node);
    virtual bool visitLambda(Node &_node);

    virtual int handlePredicateDecl(Node &_node);
    virtual int handleProcessDecl(Node &_node);

    virtual int handlePredicatePreCondition(Node &_node);
    virtual int handlePredicateBranchPreCondition(Node &_node);
    virtual int handlePredicateTypePreCondition(Node &_node);
    virtual int handlePredicateTypeBranchPreCondition(Node &_node);
    virtual int handleProcessBranchPreCondition(Node &_node);
};

Auto<Module> processPreConditions(Module &_module);

};

#endif /* GENERATE_SEMANTICS_H_ */


