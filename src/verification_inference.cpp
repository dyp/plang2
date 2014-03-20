/// \file verification.cpp
///

#include "ir/visitor.h"
#include "options.h"
#include "utils.h"
#include "pp_syntax.h"
#include "term_rewriting.h"
#include "generate_semantics.h"
#include "verification.h"

using namespace ir;
using namespace tr;
using namespace na;

namespace vf {

inline bool verifyVerbose() {
    return Options::instance().verify == V_VERBOSE;
}

inline bool verifyFormulas() {
    return Options::instance().verify == V_FORMULAS;
}

class Inference : public Visitor {
public:
    Inference(vf::Context& _context) :
        m_context(_context)
    {}

    ConditionPtr singledValue(const StatementPtr& _pStmt);

    // R system.
    bool ruleRP(const ParallelBlock& _block, const ConjunctionPtr& _pPre, const ConjunctionPtr& _pPost);
    bool ruleRS(const Block& _block, const ConjunctionPtr& _pPre, const ConjunctionPtr& _pPost);
    bool ruleRC(const If& _if, const ConjunctionPtr& _pPre, const ConjunctionPtr& _pPost);
    bool ruleRB(const Call& _call, const ConjunctionPtr& _pPre, const ConjunctionPtr& _pPost);

    // Q system.
    bool ruleQP(const ParallelBlock& _block, const ConjunctionPtr& _pPre, const ConjunctionPtr& _pPost);
    bool ruleQSB(const Block& _block, const ConjunctionPtr& _pPre, const ConjunctionPtr& _pPost);
    bool ruleQS(const Block& _block, const ConjunctionPtr& _pPre, const ConjunctionPtr& _pPost);
    bool ruleQC(const If& _if, const ConjunctionPtr& _pPre, const ConjunctionPtr& _pPost);

    // F system.
    bool ruleFP(const ParallelBlock& _block, const ConjunctionPtr& _pLeft);
    bool ruleFS(const Block& _block, const ConjunctionPtr& _pLeft);
    bool ruleFC(const If& _if, const ConjunctionPtr& _pLeft);

    // E system.
    bool ruleEP(const ParallelBlock& _block, const ValuesSet& _bounded, const ConjunctionPtr& _pLeft);
    bool ruleES(const Block& _block, const ValuesSet& _bounded, const ConjunctionPtr& _pLeft);
    bool ruleEC(const If& _if, const ValuesSet& _bounded, const ConjunctionPtr& _pLeft);
    bool ruleEB(const Call& _call, const ValuesSet& _bounded, const ConjunctionPtr& _pLeft);

    // FL system.
    bool ruleFLSP(const Block& _block, const ConjunctionPtr& _pTail, const ConjunctionPtr& _pRight);
    bool ruleFLC(const If& _if, const ConjunctionPtr& _pTail, const ConjunctionPtr& _pRight);
    bool ruleFLB(const Call& _call, const ConjunctionPtr& _pTail, const ConjunctionPtr& _pRight);

    bool split(const Sequent& _sequent);

    bool strategy(const Sequent& _sequent);
    bool strategy(const Correctness& _corr);
    bool strategy();
    void run();

private:
    vf::Context& m_context;
};

ConditionPtr Inference::singledValue(const StatementPtr& _pStmt) {
    ValuesSet results;
    getResults(_pStmt, results);

    Cloner cloner1, cloner2;
    ConjunctionPtr pConj1 = new Conjunction();
    for (ValuesSet::iterator i = results.begin(); i != results.end(); ++i) {
        VariableReferencePtr
            pVar1 = new VariableReference(cloner1.get(*i)),
            pVar2 = new VariableReference(cloner2.get(*i));
        pConj1->addExpression(new Binary(Binary::EQUALS, pVar1, pVar2));
    }

    const StatementPtr&
        pStmt1 = cloner1.get(_pStmt),
        pStmt2 = cloner2.get(_pStmt);

    ConjunctionPtr pConj2 = new Conjunction();
    pConj2->assign(getPreConditionForStatement(_pStmt, NULL, &m_context));
    pConj2->addConjunct(new LogicConjunct(pStmt1));
    pConj2->addConjunct(new LogicConjunct(pStmt2));

    return new Sequent(pConj2, pConj1);
}

static bool getSubStatementSpecification(const PredicatePtr& _pPred, const std::pair<StatementPtr, StatementPtr>& _statements,
    std::map<StatementPtr, std::pair<ConjunctionPtr, ConjunctionPtr> >& _container, const vf::ContextPtr& _pContext)
{
    ConjunctionPtr
        pPre1 = getPreConditionForStatement(_statements.first, _pPred, _pContext),
        pPre2 = getPreConditionForStatement(_statements.second, _pPred, _pContext),
        pPost1 = getPostConditionForStatement(_statements.first, _pContext),
        pPost2 = getPostConditionForStatement(_statements.second, _pContext);

    _container.insert(std::make_pair(_statements.first, std::make_pair(pPre1, pPost1)));
    _container.insert(std::make_pair(_statements.second, std::make_pair(pPre2, pPost2)));

    return !pPre1->empty() && !pPre2->empty() && !pPost1->empty() && !pPost2->empty();
}

bool Inference::ruleRP(const ParallelBlock& _block, const ConjunctionPtr& _pPre, const ConjunctionPtr& _pPost) {
    const StatementPtr&
        pB = _block.get(0),
        pC = _block.get(1);

    std::map<StatementPtr, std::pair<ConjunctionPtr, ConjunctionPtr> > spec;
    if (!getSubStatementSpecification(m_context.m_pPredicate, std::make_pair(pB, pC), spec, &m_context))
        return false;

    // Corr(A, B, P_B, Q_B)
    m_context.addCondition(new Correctness(pB, spec[pB].first, spec[pB].second));

    // Corr(A, C, P_C, Q_C)
    m_context.addCondition(new Correctness(pC, spec[pC].first, spec[pC].second));

    ConjunctionPtr pConj = new Conjunction();
    pConj->assign(spec[pB].first);
    pConj->append(spec[pC].first);

    // P |- P_B* & P_C*
    m_context.addCondition(new Sequent(_pPre, pConj));

    pConj = new Conjunction();
    pConj->assign(spec[pB].second);
    pConj->append(spec[pC].second);

    // Q_B & Q_C |- Q
    m_context.addCondition(new Sequent(pConj, _pPost));

    m_context.m_cLastUsedRule = Context::RP;
    return true;
}

bool Inference::ruleRS(const Block& _block, const ConjunctionPtr& _pPre, const ConjunctionPtr& _pPost) {
    const StatementPtr&
        pB = _block.get(0),
        pC = _block.get(1);

    std::map<StatementPtr, std::pair<ConjunctionPtr, ConjunctionPtr> > spec;
    if (!getSubStatementSpecification(m_context.m_pPredicate, std::make_pair(pB, pC), spec, &m_context))
        return false;

    // Corr(A, B, P_B, Q_B)
    m_context.addCondition(new Correctness(pB, spec[pB].first, spec[pB].second));

    // Corr(A, C, P_C, Q_C)
    m_context.addCondition(new Correctness(pC, spec[pC].first, spec[pC].second));

    // P |- P_B*
    m_context.addCondition(new Sequent(_pPre, spec[pB].first));

    ConjunctionPtr pConj1 = new Conjunction();
    pConj1->assign(_pPre);
    pConj1->append(spec[pB].second);

    // P & Q_B |- P_C*
    m_context.addCondition(new Sequent(pConj1, spec[pC].first));

    ConjunctionPtr pConj2 =  new Conjunction();
    pConj2->assign(pConj1);
    pConj2->append(spec[pC].second);;

    // P & Q_B & Q_C |- Q
    m_context.addCondition(new Sequent(pConj2, _pPost));

    m_context.m_cLastUsedRule = Context::RS;
    return true;
}

bool Inference::ruleRC(const If& _if, const ConjunctionPtr& _pPre, const ConjunctionPtr& _pPost) {
    const StatementPtr&
        pB = _if.getBody(),
        pC = _if.getElse();

    if (!pB || !pC)
        return false;

    std::map<StatementPtr, std::pair<ConjunctionPtr, ConjunctionPtr> > spec;
    if (!getSubStatementSpecification(m_context.m_pPredicate, std::make_pair(pB, pC), spec, &m_context))
        return false;

    // Corr(A, B, P_B, Q_B)
    m_context.addCondition(new Correctness(pB, spec[pB].first, spec[pB].second));

    // Corr(A, C, P_C, Q_C)
    m_context.addCondition(new Correctness(pC, spec[pC].first, spec[pC].second));

    ConjunctionPtr
        pArg = Conjunction::getConjunction(_if.getArg()),
        pNotArg = new Conjunction();

    pNotArg->assign(pArg);
    pNotArg->negate();

    ConjunctionPtr pConj1 = new Conjunction();
    pConj1->assign(_pPre);
    pConj1->append(pArg);

    // P & E |- P_B*
    m_context.addCondition(new Sequent(pConj1, spec[pB].first));

    ConjunctionPtr pConj2 = new Conjunction();
    pConj2->assign(_pPre);
    pConj2->append(pNotArg);

    // P & !E |- P_C*
    m_context.addCondition(new Sequent(pConj2, spec[pC].first));

    ConjunctionPtr pConj3 = new Conjunction();
    pConj3->assign(pConj1);
    pConj3->append(spec[pB].second);

    // P & E & Q_B |- Q
    m_context.addCondition(new Sequent(pConj3, _pPost));

    ConjunctionPtr pConj4 = new Conjunction();
    pConj4->assign(pConj2);
    pConj4->append(spec[pC].second);

    // P & !E & Q_C |- Q
    m_context.addCondition(new Sequent(pConj4, _pPost));

    m_context.m_cLastUsedRule = Context::RC;
    return true;
}

bool Inference::ruleRB(const Call& _call, const ConjunctionPtr& _pPre, const ConjunctionPtr& _pPost) {
    // P |- P_B & P_C*
    m_context.addCondition(new Sequent(_pPre, getPreConditionForStatement(&_call, m_context.m_pPredicate, &m_context)));

    ConjunctionPtr pConj1 = new Conjunction();
    pConj1->assign(_pPre);
    pConj1->append(getPostConditionForStatement(&_call, &m_context));

    // P & Q_C |- Q
    m_context.addCondition(new Sequent(pConj1, _pPost));

    StatementPtr pB = extractCallArguments(&_call);

    // Singled value arguments.
    m_context.addCondition(singledValue(pB));

    m_context.m_cLastUsedRule = Context::RB;
    return true;
}

bool Inference::ruleQP(const ParallelBlock& _block, const ConjunctionPtr& _pPre, const ConjunctionPtr& _pPost) {
    ValuesSet valuesB, valuesC;
    getResults(_block.get(0), valuesB);
    getResults(_block.get(1), valuesC);

    ConjunctionPtr
        pQB = new Conjunction(),
        pQC = new Conjunction();

    if (!_pPost->split(valuesB, *pQB, valuesC, *pQC))
        return false;

    // Corr(A, B, P, Q_B);
    m_context.addCondition(new Correctness(_block.get(0), _pPre, pQB));

    // Corr(A, B, P, Q_B);
    m_context.addCondition(new Correctness(_block.get(1), _pPre, pQC));

    m_context.m_cLastUsedRule = Context::QP;
    return true;
}

bool Inference::ruleQSB(const Block& _block, const ConjunctionPtr& _pPre, const ConjunctionPtr& _pPost) {
    ConjunctionPtr
        pPreB = getPreConditionForStatement(_block.get(0), &m_context),
        pPostB = getPostConditionForStatement(_block.get(0), &m_context);

    if (pPreB->empty() || pPostB->empty())
        return false;

    // P |- P_B
    m_context.addCondition(new Sequent(_pPre, pPreB));

    // Corr(A, B, P_B, Q_B);
    m_context.addCondition(new Correctness(_block.get(0), pPreB, pPostB));

    ConjunctionPtr pConj = new Conjunction();
    pConj->assign(_pPre);
    pConj->append(pPostB);

    // Corr(A, C, P & Q_B, Q)
    m_context.addCondition(new Correctness(_block.get(1), pConj, _pPost));

    m_context.m_cLastUsedRule = Context::QSB;
    return true;
}

bool Inference::ruleQS(const Block& _block, const ConjunctionPtr& _pPre, const ConjunctionPtr& _pPost) {
    ValuesSet results;
    getResults(_block.get(0), results);

    ConjunctPtr pLogic = new LogicConjunct(_block.get(0));

    // P(x) |- exists y. L(B(x: y))
    m_context.addCondition(new Sequent(_pPre, new Conjunction(new QuantifierConjunct(results, pLogic))));

    ConjunctionPtr pConj = new Conjunction;
    pConj->assign(_pPre);
    pConj->addConjunct(pLogic);

    // Corr(A, C, P(x) & L(B(x, y)), Q)
    m_context.addCondition(new Correctness(_block.get(1), pConj, _pPost));

    m_context.m_cLastUsedRule = Context::QS;
    return true;
}

bool Inference::ruleQC(const If& _if, const ConjunctionPtr& _pPre, const ConjunctionPtr& _pPost) {
    if (!_if.getBody() || !_if.getElse())
        return false;

    ConjunctionPtr
        pArg = Conjunction::getConjunction(_if.getArg()),
        pNotArg = new Conjunction();

    pNotArg->assign(pArg);
    pNotArg->negate();

    ConjunctionPtr pConj1 = new Conjunction();
    pConj1->assign(_pPre);
    pConj1->append(pArg);

    // Corr(A, B, P & E, Q)
    m_context.addCondition(new Correctness(_if.getBody(), pConj1, _pPost));

    ConjunctionPtr pConj2 = new Conjunction();
    pConj2->assign(_pPre);
    pConj2->append(pNotArg);

    // Corr(A, C, P & !E, Q)
    m_context.addCondition(new Correctness(_if.getElse(), pConj2, _pPost));

    m_context.m_cLastUsedRule = Context::QC;
    return true;
}

bool Inference::ruleFP(const ParallelBlock& _block, const ConjunctionPtr& _pLeft) {
    // R |- L(B)
    m_context.addCondition(new Sequent(_pLeft, new Conjunction(new LogicConjunct(_block.get(0)))));

    // R |- L(C)
    m_context.addCondition(new Sequent(_pLeft, new Conjunction(new LogicConjunct(_block.get(1)))));

    m_context.m_cLastUsedRule = Context::FP;
    return true;
}

bool Inference::ruleFS(const Block& _block, const ConjunctionPtr& _pLeft) {
    ValuesSet results;
    getResults(_block.get(0), results);

    // R |- exists z. L(B)
    m_context.addCondition(new Sequent(_pLeft, new Conjunction(new QuantifierConjunct(results,
        new LogicConjunct(_block.get(0))))));

    ConjunctionPtr pConj = new Conjunction();
    pConj->assign(_pLeft);
    pConj->addConjunct(new LogicConjunct(_block.get(0)));

    // R & L(B) |- L(C)
    m_context.addCondition(new Sequent(pConj, new Conjunction(new LogicConjunct(_block.get(1)))));

    m_context.m_cLastUsedRule = Context::FS;
    return true;
}

bool Inference::ruleFC(const If& _if, const ConjunctionPtr& _pLeft) {
    if (!_if.getBody() || !_if.getElse())
        return false;

    ConjunctionPtr
        pArg = Conjunction::getConjunction(_if.getArg()),
        pNotArg = new Conjunction();

    pNotArg->assign(pArg);
    pNotArg->negate();

    ConjunctionPtr pConj1 = new Conjunction();
    pConj1->assign(_pLeft);
    pConj1->append(pArg);

    // R & E |- L(B)
    m_context.addCondition(new Sequent(pConj1, new Conjunction(new LogicConjunct(_if.getBody()))));

    ConjunctionPtr pConj2 = new Conjunction();
    pConj2->assign(_pLeft);
    pConj2->append(pNotArg);

    // R & !E |- L(C)
    m_context.addCondition(new Sequent(pConj2, new Conjunction(new LogicConjunct(_if.getElse()))));

    m_context.m_cLastUsedRule = Context::FC;
    return true;
}

static void splitBoundedValues(std::pair<ValuesSet, ValuesSet>& _container, const ValuesSet& _allow,
    const StatementPtr& _pStmt1, const StatementPtr& _pStmt2) {
    ValuesSet results1, results2;

    getResults(_pStmt1, results1);
    getResults(_pStmt2, results2);

    std::set_intersection(_allow.begin(), _allow.end(), results1.begin(),
        results1.end(), std::inserter(_container.first, _container.first.end()));
    std::set_intersection(_allow.begin(), _allow.end(), results2.begin(),
        results2.end(), std::inserter(_container.second, _container.second.end()));
}

static inline ConjunctionPtr getQuantifiedConjunction(const ValuesSet& _bounded, const StatementPtr& _pStmt) {
    return new Conjunction(new QuantifierConjunct(_bounded, new LogicConjunct(_pStmt)));
}

bool Inference::ruleEP(const ParallelBlock& _block, const ValuesSet& _bounded, const ConjunctionPtr& _pLeft) {
    std::pair<ValuesSet, ValuesSet> newBounded;
    splitBoundedValues(newBounded, _bounded, _block.get(0), _block.get(1));

    // R |- exists y. L(B)
    m_context.addCondition(new Sequent(_pLeft, getQuantifiedConjunction(newBounded.first, _block.get(0))));

    // R |- exists z. L(C)
    m_context.addCondition(new Sequent(_pLeft, getQuantifiedConjunction(newBounded.second, _block.get(1))));

    m_context.m_cLastUsedRule = Context::EP;
    return true;
}

bool Inference::ruleES(const Block& _block, const ValuesSet& _bounded, const ConjunctionPtr& _pLeft) {
    std::pair<ValuesSet, ValuesSet> newBounded;
    splitBoundedValues(newBounded, _bounded, _block.get(0), _block.get(1));

    LogicConjunctPtr pLogic = new LogicConjunct(_block.get(0));

    // R |- exists y. L(B)
    m_context.addCondition(new Sequent(_pLeft, new Conjunction(new QuantifierConjunct(newBounded.first, pLogic))));

    ConjunctionPtr pConj = new Conjunction();
    pConj->assign(_pLeft);
    pConj->addConjunct(pLogic);

    // R & L(B) |- exists z. L(C)
    m_context.addCondition(new Sequent(pConj, getQuantifiedConjunction(newBounded.second, _block.get(1))));

    m_context.m_cLastUsedRule = Context::ES;
    return true;
}

bool Inference::ruleEC(const If& _if, const ValuesSet& _bounded, const ConjunctionPtr& _pLeft) {
    if (!_if.getBody() || !_if.getElse())
        return false;

    std::pair<ValuesSet, ValuesSet> newBounded;
    splitBoundedValues(newBounded, _bounded, _if.getBody(), _if.getElse());

    ConjunctionPtr
        pArg = Conjunction::getConjunction(_if.getArg()),
        pNotArg = new Conjunction();

    pNotArg->assign(pArg);
    pNotArg->negate();

    ConjunctionPtr pConj1 = new Conjunction();
    pConj1->assign(_pLeft);
    pConj1->append(pArg);

    // R & E |- exists y. L(B)
    m_context.addCondition(new Sequent(pConj1, getQuantifiedConjunction(newBounded.first, _if.getBody())));

    ConjunctionPtr pConj2 = new Conjunction();
    pConj2->assign(_pLeft);
    pConj2->append(pNotArg);

    // R & !E |- exists y. L(C)
    m_context.addCondition(new Sequent(pConj2, getQuantifiedConjunction(newBounded.second, _if.getElse())));

    m_context.m_cLastUsedRule = Context::EC;
    return true;
}

bool Inference::ruleEB(const Call& _call, const ValuesSet& _bounded, const ConjunctionPtr& _pLeft) {
    // R |- P
    m_context.addCondition(new Sequent(_pLeft, getPreConditionForStatement(&_call, NULL, &m_context)));

    m_context.m_cLastUsedRule = Context::EB;
    return true;
}

bool Inference::ruleFLSP(const Block& _block, const ConjunctionPtr& _pTail, const ConjunctionPtr& _pRight) {
    ConjunctPtr
        pB = new LogicConjunct(_block.get(0)),
        pC = new LogicConjunct(_block.get(1));

    ConjunctionPtr pConj = new Conjunction();
    pConj->assign(_pTail);
    pConj->addConjunct(pB);
    pConj->addConjunct(pC);

    // R & L(B) & L(C) |- H
    m_context.addCondition(new Sequent(pConj, _pRight));

    m_context.m_cLastUsedRule = (_block.getKind() == Statement::BLOCK ? Context::FLS : Context::FLP);
    return true;
}

bool Inference::ruleFLC(const If& _if, const ConjunctionPtr& _pTail, const ConjunctionPtr& _pRight) {
    if (!_if.getBody() || !_if.getElse())
        return false;

    ConjunctionPtr
        pArg = Conjunction::getConjunction(_if.getArg()),
        pNotArg = new Conjunction();

    pNotArg->assign(pArg);
    pNotArg->negate();

    ConjunctionPtr pConj1 = new Conjunction();
    pConj1->assign(_pTail);
    pConj1->append(pArg);
    pConj1->addConjunct(new LogicConjunct(_if.getBody()));

    // R & E & L(B) |- H
    m_context.addCondition(new Sequent(pConj1, _pRight));

    ConjunctionPtr pConj2 = new Conjunction();
    pConj2->assign(_pTail);
    pConj2->append(pNotArg);
    pConj2->addConjunct(new LogicConjunct(_if.getElse()));

    // R & !E & L(C) |- H
    m_context.addCondition(new Sequent(pConj2, _pRight));

    m_context.m_cLastUsedRule = Context::FLC;
    return true;
}

bool Inference::ruleFLB(const Call& _call, const ConjunctionPtr& _pTail, const ConjunctionPtr& _pRight) {
    ConjunctionPtr
        pPre = getPreConditionForStatement(&_call, NULL, &m_context),
        pPost = getPostConditionForStatement(&_call, &m_context);

    // |- P_B
    m_context.addCondition(new Sequent(NULL, pPre));

    ConjunctionPtr pConj = new Conjunction();
    pConj->assign(_pTail);
    pConj->append(pPost);

    // R & Q_B |- H
    m_context.addCondition(new Sequent(pConj, _pRight));

    m_context.m_cLastUsedRule = Context::FLB;

    return true;
}

bool Inference::strategy(const Correctness& _corr) {
    if (!_corr.getStatement())
        return false;

    switch (_corr.getStatement()->getKind()) {
        case Statement::PARALLEL_BLOCK:
            return ruleRP(*_corr.getStatement().as<ParallelBlock>(), _corr.getPrecondition(), _corr.getPostcondition())
                || ruleQP(*_corr.getStatement().as<ParallelBlock>(), _corr.getPrecondition(), _corr.getPostcondition());
        case Statement::BLOCK:
            return ruleRS(*_corr.getStatement().as<Block>(), _corr.getPrecondition(), _corr.getPostcondition())
                || ruleQSB(*_corr.getStatement().as<Block>(), _corr.getPrecondition(), _corr.getPostcondition())
                || ruleQS(*_corr.getStatement().as<Block>(), _corr.getPrecondition(), _corr.getPostcondition());
        case Statement::IF:
            return ruleRC(*_corr.getStatement().as<If>(), _corr.getPrecondition(), _corr.getPostcondition())
                || ruleQC(*_corr.getStatement().as<If>(), _corr.getPrecondition(), _corr.getPostcondition());
        case Statement::CALL:
            return ruleRB(*_corr.getStatement().as<Call>(), _corr.getPrecondition(), _corr.getPostcondition());
    }

    return false;
}

bool Inference::split(const Sequent& _sequent) {
    if (!_sequent.right() || !_sequent.right()->hasLogic()
        || _sequent.right()->size() <= 1)
        return false;

    ConjunctionPtr pTail = new Conjunction();

    const std::set<ConjunctPtr>& _conjucts = _sequent.right()->getConjuncts();
    for (std::set<ConjunctPtr>::iterator i = _conjucts.begin(); i != _conjucts.end(); ++i)
        if ((*i)->hasLogic())
            m_context.addCondition(new Sequent(_sequent.left(), new Conjunction(*i)));
        else
            pTail->addConjunct(*i);

    // A |- B & L(C) =>
    //    A |- B
    //    A |- L(C)
    m_context.addCondition(new Sequent(_sequent.left(), pTail));

    m_context.m_cLastUsedRule = Context::SPLIT;
    return true;
}

bool Inference::strategy(const Sequent& _sequent) {
    if (split(_sequent))
        return true;

    std::pair<ConjunctPtr, ConjunctionPtr>
        container = _sequent.left()->extractLogic();

    if (container.first && container.first->getKind() == Conjunct::LOGIC) {
        const StatementPtr& pStmt= container.first.as<LogicConjunct>()->getStatement();
        switch (pStmt->getKind()) {
            case Statement::IF:
                return ruleFLC(*pStmt.as<If>(), container.second, _sequent.right());
            case Statement::BLOCK:
            case Statement::PARALLEL_BLOCK:
                return ruleFLSP(*pStmt.as<Block>(), container.second, _sequent.right());
            case Statement::CALL:
                return ruleFLB(*pStmt.as<Call>(), container.second, _sequent.right());
        }
    }

    if (_sequent.right()->size() > 1 || !_sequent.right()->hasLogic())
        return false;

    const ConjunctPtr& pRight = _sequent.right()->front();
    if (pRight->getKind() == Conjunct::LOGIC) {
        const StatementPtr& pStmt= pRight.as<LogicConjunct>()->getStatement();
        switch (pStmt->getKind()) {
            case Statement::IF:
                return ruleFC(*pStmt.as<If>(), _sequent.left());
            case Statement::BLOCK:
                return ruleFS(*pStmt.as<Block>(), _sequent.left());
            case Statement::PARALLEL_BLOCK:
                return ruleFP(*pStmt.as<ParallelBlock>(), _sequent.left());
        }
    }

    if (pRight->getKind() != Conjunct::QUANTIFIER
        || pRight.as<QuantifierConjunct>()->getConjunct()->getKind() != Conjunct::LOGIC)
        return false;

    const ValuesSet& bound = pRight.as<QuantifierConjunct>()->getBound();
    const StatementPtr& pStmt = pRight.as<QuantifierConjunct>()->getConjunct().as<LogicConjunct>()->getStatement();

    switch (pStmt->getKind()) {
        case Statement::IF:
            return ruleEC(*pStmt.as<If>(), bound, _sequent.left());
        case Statement::BLOCK:
            return ruleES(*pStmt.as<Block>(), bound, _sequent.left());
        case Statement::PARALLEL_BLOCK:
            return ruleEP(*pStmt.as<ParallelBlock>(), bound, _sequent.left());
        case Statement::CALL:
            return ruleEB(*pStmt.as<Call>(), bound, _sequent.left());
    }

    return false;
}

bool Inference::strategy() {
    bool bIsInferred = false;

    bIsInferred |= m_context.releaseAssignments();

    if (!bIsInferred || !verifyVerbose())
        bIsInferred |= m_context.transferComplete();

    if (bIsInferred && verifyVerbose())
        return bIsInferred;

    for (std::list<std::pair<ConditionPtr, bool> >::iterator i = m_context.m_conditions.begin();
         i != m_context.m_conditions.end(); ++i) {
        bool bRuleApplied = false;
        switch ((*i).first->getKind()) {
            case Condition::CORRECTNESS:
                bRuleApplied |= strategy(*(*i).first.as<Correctness>());
                break;
            case Condition::SEQUENT:
                bRuleApplied |= strategy(*(*i).first.as<Sequent>());
                break;
        }

        bIsInferred |= bRuleApplied;

        if (bRuleApplied) {
            i = m_context.m_conditions.erase(i);
            if (verifyVerbose())
                break;
        }
    }

    return bIsInferred;
}

void Inference::run() {
    if (verifyVerbose()) {
        std::wcout << L"Start inference with:\n\n";
        pp::prettyPrint(m_context, std::wcout);
        std::wcout << L"\n";
    }

    size_t cStep = 1;

    while (strategy()) {
        if (verifyVerbose()) {
            std::wcout << L"Step " << cStep++;
            if (m_context.m_cLastUsedRule != 0)
                std::wcout << L" (" << pp::fmtRule(m_context.m_cLastUsedRule) << ")";
            std::wcout << ":\n\n";
            pp::prettyPrint(m_context, std::wcout);
            std::wcout << L"\n";
        }

        m_context.fixate();
    }

    if (verifyVerbose())
        std::wcout << (m_context.m_conditions.empty()
            ? L"Inference complete.\n" : L"Inference incomplete.\n");
}

void verify(vf::Context& _context) {
    Inference(_context).run();
}

class PredicateTraverser : public Visitor {
public:
    PredicateTraverser() : m_pTheories(new Module()) {}
    virtual bool visitPredicate(Predicate& _pred);
    ModulePtr getTheories() { return m_pTheories; }

private:
    ModulePtr m_pTheories;
};

bool PredicateTraverser::visitPredicate(Predicate& _pred) {
    StatementPtr pNewBody = modifyStatement(_pred.getBlock());

    const ModulePtr pTheory = new Module(_pred.getName());
    m_pTheories->getModules().add(pTheory);

    if (verifyVerbose()) {
        std::wcout << L"Predicate " << _pred.getName() << L".\n\n";
        std::wcout << L"Original statement:\n\n";
        pp::prettyPrintSyntax(*_pred.getBlock(), std::wcout);
        std::wcout << L"\n\nSimplified to:\n\n";
        pp::prettyPrintSyntax(*pNewBody, std::wcout);
        std::wcout << L"\n\n";
    }

    Context context;
    context.m_pPredicate = &_pred;
    context.m_conditions.push_back(std::make_pair(new Correctness(pNewBody,
        Conjunction::getConjunction(makeCall(context.getPrecondition(_pred), _pred)),
        Conjunction::getConjunction(makeCall(context.getPostcondition(_pred), _pred))), true));

    verify(context);

    if (verifyFormulas())
        std::wcout << L"Formulas for " << _pred.getName() << L":\n";

    for (std::list<std::pair<ir::ExpressionPtr, bool> >::iterator i = context.m_lemmas.begin();
        i != context.m_lemmas.end(); ++i) {
        declareLemma(pTheory, i->first);
        if (verifyFormulas()) {
            pp::prettyPrintSyntax(*i->first, std::wcout);
            std::wcout << "\n";
        }
    }

    return true;
}

ModulePtr verify(const ModulePtr &_pModule) {
    PredicateTraverser traverser;
    traverser.traverseNode(*_pModule);
    return traverser.getTheories();
}

}
