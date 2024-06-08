/// \file predicate_inlining.cpp
///

#include "predicate_inlining.h"

using namespace ir;

// Collect nonrecursive calls and containing them blocks.
class CallSearch : public Visitor {
public:
    // m_recursivePredicates are program's recursive predicates.
    // m_calls are nonrecursive calls (only named referenced).
    // m_blocks are blocks which contains nonrecursive calls.
    CallSearch(const std::set<AnonymousPredicatePtr> &_recursivePredicates) : Visitor(CHILDREN_FIRST),
        m_recursivePredicates(_recursivePredicates) {}

    bool visitCall(const CallPtr &_node) override {
        // Pushes parent (Block statement) of tail recursion call to the map.
        if (_node->getPredicate()->getKind() == Expression::PREDICATE &&
                m_recursivePredicates.find(_node->getPredicate()->as<PredicateReference>()->getTarget()) ==
                    m_recursivePredicates.end())
        {
            Loc *pParent = &*(::next(m_path.rbegin()));
            if (!(pParent->pNode->getNodeKind() == Node::STATEMENT &&
               pParent->pNode->as<Statement>()->getKind() == Statement::BLOCK))
            {
                const auto pEnvelope = std::make_shared<Block>();
                pEnvelope->add(_node);
                callSetter(pEnvelope);
                m_blocks.insert(pEnvelope);
            } else
                m_blocks.insert(pParent->pNode->as<Block>());

            m_calls.insert(_node);
        }

        return true;
    }

    std::set<BlockPtr> &getBlocks() {
        return m_blocks;
    }

    std::set<CallPtr> &getCalls() {
        return m_calls;
    }

private:
    const std::set<AnonymousPredicatePtr> m_recursivePredicates;
    std::set<BlockPtr> m_blocks;
    std::set<CallPtr> m_calls;
};

// Set std::make_shared<destination for jumps (is used for predicates' blocks).
class JumpAlteration : public Visitor {
public:
    // m_pNewDestionation is std::make_shared<destination for jumps.
    // m_labels are labels which jumps should be redirected.
    JumpAlteration(const LabelPtr &_pNewDestionation) :
        m_pNewDestionation(_pNewDestionation) {}

    bool visitJump(const JumpPtr &_node) override {
        if (m_labels.find(_node->getDestination()) != m_labels.end())
            _node->setDestination(m_pNewDestionation);
        return true;
    }

    void addLabel(const LabelPtr &_pLabel) {
        m_labels.insert(_pLabel);
    }

    bool hasLabels() {
        return !m_labels.empty();
    }

private:
    const LabelPtr m_pNewDestionation;
    std::set<LabelPtr> m_labels;
};

// Retarget variables (is used for retargeting predicates' parameters to their std::make_shared<declarations).
class VariableRetargeting : public Visitor {
public:
    VariableRetargeting(const std::map<NamedValuePtr, NamedValuePtr> &_targetMap) : m_targetMap(_targetMap) {}

    bool visitVariableReference(const VariableReferencePtr &_var) override {
        auto i = m_targetMap.find(_var->getTarget());
        if (i != m_targetMap.end())
            _var->setTarget(i->second);
        return true;
    }

private:
    const std::map<NamedValuePtr, NamedValuePtr> m_targetMap;
};

// Collect all recursive predicates from _predicateOrder (which contains all module's predicates).
static void _collectRecursivePredicates(const std::list<CallGraphNode> &_predicateOrder,
        std::set<AnonymousPredicatePtr> &_recursivePredicates)
{
    for (const auto &i: _predicateOrder)
        if (i.getPredicates().size() > 1)
            for (const auto &j: i.getPredicates())
                _recursivePredicates.insert(j.first->as<AnonymousPredicate>());
        else if (i.isRecursive())
            _recursivePredicates.insert(i.getPredicate()->as<AnonymousPredicate>());
}

// TODO: inline predicates inside strogly connected components. For example: A -> B -> C -> A into A -> A.
// Need to find a criterion for selecting which predicates will be inlined.

// Generate label to jump to after call.
static LabelPtr _generatePostCallLabel(const size_t _cCallPosition, const BlockPtr &_pBlock) {
    LabelPtr pPostJumpLabel;
    if (_cCallPosition < _pBlock->size() - 1 && _pBlock->get(_cCallPosition + 1)->getLabel())
        // In case statement after call already has label.
        pPostJumpLabel = _pBlock->get(_cCallPosition + 1)->getLabel();
    else {
        pPostJumpLabel = std::make_shared<Label>();
        if (_cCallPosition < _pBlock->size() - 1)
            _pBlock->get(_cCallPosition + 1)->setLabel(pPostJumpLabel);
        else if (_cCallPosition == _pBlock->size() - 1) {
            // In case call is last statement in block. Adds void statement with label to jump to.
            const auto pStmt = std::make_shared<Statement>(pPostJumpLabel);
            pStmt->setLabel(pPostJumpLabel);
            _pBlock->add(pStmt);
        }
    }

    return pPostJumpLabel;
}

// Put branches' post statements after predicate block and handle predicate's jumps.
static void _handlePredicateBranches(const PredicatePtr &_pCalledPred, const size_t _cCallPosition,
        const BlockPtr &_pBlock, const BlockPtr &_pInsertBlock)
{
    bool bPredHasBranchLabels = false;
    for (size_t k = 0; k < _pCalledPred->getOutParams().size(); ++k)
        if (_pCalledPred->getOutParams().get(k)->getLabel()) {
            bPredHasBranchLabels = true;
            break;
        }

    if (!bPredHasBranchLabels)
        return;

    LabelPtr pPostJumpLabel = _generatePostCallLabel(_cCallPosition, _pBlock);
    JumpAlteration jumpAlteration(pPostJumpLabel);

    // Set branches' labels from callee predicate on call branch hadlers (if they exist).
    // Also add to the end of call branch hadlers jump to the statement after call.
    for (size_t k = 0; k < _pCalledPred->getOutParams().size(); ++k) {
        assert(_pBlock->get(_cCallPosition)->getKind() == Statement::CALL);
        StatementPtr pBranchStmt = _pBlock->get(_cCallPosition)->as<Call>()->getBranches().get(k)->getHandler();
        const LabelPtr pPredBranchLabel = _pCalledPred->getOutParams().get(k)->getLabel();

        if (pBranchStmt) {
            JumpPtr pPostJump = std::make_shared<Jump>(pPostJumpLabel);
            if (pBranchStmt->getKind() == Statement::BLOCK) {
                BlockPtr pBlock = pBranchStmt->as<Block>();
                pBranchStmt->setLabel(pPredBranchLabel);

                if (pBlock->size() > 0 && pBlock->get(pBlock->size() - 1)->getKind() == Statement::NOP) {
                    // Pass label from current empty last statement to its substitution.
                    pPostJump->setLabel(pBlock->get(pBlock->size() - 1)->getLabel());
                    pBlock->set(pBlock->size() - 1, pPostJump);
                } else
                    pBlock->add(pPostJump);

                _pInsertBlock->add(pBranchStmt);
            } else {
                BlockPtr pBlock = std::make_shared<Block>();
                pBlock->add(pBranchStmt);
                pBlock->add(pPostJump);
                pBlock->setLabel(pPredBranchLabel);
                _pInsertBlock->add(pBlock);
            }
            // In case predicate has branch labels and call doesn't have branch handlers
            // set unhandled jumps in predicate body (branches's returns) to the statement after call.
        } else if (pPredBranchLabel)
            jumpAlteration.addLabel(pPredBranchLabel);
    }

    if (jumpAlteration.hasLabels())
        jumpAlteration.traverseNode(_pCalledPred->getBlock());
}

// Substitute in predicate's body resulting vars with сall resulting vars.
static void _predResultsAlteration(const CallPtr &_pCall, const PredicatePtr &_pCalledPred) {
    std::map<NamedValuePtr, NamedValuePtr> alterations;
    for (size_t k = 0; k < _pCall->getBranches().size(); ++k)
        for (size_t l = 0; l < _pCall->getBranches().get(k)->size(); ++l) {
            assert(_pCall->getBranches().get(k)->get(l)->getKind() == Expression::VAR);
            alterations.insert(std::make_pair(_pCalledPred->getOutParams().get(k)->get(l),
                    _pCall->getBranches().get(k)->get(l)->as<VariableReference>()->getTarget()));
        }

    VariableRetargeting(alterations).traverseNode(_pCalledPred->getBlock());
}

// TODO: empty call results: fix parsing and insert definition of corresponding formal result before inlined body.

// Inline predicates to the places of their calls.
void ir::predicateInlining(const ModulePtr &_module) {
    CallGraph graph;
    generateCallGraph(_module, graph);

    std::list<CallGraphNode> predicateOrder;
    predicateOrdering(graph, predicateOrder);

    std::set<AnonymousPredicatePtr> recursivePredicates;
    _collectRecursivePredicates(predicateOrder, recursivePredicates);

    CallSearch callSearch(recursivePredicates);
    // Inlining predicate in order from the bottom to the top of the callgraph.
    for (const auto &i: predicateOrder)
        for (const auto &j: i.getPredicates()) {

            callSearch.traverseNode(j.first->as<Node>());
            std::set<BlockPtr> &blocks = callSearch.getBlocks();
            const std::set<CallPtr> &calls = callSearch.getCalls();
            // Search calls in blocks to inline them.
            for (const auto &pBlock: blocks)
                for (size_t k = 0; k != pBlock->size(); ++k) {
                    // (*iBlock)->size() is increasing in some iterations.

                    if (pBlock->get(k)->getKind() != Statement::CALL ||
                           calls.find(pBlock->get(k)->as<Call>()) == calls.end())
                        continue;

                    assert(pBlock->get(k)->as<Call>()->getPredicate()->getKind() == Expression::PREDICATE);
                    const auto pCall = pBlock->get(k)->as<Call>();
                    const auto pCalledPred = clone(pCall->getPredicate()->as<PredicateReference>()->getTarget());
                    const auto pInsertBlock = std::make_shared<Block>();

                    // Add declarations of callee predicate's arguments before predicate's body.
                    std::map<NamedValuePtr, NamedValuePtr> mapVariableSubstitution;
                    for (size_t l = 0; l != pCalledPred->getInParams().size(); ++l) {
                        VariablePtr pVar = std::make_shared<Variable>(true, L"", pCalledPred->getInParams().get(l)->getType());
                        VariableDeclarationPtr pDecl = std::make_shared<VariableDeclaration>(pVar, pCall->getArgs().get(l));
                        pVar->setDeclaration(pDecl);
                        pInsertBlock->add(pDecl);
                        mapVariableSubstitution.insert(std::make_pair(pCalledPred->getInParams().get(l), pVar));
                    }
                    // Retarget arguments inside predicate's body to these std::make_shared<declarations.
                    VariableRetargeting(mapVariableSubstitution).traverseNode(pCalledPred->getBlock());

                    pInsertBlock->append(pCall->getDeclarations());
                    pInsertBlock->append(*pCalledPred->getBlock());

                    _handlePredicateBranches(pCalledPred, k, pBlock, pInsertBlock);

                    _predResultsAlteration(pCall, pCalledPred);

                    if (pInsertBlock->size() != 0)
                        // In case current call has label to jump to.
                        pInsertBlock->setLabel(pBlock->get(k)->getLabel());

                    pBlock->set(k, pInsertBlock);
                }
        }
}
