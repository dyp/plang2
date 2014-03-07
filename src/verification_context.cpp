/// \file verification_context.cpp
///

#include "ir/visitor.h"
#include "generate_semantics.h"
#include "node_analysis.h"
#include "term_rewriting.h"

using namespace ir;
using namespace na;
using namespace tr;

namespace vf {

static ConjunctPtr _releaseAssignment(const ConjunctPtr& _pConj) {
    switch (_pConj->getKind()) {
        case Conjunct::FORMULA:
            return NULL;

        case Conjunct::QUANTIFIER: {
            const QuantifierConjunctPtr& pConj = _pConj.as<QuantifierConjunct>();
            const ConjunctPtr pSub = _releaseAssignment(pConj->getConjunct());
            return (pSub ? new QuantifierConjunct(pConj->getBound(), pSub) : NULL);
        }

        case Conjunct::LOGIC: {
            if (_pConj.as<LogicConjunct>()->getStatement()->getKind() != Statement::ASSIGNMENT)
                return NULL;
            const Assignment &assignment = *_pConj.as<LogicConjunct>()->getStatement().as<Assignment>();
            // R(x) && a = E(x)
            return new FormulaConjunct(new Binary(Binary::EQUALS, assignment.getLValue(), assignment.getExpression()));
        }
    }

    assert(false && "Unreachable");
    return nullptr;
}

ExpressionPtr QuantifierConjunct::mergeToExpression() const {
    FormulaPtr pExpression;
    pExpression = new Formula(Formula::EXISTENTIAL, m_pConjunct->mergeToExpression());
    pExpression->getBoundVariables().prepend(m_bound.begin(), m_bound.end());
    return pExpression;
}

void Conjunction::getFreeValues(ValuesSet& _container) const {
    for (std::set<ConjunctPtr>::const_iterator i = m_conjuncts.begin(); i != m_conjuncts.end(); ++i)
        (*i)->getFreeValues(_container);
}

bool Conjunction::hasLogic() const {
    for (std::set<ConjunctPtr>::const_iterator i = m_conjuncts.begin(); i != m_conjuncts.end(); ++i)
        if ((*i)->hasLogic())
            return true;
    return false;
}

ExpressionPtr Conjunction::mergeToExpression() const {
    if (hasLogic())
        return NULL;
    if (m_conjuncts.empty())
        return new Literal(false);
    if (m_conjuncts.size() == 1)
        return front()->mergeToExpression();

    BinaryPtr
        pExpression = new Binary(Binary::BOOL_AND),
        pCurrent = pExpression;

    std::set<ConjunctPtr>::const_iterator i = m_conjuncts.begin();
    while (1) {
        pCurrent->setLeftSide((*i)->mergeToExpression());
        if (::next(++i) == m_conjuncts.end()) {
            pCurrent->setRightSide((*i)->mergeToExpression());
            break;
        }
        pCurrent->setRightSide(new Binary(Binary::BOOL_AND));
        pCurrent = pCurrent->getRightSide().as<Binary>();
    }

    return pExpression;
}

static void _divide(const ExpressionPtr& _pExpr, std::set<ExpressionPtr>& _container, int _nOperator = Binary::BOOL_AND) {
    if (_pExpr->getKind() != Expression::BINARY) {
        _container.insert(_pExpr);
        return;
    }

    const Binary& bin = *_pExpr.as<Binary>();
    if (bin.getOperator() != _nOperator) {
        _container.insert(_pExpr);
        return;
    }

    _divide(bin.getLeftSide(), _container, _nOperator);
    _divide(bin.getRightSide(), _container, _nOperator);
}

ConjunctionPtr Conjunction::getConjunction(const ExpressionPtr& _pExpr, bool _bExpandCalls) {
    if (!_pExpr)
        return new Conjunction();

    if (_pExpr->getKind() == Expression::FORMULA_CALL && _bExpandCalls) {
        const FormulaCall &_fCall = *_pExpr.as<FormulaCall>();
        ExpressionPtr pFormulaBody = _fCall.getTarget()->getFormula();
        for (size_t i = 0; i < _fCall.getArgs().size(); ++i)
            pFormulaBody = Expression::substitute(pFormulaBody,
                new VariableReference(_fCall.getTarget()->getParams().get(i)), _fCall.getArgs().get(i)).as<Expression>();
        return getConjunction(pFormulaBody, false);
    }

    ConjunctionPtr pResult = new Conjunction(new FormulaConjunct(_pExpr));
    pResult->_normalize();

    return pResult;
}

template <typename T>
static bool _doesIntersect(const std::set<T>& _first, const std::set<T>& _second) {
    return std::find_first_of(_first.begin(), _first.end(), _second.begin(), _second.end()) != _first.end();
}

bool Conjunction::_split(const ValuesSet& _leftValues, Conjunction& _left, const ValuesSet& _rightValues, Conjunction& _right,
    const std::set<FormulaDeclarationPtr>& _traversedFormulas)
{
    for (std::set<ConjunctPtr>::iterator i = getConjuncts().begin(); i != getConjuncts().end(); ++i) {
        std::set<FormulaDeclarationPtr> traversedFormulas = _traversedFormulas;

        ValuesSet values;
        (*i)->getFreeValues(values);

        const bool
            bIsLeft = _doesIntersect(values, _leftValues),
            bIsRight = _doesIntersect(values, _rightValues);

        if (!bIsLeft || !bIsRight) {
            if (!bIsLeft)
                _right.addConjunct(*i);
            if (!bIsRight)
                _left.addConjunct(*i);
            continue;
        }

        if ((*i)->getKind() != Conjunct::FORMULA)
            return false;

        const FormulaConjunct& formula = *(*i).as<FormulaConjunct>();
        if (formula.getExpression()->getKind() != Expression::FORMULA_CALL
            || !traversedFormulas.insert(formula.getExpression().as<FormulaCall>()->getTarget()).second)
            return false;

        ConjunctionPtr pConjunction = getConjunction(formula.getExpression(), true);

        if (!pConjunction)
            return false;
        if (pConjunction->getConjuncts().size() == 1
            && pConjunction->front().as<FormulaConjunct>()->getExpression() == formula.getExpression())
            return false;
        if (!pConjunction->_split(_leftValues, _left, _rightValues, _right, traversedFormulas))
            return false;
    }

    return true;
}

bool Conjunction::split(const ValuesSet& _leftValues, Conjunction& _left, const ValuesSet& _rightValues, Conjunction& _right) {
    std::set<FormulaDeclarationPtr> traversedFormulas;
    return _split(_leftValues, _left, _rightValues, _right, traversedFormulas);
}

void Conjunction::_negate(const ConjunctPtr& _pConjunct, Conjunction& _result) {
    if (_pConjunct->getKind() == Conjunct::LOGIC)
        return;

    if (_pConjunct->getKind() == Conjunct::QUANTIFIER) {
        _result.addExpression(new Unary(Unary::BOOL_NEGATE, _pConjunct->mergeToExpression()));
        return;
    }

    std::set<ExpressionPtr> parts;

    _divide(_pConjunct.as<FormulaConjunct>()->getExpression(), parts, Binary::BOOL_OR);
    if (parts.size() > 1) {
        for (std::set<ExpressionPtr>::iterator i = parts.begin(); i != parts.end(); ++i) {
            Conjunction result;
            _negate(new FormulaConjunct(*i), result);
            _result.append(result);
        }
        return;
    }

    _divide(_pConjunct.as<FormulaConjunct>()->getExpression(), parts, Binary::BOOL_AND);
    if (parts.size() > 1) {
        Conjunction conj;

        for (std::set<ExpressionPtr>::iterator i = parts.begin(); i != parts.end(); ++i) {
            Conjunction result;
            _negate(new FormulaConjunct(*i), result);
            conj.disjunct(&result);
        }

        _result.append(conj);
        return;
    }

    _result.addExpression(new Unary(Unary::BOOL_NEGATE, _pConjunct->mergeToExpression()));
}

void Conjunction::negate() {
    Conjunction container;

    for (std::set<ConjunctPtr>::iterator i = m_conjuncts.begin(); i != m_conjuncts.end(); ++i) {
        Conjunction negation;
        _negate(*i, negation);
        container.disjunct(&negation);
    }

    m_conjuncts.swap(container.getConjuncts());
}

void Conjunction::disjunct(const ConjunctionPtr& _pOther) {
    if (!_pOther)
        return;
    assign(_disjunct(this, _pOther));
}

bool Conjunction::releaseAssignments() {
    bool bIsReleased = false;
    while (_releaseFirstAssignment())
        bIsReleased = true;
    return bIsReleased;
}

std::pair<ConjunctPtr, ConjunctionPtr> Conjunction::extractLogic() const {
    if (!hasLogic())
        return std::make_pair(ConjunctPtr(NULL), this);

    ConjunctionPtr pTail = new Conjunction();
    pTail->assign(this);

    for (std::set<ConjunctPtr>::iterator i = pTail->getConjuncts().begin();
        i != pTail->getConjuncts().end(); ++i)
        if ((*i)->hasLogic()) {
            ConjunctPtr pLogic = *i;
            pTail->getConjuncts().erase(i);
            return std::make_pair(pLogic, pTail);
        }

    return std::make_pair(ConjunctPtr(NULL), this);
}

void Conjunction::_normalize(const ConjunctPtr& _pConjunct, Conjunction& _result) {
    if (_pConjunct->getKind() != Conjunct::FORMULA) {
        _result.addConjunct(_pConjunct);
        return;
    }

    std::set<ExpressionPtr> parts;

    _divide(_pConjunct.as<FormulaConjunct>()->getExpression(), parts, Binary::BOOL_AND);
    if (parts.size() > 1) {
        for (std::set<ExpressionPtr>::iterator i = parts.begin(); i != parts.end(); ++i) {
            Conjunction result;
            _normalize(new FormulaConjunct(*i), result);
            _result.append(result);
        }
        return;
    }

    parts.clear();

    _divide(_pConjunct.as<FormulaConjunct>()->getExpression(), parts, Binary::BOOL_OR);
    if (parts.size() > 1) {
        Conjunction conj;

        for (std::set<ExpressionPtr>::iterator i = parts.begin(); i != parts.end(); ++i) {
            Conjunction result;
            _normalize(new FormulaConjunct(*i), result);
            conj.disjunct(&result);
        }

        _result.append(conj);
        return;
    }

    _result.addConjunct(_pConjunct);
}

void Conjunction::_normalize() {
    Conjunction container;
    for (std::set<ConjunctPtr>::iterator i = m_conjuncts.begin(); i != m_conjuncts.end(); ++i)
        _normalize(*i, container);
    m_conjuncts.swap(container.getConjuncts());
}

ConjunctionPtr Conjunction::_disjunct(const ConjunctionPtr& _pLeft, const ConjunctionPtr& _pRight) {
    if (!_pLeft || !_pRight)
        return nullptr;
    if (_pLeft->empty() && _pRight->empty())
        return new Conjunction();
    if (_pLeft->empty() || _pRight->empty())
        return _pLeft->empty() ? _pRight : _pLeft;

    ConjunctionPtr pResult = new Conjunction();
    for (auto i: _pLeft->getConjuncts())
        for (auto j: _pRight->getConjuncts())
            pResult->addExpression(new Binary(Binary::BOOL_OR,
                i->mergeToExpression(), j->mergeToExpression()));

    return pResult;
}

bool Conjunction::_releaseFirstAssignment() {
    std::set<ConjunctPtr>::iterator i;
    for (i = m_conjuncts.begin(); i != m_conjuncts.end(); ++i)
        if ((*i)->hasLogic())
            break;

    if (i == m_conjuncts.end())
        return false;

    ConjunctPtr pConj = _releaseAssignment(*i);
    if (!pConj)
        return false;

    m_conjuncts.erase(i);
    m_conjuncts.insert(pConj);

    return true;
}

bool Sequent::releaseAssignments() {
    const bool
        bLeft = m_pLeft && m_pLeft->releaseAssignments(),
        bRight = m_pRight && m_pRight->releaseAssignments();
    return bLeft || bRight;
}

ExpressionPtr Sequent::mergeToExpression() const {
    if (hasLogic())
        return NULL;
    if (!left() && !right())
        return new Literal(true);
    if (!left())
        return right()->mergeToExpression();
    if (!right())
        return new Unary(Unary::BOOL_NEGATE, left()->mergeToExpression());
    return new Binary(Binary::IMPLIES, left()->mergeToExpression(), right()->mergeToExpression());
}

void Correctness::makeSequent(std::list<SequentPtr>& _container) const {
    ConjunctionPtr pPre = new Conjunction();
    pPre->assign(m_pPre);
    pPre->addConjunct(new LogicConjunct(m_pStmt));

    // P(x) && L(S(x: y)) |- Q(x, y)
    _container.push_back(new Sequent(pPre, m_pPost));

    ValuesSet results;
    getResults(m_pStmt, results);

    // P(x) |- exists y. L(S(x: y))
    _container.push_back(new Sequent(m_pPre,
        new Conjunction(new QuantifierConjunct(results, new LogicConjunct(m_pStmt)))));
}

void Correctness::makeSequent(std::list<ConditionPtr>& _container) const {
    std::list<SequentPtr> container;
    makeSequent(container);
    _container.insert(_container.begin(), container.begin(), container.end());
}

void Context::fixate() {
    for (std::list<std::pair<ConditionPtr, bool> >::iterator i = m_conditions.begin();
        i != m_conditions.end(); ++i)
        i->second = false;
    for (std::list<std::pair<ir::ExpressionPtr, bool> >::iterator i = m_lemmas.begin();
        i != m_lemmas.end(); ++i)
        i->second = false;
    m_cLastUsedRule = 0;
}

void Context::addConditions(const std::list<ConditionPtr>& _conditions) {
    for (std::list<ConditionPtr>::const_iterator i = _conditions.begin();
        i != _conditions.end(); ++i)
        m_conditions.push_back(std::make_pair(*i, true));
}

void Context::clear() {
    m_conditions.clear();
    m_lemmas.clear();
}

bool Context::transferComplete() {
    bool bIsTransferred = false;

    for (std::list<std::pair<ConditionPtr, bool> >::iterator i = m_conditions.begin();
        i != m_conditions.end();) {
        if (i->first->getKind() == Condition::CORRECTNESS) {
            ++i;
            continue;
        }

        const Sequent& sequent = *i->first.as<Sequent>();
        if (sequent.hasLogic()) {
            ++i;
            continue;
        }

        const ExpressionPtr& pNewLemma = generalize(sequent.mergeToExpression());
        m_lemmas.push_back(std::make_pair(pNewLemma, true));
        i = m_conditions.erase(i);
        bIsTransferred = true;
    }

    if (bIsTransferred)
        m_cLastUsedRule = TRANSFER;

    return bIsTransferred;
}

bool Context::releaseAssignments() {
    bool bIsReleased = false;

    for (std::list<std::pair<ConditionPtr, bool> >::iterator i = m_conditions.begin();
        i != m_conditions.end();) {
        switch (i->first->getKind()) {
            case Condition::CORRECTNESS: {
                const Correctness& corr = *i->first.as<Correctness>();
                if (corr.getStatement()->getKind() != Statement::ASSIGNMENT) {
                    ++i;
                    continue;
                }

                std::list<ConditionPtr> container;
                corr.makeSequent(container);
                addConditions(container);

                i = m_conditions.erase(i);

                bIsReleased = true;
                break;
            }

            case Condition::SEQUENT: {
                Sequent &seq = *i->first.as<Sequent>();
                bIsReleased |= seq.releaseAssignments();
                ++i;
                break;
            }
        }
    }

    if (bIsReleased)
        m_cLastUsedRule = SIMPLIFICATION;

    return bIsReleased;
}

FormulaDeclarationPtr Context::getFormula(std::map<PredicatePtr, std::vector<FormulaDeclarationPtr> >& _map,
    const Predicate& _pred, const ExpressionPtr& _pExpr, const std::wstring& _strPrefix, size_t _cBranch)
{
    std::map<PredicatePtr, std::vector<FormulaDeclarationPtr> >::iterator it = _map.find(&_pred);
    if (it == _map.end()) {
        _map.insert(make_pair(&_pred, std::vector<FormulaDeclarationPtr>()));
        _map[&_pred].insert(_map[&_pred].begin(), _pred.getOutParams().size() + 1, NULL);
    }

    if (_map[&_pred][_cBranch])
        return _map[&_pred][_cBranch];

    if (_pred.getOutParams().size() <= _cBranch)
        return NULL;

    std::wstring strName, strBranchName;
    if (_pred.getOutParams().size() > 1 && _cBranch != 0) {
        const LabelPtr& pLabel = _pred.getOutParams().get(_cBranch - 1)->getLabel();
        strBranchName = L"_" + (!pLabel || pLabel->getName().empty()
            ? strWiden(intToStr(_cBranch - 1))
            : pLabel->getName());
    }

    strName = _strPrefix + _pred.getName() + strBranchName;

    FormulaDeclarationPtr
        pDecl = declareFormula(strName, _pred, *_pExpr);

    if (_pred.getOutParams().size() <= 1)
        _map[&_pred].insert(_map[&_pred].begin(), _pred.getOutParams().size() + 1, pDecl);
    else
        _map[&_pred][_cBranch] = pDecl;

    return pDecl;
}

FormulaDeclarationPtr Context::getFormula(std::map<PredicateTypePtr, std::vector<FormulaDeclarationPtr> >& _map,
    const PredicateType& _pred, const ExpressionPtr& _pExpr, const std::wstring& _strPrefix, size_t _cBranch)
{
    std::map<PredicateTypePtr, std::vector<FormulaDeclarationPtr> >::iterator it = _map.find(&_pred);
    if (it == _map.end()) {
        _map.insert(make_pair(&_pred, std::vector<FormulaDeclarationPtr>()));
        _map[&_pred].insert(_map[&_pred].begin(), _pred.getOutParams().size() + 1, NULL);
    }

    if (_map[&_pred][_cBranch])
        return _map[&_pred][_cBranch];

    if (_pred.getOutParams().size() <= _cBranch)
        return NULL;

    std::wstring strName, strBranchName;
    if (_pred.getOutParams().size() > 1 && _cBranch != 0) {
        const LabelPtr& pLabel = _pred.getOutParams().get(_cBranch - 1)->getLabel();
        strBranchName = L"_" + (!pLabel || pLabel->getName().empty()
            ? strWiden(intToStr(_cBranch - 1))
            : pLabel->getName());
    }

    int cIndex = 1;
    while (!m_usedNames.insert(strName = _strPrefix + strWiden(intToStr(cIndex++)) + strBranchName).second);

    NamedValues params;
    getPredicateParams(_pred, params);

    FormulaDeclarationPtr
        pDecl = declareFormula(strName, _pExpr, params);

    if (_pred.getOutParams().size() <= 1)
        _map[&_pred].insert(_map[&_pred].begin(), _pred.getOutParams().size() + 1, pDecl);
    else
        _map[&_pred][_cBranch] = pDecl;

    return pDecl;
}

FormulaDeclarationPtr Context::getPrecondition(const Predicate& _pred, size_t _nBranch) {
    const ExpressionPtr& pExpr = _pred.getOutParams().size() != 1 && _nBranch != 0
        ? _pred.getOutParams().get(_nBranch-1)->getPreCondition()
        : _pred.getPreCondition();
    return getFormula(m_preCondMap, _pred, pExpr, L"P_", _nBranch);
}

FormulaDeclarationPtr Context::getPostcondition(const Predicate& _pred, size_t _nBranch) {
    const ExpressionPtr& pExpr = _pred.getOutParams().size() != 1 && _nBranch != 0
        ? _pred.getOutParams().get(_nBranch-1)->getPostCondition()
        : _pred.getPostCondition();
    return getFormula(m_postCondMap, _pred, pExpr, L"Q_", _nBranch);
}

FormulaDeclarationPtr Context::getMeasure(const Predicate& _pred) {
    return getFormula(m_measureMap, _pred, _pred.getMeasure(), L"m_", 0);
}

FormulaDeclarationPtr Context::getPrecondition(const PredicateType& _pred, size_t _nBranch) {
    const ExpressionPtr& pExpr = _pred.getOutParams().size() != 1 && _nBranch != 0
        ? _pred.getOutParams().get(_nBranch)->getPreCondition()
        : _pred.getPreCondition();
    return getFormula(m_preCondTypeMap, _pred, pExpr, L"P_", _nBranch);
}

FormulaDeclarationPtr Context::getPostcondition(const PredicateType& _pred, size_t _nBranch) {
    const ExpressionPtr& pExpr = _pred.getOutParams().size() != 1 && _nBranch != 0
        ? _pred.getOutParams().get(_nBranch)->getPostCondition()
        : _pred.getPostCondition();
    return getFormula(m_postCondTypeMap, _pred, pExpr, L"Q_", _nBranch);
}

ir::FormulaDeclarationPtr Context::getPrecondition(const ir::Call& _call, size_t _nBranch) {
    if (_call.getPredicate()->getKind() == Expression::PREDICATE)
        return getPrecondition(*_call.getPredicate().as<PredicateReference>()->getTarget(), _nBranch);
    else
        return getPrecondition(*_call.getPredicate()->getType().as<PredicateType>(), _nBranch);
}

ir::FormulaDeclarationPtr Context::getPostcondition(const ir::Call& _call, size_t _nBranch) {
    if (_call.getPredicate()->getKind() == Expression::PREDICATE)
        return getPostcondition(*_call.getPredicate().as<PredicateReference>()->getTarget(), _nBranch);
    else
        return getPostcondition(*_call.getPredicate()->getType().as<PredicateType>(), _nBranch);
}

ir::FormulaDeclarationPtr Context::getMeasure(const ir::Call& _call) {
    if (_call.getPredicate()->getKind() == Expression::PREDICATE)
        return getMeasure(*_call.getPredicate().as<PredicateReference>()->getTarget());
    else
        return NULL;
}

}
