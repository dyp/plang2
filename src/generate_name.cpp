/// \file generate_name.cpp
///

#include <iostream>
#include <fstream>
#include <sstream>

#include "ir/declarations.h"
#include "ir/statements.h"
#include "generate_name.h"
#include "lexer.h"
#include "utils.h"
#include "llir.h"


std::wstring NameGenerator::makeNamePredicatePrecondition(ir::Predicate &_predicate){
    return _predicate.getName() + L"Precondition";
}

std::wstring NameGenerator::makeNamePredicatePostcondition(ir::Predicate &_predicate){
    return _predicate.getName() + L"Postcondition";
}

std::wstring NameGenerator::makeNamePredicateMeasure(ir::Predicate &_predicate){
    return _predicate.getName() + L"Measure";
}

std::wstring NameGenerator::makeNamePredicateBranchPrecondition(ir::Predicate &_predicate, size_t _branchNumber){
    return _predicate.getName() + fmtInt(_branchNumber, L"Precondition%u");
}

std::wstring NameGenerator::makeNameTypePreCondition(){
    return fmtInt(m_typeNumber, L"Type%u");
}

std::wstring NameGenerator::makeNameTypeBranchPreCondition(size_t _branchNumber){
    return fmtInt(m_typeNumber, L"Type%u") + fmtInt(_branchNumber, L"Precondition%u");
}

std::wstring NameGenerator::makeNameProcessBranchPreCondition(ir::Process &_process, size_t _branchNumber){
    return _process.getName() + fmtInt(_branchNumber, L"Precondition%u");
}

std::wstring NameGenerator::makeNameSubmoduleForType(){
    m_typeNumber++;
    return fmtInt(m_typeNumber, L"Type%u");
}

std::wstring NameGenerator::makeNameSubmoduleForLambda(){
    m_lambdaNumber++;
    return fmtInt(m_lambdaNumber, L"Lambda%u");
}

std::wstring NameGenerator::makeNameLambdaToPredicate(){
    return fmtInt(m_lambdaNumber, L"Lambda%u");
}

std::wstring NameGenerator::makeNameSubmoduleForProcess(ir::Process &_process){
    return _process.getName();
}

std::wstring NameGenerator::makeNameSubmoduleForPredicate(ir::Predicate &_predicate){
    return _predicate.getName();
}