//
// Created by auzubarev on 04.04.18.
//

#ifndef PLANG_STATIC_TYPECHECK_H
#define PLANG_STATIC_TYPECHECK_H

#include <ir/statements.h>
#include <algorithm>

using namespace ir;

class StaticTypeProver {
public:
    static bool checkRange(Range &_type);
    static bool checkArrayType(ArrayType &_type);
    static bool checkVariableReference(PredicateReference &_var);
    static bool checkPredicateReference(PredicateReference &_ref);
    static bool checkFormulaCall(FormulaCall &_call);
    static bool checkFunctionCall(FunctionCall &_call);
    static bool checkLambda(Lambda &_lambda);
    static bool checkBinder(Binder &_binder);
    static bool checkCall(Call &_call);
    static bool checkLiteral(Literal &_lit);
    static bool checkUnary(Unary &_unary);
    static bool checkBinary(Binary &_binary);
    static bool checkTernary(Ternary &_ternary);
    static bool checkFormula(Formula &_formula);
    static bool checkArrayPartExpr(ArrayPartExpr &_array);
    static bool checkRecognizerExpr(RecognizerExpr& _expr);
    static bool checkAccessorExpr(AccessorExpr& _expr);
    static bool checkStructConstructor(StructConstructor &_cons);
    static bool checkUnionConstructor(UnionConstructor &_cons);
    static bool checkListConstructor(ListConstructor &_cons);
    static bool checkSetConstructor(SetConstructor &_cons);
    static bool checkArrayConstructor(ArrayConstructor &_cons);
    static bool checkArrayIteration(ArrayIteration& _iter);
    static bool checkMapConstructor(MapConstructor &_cons);
    static bool checkFieldExpr(FieldExpr &_field);
    static bool checkReplacement(Replacement &_repl);
    static bool checkCastExpr(CastExpr &_cast);
    static bool checkAssignment(Assignment &_assignment);
    static bool checkVariableDeclaration(VariableDeclaration &_var);
    static bool checkIf(If &_if);
    static bool checkTypeExpr(TypeExpr &_expr);

private:
    static bool isSubtype(const TypePtr &_t1, const TypePtr &_t2);
    static bool isContains(const ExpressionPtr &_expr, const TypePtr &_type);
    static bool isTypeEqual(const TypePtr &_t1, const TypePtr &_t2);
    static TypePtr getTypeJoin(const TypePtr &_t1, const TypePtr &_t2);
    static void typeError (std::string _msg, bool _expr = false);
    static void typeWarning (std::string _msg, bool _expr = false);
    static bool isFresh(const TypePtr &_t);
    static void printTypecheckInfo (const std::wstring &_operation, const std::wstring &_msg = L"", bool _result = true, unsigned int _indent = 0);
    static std::wstring str (Node &_node);
    static void setType (Node &_node, TypePtr &_type);
};

#endif //PLANG_STATIC_TYPECHECK_H
