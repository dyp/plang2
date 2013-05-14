/// \file optimization.cpp
///

#include "ir/visitor.h"
#include "ir/expressions.h"
#include "verification.h"

using namespace ir;

void reduceExpressions(ir::Node &_node) {
    // a => a -> true
    Expression::substitute(&_node,
        new ir::Binary(ir::Binary::IMPLIES,
                       new ir::Wild(L"a"),
                       new ir::Wild(L"a")),
        new ir::Literal(true));

    // !!a -> a
    Expression::substitute(&_node,
        new ir::Unary(ir::Unary::BOOL_NEGATE,
                      new ir::Unary(ir::Unary::BOOL_NEGATE,
                                    new ir::Wild(L"a"))),
        new ir::Wild(L"a"));

    // !(a = b) -> a != b
    Expression::substitute(&_node,
        new ir::Unary(ir::Unary::BOOL_NEGATE,
                      new ir::Binary(ir::Binary::EQUALS,
                                     new ir::Wild(L"a"),
                                     new ir::Wild(L"b"))),
        new ir::Binary(ir::Binary::NOT_EQUALS,
                       new ir::Wild(L"a"),
                       new ir::Wild(L"b")));

    // !(a < b) -> a >= b
    Expression::substitute(&_node,
        new ir::Unary(ir::Unary::BOOL_NEGATE,
                      new ir::Binary(ir::Binary::LESS,
                                     new ir::Wild(L"a"),
                                     new ir::Wild(L"b"))),
        new ir::Binary(ir::Binary::GREATER_OR_EQUALS,
                       new ir::Wild(L"a"),
                       new ir::Wild(L"b")));

    // !(a > b) -> a <= b
    Expression::substitute(&_node,
        new ir::Unary(ir::Unary::BOOL_NEGATE,
                      new ir::Binary(ir::Binary::GREATER,
                                     new ir::Wild(L"a"),
                                     new ir::Wild(L"b"))),
        new ir::Binary(ir::Binary::LESS_OR_EQUALS,
                       new ir::Wild(L"a"),
                       new ir::Wild(L"b")));

    // a => (b => c) -> a & b => c
    Expression::substitute(&_node,
        new ir::Binary(ir::Binary::IMPLIES,
                       new ir::Wild(L"a"),
                       new ir::Binary(ir::Binary::IMPLIES,
                                      new ir::Wild(L"b"),
                                      new ir::Wild(L"c"))),
        new ir::Binary(ir::Binary::IMPLIES,
                       new ir::Binary(ir::Binary::BOOL_AND,
                                      new ir::Wild(L"a"),
                                      new ir::Wild(L"b")),
                       new ir::Wild(L"c")));

    // a => (a & b) -> a => b
    Expression::substitute(&_node,
        new ir::Binary(ir::Binary::IMPLIES,
                       new ir::Wild(L"a"),
                       new ir::Binary(ir::Binary::BOOL_AND,
                                      new ir::Wild(L"a"),
                                      new ir::Wild(L"b"))),
        new ir::Binary(ir::Binary::IMPLIES,
                       new ir::Wild(L"a"),
                       new ir::Wild(L"b")));
}

// Reduce an extra formula call.
class ReduceFormulaCall : public Visitor {
public:
    ReduceFormulaCall() : Visitor(CHILDREN_FIRST) {}

    bool visitFormulaCall(ir::FormulaCall &_call) {
        const ir::ExpressionPtr pExpr = _call.getTarget()->getFormula();
        if (pExpr->getKind() != ir::Expression::FORMULA_CALL)
            return true;

        FormulaCallPtr pCall = clone(pExpr.as<FormulaCall>());
        for (size_t i=0; i<_call.getArgs().size(); ++i)
            pCall = Expression::substitute(pCall, new VariableReference(_call.getTarget()->getParams().get(i)), _call.getArgs().get(i)).as<FormulaCall>();

        callSetter(pCall);

        return true;
    }
};

void optimize(ir::Node &_node) {
    reduceExpressions(_node);
    ReduceFormulaCall().traverseNode(_node);
}
