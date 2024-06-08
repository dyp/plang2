/// \file generate_callgraph.cpp
///

#include <iostream>
#include <string>

#include "generate_callgraph.h"

using namespace ir;

class CallGraphTriple {
public:
    // m_pExpression is an expression of predicate type.
    // m_path is a path in the tree to m_pExpression.
    // m_predicates is a collection of predicates, references to which are contained in m_pExpression.
    CallGraphTriple(const ExpressionPtr &_pExpression, const std::list<Visitor::Loc> &_path,
            const std::list<AnonymousPredicate *> &_predicates) :
            m_pExpression(_pExpression), m_path(_path), m_predicates(_predicates) {}

    const NodePtr getParent() const {
        assert(!m_path.empty());
        return (::next(m_path.rbegin()))->pNode;
    }

    const NodeRole &getRole() const { return getPathEnd().role; }

    const size_t getPosInCollection() const { return getPathEnd().cPosInCollection; }

    const ExpressionPtr &getExpression() const { return m_pExpression; }

    const std::list<Visitor::Loc> &getPath() const { return m_path; }
    std::list<Visitor::Loc> &getPath() { return m_path; }

    const Visitor::Loc &getPathEnd() const {
        assert(!m_path.empty());
        return *m_path.rbegin();
    }

    const std::list<AnonymousPredicate *> &getPredicates() const { return m_predicates; }
    std::list<AnonymousPredicate *> &getPredicates() { return m_predicates; }

    bool operator<(const CallGraphTriple &_other) const { return m_pExpression < _other.m_pExpression; }

private:
    ExpressionPtr m_pExpression;
    std::list<Visitor::Loc> m_path;
    std::list<AnonymousPredicate *> m_predicates;
};

// Class that adds PredicateDeclarations as nodes to the graph and pushes all PredicateReferences into queue.
class CollectPredicateRefAndDecl : public Visitor {
public:
    CollectPredicateRefAndDecl(CallGraph *_pGraph, std::queue<CallGraphTriple> *_pQueue) :
        m_pGraph(_pGraph), m_pQueue(_pQueue), m_cLambdaNumber(0) {}

    int handlePredicateDecl(NodePtr &_node) override {
        // Don't add predicate declarations, only predicate defintions (to avoid copies).
        if (_node->as<Predicate>()->getBlock())
            m_pGraph->addNode(&(Predicate &)_node);
        return 0;
    }

    bool visitPredicateReference(const PredicateReferencePtr &_node) override {
        std::list<AnonymousPredicate *> predicates;
        assert(_node->getTarget());
        predicates.push_back(&*_node->getTarget());
        const CallGraphTriple triple(_node, m_path, predicates);
        m_pQueue->push(triple);
        return true;
    }

    bool visitLambda(const LambdaPtr &_node) override {
        ++m_cLambdaNumber;
        m_pGraph->addNode(&_node->getPredicate(), m_cLambdaNumber);
        std::list<AnonymousPredicate *> predicates;
        predicates.push_back(&_node->getPredicate());
        const CallGraphTriple triple(NULL, m_path, predicates);
        m_pQueue->push(triple);
        return true;
    }

private:
    CallGraph *m_pGraph;
    std::queue<CallGraphTriple> *m_pQueue;
    size_t m_cLambdaNumber;
};

// Class that finds and pushes into queue all VariableReferences which refers to variables from collected set.
class CollectVarRef : public Visitor {
public:
    CollectVarRef(std::queue<CallGraphTriple> *_pQueue, std::map<NamedValuePtr, std::list<AnonymousPredicate *> > &
            _trackingVars, std::map<FunctionCallPtr, std::list<AnonymousPredicate *> > &_trackingFunctionCalls) :
                m_pQueue(_pQueue), m_trackingVars(std::move(_trackingVars)),
                m_trackingFunctionCalls(std::move(_trackingFunctionCalls)) {}

    bool visitVariableReference(const VariableReferencePtr &_node) override {
        auto i = m_trackingVars.find(_node->getTarget());
        if (i != m_trackingVars.end())
            m_pQueue->push(CallGraphTriple(_node, m_path, i->second));

        return true;
    }

    bool visitFunctionCall(const FunctionCallPtr &_node) override {
        auto i = m_trackingFunctionCalls.find(_node);
        if (i != m_trackingFunctionCalls.end())
            m_pQueue->push(CallGraphTriple(_node, m_path, i->second));

        return true;
    }

private:
    std::queue<CallGraphTriple> *m_pQueue;
    const std::map<NamedValuePtr, std::list<AnonymousPredicate *> > m_trackingVars;
    const std::map<FunctionCallPtr, std::list<AnonymousPredicate *> > m_trackingFunctionCalls;
};

struct CGQueue {
    // m_pQueue - queue with processing triples.
    // m_argsList - list of call arguments.
    // m_calleeList - list of called predicates.
    // m_trackingVars - vars with predicate type value.
    // m_trackingResultingVars - map of call results.
    // m_trackingFunctionCalls - map with FunctionCalls.
    // m_resultsMap - multimap from callee's results to call's results.
    CGQueue(std::queue<CallGraphTriple> *_pQueue, CallGraph *_pGraph) :
                m_pQueue(_pQueue), m_pGraph(_pGraph) {}

    void queueIteration(CallGraphTriple &_triple);

    void handleCall(const CallGraphTriple &_triple);
    void handleVarDecl(const CallGraphTriple &_triple);
    void handleAssignment(const CallGraphTriple &_triple);
    void handleFunctionCall(const CallGraphTriple &_triple);
    void passToParent(CallGraphTriple &_triple);

    void handleCallArgsWithVarAsCallee();
    void handleCallResults();

    void addCallToCallGraph(const CallGraphTriple &_triple);
    void addCallArgToTrackingVars(const size_t _cArgument, const ExpressionPtr &_pCalledExpression,
            const CallGraphTriple &_triple);
    void addPredResultsToMap(const CallPtr &_pCall, const CallGraphTriple &_triple);
    void addPredResultsToMap(const FunctionCallPtr &_pCall, const CallGraphTriple &_triple);

    std::queue<CallGraphTriple> *m_pQueue;
    std::list<CallGraphTriple> m_argsList;
    std::list<CallGraphTriple> m_calleeList;
    std::map<NamedValuePtr, std::list<AnonymousPredicate *> > m_trackingVars;
    std::map<NamedValuePtr, std::list<AnonymousPredicate *> > m_trackingResultingVars;
    std::map<FunctionCallPtr, std::list<AnonymousPredicate *> > m_trackingFunctionCalls;
    std::multimap<NamedValuePtr, ExpressionPtr> m_resultsMap;
    CallGraph *m_pGraph;
};

void CGQueue::addCallToCallGraph(const CallGraphTriple &_triple) {
    // \code
    // baz2(:) {}
    // foo2(:) {
    //     P a = predicate (:) {baz2(:)};
    //     a(:);
    //     baz2(:);
    // }
    // \endcode
    // In the above example one _triple will be PredicateReference to baz2 inside definition of lambda and
    // another _triple will be PredicateReference to baz2 inside foo2 body. Passes _triple.getPath() to find
    // the callee which is either lambda or predicate.
    AnonymousPredicate* pCallingPredicate;
    for (std::list<Visitor::Loc>::const_reverse_iterator i = _triple.getPath().rbegin();
         i != _triple.getPath().rend(); ++i)
        if (i->role == R_PredicateBody) {
            if (::next(i)->pNode->getNodeKind() == Node::EXPRESSION &&
                    ::next(i)->pNode->as<Expression>()->getKind() == Expression::LAMBDA)
            {
                const auto pExpr = ::next(i)->pNode->as<Expression>();
                pCallingPredicate = &pExpr->as<Lambda>()->getPredicate();
            } else if (::next(i)->pNode->getNodeKind() == Node::STATEMENT &&
                    ::next(i)->pNode->as<Statement>()->getKind() == Statement::PREDICATE_DECLARATION)
            {
                const auto pStmnt = ::next(i)->pNode->as<Statement>();
                pCallingPredicate = &*pStmnt->as<Predicate>();
            }
            break;
        }

    assert(!_triple.getPredicates().empty());
    for (const auto &i: _triple.getPredicates())
        m_pGraph->addCall(pCallingPredicate, i);
}

// Adding to the set of tracking vars argument of called predicate with number _cArgument.
void CGQueue::addCallArgToTrackingVars(const size_t _cArgument, const ExpressionPtr &_pCalledExpression,
        const CallGraphTriple &_triple)
{
    assert(_pCalledExpression->getKind() == Expression::PREDICATE);
    const auto pNamedValue = _pCalledExpression->as<PredicateReference>()->
            getTarget()->getInParams().get(_cArgument);
    for (const auto &i: _triple.getPredicates())
        m_trackingVars[pNamedValue].push_back(i);
}

// Adding match of callee predicate's results to call's results of predicate type.
void CGQueue::addPredResultsToMap(const CallPtr &_pCall, const CallGraphTriple &_triple) {
    for (const auto &pTriplePred: _triple.getPredicates())
        for (size_t i = 0; i < _pCall->getBranches().size(); ++i)
            if (pTriplePred->getOutParams().size() == _pCall->getBranches().size() &&
                    pTriplePred->getOutParams().get(i)->size() == _pCall->getBranches().get(i)->size())
            {
                const CallBranchPtr pBranch = _pCall->getBranches().get(i);
                for (size_t j = 0; j < pBranch->size(); ++j)
                    if (pBranch->get(j)->getKind() == Expression::VAR &&
                            pBranch->get(j)->getType()->getKind() == Type::PREDICATE)
                        m_resultsMap.insert(std::make_pair(pTriplePred->getOutParams().get(i)->get(j),
                                pBranch->get(j)->as<VariableReference>()));
            }
}

// Version of previous function for functioncalls.
void CGQueue::addPredResultsToMap(const FunctionCallPtr &_pCall, const CallGraphTriple &_triple) {
    for (const auto &k: _triple.getPredicates())
        if (k->getOutParams().size() == 1 && k->getOutParams().get(0)->size() == 1)
            m_resultsMap.insert(std::make_pair(k->getOutParams().get(0)->get(0), _pCall));
}

void CGQueue::handleCall(const CallGraphTriple &_triple) {
    assert(_triple.getParent()->getNodeKind() == Node::STATEMENT &&
            _triple.getParent()->as<Statement>()->getKind() == Statement::CALL);

    const auto pCall = _triple.getParent()->as<Call>();
    if (_triple.getRole() == R_PredicateCallee) {
        addCallToCallGraph(_triple);
        m_calleeList.push_back(_triple);
        addPredResultsToMap(pCall, _triple);
    } else if (_triple.getRole() == R_PredicateCallArgs)
        if (pCall->getPredicate()->getKind() == Expression::PREDICATE)
            addCallArgToTrackingVars(_triple.getPathEnd().cPosInCollection,
                    pCall->getPredicate(), _triple);
        else
            m_argsList.push_back(_triple);
    else if (_triple.getRole() == R_PredicateCallBranchResults)
        if (_triple.getExpression()->getKind() == Expression::VAR &&
                _triple.getExpression()->as<VariableReference>()->getTarget()->getKind() ==
                        NamedValue::PREDICATE_PARAMETER &&
                _triple.getExpression()->as<VariableReference>()->getTarget()->as<Param>()->isOutput())
        {
            const auto pVal = _triple.getExpression()->as<VariableReference>()->getTarget();
            for (const auto &i: _triple.getPredicates())
                m_trackingResultingVars[pVal].push_back(i);
        }
}

void CGQueue::handleVarDecl(const CallGraphTriple &_triple) {
    assert(_triple.getParent()->getNodeKind() == Node::STATEMENT &&
            _triple.getParent()->as<Statement>()->getKind() == Statement::VARIABLE_DECLARATION);

    const auto pVarDecl = _triple.getParent()->as<VariableDeclaration>();
    assert(pVarDecl->getValue());
    for (const auto &i: _triple.getPredicates())
        m_trackingVars[pVarDecl->getVariable()].push_back(i);
}

void CGQueue::handleAssignment(const CallGraphTriple &_triple) {
    assert(_triple.getParent()->getNodeKind() == Node::STATEMENT &&
            _triple.getParent()->as<Statement>()->getKind() == Statement::ASSIGNMENT);

    const auto pAsmnt = _triple.getParent()->as<Assignment>();
    if (pAsmnt->getLValue()->getKind() == Expression::VAR) {
        const auto pVal = pAsmnt->getLValue()->as<VariableReference>()->getTarget();
        for (const auto &i: _triple.getPredicates())
            m_trackingVars[pVal].push_back(i);

        if (pVal->getKind() == NamedValue::PREDICATE_PARAMETER && pVal->as<Param>()->isOutput())
            for (const auto &j: _triple.getPredicates())
                m_trackingResultingVars[pVal].push_back(j);
    }
}

void CGQueue::handleFunctionCall(const CallGraphTriple &_triple) {
    assert(_triple.getParent()->getNodeKind() == Node::EXPRESSION &&
            _triple.getParent()->as<Expression>()->getKind() == Expression::FUNCTION_CALL);

    const auto pCall = _triple.getParent()->as<FunctionCall>();
    const auto pCalledExpression = pCall->getPredicate();
    if (_triple.getRole() == R_FunctionCallee) {
        addCallToCallGraph(_triple);
        m_calleeList.push_back(_triple);
        addPredResultsToMap(pCall, _triple);
    } else if (_triple.getRole() == R_FunctionCallArgs) {
        if (pCalledExpression->getKind() == Expression::PREDICATE)
            addCallArgToTrackingVars(_triple.getPosInCollection(), pCalledExpression, _triple);
        else
            m_argsList.push_back(_triple);
    }
}

// Transfer possible predicates to parent expression and add it to the queue.
void CGQueue::passToParent(CallGraphTriple &_triple) {
    const std::list<Visitor::Loc> pathToTriple = _triple.getPath();
    ExpressionPtr pParentExpr;
    switch (_triple.getParent()->getNodeKind()) {
        case Node::EXPRESSION:
            pParentExpr = _triple.getParent()->as<Expression>();
            break;
        case Node::UNION_CONSTRUCTOR_DECLARATION:
        case Node::ELEMENT_DEFINITION:
        case Node::STRUCT_FIELD_DEFINITION:
        case Node::ARRAY_PART_DEFINITION:
            assert(pathToTriple.size() > 2 && std::next(pathToTriple.rbegin(), 2)->pNode->getNodeKind() == Node::EXPRESSION);
            pParentExpr = std::next(pathToTriple.rbegin(), 2)->pNode->as<Expression>();
            break;
        default:
            throw std::logic_error("CallGraph construction: triple's parent's type cannot be handled.");
            break;
    }

    // Pass set of predicates to parent expression if it is already in queue.
    bool bParentAlreadyInQueue = false;
    for (size_t i = 0; i < m_pQueue->size(); i++) {
        CallGraphTriple currentTriple = m_pQueue->front();
        m_pQueue->pop();

        if (currentTriple.getExpression() == pParentExpr) {
            bParentAlreadyInQueue = true;
            currentTriple.getPredicates().merge(_triple.getPredicates());
            // No break to preserve queue's order.
        }
        m_pQueue->push(currentTriple);
    }
    // If not, push parent into queue and pass set of predicates to it.
    if (!bParentAlreadyInQueue) {
        std::list<Visitor::Loc> pathToParent(pathToTriple);
        pathToParent.pop_back();

        if (_triple.getParent()->getNodeKind() == Node::UNION_CONSTRUCTOR_DECLARATION ||
            _triple.getParent()->getNodeKind() == Node::ELEMENT_DEFINITION ||
            _triple.getParent()->getNodeKind() == Node::STRUCT_FIELD_DEFINITION ||
            _triple.getParent()->getNodeKind() == Node::ARRAY_PART_DEFINITION)
            pathToParent.pop_back();

        const CallGraphTriple parentTriple(pParentExpr, pathToParent, _triple.getPredicates());
        m_pQueue->push(parentTriple);
    }
}

// Processing front element of queue (_triple).
void CGQueue::queueIteration(CallGraphTriple &_triple) {
    switch (_triple.getParent()->getNodeKind()) {
        case Node::STATEMENT:
            switch (_triple.getParent()->as<Statement>()->getKind()) {
                case Statement::CALL:
                    handleCall(_triple);
                    break;
                case Statement::VARIABLE_DECLARATION:
                    handleVarDecl(_triple);
                    break;
                case Statement::ASSIGNMENT:
                    if (_triple.getRole() == R_RValue)
                        handleAssignment(_triple);
                    break;
            }
            break;
        case Node::EXPRESSION:
            if (_triple.getParent()->as<Expression>()->getKind() == Expression::FUNCTION_CALL) {
                handleFunctionCall(_triple);
                break;
            }
            // No break.
        case Node::UNION_CONSTRUCTOR_DECLARATION:
        case Node::ELEMENT_DEFINITION:
        case Node::STRUCT_FIELD_DEFINITION:
        case Node::ARRAY_PART_DEFINITION:
            passToParent(_triple);
            break;
    }
}

// Situation, where call arguments has predicate type and callee predicate is variable.
// Push to tracking vars formal arguments of a callee with collected predicate values.
void CGQueue::handleCallArgsWithVarAsCallee() {
    for (auto k = m_argsList.begin(); k != m_argsList.end(); ++k) {
        ExpressionPtr pCalledExpression;

        if (k->getParent()->getNodeKind() == Node::STATEMENT)
            pCalledExpression = k->getParent()->as<Call>()->getPredicate();
        else if (k->getParent()->getNodeKind() == Node::EXPRESSION)
            pCalledExpression = k->getParent()->as<FunctionCall>()->getPredicate();

        bool bFoundCallee = false;
        for (const auto &i: m_calleeList)
            if (pCalledExpression->getKind() == Expression::VAR &&
                    i.getExpression()->getKind() == Expression::VAR &&
                    pCalledExpression->as<VariableReference>()->getTarget() ==
                            i.getExpression()->as<VariableReference>()->getTarget())
            {
                const size_t cArgument = k->getPath().rbegin()->cPosInCollection;
                for (const auto &j: i.getPredicates())
                    if (cArgument <= j->getInParams().size() &&
                            j->getInParams().get(cArgument)->getType()->getKind() == Type::PREDICATE)
                        for (const auto &l: k->getPredicates())
                            m_trackingVars[j->getInParams().get(cArgument)].push_back(l);

                bFoundCallee = true;
            }

        if (bFoundCallee)
            k = m_argsList.erase(k);
    }
}

// If value of resulting var is found, then it will be sent to results of calls of current predicate.
void CGQueue::handleCallResults() {
    for (const auto &i: m_trackingResultingVars) {
        const auto range = m_resultsMap.equal_range(i.first);
        for (auto it = range.first; it != range.second; ++it)
            if (it->second->getKind() == Expression::FUNCTION_CALL)
                for (const auto &j: i.second)
                    m_trackingFunctionCalls[it->second->as<FunctionCall>()].push_back(j);
            else if (it->second->getKind() == Expression::VAR)
                for (const auto &j: i.second)
                    m_trackingVars[it->second->as<VariableReference>()->getTarget()].push_back(j);

        m_resultsMap.erase(range.first, range.second);
    }
}

void ir::generateCallGraph(const ModulePtr &_module, CallGraph &_graph) {
    std::queue<CallGraphTriple> queue;
    CollectPredicateRefAndDecl(&_graph, &queue).traverseNode(_module);
    CGQueue cgQueue(&queue, &_graph);

    while (!queue.empty()) {
        CallGraphTriple triple = queue.front();
        queue.pop();
        cgQueue.queueIteration(triple);

        if (queue.empty()) {
            cgQueue.handleCallArgsWithVarAsCallee();
            cgQueue.handleCallResults();

            if (!cgQueue.m_trackingVars.empty())
                CollectVarRef(&queue, cgQueue.m_trackingVars, cgQueue.m_trackingFunctionCalls).traverseNode(_module);
        }
    }
}

void ir::printModuleCallGraph(const ModulePtr &_module, std::wostream &_os) {
    CallGraph graph;
    generateCallGraph(_module, graph);
    printCallGraph(graph, _os);
}
