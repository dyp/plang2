/// \file generate_name.h
///


#ifndef GENERATE_NAME_H_
#define GENERATE_NAME_H_

#include <iostream>
#include <fstream>
#include <sstream>

#include "ir/declarations.h"
#include "ir/statements.h"
#include "lexer.h"
#include "utils.h"
#include "llir.h"

struct NameGenerator{
private:
    size_t m_typeNumber;   //for naming modules
    size_t m_lambdaNumber;
    size_t m_callNumber;   //for naming lemmas
    size_t m_switchDefaultNumber;
    size_t m_switchCaseNumber;
    size_t m_arrayPartIndexNumber;
    size_t m_subtypeParamNumber;
    size_t m_unionConsFieldNumber;
    size_t m_assignmentNumber;
    size_t m_divideNumber;
    size_t m_arrayConsNumber;
    size_t m_arrayModNumber;
    size_t m_ifNumber;
    size_t m_arrayUnionNumber;

public:
    NameGenerator() : m_typeNumber(0), m_lambdaNumber(0), m_callNumber(0), m_switchDefaultNumber(0),
                      m_switchCaseNumber(0), m_arrayPartIndexNumber(0), m_subtypeParamNumber(0), m_unionConsFieldNumber(0),
                      m_assignmentNumber(0), m_divideNumber(0), m_arrayConsNumber(0), m_arrayModNumber(0), m_ifNumber(0),
                      m_arrayUnionNumber(0) {}

    std::wstring makeNameSubmoduleForType();     //for modules
    std::wstring makeNameSubmoduleForLambda();
    std::wstring makeNameSubmoduleForProcess(ir::Process &_process);
    std::wstring makeNameSubmoduleForPredicate(ir::Predicate &_predicate);

    std::wstring makeNamePredicatePrecondition(ir::Predicate &_predicate);    //for preconditions
    std::wstring makeNamePredicateBranchPrecondition(ir::Predicate &_predicate, size_t _branchNumber);
    std::wstring makeNameTypePreCondition();
    std::wstring makeNameTypeBranchPreCondition(size_t _branchNumber);
    std::wstring makeNameProcessBranchPreCondition(ir::Process &_process, size_t _branchNumber);
    std::wstring makeNameLambdaToPredicate();   //for lambdas

    std::wstring makeNamePredicatePostcondition(ir::Predicate &_predicate);     //for verification
    std::wstring makeNamePredicateMeasure(ir::Predicate &_predicate);

    std::wstring makeNameLemmaCall();     //for lemmas
    std::wstring makeNameLemmaSwitchDefault();
    std::wstring makeNameLemmaSwitchCase();
    std::wstring makeNameLemmaArrayPartIndex();
    std::wstring makeNameLemmaSubtypeParam();
    std::wstring makeNameLemmaUnionConsField();
    std::wstring makeNameLemmaAssignment();
    std::wstring makeNameLemmaDivide();
    std::wstring makeNameLemmaArrayCons();
    std::wstring makeNameLemmaArrayMod();
    std::wstring makeNameLemmaIf();
    std::wstring makeNameLemmaArrayUnion();
};

#endif /* GENERATE_NAME_H_ */
