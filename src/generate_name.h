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
    size_t m_typeNumber;
    size_t m_lambdaNumber;

public:
    NameGenerator() : m_typeNumber(0), m_lambdaNumber(0) {}

    std::wstring makeNamePredicatePrecondition(ir::Predicate &_predicate);
    std::wstring makeNamePredicateBranchPrecondition(ir::Predicate &_predicate, size_t _branchNumber);
    std::wstring makeNameTypePreCondition();
    std::wstring makeNameTypeBranchPreCondition(size_t _branchNumber);
    std::wstring makeNameProcessBranchPreCondition(ir::Process &_process, size_t _branchNumber);
    std::wstring makeNameSubmoduleForType();
    std::wstring makeNameSubmoduleForLambda();
    std::wstring makeNameSubmoduleForProcess(ir::Process &_process);
    std::wstring makeNameSubmoduleForPredicate(ir::Predicate &_predicate);
    std::wstring makeNameLambdaToPredicate();
};

#endif /* GENERATE_NAME_H_ */