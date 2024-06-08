//
// Created by auzubarev on 04.04.18.
//

#pragma once

#include <ir/statements.h>
#include <algorithm>

#include <parser_context.h>

using namespace ir;

class StaticTypeChecker {
public:
    static bool checkAccessorExpr(AccessorExpr& accessorExpr);
    static bool checkArrayConstructor(ArrayConstructor& arrayConstructor);
    static bool checkArrayIteration(ArrayIteration& arrayIteration);
    static bool checkArrayPartExpr(ArrayPartExpr& arrayPartExpr);
    static bool checkArrayType(ArrayType& arrayType);
    static bool checkAssignment(Assignment& assignment);
    static bool checkBinary(Binary& binary);
    static bool checkBinder(Binder& binder);
    static bool checkCall(Call& call, const ir::Context &context);
    static bool checkCastExpr(CastExpr& castExpr);
    static bool checkFieldExpr(FieldExpr& fieldExpr);
    static bool checkFormula(Formula& formula);
    static bool checkFormulaCall(FormulaCall& formulaCall);
    static bool checkFunctionCall(FunctionCall& functionCall, const ir::Context &context);
    static bool checkIf(If& conditional);
    static bool checkLambda(Lambda& lambda);
    static bool checkListConstructor(ListConstructor& listConstructor);
    static bool checkLiteral(Literal& literal);
    static bool checkMapConstructor(MapConstructor& mapConstructor);
    static bool checkPredicateReference(PredicateReference& predicateReference, const ir::Context &context);
    static SubtypePtr checkRange(Range& range);
    static bool checkRecognizerExpr(RecognizerExpr& recognizerExpr);
    static bool checkReplacement(Replacement& replacement);
    static bool checkSetConstructor(SetConstructor& setConstructor);
    static bool checkStructConstructor(StructConstructor& structConstructor);
    static bool checkTernary(Ternary& ternary);
    static bool checkTypeExpr(TypeExpr& typeExpr);
    static bool checkUnary(Unary& unary);
    static bool checkUnionConstructor(UnionConstructor& unionConstructor);
    static bool checkVariableDeclaration(VariableDeclaration& variableDeclaration);
    static bool checkVariableReference(VariableReference& variableReference);
    static void printTypecheckInfo(std::wstring head, std::wstring msg = L"", int color = PRINT_BLUE, int indent = 0);
    static std::wstring str(const NodePtr &node);
    static std::wstring str(Node &node) {
        return str(node.as<Node>());
    }
    enum {
        PRINT_RED,
        PRINT_BLUE,
        PRINT_GREEN,
        PRINT_BLACK,
    };
private:
    static bool isSubtype(const TypePtr &type1, const TypePtr &type2);
    static bool isContains(const ExpressionPtr &expr, const TypePtr &type);
    static bool isTypeEqual(const TypePtr &type1, const TypePtr &type2);
    static TypePtr getTypeJoin(const TypePtr &type1, const TypePtr &type2);
    static void typeError(std::string msg, bool expr = false);
    static void typeWarning(const std::wstring msg, bool expr = false);
    static bool isFresh(const TypePtr &type);
    static void setType(Node &node, const TypePtr type);
    static TypePtr getGeneralType(const TypePtr &type);
};

