/// \file cvc3_solver.h
///

#ifndef CVC3_SOLVER_
#define CVC3_SOLVER_

#include <iostream>
#include <map>

#include "ir/expressions.h"
#include "cvc3/queryresult.h"

namespace cvc3 {

typedef std::map<ir::LemmaDeclarationPtr, CVC3::QueryResult> QueryResult;

CVC3::QueryResult checkValidity(const ir::ExpressionPtr& _pExpr);
void checkValidity(const ir::ModulePtr& _pModule, QueryResult& _result);

std::wstring fmtResult(CVC3::QueryResult _result, bool _bValid = true);
void printImage(const ir::Expression& _expr, std::ostream& _os);
void printImage(const ir::Type& _type, std::ostream& _os);

} // namespace cvc3

#endif // CVC3_SOLVER_
