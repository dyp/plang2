/// \file test_statement_tree.h
///

#ifndef TEST_PRECONDITION_H_
#define TEST_PRECONDITION_H_

#include "ir/visitor.h"
#include "generate_semantics.h"
#include "pp_syntax.h"
#include "term_rewriting.h"

class PreconditionsPrinter : public ir::Visitor {
public:
    PreconditionsPrinter(std::wostream & _os = std::wcout) :
        m_os(_os)
    {}

    virtual bool visitExpression(ir::Expression& _expr) {
        vf::ConjunctionPtr pConj = getPreConditionForExpression(&_expr);

        if (pConj->empty())
            return false;

        ir::ExpressionPtr pExpr = pConj->mergeToExpression();
        if (!pExpr)
            return true;

        pExpr = tr::normalizeExpressions(pExpr).as<ir::Expression>();
        pp::prettyPrintSyntax(*pExpr, m_os, nullptr, true);
        return true;
    }

    virtual bool visitPredicate(ir::Predicate& _pred) {
        m_pPred = &_pred;
        return true;
    }

    virtual bool visitStatement(ir::Statement& _stmt) {
        vf::ConjunctionPtr
            pConjPre = getPreConditionForStatement(&_stmt, m_pPred),
            pConjPost = getPostConditionForStatement(&_stmt);

        ir::ExpressionPtr pExpr = pConjPre->mergeToExpression();
        pExpr = tr::normalizeExpressions(pExpr, true).as<ir::Expression>();

        if (!pConjPre->empty() && pExpr)
            pp::prettyPrintSyntax(*pExpr, m_os, nullptr, true);

        pExpr = pConjPost->mergeToExpression();
        pExpr = tr::normalizeExpressions(pExpr, true).as<ir::Expression>();

        if (!pConjPost->empty() && pExpr)
            pp::prettyPrintSyntax(*pExpr, m_os, nullptr, true);

        return true;
    }

private:
    std::wostream &m_os;
    ir::PredicatePtr m_pPred;
};

#endif // TEST_PRECONDITION_H_
