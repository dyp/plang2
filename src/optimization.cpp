/// \file optimization.cpp
///

#include "ir/visitor.h"
#include "ir/expressions.h"
#include "verification.h"

using namespace ir;

void reduceExpressions(const ir::NodePtr &_node) {
    // a => a -> true
    Expression::substitute(_node,
        std::make_shared<ir::Binary>(ir::Binary::IMPLIES,
                       std::make_shared<ir::Wild>(L"a"),
                       std::make_shared<ir::Wild>(L"a")),
        std::make_shared<ir::Literal>(true));

    // !!a -> a
    Expression::substitute(_node,
        std::make_shared<ir::Unary>(ir::Unary::BOOL_NEGATE,
                      std::make_shared<ir::Unary>(ir::Unary::BOOL_NEGATE,
                                    std::make_shared<ir::Wild>(L"a"))),
        std::make_shared<ir::Wild>(L"a"));

    // !(a = b) -> a != b
    Expression::substitute(_node,
        std::make_shared<ir::Unary>(ir::Unary::BOOL_NEGATE,
                      std::make_shared<ir::Binary>(ir::Binary::EQUALS,
                                     std::make_shared<ir::Wild>(L"a"),
                                     std::make_shared<ir::Wild>(L"b"))),
        std::make_shared<ir::Binary>(ir::Binary::NOT_EQUALS,
                       std::make_shared<ir::Wild>(L"a"),
                       std::make_shared<ir::Wild>(L"b")));

    // !(a < b) -> a >= b
    Expression::substitute(_node,
        std::make_shared<ir::Unary>(ir::Unary::BOOL_NEGATE,
                      std::make_shared<ir::Binary>(ir::Binary::LESS,
                                     std::make_shared<ir::Wild>(L"a"),
                                     std::make_shared<ir::Wild>(L"b"))),
        std::make_shared<ir::Binary>(ir::Binary::GREATER_OR_EQUALS,
                       std::make_shared<ir::Wild>(L"a"),
                       std::make_shared<ir::Wild>(L"b")));

    // !(a <= b) -> a > b
    Expression::substitute(_node,
        std::make_shared<ir::Unary>(ir::Unary::BOOL_NEGATE,
                      std::make_shared<ir::Binary>(ir::Binary::LESS_OR_EQUALS,
                                     std::make_shared<ir::Wild>(L"a"),
                                     std::make_shared<ir::Wild>(L"b"))),
        std::make_shared<ir::Binary>(ir::Binary::GREATER,
                       std::make_shared<ir::Wild>(L"a"),
                       std::make_shared<ir::Wild>(L"b")));

    // !(a > b) -> a <= b
    Expression::substitute(_node,
        std::make_shared<ir::Unary>(ir::Unary::BOOL_NEGATE,
                      std::make_shared<ir::Binary>(ir::Binary::GREATER,
                                     std::make_shared<ir::Wild>(L"a"),
                                     std::make_shared<ir::Wild>(L"b"))),
        std::make_shared<ir::Binary>(ir::Binary::LESS_OR_EQUALS,
                       std::make_shared<ir::Wild>(L"a"),
                       std::make_shared<ir::Wild>(L"b")));

    // !(a >= b) -> a < b
    Expression::substitute(_node,
        std::make_shared<ir::Unary>(ir::Unary::BOOL_NEGATE,
                      std::make_shared<ir::Binary>(ir::Binary::GREATER_OR_EQUALS,
                                     std::make_shared<ir::Wild>(L"a"),
                                     std::make_shared<ir::Wild>(L"b"))),
        std::make_shared<ir::Binary>(ir::Binary::LESS,
                       std::make_shared<ir::Wild>(L"a"),
                       std::make_shared<ir::Wild>(L"b")));

    // a => (b => c) -> a & b => c
    Expression::substitute(_node,
        std::make_shared<ir::Binary>(ir::Binary::IMPLIES,
                       std::make_shared<ir::Wild>(L"a"),
                       std::make_shared<ir::Binary>(ir::Binary::IMPLIES,
                                      std::make_shared<ir::Wild>(L"b"),
                                      std::make_shared<ir::Wild>(L"c"))),
        std::make_shared<ir::Binary>(ir::Binary::IMPLIES,
                       std::make_shared<ir::Binary>(ir::Binary::BOOL_AND,
                                      std::make_shared<ir::Wild>(L"a"),
                                      std::make_shared<ir::Wild>(L"b")),
                       std::make_shared<ir::Wild>(L"c")));

    // a => (a & b) -> a => b
    Expression::substitute(_node,
        std::make_shared<ir::Binary>(ir::Binary::IMPLIES,
                       std::make_shared<ir::Wild>(L"a"),
                       std::make_shared<ir::Binary>(ir::Binary::BOOL_AND,
                                      std::make_shared<ir::Wild>(L"a"),
                                      std::make_shared<ir::Wild>(L"b"))),
        std::make_shared<ir::Binary>(ir::Binary::IMPLIES,
                       std::make_shared<ir::Wild>(L"a"),
                       std::make_shared<ir::Wild>(L"b")));
}

// Reduce an extra formula call.
class ReduceFormulaCall : public Visitor {
public:
    ReduceFormulaCall() : Visitor(CHILDREN_FIRST) {}

    bool visitFormulaCall(const ir::FormulaCallPtr &_call) override {
        const auto pExpr = _call->getTarget()->getFormula();
        if (pExpr->getKind() != ir::Expression::FORMULA_CALL)
            return true;

        auto pCall = clone(pExpr->as<FormulaCall>());
        for (size_t i=0; i<_call->getArgs().size(); ++i)
            pCall = Expression::substitute(pCall, std::make_shared<VariableReference>(_call->getTarget()->getParams().get(i)), _call->getArgs().get(i))->as<FormulaCall>();

        callSetter(pCall);

        return true;
    }
};

void optimize(const ir::NodePtr &_node) {
    reduceExpressions(_node);
    ReduceFormulaCall().traverseNode(_node);
}
