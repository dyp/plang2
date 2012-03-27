/// \file optimization.cpp
///

#include "ir/visitor.h"
#include "verification.h"

using namespace ir;

class ReduceExpression : public Visitor {
public:
    ReduceExpression(ir::Node &_node, const ir::ExpressionPtr &_pFrom, const ir::ExpressionPtr &_pTo) :
        Visitor(CHILDREN_FIRST), m_pNode(&_node), m_pFrom(_pFrom), m_pTo(_pTo)
    {}

    bool visitExpression(ir::Expression &_expr) {
        Matches matches;
        if (!_expr.matches(*m_pFrom, &matches))
            return true;

        ir::ExpressionPtr pNewNode = clone(*m_pTo);
        Expression::substitute(pNewNode, matches);

        // FIXME If _expr is root node.
        callSetter(pNewNode);

        return true;
    }

    void reduce() {
        traverseNode(*m_pNode);
    }

private:
    ir::NodePtr m_pNode;
    ir::ExpressionPtr m_pFrom, m_pTo;

};

inline void reduce(ir::Node &_node, const ir::ExpressionPtr &_pFrom, const ir::ExpressionPtr &_pTo) {
    ReduceExpression(_node, _pFrom, _pTo).reduce();
}

void reduceExpressions(ir::Node &_node) {
    // !!a -> a
    reduce(_node,
           new ir::Unary(ir::Unary::BOOL_NEGATE,
                         new ir::Unary(ir::Unary::BOOL_NEGATE,
                                       new ir::Wild(L"a"))),
           new ir::Wild(L"a"));

    // !(a = b) -> a != b
    reduce(_node,
           new ir::Unary(ir::Unary::BOOL_NEGATE,
                         new ir::Binary(ir::Binary::EQUALS,
                                        new ir::Wild(L"a"),
                                        new ir::Wild(L"b"))),
           new ir::Binary(ir::Binary::NOT_EQUALS,
                          new ir::Wild(L"a"),
                          new ir::Wild(L"b")));

    // !(a < b) -> a >= b
    reduce(_node,
           new ir::Unary(ir::Unary::BOOL_NEGATE,
                         new ir::Binary(ir::Binary::LESS,
                                        new ir::Wild(L"a"),
                                        new ir::Wild(L"b"))),
           new ir::Binary(ir::Binary::GREATER_OR_EQUALS,
                          new ir::Wild(L"a"),
                          new ir::Wild(L"b")));

    // !(a > b) -> a <= b
    reduce(_node,
           new ir::Unary(ir::Unary::BOOL_NEGATE,
                         new ir::Binary(ir::Binary::GREATER,
                                        new ir::Wild(L"a"),
                                        new ir::Wild(L"b"))),
           new ir::Binary(ir::Binary::LESS_OR_EQUALS,
                          new ir::Wild(L"a"),
                          new ir::Wild(L"b")));

    // a => (b => c) -> a & b => c
    reduce(_node,
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

}

// Reduce an extra formula call.
class ReduceFormulaCall : public Visitor {
public:
    ReduceFormulaCall() : Visitor(CHILDREN_FIRST) {}

    bool visitFormulaCall(ir::FormulaCall &_call) {
        const ir::ExpressionPtr pExpr = _call.getTarget()->getFormula();
        if (pExpr->getKind() == ir::Expression::FORMULA_CALL) {
            ir::FormulaCallPtr pCall = new ir::FormulaCall(pExpr.as<FormulaCall>()->getTarget());
            copyCallArgs(pCall, _call);
            callSetter(pCall);
        }
        return true;
    }
};

void optimize(ir::Node &_node) {
    reduceExpressions(_node);
    ReduceFormulaCall().traverseNode(_node);
}