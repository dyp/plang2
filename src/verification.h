/// \file verification.h
///

#ifndef VERIFICATION_H_
#define VERIFICATION_H_

#include "ir/base.h"
#include "utils.h"
#include "node_analysis.h"

namespace vf {

class Conjunct : public std::enable_shared_from_this<Conjunct>{
public:
    enum {
        LOGIC,
        FORMULA,
        QUANTIFIER,
    };
    Conjunct() {}
    virtual ~Conjunct() {}

    virtual int getKind() const = 0;
    virtual bool hasLogic() const = 0;
    virtual ir::ExpressionPtr mergeToExpression() const = 0;
    virtual void getFreeValues(na::ValuesSet& _container) const = 0;

    template <class _Class>
    std::shared_ptr<_Class> as() {
        return std::static_pointer_cast<_Class>(shared_from_this());
    }

    template <class _Class>
    std::shared_ptr<const _Class> as() const {
        return std::static_pointer_cast<_Class>(shared_from_this());
    }
};
using ConjunctPtr = std::shared_ptr<Conjunct>;

class LogicConjunct : public Conjunct {
public:
    LogicConjunct(const ir::StatementPtr& _pStmt) :
        m_pStatement(_pStmt)
    {}

    virtual int getKind() const { return LOGIC; }
    virtual bool hasLogic() const { return true; }
    virtual ir::ExpressionPtr mergeToExpression() const { return NULL; }
    virtual void getFreeValues(na::ValuesSet& _container) const { na::getParameters(m_pStatement, _container); }

    const ir::StatementPtr& getStatement() const { return m_pStatement; }
    void setStatement(const ir::StatementPtr& _pStmt) { m_pStatement = _pStmt; }

private:
    ir::StatementPtr m_pStatement;
};

using LogicConjunctPtr = std::shared_ptr<LogicConjunct>;

class FormulaConjunct : public Conjunct {
public:
    FormulaConjunct(const ir::ExpressionPtr& _pExpr) :
        m_pExpression(_pExpr)
    {}

    virtual int getKind() const { return FORMULA; }
    virtual bool hasLogic() const { return false; }
    virtual ir::ExpressionPtr mergeToExpression() const { return Cloner().get(m_pExpression, true); }
    virtual void getFreeValues(na::ValuesSet& _container) const { na::collectValues(m_pExpression, _container); }

    const ir::ExpressionPtr& getExpression() const { return m_pExpression; }
    void setExpression(const ir::ExpressionPtr& _pExpr) { m_pExpression = _pExpr; }

private:
    ir::ExpressionPtr m_pExpression;
};
using FormulaConjunctPtr = std::shared_ptr<FormulaConjunct>;

class QuantifierConjunct : public Conjunct {
public:
    QuantifierConjunct(const ConjunctPtr& _pConjunct = NULL) :
        m_pConjunct(_pConjunct)
    {}
    QuantifierConjunct(const ir::NamedValuesPtr& _pBound, const ConjunctPtr& _pConjunct = NULL) :
        m_bound(_pBound->begin(), _pBound->end()), m_pConjunct(_pConjunct)
    {}
    QuantifierConjunct(const na::ValuesSet& _bound, const ConjunctPtr& _pConjunct = NULL) :
        m_bound(_bound.begin(), _bound.end()), m_pConjunct(_pConjunct)
    {}

    virtual int getKind() const { return QUANTIFIER; }
    virtual bool hasLogic() const { return m_pConjunct->hasLogic(); }
    virtual void getFreeValues(na::ValuesSet& _container) const { m_pConjunct->getFreeValues(_container); }
    virtual ir::ExpressionPtr mergeToExpression() const;

    const ConjunctPtr& getConjunct() const { return m_pConjunct; }
    void setConjunct(const ConjunctPtr& _pConjunct = NULL) { m_pConjunct = _pConjunct; }
    const na::ValuesSet& getBound() const { return m_bound; }
    void setBound(const na::ValuesSet& _bound) { m_bound = na::ValuesSet(_bound.begin(), _bound.end()); }

private:
    na::ValuesSet m_bound;
    ConjunctPtr m_pConjunct;
};

using QuantifierConjunctPtr = std::shared_ptr<QuantifierConjunct>;

using ConjunctionPtr = std::shared_ptr<class Conjunction>;

class Conjunction : public std::enable_shared_from_this<Conjunction> {
public:
    Conjunction() {}
    Conjunction(const ConjunctPtr& _pConjunct) { m_conjuncts.insert(_pConjunct); }

    typedef std::set<ConjunctPtr> Conjuncts;

    Conjuncts& getConjuncts() { return m_conjuncts; }
    const Conjuncts& getConjuncts() const { return m_conjuncts; }

    const ConjunctPtr& front() const { return *(m_conjuncts.begin()); }
    const ConjunctPtr& back() const { return *(::prev(m_conjuncts.end())); }

    bool empty() const { return m_conjuncts.empty(); }
    size_t size() const { return m_conjuncts.size(); }
    void clear() { m_conjuncts.clear(); }

    void addConjunct(const ConjunctPtr& _pConjunct) { getConjuncts().insert(_pConjunct); }
    void addExpression(const ir::ExpressionPtr& _pExpr) { getConjuncts().insert(std::make_shared<FormulaConjunct>(_pExpr)); }

    void assign(const Conjunction& _conjunction) { m_conjuncts = _conjunction.m_conjuncts; }
    void assign(const ConjunctionPtr& _pConjunction) { m_conjuncts = _pConjunction->m_conjuncts; }

    void append(const Conjunction& _conjunction)
        { getConjuncts().insert(_conjunction.getConjuncts().begin(), _conjunction.getConjuncts().end()); }
    void append(const ConjunctionPtr& _pConjunction)
        { if (_pConjunction) append(*_pConjunction); }

    bool hasLogic() const;
    void getFreeValues(na::ValuesSet& _container) const;
    ir::ExpressionPtr mergeToExpression() const;

    static ConjunctionPtr getConjunction(const ir::ExpressionPtr& _pExpr, bool _bExpandCalls = false);
    bool split(const na::ValuesSet& _leftValues, Conjunction& _left, const na::ValuesSet& _rightValues, Conjunction& _right);

    void negate();
    void disjunct(const ConjunctionPtr& _pOther);

    static ConjunctionPtr implies(const ConjunctionPtr& _pLeft, const ConjunctionPtr& _pRight);
    void implies(const ConjunctionPtr& _pOther);

    bool releaseAssignments();
    std::pair<ConjunctPtr, ConjunctionPtr> extractLogic();

private:
    Conjuncts m_conjuncts;

    bool _split(const na::ValuesSet& _leftValues, Conjunction& _left, const na::ValuesSet& _rightValues, Conjunction& _right,
        const std::set<ir::FormulaDeclarationPtr>& _traversedFormulas);
    bool _releaseFirstAssignment();
    void _normalize();

    static ConjunctionPtr _negate(const ConjunctPtr& _pConjunct);
    static void _normalize(const ConjunctPtr& _pConjunct, const ConjunctionPtr& _result);
    static ConjunctionPtr _disjunct(const ConjunctionPtr& _pLeft, const ConjunctionPtr& _pRight);
    static ConjunctionPtr _implies(const ConjunctionPtr& _pLeft, const ConjunctionPtr& _pRight);
};

class Condition : public std::enable_shared_from_this<Condition> {
public:
    enum {
        SEQUENT,
        CORRECTNESS,
    };
    Condition() {}
    virtual int getKind() const = 0;

    template <class _Class>
    std::shared_ptr<_Class> as() {
        return std::static_pointer_cast<_Class>(shared_from_this());
    }

    template <class _Class>
    std::shared_ptr<const _Class> as() const {
        return std::static_pointer_cast<_Class>(shared_from_this());
    }
};

using ConditionPtr = std::shared_ptr<Condition>;

class Sequent : public Condition {
public:
    Sequent() :
        m_pLeft(std::make_shared<Conjunction>()), m_pRight(std::make_shared<Conjunction>())
    {}
    Sequent(const ConjunctionPtr& _pLeft, const ConjunctionPtr& _pRigth) :
        m_pLeft(!_pLeft ? std::make_shared<Conjunction>() : _pLeft),
        m_pRight(!_pRigth ? std::make_shared<Conjunction>() : _pRigth)
    {}

    virtual int getKind() const { return SEQUENT; }
    const ConjunctionPtr& left() const { return m_pLeft; }
    const ConjunctionPtr& right() const { return m_pRight; }

    bool hasLogic() const
        { return (m_pLeft && m_pLeft->hasLogic()) || (m_pRight && m_pRight->hasLogic()); }
    bool releaseAssignments();
    ir::ExpressionPtr mergeToExpression() const;

private:
    ConjunctionPtr m_pLeft, m_pRight;
};
using SequentPtr = std::shared_ptr<Sequent>;

class Correctness : public Condition {
public:
    Correctness() :
        m_pStmt(NULL), m_pPre(NULL), m_pPost(NULL)
    {}
    Correctness(const ir::StatementPtr& _pStmt,
        const ConjunctionPtr& _pPre, const ConjunctionPtr& _pPost) :
        m_pStmt(_pStmt), m_pPre(_pPre), m_pPost(_pPost)
    {}

    virtual int getKind() const { return CORRECTNESS; }

    const ConjunctionPtr& getPrecondition() const { return m_pPre; }
    const ConjunctionPtr& getPostcondition() const { return m_pPost; }
    const ir::StatementPtr& getStatement() const { return m_pStmt; }

    void makeSequent(std::list<SequentPtr>& _container) const;
    void makeSequent(std::list<ConditionPtr>& _container) const;

private:
    ir::StatementPtr m_pStmt;
    ConjunctionPtr m_pPre, m_pPost;
};

using CorrectnessPtr = std::shared_ptr<Correctness>;

struct Context {
    // Rules
    enum {
        TRANSFER = 1,
        SIMPLIFICATION,
        SPLIT,

        // R system
        RP, RS, RC, RB,
        // Q system
        QP, QSB, QS, QC,
        // F system
        FP, FS, FC,
        // E system
        EP, ES, EC, EB,
        // FL system
        FLS, FLP, FLC, FLB
    };

    std::list<std::pair<ConditionPtr, bool> > m_conditions;
    std::list<std::pair<ir::ExpressionPtr, bool> > m_lemmas;

    std::map<ir::PredicatePtr, std::vector<ir::FormulaDeclarationPtr> > m_preCondMap, m_postCondMap, m_measureMap;
    std::map<ir::PredicateTypePtr, std::vector<ir::FormulaDeclarationPtr> > m_preCondTypeMap, m_postCondTypeMap;
    std::set<std::wstring> m_usedNames;

    ir::PredicatePtr m_pPredicate;
    size_t m_cLastUsedRule = 0;

    int m_nPreCondType = 0, m_nPostCondType = 0;

    void fixate();

    void addCondition(const ConditionPtr& _pCond) { m_conditions.push_back(std::make_pair(_pCond, true)); }
    void addLemma(const ir::ExpressionPtr& _pExpr) { m_lemmas.push_back(std::make_pair(_pExpr, true)); }
    void addConditions(const std::list<ConditionPtr>& _conditions);

    void clear();

    bool transferComplete();
    bool releaseAssignments();

    ir::FormulaDeclarationPtr getFormula(std::map<ir::PredicatePtr, std::vector<ir::FormulaDeclarationPtr> >& _map,
        const ir::PredicatePtr& _pred, const ir::ExpressionPtr& _pExpr, const std::wstring& _sPrefix, size_t _nBranch);
    ir::FormulaDeclarationPtr getFormula(std::map<ir::PredicateTypePtr, std::vector<ir::FormulaDeclarationPtr> >& _map,
        const ir::PredicateTypePtr& _pred, const ir::ExpressionPtr& _pExpr, const std::wstring& _sPrefix, size_t _nBranch);

    ir::FormulaDeclarationPtr getPrecondition(const ir::PredicatePtr& _pred, size_t _nBranch = 0);
    ir::FormulaDeclarationPtr getPostcondition(const ir::PredicatePtr& _pred, size_t _nBranch = 0);
    ir::FormulaDeclarationPtr getMeasure(const ir::PredicatePtr& _pred);

    ir::FormulaDeclarationPtr getPrecondition(const ir::PredicateTypePtr& _pred, size_t _nBranch = 0);
    ir::FormulaDeclarationPtr getPostcondition(const ir::PredicateTypePtr& _pred, size_t _nBranch = 0);

    ir::FormulaDeclarationPtr getPrecondition(const ir::CallPtr& _call, size_t _nBranch = 0);
    ir::FormulaDeclarationPtr getPostcondition(const ir::CallPtr& _call, size_t _nBranch = 0);
    ir::FormulaDeclarationPtr getMeasure(const ir::CallPtr& _call);

};

using ContextPtr = std::shared_ptr<Context>;

ir::ModulePtr verify(const ir::ModulePtr &_pModule);

}

#endif /* VERIFICATION_H_ */
