/// \file reduce_expression.cpp
///

#include "ir/visitor.h"

using namespace ir;

bool compareExpressions(const ir::Expression &_factual, const ir::Expression &_formal) {
    if (_factual.matches(_formal)) {
        const NodesPtr pFactualChildren = _factual.getChildren(), pFormalChildren = _formal.getChildren();
        if (pFactualChildren->size() != pFormalChildren->size())
            return false;
        for(size_t i = 0; i<pFactualChildren->size(); ++i)
            if (pFormalChildren->get(i).as<Expression>()->getKind() != ir::Expression::WILD)
                if (!compareExpressions(*pFactualChildren->get(i).as<Expression>(), *pFormalChildren->get(i).as<Expression>()))
                    return false;
        return true;
    }
    else
        return false;
}

ir::ExpressionPtr getNamedExpression(const ir::Expression &_factual, const ir::Expression &_formal, const ir::Wild &_wild) {
    for(size_t i = 0; i<_factual.getChildren()->size(); ++i)
        if (_formal.getChildren()->get(i).as<Expression>()->getKind() == ir::Expression::WILD) {
            if (_formal.getChildren()->get(i).as<Wild>()->getName() == _wild.getName())
                return _factual.getChildren()->get(i).as<Expression>();
        }
        else
            return getNamedExpression(*_factual.getChildren()->get(i).as<Expression>(), *_formal.getChildren()->get(i).as<Expression>(), _wild);
    return NULL;
}

class DerefExpression : public Visitor {
public:
    DerefExpression(ir::Expression &_node, ir::Expression &_oldNode, const ir::ExpressionPtr &_pFrom, const ir::ExpressionPtr &_pTo) :
        m_pNode(&_node), m_pOldNode(&_oldNode), m_pFrom(_pFrom), m_pTo(_pTo)
    {}

    bool visitWild(ir::Wild &_wild) {
        ir::ExpressionPtr _pNewExpr = getNamedExpression(*m_pOldNode, *m_pFrom, _wild);
        if (_pNewExpr)
            callSetter(_pNewExpr);
        return false;
    }

    void run() {
        traverseNode(*m_pNode);
    }

private:
    ir::ExpressionPtr m_pNode, m_pOldNode;
    ir::ExpressionPtr m_pFrom, m_pTo;
};

inline void derefExpression(ir::Expression &_node, ir::Expression &_oldNode, const ir::ExpressionPtr &_pFrom, const ir::ExpressionPtr &_pTo) {
    DerefExpression(_node, _oldNode, _pFrom, _pTo).run();
}

class ReduceExpression : public Visitor {
public:
    ReduceExpression(ir::Node &_node, const ir::ExpressionPtr &_pFrom, const ir::ExpressionPtr &_pTo) :
        Visitor(CHILDREN_FIRST), m_pNode(&_node), m_pFrom(_pFrom), m_pTo(_pTo)
    {}

    bool visitExpression(ir::Expression &_expr) {
        if (compareExpressions(_expr, *m_pFrom)) {
            ir::ExpressionPtr pNewNode = clone(*m_pTo);
            derefExpression(*pNewNode, _expr, m_pFrom, m_pTo);
            callSetter(pNewNode);
        }
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

void reduceExpression(ir::Node &_node) {

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