/// \file term_rewriting.h
///

#ifndef TERM_REWRITING_H_
#define TERM_REWRITING_H_

#include "ir/base.h"
#include "node_analysis.h"

namespace tr {

std::pair<ir::NodePtr, ir::NodePtr> extractFirstCall(const ir::Node& _node);

// Modify statements to verification.
ir::StatementPtr modifyStatement(const ir::StatementPtr& _pStatement);

// Arguments addition.
ir::FormulaCallPtr makeCall(const ir::FormulaDeclarationPtr& _pFormula, na::ArgsMap& _args);
ir::FormulaCallPtr makeCall(const ir::FormulaDeclarationPtr& _pFormula, const ir::Predicate& _predicate);
ir::FormulaCallPtr makeCall(const ir::FormulaDeclarationPtr& _pFormula, const ir::FormulaCall &_call);
ir::FormulaCallPtr makeCall(const ir::FormulaDeclarationPtr& _pFormula, const ir::Call &_call);

// Declare lemma and required formulas.
void declareLemma(const ir::ModulePtr& _pModule, const ir::ExpressionPtr& _pProposition);

// Make instance for parameterized module.
void instantiateModule(const ir::ModulePtr& _pModule, const ir::Collection<ir::Expression>& _args);

// Make expression an ordered.
void normalizeExpressions(const ir::NodePtr& _pNode);

} // namespace tr

#endif // TERM_REWRITING_H_
