/// \file node_analysis.h
///

#ifndef NODE_ANALYSIS_H_
#define NODE_ANALISIS_H_

#include <set>
#include "ir/base.h"
#include "ir/expressions.h"
#include "ir/declarations.h"

namespace na {

// Check existence of recursive formulas, existential quantifiers and fresh types.
bool containsBannedNodes(const ir::NodePtr& _pNode);

// Analyze call existing.
bool containsCall(ir::NodePtr _pNode);

// Formulas collector.
void collectFormulas(ir::Node &_node, std::set<ir::FormulaDeclarationPtr>& _container);

// Values analysis.
typedef std::set<ir::NamedValuePtr> ValuesSet;

bool nodeExists(const ir::NodePtr& _pNode, const ir::NodePtr&  _pPattern);

// Collect values from _pNode.
void collectValues(const ir::NodePtr& _pNode, ValuesSet& _container);
void collectValues(const ir::NodePtr& _pNode, ValuesSet& _container, const ValuesSet& _allow);
void collectValues(const ir::NodePtr& _pNode, ir::NamedValues& _container);
void collectValues(const ir::NodePtr& _pNode, ir::NamedValues& _container, const ir::NamedValues& _allow);

// Collect arguments and results from _pStatement.
void getArgs(const ir::StatementPtr& _pStatement, ValuesSet& _container);
void getResults(const ir::StatementPtr& _pStatement, ValuesSet& _container);
void getParameters(const ir::StatementPtr& _pStatement, ValuesSet& _container);

// Collect arguments and results.
typedef ir::MarkedMap<ir::NamedValue> ArgsMap;
typedef Auto<ArgsMap> ArgsMapPtr;

void getPredicateParams(const ir::Predicate &_predicate, ir::NamedValues& _params);
void getPredicateParams(const ir::PredicateType &_predicateType, ir::NamedValues& _params);

ir::StatementPtr extractCallArguments(const ir::CallPtr& _pCall);

void getArgsMap(const ir::FormulaCall &_call, ArgsMap& _args);
void getArgsMap(const ir::FunctionCall &_call, ArgsMap& _args);
void getArgsMap(const ir::Call &_call, ArgsMap& _args);

bool isRecursiveCall(const ir::CallPtr& _pCall, const ir::PredicatePtr& _pPred);

// Quantifier formulas.
ir::FormulaPtr generalize(const ir::ExpressionPtr& _pExpr);
ir::FormulaPtr setQuantifier(int _quantifier, const ir::ExpressionPtr& _pExpr, const ValuesSet& _bound);

// Case resolving.
ir::ExpressionPtr resolveCase(const ir::NamedValue& _index, const ir::ExpressionPtr& _pCase);
ir::ExpressionPtr resolveCase(const ir::NamedValues& _indexes, const ir::ExpressionPtr& _pCase);
ir::ExpressionPtr resolveCase(const ir::NamedValues& _indexes, const ir::Collection<ir::Expression>& _case);

// Formula declaration.
ir::FormulaDeclarationPtr declareFormula(const std::wstring &_strName, const ir::ExpressionPtr &_pExpr);
ir::FormulaDeclarationPtr declareFormula(const std::wstring &_strName, const ir::ExpressionPtr &_pExpr, const ir::NamedValues& _params);
ir::FormulaDeclarationPtr declareFormula(const std::wstring &_strName, const ir::Predicate &_predicate, const ir::Expression &_expr);

} // namespace na

#endif // NODE_ANALYSIS_H_
