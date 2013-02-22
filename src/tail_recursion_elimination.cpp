/// \file tail_recursion_elimination.cpp
///

#include "tail_recursion_elimination.h"

using namespace ir;

/// Matches predicates with their tail recursion calls.
class TailRecursionSearch : public Visitor {
public:
    /// m_pPred is currently traversed predicate.
    /// m_mapTailCalls matches predicate with set of Blocks that contains tail calls as last statement.
    virtual int handlePredicateDecl(Node &_node) {
        /// Collecting tail calls for each predicate.
        m_pPred = &_node;
        BlockPtr pBlock = m_pPred->getBlock();
        if (pBlock)
            tailRecursionStatementCheck(pBlock);
        return 0;
    }

    void tailRecursionStatementCheck(const StatementPtr &_pStmnt);

    std::multimap<PredicatePtr, BlockPtr> &getMap() {
        return m_mapTailCalls;
    }

private:
    PredicatePtr m_pPred;
    std::multimap<PredicatePtr, BlockPtr> m_mapTailCalls;
};

/// Searches last executable statements for tail recursive calls.
void TailRecursionSearch::tailRecursionStatementCheck(const StatementPtr &_pStmnt)
{
    switch (_pStmnt->getKind()) {
        case Statement::BLOCK: {
            StatementPtr pLastStmt = _pStmnt.as<Block>()->get(_pStmnt.as<Block>()->size() - 1);
            if (pLastStmt->getKind() == Statement::CALL &&
                    pLastStmt.as<Call>()->getPredicate()->getKind() == Expression::PREDICATE &&
                    pLastStmt.as<Call>()->getPredicate().as<PredicateReference>()->getTarget() == m_pPred)
            {
                /// Check that call results equals to corresponding predicate's results.
                /// Otherwise it's not a tail call.
                CallPtr pCall = pLastStmt.as<Call>();
                for (size_t j = 0; j < pCall->getBranches().size(); ++j) {
                    for (size_t k = 0; k < pCall->getBranches().get(j)->size(); ++k) {
                        ExpressionPtr pResult = pCall->getBranches().get(j)->get(k);
                        if (pResult->getKind() != Expression::VAR ||
                            pResult.as<VariableReference>()->getTarget() !=
                                    m_pPred->getOutParams().get(j)->get(k))
                            return;
                    }
                }
                m_mapTailCalls.insert(std::make_pair(m_pPred, _pStmnt.as<Block>()));
            }
            else
                tailRecursionStatementCheck(pLastStmt);
            break;
        }
        case Statement::SWITCH:
            for (size_t i = 0; i != _pStmnt.as<Switch>()->size(); ++i) {
                tailRecursionStatementCheck(_pStmnt.as<Switch>()->get(i)->getBody());
            }
            tailRecursionStatementCheck(_pStmnt.as<Switch>()->getDefault());
            break;
        case Statement::IF:
            tailRecursionStatementCheck(_pStmnt.as<If>()->getBody());
            tailRecursionStatementCheck(_pStmnt.as<If>()->getElse());
            break;
    }
}

void ir::tailRecursionElimination(Module &_module) {
    TailRecursionSearch tailRecursionCheck;
    tailRecursionCheck.traverseNode(_module);
    std::multimap<PredicatePtr, BlockPtr> &tailCallMap = tailRecursionCheck.getMap();

    for (std::multimap<PredicatePtr, BlockPtr>::iterator i = tailCallMap.begin(); i != tailCallMap.end(); ++i) {
        PredicatePtr pPred = i->first;
        BlockPtr pBlock = i->second;
        const CallPtr pCall = pBlock->get(pBlock->size() - 1).as<Call>();

        /// Label for predicate's first statement, to which we will jump after tail call.
        if (!pPred->getBlock()->get(0)->getLabel())
            pPred->getBlock()->get(0)->setLabel(new Label());
        const JumpPtr pJump = new Jump(pPred->getBlock()->get(0)->getLabel());

        /// Multiassignment with predicate arguments in the left part and call arguments in the right.
        MultiassignmentPtr pMulti = new Multiassignment();
        for (size_t k = 0; k != pPred->getInParams().size(); ++k) {
            pMulti->getLValues().add(new VariableReference(pPred->getInParams().get(k)->getName(),
                    pPred->getInParams().get(k)));
        }
        pMulti->getExpressions().append(pCall->getArgs());
        /// In case current Call have label to jump to.
        pMulti->setLabel(pCall->getLabel());

        /// Replace tail recursion call with multiassignment and jump to the first statement of predicate.
        pBlock->remove(pBlock->size() - 1);
        pBlock->add(pMulti);
        pBlock->add(pJump);
    }
}
