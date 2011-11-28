/// \file verification.h
///

#ifndef VERIFICATION_H_
#define VERIFICATION_H_

#include "ir/declarations.h"

ir::FormulaDeclarationPtr declareFormula(const std::wstring &_sName, const ir::Expression &_expr,
                                         const ir::NamedValuesPtr _pParams = NULL, const ir::TypePtr _pType = NULL);

ir::FormulaDeclarationPtr extractFormulaFromPredicate(const std::wstring &_sName, const ir::Predicate &_predicate,
                                                      const ir::Expression &_expr, const ir::TypePtr _pType = NULL);

ir::ModulePtr verify(const ir::Module &_module);

#endif /* VERIFICATION_H_ */