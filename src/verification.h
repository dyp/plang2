/// \file verification.h
///

#ifndef VERIFICATION_H_
#define VERIFICATION_H_

#include "ir/declarations.h"

// Copy call args from _call to any call in _pExpr.
// _pExpr can be NULL.
ir::ExpressionPtr copyCallArgs(const ir::ExpressionPtr _pExpr, const ir::FormulaCall &_call);

// Declare new formula from _expr.
// Example: declareFormula("P", *predicate.getPrecondition(), predicate.getInParams());
ir::FormulaDeclarationPtr declareFormula(const std::wstring &_sName, const ir::Expression &_expr,
                                         const ir::NamedValuesPtr _pParams = NULL, const ir::TypePtr _pType = NULL);

// Extract condirion formula or measure from _predicate.
// Example: extractFormulaFromPredicate("P", predicate, *predicate.getPrecondition());
ir::FormulaDeclarationPtr extractFormulaFromPredicate(const std::wstring &_sName, const ir::Predicate &_predicate,
                                                      const ir::Expression &_expr, const ir::TypePtr _pType = NULL);

ir::ModulePtr verify(const ir::Module &_module);

#endif /* VERIFICATION_H_ */