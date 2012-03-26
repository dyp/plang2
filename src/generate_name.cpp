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


///preconditions

std::wstring NameGenerator::makeNamePredicatePrecondition(ir::Predicate &_predicate){
    return _predicate.getName() + L"Precondition";
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


///verification

std::wstring NameGenerator::makeNamePredicatePostcondition(ir::Predicate &_predicate){
    return _predicate.getName() + L"Postcondition";
}

std::wstring NameGenerator::makeNamePredicateMeasure(ir::Predicate &_predicate){
    return _predicate.getName() + L"Measure";
}


///submodules

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



///semantics' lemmas

std::wstring NameGenerator::makeNameLemmaCall(){
    m_callNumber++;
    return fmtInt(m_callNumber, L"Call%u");
}

std::wstring NameGenerator::makeNameLemmaSwitchDefault(){
    m_switchDefaultNumber++;
    return fmtInt(m_switchDefaultNumber, L"SwitchDefault%u");
}

std::wstring NameGenerator::makeNameLemmaSwitchCase(){
    m_switchCaseNumber++;
    return fmtInt(m_switchCaseNumber, L"SwitchCase%u");
}

std::wstring NameGenerator::makeNameLemmaArrayPartIndex(){
    m_arrayPartIndexNumber++;
    return fmtInt(m_arrayPartIndexNumber, L"ArrayPartIndex%u");
}

std::wstring NameGenerator::makeNameLemmaSubtypeParam(){
    m_subtypeParamNumber++;
    return fmtInt(m_subtypeParamNumber, L"SubtypeParam%u");
}

std::wstring NameGenerator::makeNameLemmaUnionConsField(){
    m_unionConsFieldNumber++;
    return fmtInt(m_unionConsFieldNumber, L"UnionConsField%u");
}

std::wstring NameGenerator::makeNameLemmaAssignment(){
    m_assignmentNumber++;
    return fmtInt(m_assignmentNumber, L"Assignment%u");
}

std::wstring NameGenerator::makeNameLemmaDivide(){
    m_divideNumber++;
    return fmtInt(m_divideNumber, L"Divide%u");
}

std::wstring NameGenerator::makeNameLemmaArrayCons(){
    m_arrayConsNumber++;
    return fmtInt(m_arrayConsNumber, L"ArrayConstructor%u");
}

std::wstring NameGenerator::makeNameLemmaArrayMod(){
    m_arrayModNumber++;
    return fmtInt(m_arrayModNumber, L"ArrayModification%u");
}

std::wstring NameGenerator::makeNameLemmaIf(){
    m_ifNumber++;
    return fmtInt(m_ifNumber, L"If%u");
}

std::wstring NameGenerator::makeNameLemmaArrayUnion(){
    m_arrayUnionNumber++;
    return fmtInt(m_arrayUnionNumber, L"ArrayUnion%u");
}
