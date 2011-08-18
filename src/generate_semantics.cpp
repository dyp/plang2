/// \file generate_semantics.cpp
///


#include <iostream>
#include <fstream>
#include <sstream>

#include "ir/statements.h"
#include "generate_semantics.h"
#include "generate_name.h"
#include "lexer.h"
#include "utils.h"
#include "llir.h"

using namespace ir;

int CollectPreConditions::handlePredicateDecl(Node &_node){
    m_pPredicate = &(Predicate &)_node;
    m_pNewModule = new Module(m_nameGen.makeNameSubmoduleForPredicate(*m_pPredicate));
    m_module.getModules().add(m_pNewModule);
    return 0;
}

int CollectPreConditions::handleProcessDecl(Node &_node){
    m_pProcess = &(Process &)_node;
    m_pNewModule = new Module(m_nameGen.makeNameSubmoduleForProcess(*m_pProcess));
    m_module.getModules().add(m_pNewModule);
    return 0;
}

bool CollectPreConditions::visitPredicateType(Node &_node){
    m_pNewModule = new Module(m_nameGen.makeNameSubmoduleForType());
    m_module.getModules().add(m_pNewModule);
    return true;
}

bool CollectPreConditions::visitLambda(Node &_node){
    Lambda &_lambda = (Lambda &)_node;
    m_pPredicate = &(Predicate &)_lambda.getPredicate();
    m_pPredicate->setName(m_nameGen.makeNameLambdaToPredicate());

    m_pNewModule = new Module(m_nameGen.makeNameSubmoduleForLambda());
    m_module.getModules().add(m_pNewModule);
    return true;
}

int CollectPreConditions::handlePredicatePreCondition(Node &_node) {
    FormulaDeclarationPtr pNewDecl = new FormulaDeclaration();
    pNewDecl->setName(m_nameGen.makeNamePredicatePrecondition(*m_pPredicate));

    m_pre = (Formula &)_node;
    m_pNewModule->getFormulas().add(pNewDecl);
    pNewDecl->setFormula(clone(m_pre));
    return 0;
}

int CollectPreConditions::handlePredicateBranchPreCondition(Node &_node) {
    FormulaDeclarationPtr pNewDecl = new FormulaDeclaration();
    pNewDecl->setName(m_nameGen.makeNamePredicateBranchPrecondition(*m_pPredicate, getLoc().cPosInCollection));

    Formula &_formula = (Formula &)_node;
    m_branchPre.add(clone(_formula));
    m_pNewModule->getFormulas().add(pNewDecl);
    pNewDecl->setFormula(clone(_formula));
    return 0;
}

int CollectPreConditions::handlePredicateTypePreCondition(Node &_node) {
    FormulaDeclarationPtr pNewDecl = new FormulaDeclaration();
    pNewDecl->setName(m_nameGen.makeNameTypePreCondition());

    m_pre = (Formula &)_node;
    m_pNewModule->getFormulas().add(pNewDecl);
    pNewDecl->setFormula(clone(m_pre));
    return 0;
}

int CollectPreConditions::handlePredicateTypeBranchPreCondition(Node &_node) {
    FormulaDeclarationPtr pNewDecl = new FormulaDeclaration();
    pNewDecl->setName(m_nameGen.makeNameTypeBranchPreCondition(getLoc().cPosInCollection));

    Formula &_formula = (Formula &)_node;
    m_branchPre.add(clone(_formula));
    m_pNewModule->getFormulas().add(pNewDecl);
    pNewDecl->setFormula(clone(_formula));
    return 0;
}

int CollectPreConditions::handleProcessBranchPreCondition(Node &_node) {
    FormulaDeclarationPtr pNewDecl = new FormulaDeclaration();
    pNewDecl->setName(m_nameGen.makeNameProcessBranchPreCondition(*m_pProcess, getLoc().cPosInCollection));

    Formula &_formula = (Formula &)_node;
    m_branchPre.add(clone(_formula));
    m_pNewModule->getFormulas().add(pNewDecl);
    pNewDecl->setFormula(clone(_formula));
    return 0;
}

Auto<Module> ir::processPreConditions(Module &_module) {
    Auto<Module> pNewModule = new Module();
    CollectPreConditions collector(*pNewModule);
    collector.traverseNode(_module);
    return pNewModule;
}