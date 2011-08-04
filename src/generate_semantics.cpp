/// \file generate_semantics.cpp
///


#include <iostream>
#include <fstream>
#include <sstream>

#include "ir/statements.h"
#include "generate_semantics.h"
#include "lexer.h"
#include "utils.h"
#include "llir.h"

using namespace ir;

int CollectPreConditions::handlePredicatePreCondition(Node &_node) {
    Formula &formula = (Formula &)_node;
    FormulaDeclarationPtr pNewDecl = new FormulaDeclaration();
    m_module.getFormulas().add(pNewDecl);
    pNewDecl->setFormula(clone(formula));
    return 0;
}

int CollectPreConditions::handlePredicateBranchPreCondition(Node &_node) {
    Formula &formula = (Formula &)_node;
    FormulaDeclarationPtr pNewDecl = new FormulaDeclaration();
    m_module.getFormulas().add(pNewDecl);
    pNewDecl->setFormula(clone(formula));
    return 0;
}

int CollectPreConditions::handlePredicateTypePreCondition(Node &_node) {
    Formula &formula = (Formula &)_node;
    FormulaDeclarationPtr pNewDecl = new FormulaDeclaration();
    m_module.getFormulas().add(pNewDecl);
    pNewDecl->setFormula(clone(formula));
    return 0;
}

int CollectPreConditions::handlePredicateTypeBranchPreCondition(Node &_node) {
    Formula &formula = (Formula &)_node;
    FormulaDeclarationPtr pNewDecl = new FormulaDeclaration();
    m_module.getFormulas().add(pNewDecl);
    pNewDecl->setFormula(clone(formula));
    return 0;
}

int CollectPreConditions::handleProcessBranchPreCondition(Node &_node) {
    Formula &formula = (Formula &)_node;
    FormulaDeclarationPtr pNewDecl = new FormulaDeclaration();
    m_module.getFormulas().add(pNewDecl);
    pNewDecl->setFormula(clone(formula));
    return 0;
}

Auto<Module> ir::processPreConditions(Module &_module) {
    Auto<Module> pNewModule = new Module();
    CollectPreConditions collector(*pNewModule);
    collector.traverseNode(_module);
    return pNewModule;
}