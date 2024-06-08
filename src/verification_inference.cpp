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
    Inference(const vf::ContextPtr& _context) :
        m_context(_context)
    {}

    ConditionPtr singledValue(const StatementPtr& _pStmt);

    // R system.
    bool ruleRP(const ParallelBlockPtr& _block, const ConjunctionPtr& _pPre, const ConjunctionPtr& _pPost);
    bool ruleRS(const BlockPtr& _block, const ConjunctionPtr& _pPre, const ConjunctionPtr& _pPost);
    bool ruleRC(const IfPtr& _if, const ConjunctionPtr& _pPre, const ConjunctionPtr& _pPost);
    bool ruleRB(const CallPtr& _call, const ConjunctionPtr& _pPre, const ConjunctionPtr& _pPost);

    // Q system.
    bool ruleQP(const ParallelBlockPtr& _block, const ConjunctionPtr& _pPre, const ConjunctionPtr& _pPost);
    bool ruleQSB(const BlockPtr& _block, const ConjunctionPtr& _pPre, const ConjunctionPtr& _pPost);
    bool ruleQS(const BlockPtr& _block, const ConjunctionPtr& _pPre, const ConjunctionPtr& _pPost);
    bool ruleQC(const IfPtr& _if, const ConjunctionPtr& _pPre, const ConjunctionPtr& _pPost);

    // F system.
    bool ruleFP(const ParallelBlockPtr& _block, const ConjunctionPtr& _pLeft);
    bool ruleFS(const BlockPtr& _block, const ConjunctionPtr& _pLeft);
    bool ruleFC(const IfPtr& _if, const ConjunctionPtr& _pLeft);

    // E system.
    bool ruleEP(const ParallelBlockPtr& _block, const ValuesSet& _bounded, const ConjunctionPtr& _pLeft);
    bool ruleES(const BlockPtr& _block, const ValuesSet& _bounded, const ConjunctionPtr& _pLeft);
    bool ruleEC(const IfPtr& _if, const ValuesSet& _bounded, const ConjunctionPtr& _pLeft);
    bool ruleEB(const CallPtr& _call, const ValuesSet& _bounded, const ConjunctionPtr& _pLeft);

    // FL system.
    bool ruleFLSP(const BlockPtr& _block, const ConjunctionPtr& _pTail, const ConjunctionPtr& _pRight);
    bool ruleFLC(const IfPtr& _if, const ConjunctionPtr& _pTail, const ConjunctionPtr& _pRight);
    bool ruleFLB(const CallPtr& _call, const ConjunctionPtr& _pTail, const ConjunctionPtr& _pRight);

    bool split(const SequentPtr& _sequent);

    bool strategy(const SequentPtr& _sequent);
    bool strategy(const CorrectnessPtr& _corr);
    bool strategy();
    void run();

private:
    const vf::ContextPtr m_context;
};

ConditionPtr Inference::singledValue(const StatementPtr& _pStmt) {
    ValuesSet results;
    getResults(_pStmt, results);

    Cloner cloner1, cloner2;
    const auto pConj1 = std::make_shared<Conjunction>();
    for (ValuesSet::iterator i = results.begin(); i != results.end(); ++i) {
        auto
            pVar1 = std::make_shared<VariableReference>(cloner1.get(*i)),
            pVar2 = std::make_shared<VariableReference>(cloner2.get(*i));
        pConj1->addExpression(std::make_shared<Binary>(Binary::EQUALS, pVar1, pVar2));
    }

    const StatementPtr&
        pStmt1 = cloner1.get(_pStmt),
        pStmt2 = cloner2.get(_pStmt);

    ConjunctionPtr pConj2 = std::make_shared<Conjunction>();
    pConj2->assign(getPreConditionForStatement(_pStmt, NULL, m_context));
    pConj2->addConjunct(std::make_shared<LogicConjunct>(pStmt1));
    pConj2->addConjunct(std::make_shared<LogicConjunct>(pStmt2));

    return std::make_shared<Sequent>(pConj2, pConj1);
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

bool Inference::ruleRP(const ParallelBlockPtr& _block, const ConjunctionPtr& _pPre, const ConjunctionPtr& _pPost) {
    const auto
        pB = _block->get(0),
        pC = _block->get(1);

    std::map<StatementPtr, std::pair<ConjunctionPtr, ConjunctionPtr> > spec;
    if (!getSubStatementSpecification(m_context->m_pPredicate, std::make_pair(pB, pC), spec, m_context))
        return false;

    // Corr(A, B, P_B, Q_B)
    m_context->addCondition(std::make_shared<Correctness>(pB, spec[pB].first, spec[pB].second));

    // Corr(A, C, P_C, Q_C)
    m_context->addCondition(std::make_shared<Correctness>(pC, spec[pC].first, spec[pC].second));

    ConjunctionPtr pConj = std::make_shared<Conjunction>();
    pConj->assign(spec[pB].first);
    pConj->append(spec[pC].first);

    // P |- P_B* & P_C*
    m_context->addCondition(std::make_shared<Sequent>(_pPre, pConj));

    pConj = std::make_shared<Conjunction>();
    pConj->assign(spec[pB].second);
    pConj->append(spec[pC].second);

    // Q_B & Q_C |- Q
    m_context->addCondition(std::make_shared<Sequent>(pConj, _pPost));

    m_context->m_cLastUsedRule = Context::RP;
    return true;
}

bool Inference::ruleRS(const BlockPtr& _block, const ConjunctionPtr& _pPre, const ConjunctionPtr& _pPost) {
    const auto
        pB = _block->get(0),
        pC = _block->get(1);

    std::map<StatementPtr, std::pair<ConjunctionPtr, ConjunctionPtr> > spec;
    if (!getSubStatementSpecification(m_context->m_pPredicate, std::make_pair(pB, pC), spec, m_context))
        return false;

    // Corr(A, B, P_B, Q_B)
    m_context->addCondition(std::make_shared<Correctness>(pB, spec[pB].first, spec[pB].second));

    // Corr(A, C, P_C, Q_C)
    m_context->addCondition(std::make_shared<Correctness>(pC, spec[pC].first, spec[pC].second));

    // P |- P_B*
    m_context->addCondition(std::make_shared<Sequent>(_pPre, spec[pB].first));

    ConjunctionPtr pConj1 = std::make_shared<Conjunction>();
    pConj1->assign(_pPre);
    pConj1->append(spec[pB].second);

    // P & Q_B |- P_C*
    m_context->addCondition(std::make_shared<Sequent>(pConj1, spec[pC].first));

    ConjunctionPtr pConj2 =  std::make_shared<Conjunction>();
    pConj2->assign(pConj1);
    pConj2->append(spec[pC].second);;

    // P & Q_B & Q_C |- Q
    m_context->addCondition(std::make_shared<Sequent>(pConj2, _pPost));

    m_context->m_cLastUsedRule = Context::RS;
    return true;
}

bool Inference::ruleRC(const IfPtr& _if, const ConjunctionPtr& _pPre, const ConjunctionPtr& _pPost) {
    const auto
        pB = _if->getBody(),
        pC = _if->getElse();

    if (!pB || !pC)
        return false;

    std::map<StatementPtr, std::pair<ConjunctionPtr, ConjunctionPtr> > spec;
    if (!getSubStatementSpecification(m_context->m_pPredicate, std::make_pair(pB, pC), spec, m_context))
        return false;

    // Corr(A, B, P_B, Q_B)
    m_context->addCondition(std::make_shared<Correctness>(pB, spec[pB].first, spec[pB].second));

    // Corr(A, C, P_C, Q_C)
    m_context->addCondition(std::make_shared<Correctness>(pC, spec[pC].first, spec[pC].second));

    ConjunctionPtr
        pArg = Conjunction::getConjunction(_if->getArg()),
        pNotArg = std::make_shared<Conjunction>();

    pNotArg->assign(pArg);
    pNotArg->negate();

    ConjunctionPtr pConj1 = std::make_shared<Conjunction>();
    pConj1->assign(_pPre);
    pConj1->append(pArg);

    // P & E |- P_B*
    m_context->addCondition(std::make_shared<Sequent>(pConj1, spec[pB].first));

    ConjunctionPtr pConj2 = std::make_shared<Conjunction>();
    pConj2->assign(_pPre);
    pConj2->append(pNotArg);

    // P & !E |- P_C*
    m_context->addCondition(std::make_shared<Sequent>(pConj2, spec[pC].first));

    ConjunctionPtr pConj3 = std::make_shared<Conjunction>();
    pConj3->assign(pConj1);
    pConj3->append(spec[pB].second);

    // P & E & Q_B |- Q
    m_context->addCondition(std::make_shared<Sequent>(pConj3, _pPost));

    ConjunctionPtr pConj4 = std::make_shared<Conjunction>();
    pConj4->assign(pConj2);
    pConj4->append(spec[pC].second);

    // P & !E & Q_C |- Q
    m_context->addCondition(std::make_shared<Sequent>(pConj4, _pPost));

    m_context->m_cLastUsedRule = Context::RC;
    return true;
}

bool Inference::ruleRB(const CallPtr& _call, const ConjunctionPtr& _pPre, const ConjunctionPtr& _pPost) {
    // P |- P_B & P_C*
    m_context->addCondition(std::make_shared<Sequent>(_pPre, getPreConditionForStatement(_call, m_context->m_pPredicate, m_context)));

    ConjunctionPtr pConj1 = std::make_shared<Conjunction>();
    pConj1->assign(_pPre);
    pConj1->append(getPostConditionForStatement(_call, m_context));

    // P & Q_C |- Q
    m_context->addCondition(std::make_shared<Sequent>(pConj1, _pPost));

    StatementPtr pB = extractCallArguments(_call);

    // Singled value arguments.
    m_context->addCondition(singledValue(pB));

    m_context->m_cLastUsedRule = Context::RB;
    return true;
}

bool Inference::ruleQP(const ParallelBlockPtr& _block, const ConjunctionPtr& _pPre, const ConjunctionPtr& _pPost) {
    ValuesSet valuesB, valuesC;
    getResults(_block->get(0), valuesB);
    getResults(_block->get(1), valuesC);

    ConjunctionPtr
        pQB = std::make_shared<Conjunction>(),
        pQC = std::make_shared<Conjunction>();

    if (!_pPost->split(valuesB, *pQB, valuesC, *pQC))
        return false;

    // Corr(A, B, P, Q_B);
    m_context->addCondition(std::make_shared<Correctness>(_block->get(0), _pPre, pQB));

    // Corr(A, B, P, Q_B);
    m_context->addCondition(std::make_shared<Correctness>(_block->get(1), _pPre, pQC));

    m_context->m_cLastUsedRule = Context::QP;
    return true;
}

bool Inference::ruleQSB(const BlockPtr& _block, const ConjunctionPtr& _pPre, const ConjunctionPtr& _pPost) {
    auto
        pPreB = getPreConditionForStatement(_block->get(0), nullptr, m_context),
        pPostB = getPostConditionForStatement(_block->get(0), m_context);

    if (pPreB->empty() || pPostB->empty())
        return false;

    // P |- P_B
    m_context->addCondition(std::make_shared<Sequent>(_pPre, pPreB));

    // Corr(A, B, P_B, Q_B);
    m_context->addCondition(std::make_shared<Correctness>(_block->get(0), pPreB, pPostB));

    ConjunctionPtr pConj = std::make_shared<Conjunction>();
    pConj->assign(_pPre);
    pConj->append(pPostB);

    // Corr(A, C, P & Q_B, Q)
    m_context->addCondition(std::make_shared<Correctness>(_block->get(1), pConj, _pPost));

    m_context->m_cLastUsedRule = Context::QSB;
    return true;
}

bool Inference::ruleQS(const BlockPtr& _block, const ConjunctionPtr& _pPre, const ConjunctionPtr& _pPost) {
    ValuesSet results;
    getResults(_block->get(0), results);

    const auto pLogic = std::make_shared<LogicConjunct>(_block->get(0));

    // P(x) |- exists y. L(B(x: y))
    m_context->addCondition(std::make_shared<Sequent>(_pPre, std::make_shared<Conjunction>(std::make_shared<QuantifierConjunct>(results, pLogic))));

    ConjunctionPtr pConj = std::make_shared<Conjunction>();
    pConj->assign(_pPre);
    pConj->addConjunct(pLogic);

    // Corr(A, C, P(x) & L(B(x, y)), Q)
    m_context->addCondition(std::make_shared<Correctness>(_block->get(1), pConj, _pPost));

    m_context->m_cLastUsedRule = Context::QS;
    return true;
}

bool Inference::ruleQC(const IfPtr& _if, const ConjunctionPtr& _pPre, const ConjunctionPtr& _pPost) {
    if (!_if->getBody() || !_if->getElse())
        return false;

    ConjunctionPtr
        pArg = Conjunction::getConjunction(_if->getArg()),
        pNotArg = std::make_shared<Conjunction>();

    pNotArg->assign(pArg);
    pNotArg->negate();

    ConjunctionPtr pConj1 = std::make_shared<Conjunction>();
    pConj1->assign(_pPre);
    pConj1->append(pArg);

    // Corr(A, B, P & E, Q)
    m_context->addCondition(std::make_shared<Correctness>(_if->getBody(), pConj1, _pPost));

    ConjunctionPtr pConj2 = std::make_shared<Conjunction>();
    pConj2->assign(_pPre);
    pConj2->append(pNotArg);

    // Corr(A, C, P & !E, Q)
    m_context->addCondition(std::make_shared<Correctness>(_if->getElse(), pConj2, _pPost));

    m_context->m_cLastUsedRule = Context::QC;
    return true;
}

bool Inference::ruleFP(const ParallelBlockPtr& _block, const ConjunctionPtr& _pLeft) {
    // R |- L(B)
    m_context->addCondition(std::make_shared<Sequent>(_pLeft, std::make_shared<Conjunction>(std::make_shared<LogicConjunct>(_block->get(0)))));

    // R |- L(C)
    m_context->addCondition(std::make_shared<Sequent>(_pLeft, std::make_shared<Conjunction>(std::make_shared<LogicConjunct>(_block->get(1)))));

    m_context->m_cLastUsedRule = Context::FP;
    return true;
}

bool Inference::ruleFS(const BlockPtr& _block, const ConjunctionPtr& _pLeft) {
    ValuesSet results;
    getResults(_block->get(0), results);

    // R |- exists z. L(B)
    m_context->addCondition(std::make_shared<Sequent>(_pLeft, std::make_shared<Conjunction>(std::make_shared<QuantifierConjunct>(results,
        std::make_shared<LogicConjunct>(_block->get(0))))));

    const auto pConj = std::make_shared<Conjunction>();
    pConj->assign(_pLeft);
    pConj->addConjunct(std::make_shared<LogicConjunct>(_block->get(0)));

    // R & L(B) |- L(C)
    m_context->addCondition(std::make_shared<Sequent>(pConj, std::make_shared<Conjunction>(std::make_shared<LogicConjunct>(_block->get(1)))));

    m_context->m_cLastUsedRule = Context::FS;
    return true;
}

bool Inference::ruleFC(const IfPtr& _if, const ConjunctionPtr& _pLeft) {
    if (!_if->getBody() || !_if->getElse())
        return false;

    const auto
        pArg = Conjunction::getConjunction(_if->getArg()),
        pNotArg = std::make_shared<Conjunction>();

    pNotArg->assign(pArg);
    pNotArg->negate();

    ConjunctionPtr pConj1 = std::make_shared<Conjunction>();
    pConj1->assign(_pLeft);
    pConj1->append(pArg);

    // R & E |- L(B)
    m_context->addCondition(std::make_shared<Sequent>(pConj1, std::make_shared<Conjunction>(std::make_shared<LogicConjunct>(_if->getBody()))));

    ConjunctionPtr pConj2 = std::make_shared<Conjunction>();
    pConj2->assign(_pLeft);
    pConj2->append(pNotArg);

    // R & !E |- L(C)
    m_context->addCondition(std::make_shared<Sequent>(pConj2, std::make_shared<Conjunction>(std::make_shared<LogicConjunct>(_if->getElse()))));

    m_context->m_cLastUsedRule = Context::FC;
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
    return std::make_shared<Conjunction>(std::make_shared<QuantifierConjunct>(_bounded, std::make_shared<LogicConjunct>(_pStmt)));
}

bool Inference::ruleEP(const ParallelBlockPtr& _block, const ValuesSet& _bounded, const ConjunctionPtr& _pLeft) {
    std::pair<ValuesSet, ValuesSet> newBounded;
    splitBoundedValues(newBounded, _bounded, _block->get(0), _block->get(1));

    // R |- exists y. L(B)
    m_context->addCondition(std::make_shared<Sequent>(_pLeft, getQuantifiedConjunction(newBounded.first, _block->get(0))));

    // R |- exists z. L(C)
    m_context->addCondition(std::make_shared<Sequent>(_pLeft, getQuantifiedConjunction(newBounded.second, _block->get(1))));

    m_context->m_cLastUsedRule = Context::EP;
    return true;
}

bool Inference::ruleES(const BlockPtr& _block, const ValuesSet& _bounded, const ConjunctionPtr& _pLeft) {
    std::pair<ValuesSet, ValuesSet> newBounded;
    splitBoundedValues(newBounded, _bounded, _block->get(0), _block->get(1));

    const auto pLogic = std::make_shared<LogicConjunct>(_block->get(0));

    // R |- exists y. L(B)
    m_context->addCondition(std::make_shared<Sequent>(_pLeft, std::make_shared<Conjunction>(std::make_shared<QuantifierConjunct>(newBounded.first, pLogic))));

    const auto pConj = std::make_shared<Conjunction>();
    pConj->assign(_pLeft);
    pConj->addConjunct(pLogic);

    // R & L(B) |- exists z. L(C)
    m_context->addCondition(std::make_shared<Sequent>(pConj, getQuantifiedConjunction(newBounded.second, _block->get(1))));

    m_context->m_cLastUsedRule = Context::ES;
    return true;
}

bool Inference::ruleEC(const IfPtr& _if, const ValuesSet& _bounded, const ConjunctionPtr& _pLeft) {
    if (!_if->getBody() || !_if->getElse())
        return false;

    std::pair<ValuesSet, ValuesSet> newBounded;
    splitBoundedValues(newBounded, _bounded, _if->getBody(), _if->getElse());

    const auto
        pArg = Conjunction::getConjunction(_if->getArg()),
        pNotArg = std::make_shared<Conjunction>();

    pNotArg->assign(pArg);
    pNotArg->negate();

    const auto pConj1 = std::make_shared<Conjunction>();
    pConj1->assign(_pLeft);
    pConj1->append(pArg);

    // R & E |- exists y. L(B)
    m_context->addCondition(std::make_shared<Sequent>(pConj1, getQuantifiedConjunction(newBounded.first, _if->getBody())));

    const auto pConj2 = std::make_shared<Conjunction>();
    pConj2->assign(_pLeft);
    pConj2->append(pNotArg);

    // R & !E |- exists y. L(C)
    m_context->addCondition(std::make_shared<Sequent>(pConj2, getQuantifiedConjunction(newBounded.second, _if->getElse())));

    m_context->m_cLastUsedRule = Context::EC;
    return true;
}

bool Inference::ruleEB(const CallPtr& _call, const ValuesSet& _bounded, const ConjunctionPtr& _pLeft) {
    // R |- P
    m_context->addCondition(std::make_shared<Sequent>(_pLeft, getPreConditionForStatement(_call, NULL, m_context)));

    m_context->m_cLastUsedRule = Context::EB;
    return true;
}

bool Inference::ruleFLSP(const BlockPtr& _block, const ConjunctionPtr& _pTail, const ConjunctionPtr& _pRight) {
    const auto
        pB = std::make_shared<LogicConjunct>(_block->get(0)),
        pC = std::make_shared<LogicConjunct>(_block->get(1));

    const auto pConj = std::make_shared<Conjunction>();
    pConj->assign(_pTail);
    pConj->addConjunct(pB);
    pConj->addConjunct(pC);

    // R & L(B) & L(C) |- H
    m_context->addCondition(std::make_shared<Sequent>(pConj, _pRight));

    m_context->m_cLastUsedRule = (_block->getKind() == Statement::BLOCK ? Context::FLS : Context::FLP);
    return true;
}

bool Inference::ruleFLC(const IfPtr& _if, const ConjunctionPtr& _pTail, const ConjunctionPtr& _pRight) {
    if (!_if->getBody() || !_if->getElse())
        return false;

    const auto
        pArg = Conjunction::getConjunction(_if->getArg()),
        pNotArg = std::make_shared<Conjunction>();

    pNotArg->assign(pArg);
    pNotArg->negate();

    const auto pConj1 = std::make_shared<Conjunction>();
    pConj1->assign(_pTail);
    pConj1->append(pArg);
    pConj1->addConjunct(std::make_shared<LogicConjunct>(_if->getBody()));

    // R & E & L(B) |- H
    m_context->addCondition(std::make_shared<Sequent>(pConj1, _pRight));

    const auto pConj2 = std::make_shared<Conjunction>();
    pConj2->assign(_pTail);
    pConj2->append(pNotArg);
    pConj2->addConjunct(std::make_shared<LogicConjunct>(_if->getElse()));

    // R & !E & L(C) |- H
    m_context->addCondition(std::make_shared<Sequent>(pConj2, _pRight));

    m_context->m_cLastUsedRule = Context::FLC;
    return true;
}

bool Inference::ruleFLB(const CallPtr& _call, const ConjunctionPtr& _pTail, const ConjunctionPtr& _pRight) {
    const auto
        pPre = getPreConditionForStatement(_call, NULL, m_context),
        pPost = getPostConditionForStatement(_call, m_context);

    // R |- P_B
    m_context->addCondition(std::make_shared<Sequent>(_pTail, pPre));

    const auto pConj = std::make_shared<Conjunction>();
    pConj->assign(_pTail);
    pConj->append(pPost);

    // R & Q_B |- H
    m_context->addCondition(std::make_shared<Sequent>(pConj, _pRight));

    m_context->m_cLastUsedRule = Context::FLB;

    return true;
}

bool Inference::strategy(const CorrectnessPtr& _corr) {
    if (!_corr->getStatement())
        return false;

    switch (_corr->getStatement()->getKind()) {
        case Statement::PARALLEL_BLOCK:
            return ruleRP(_corr->getStatement()->as<ParallelBlock>(), _corr->getPrecondition(), _corr->getPostcondition())
                || ruleQP(_corr->getStatement()->as<ParallelBlock>(), _corr->getPrecondition(), _corr->getPostcondition());
        case Statement::BLOCK:
            return ruleRS(_corr->getStatement()->as<Block>(), _corr->getPrecondition(), _corr->getPostcondition())
                || ruleQSB(_corr->getStatement()->as<Block>(), _corr->getPrecondition(), _corr->getPostcondition())
                || ruleQS(_corr->getStatement()->as<Block>(), _corr->getPrecondition(), _corr->getPostcondition());
        case Statement::IF:
            return ruleRC(_corr->getStatement()->as<If>(), _corr->getPrecondition(), _corr->getPostcondition())
                || ruleQC(_corr->getStatement()->as<If>(), _corr->getPrecondition(), _corr->getPostcondition());
        case Statement::CALL:
            return ruleRB(_corr->getStatement()->as<Call>(), _corr->getPrecondition(), _corr->getPostcondition());
    }

    return false;
}

bool Inference::split(const SequentPtr& _sequent) {
    if (!_sequent->right() || !_sequent->right()->hasLogic()
        || _sequent->right()->size() <= 1)
        return false;

    const auto pTail = std::make_shared<Conjunction>();

    for (auto i: _sequent->right()->getConjuncts())
        if (i->hasLogic())
            m_context->addCondition(std::make_shared<Sequent>(_sequent->left(), std::make_shared<Conjunction>(i)));
        else
            pTail->addConjunct(i);

    // A |- B & L(C) =>
    //    A |- B
    //    A |- L(C)
    m_context->addCondition(std::make_shared<Sequent>(_sequent->left(), pTail));

    m_context->m_cLastUsedRule = Context::SPLIT;
    return true;
}

bool Inference::strategy(const SequentPtr& _sequent) {
    if (split(_sequent))
        return true;

    const auto container = _sequent->left()->extractLogic();

    if (container.first && container.first->getKind() == Conjunct::LOGIC) {
        const StatementPtr& pStmt= container.first->as<LogicConjunct>()->getStatement();
        switch (pStmt->getKind()) {
            case Statement::IF:
                return ruleFLC(pStmt->as<If>(), container.second, _sequent->right());
            case Statement::BLOCK:
            case Statement::PARALLEL_BLOCK:
                return ruleFLSP(pStmt->as<Block>(), container.second, _sequent->right());
            case Statement::CALL:
                return ruleFLB(pStmt->as<Call>(), container.second, _sequent->right());
        }
    }

    if (_sequent->right()->size() > 1 || !_sequent->right()->hasLogic())
        return false;

    const auto& pRight = _sequent->right()->front();
    if (pRight->getKind() == Conjunct::LOGIC) {
        const StatementPtr& pStmt= pRight->as<LogicConjunct>()->getStatement();
        switch (pStmt->getKind()) {
            case Statement::IF:
                return ruleFC(pStmt->as<If>(), _sequent->left());
            case Statement::BLOCK:
                return ruleFS(pStmt->as<Block>(), _sequent->left());
            case Statement::PARALLEL_BLOCK:
                return ruleFP(pStmt->as<ParallelBlock>(), _sequent->left());
        }
    }

    if (pRight->getKind() != Conjunct::QUANTIFIER
        || pRight->as<QuantifierConjunct>()->getConjunct()->getKind() != Conjunct::LOGIC)
        return false;

    const ValuesSet& bound = pRight->as<QuantifierConjunct>()->getBound();
    const auto pStmt = pRight->as<QuantifierConjunct>()->getConjunct()->as<LogicConjunct>()->getStatement();

    switch (pStmt->getKind()) {
        case Statement::IF:
            return ruleEC(pStmt->as<If>(), bound, _sequent->left());
        case Statement::BLOCK:
            return ruleES(pStmt->as<Block>(), bound, _sequent->left());
        case Statement::PARALLEL_BLOCK:
            return ruleEP(pStmt->as<ParallelBlock>(), bound, _sequent->left());
        case Statement::CALL:
            return ruleEB(pStmt->as<Call>(), bound, _sequent->left());
    }

    return false;
}

bool Inference::strategy() {
    bool bIsInferred = false;

    bIsInferred |= m_context->releaseAssignments();

    if (!bIsInferred || !verifyVerbose())
        bIsInferred |= m_context->transferComplete();

    if (bIsInferred && verifyVerbose())
        return bIsInferred;

    for (std::list<std::pair<ConditionPtr, bool> >::iterator i = m_context->m_conditions.begin();
         i != m_context->m_conditions.end(); ++i) {
        bool bRuleApplied = false;
        switch ((*i).first->getKind()) {
            case Condition::CORRECTNESS:
                bRuleApplied |= strategy(i->first->as<Correctness>());
                break;
            case Condition::SEQUENT:
                bRuleApplied |= strategy(i->first->as<Sequent>());
                break;
        }

        bIsInferred |= bRuleApplied;

        if (bRuleApplied) {
            i = m_context->m_conditions.erase(i);
            if (verifyVerbose())
                break;
        }
    }

    return bIsInferred;
}

void Inference::run() {
    if (verifyVerbose()) {
        std::wcout << L"Start inference with:\n\n";
        pp::prettyPrint(*m_context, std::wcout);
        std::wcout << L"\n";
    }

    size_t cStep = 1;

    while (strategy()) {
        if (verifyVerbose()) {
            std::wcout << L"Step " << cStep++;
            if (m_context->m_cLastUsedRule != 0)
                std::wcout << L" (" << pp::fmtRule(m_context->m_cLastUsedRule) << ")";
            std::wcout << ":\n\n";
            pp::prettyPrint(*m_context, std::wcout);
            std::wcout << L"\n";
        }

        m_context->fixate();
    }

    if (verifyVerbose())
        std::wcout << (m_context->m_conditions.empty()
            ? L"Inference complete.\n" : L"Inference incomplete.\n");
}

void verify(const vf::ContextPtr& _context) {
    Inference(_context).run();
}

class PredicateTraverser : public Visitor {
public:
    PredicateTraverser() : m_pTheories(std::make_shared<Module>()), m_context(std::make_shared<Context>()) {}
    bool visitPredicate(const PredicatePtr& _pred) override;
    ModulePtr getTheories();

private:
    void declareLemmas(const ModulePtr& _pTheory);

    ModulePtr m_pTheories;
    std::set<FormulaDeclarationPtr> m_declaredFormulas;
    const ContextPtr m_context;
};

bool PredicateTraverser::visitPredicate(const PredicatePtr& _pred) {
    StatementPtr pNewBody = modifyStatement(_pred->getBlock());

    const ModulePtr pTheory = std::make_shared<Module>(_pred->getName());
    m_pTheories->getModules().add(pTheory);

    if (verifyVerbose()) {
        std::wcout << L"Predicate " << _pred->getName() << L".\n\n";
        std::wcout << L"Original statement:\n\n";
        pp::prettyPrintSyntax(_pred->getBlock(), std::wcout);
        std::wcout << L"\n\nSimplified to:\n\n";
        pp::prettyPrintSyntax(pNewBody, std::wcout);
        std::wcout << L"\n\n";
    }

    m_context->clear();
    m_context->m_pPredicate = _pred;
    m_context->m_conditions.push_back(std::make_pair(std::make_shared<Correctness>(pNewBody,
        Conjunction::getConjunction(makeCall(m_context->getPrecondition(_pred), _pred)),//TODO:dyp: fix
        Conjunction::getConjunction(makeCall(m_context->getPostcondition(_pred), _pred))), true));//TODO:dyp: fix

    verify(m_context);

    declareLemmas(pTheory);

    if (verifyFormulas()) {
        std::wcout << L"Formulas for " << _pred->getName() << L":\n";
        for (auto i: m_context->m_lemmas)
            pp::prettyPrintSyntax(i.first, std::wcout, nullptr, true);
    }

    return true;
}

void PredicateTraverser::declareLemmas(const ModulePtr& _pTheory) {
    std::set<FormulaDeclarationPtr> formulas, diff;

    for (auto i: m_context->m_lemmas)
        declareLemma(i.first, formulas, _pTheory);

    std::set_difference(formulas.begin(), formulas.end(), m_declaredFormulas.begin(),
        m_declaredFormulas.end(), std::inserter(diff, diff.begin()));

    if (!diff.empty())
        _pTheory->getImports().push_back(L"Formulas");

    m_declaredFormulas.insert(formulas.begin(), formulas.end());
}

ModulePtr PredicateTraverser::getTheories() {
    ModulePtr
        pTheories = std::make_shared<Module>(),
        pFormulas = std::make_shared<Module>(L"Formulas");

    pFormulas->getFormulas().insert(pFormulas->getFormulas().begin(),
        m_declaredFormulas.begin(), m_declaredFormulas.end());

    pTheories->getModules().assign(m_pTheories->getModules());
    pTheories->getModules().prepend(pFormulas);

    return pTheories;
}

ModulePtr verify(const ModulePtr &_pModule) {
    PredicateTraverser traverser;
    traverser.traverseNode(_pModule);
    return traverser.getTheories();
}

}
