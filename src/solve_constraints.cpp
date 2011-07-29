/// \file solve_constraints.cpp
///

#include <iostream>
#include "typecheck.h"
#include "prettyprinter.h"
#include "utils.h"
#include "options.h"

using namespace ir;

typedef std::map<tc::FreshTypePtr, TypePtr> TypeMap;
typedef std::multimap<tc::FreshTypePtr, TypePtr> TypeMultiMap;
typedef tc::ContextStack CS;

class Solver {
public:
    bool unify(bool _bCompound = false);
    bool lift();
    bool run();
    bool sequence(int &_result);
    bool eval(int &_result);
    bool prune();
    bool refute(int &_result);
    bool compact();
    bool infer();
    bool inferCompound();
    bool expand(int &_result);
    bool guess();
    bool fork();


    tc::Context &context() { return *CS::top(); }
protected:
    bool refute(tc::Formula &_f);
    bool expandPredicate(int _kind, const PredicateTypePtr &_pLhs,
            const PredicateTypePtr &_pRhs, tc::FormulaList &_formulas);
    bool expandStruct(int _kind, const StructTypePtr &_pLhs, const StructTypePtr &_pRhs, tc::FormulaList &_formulas);
    bool expandSet(int _kind, const SetTypePtr &_pLhs, const SetTypePtr &_pRhs, tc::FormulaList &_formulas);
    bool expandList(int _kind, const ListTypePtr &_pLhs, const ListTypePtr &_pRhs, tc::FormulaList &_formulas);
    bool expandMap(int _kind, const MapTypePtr &_pLhs, const MapTypePtr &_pRhs, tc::FormulaList &_formulas);
    bool expandType(int _kind, const TypeTypePtr &_pLhs, const TypeTypePtr &_pRhs, tc::FormulaList &_formulas);

    // .first is X < A (lower limit), .second is  A < X (upper limit).
    typedef std::pair<tc::Formula, tc::Formula> Limits;
    typedef std::map<size_t, Limits> LimitMap;

    void collectLimits(tc::Formulas &_formulas, LimitMap &_limits);
};

bool Solver::fork() {
    tc::Formulas::iterator iCF = context()->beginCompound();

    if (iCF == context()->end())
        return false;

    tc::CompoundFormulaPtr pCF = iCF->as<tc::CompoundFormula>();

    context()->erase(iCF);

    tc::ContextPtr pCopy = clone(context());

    context()->insert(pCF->getPart(0).begin(), pCF->getPart(0).end());

    if (pCF->size() > 2) {
        pCF->removePart(0);
        (*pCopy)->insert(pCF);
    } else
        (*pCopy)->insert(pCF->getPart(1).begin(), pCF->getPart(1).end());

    CS::push(pCopy);

    return true;
}

static
bool _insertBounds(tc::Context &_formulas, const tc::TypeSets &_bounds, bool _bUpper) {
    bool bModified = false;

    for (tc::TypeSets::const_iterator i = _bounds.begin(); i != _bounds.end(); ++i) {
        TypePtr pType = i->first;
        const tc::TypeSet &types = i->second;

        if (_formulas.substs->findSubst(pType) != _formulas.substs->end() ||
                _formulas.fs->findSubst(pType) != _formulas.fs->end())
            continue;

        for (tc::TypeSet::const_iterator j = types.begin(); j != types.end(); ++j) {
            assert(pType->hasFresh() || (*j)->hasFresh());

            const int kind = j->bStrict ? tc::Formula::SUBTYPE_STRICT : tc::Formula::SUBTYPE;
            tc::FormulaPtr pFormula = _bUpper ? new tc::Formula(kind, pType, *j) :
                    new tc::Formula(kind, *j, pType);

            bModified |= _formulas.add(pFormula);
        }
    }

    return bModified;
}

bool Solver::infer() {
    bool bModified = false;
    tc::Formulas::iterator iNext = context()->empty() ? context()->end() : next(context()->begin());

    context().pExtrema->invalidate();

    for (tc::Formulas::iterator i = context()->begin(); i != context()->end(); i = iNext) {
        tc::Formula &f = **i;

        iNext = next(i);

        if (!f.is(tc::Formula::SUBTYPE | tc::Formula::SUBTYPE_STRICT))
            continue;

        // Check if formula f is required for bounding it's left or right operand,
        // Given f is A <= B only check for upper bound if A contains fresh (similarly for lower bound).
        if (f.getLhs()->hasFresh()) {
            const tc::TypeSet &bounds = context().pExtrema->sup(f.getLhs());

            if (bounds.find(f.getRhs()) != bounds.end())
                continue;
        }

        if (f.getRhs()->hasFresh()) {
            const tc::TypeSet &bounds = context().pExtrema->inf(f.getRhs());

            if (bounds.find(f.getLhs()) != bounds.end())
                continue;
        }

        context()->erase(i);
        bModified = true;
    }

    bModified |= _insertBounds(context(), context().pExtrema->infs(), false);
    bModified |= _insertBounds(context(), context().pExtrema->sups(), true);

    return bModified;
}

bool Solver::inferCompound() {
    // We assume that unify() and infer() were already run.
    tc::FormulaList formulas;
    tc::Formulas::iterator iCF = context()->beginCompound();
    bool bModified = false;

    if (iCF == context()->end())
        return false;

    Cloner cloner;
    tc::ContextPtr pCtxBase = new tc::Context();

    for (tc::Formulas::iterator i = context()->begin(); i != iCF; ++i)
        (*pCtxBase)->insert(cloner.get(i->ptr()));

    for (tc::Formulas::iterator i = iCF; i != context()->end();) {
        tc::CompoundFormula &cf = (tc::CompoundFormula &)**i;
        bool bFormulaModified = false;

        for (size_t j = 0; j < cf.size(); ++j) {
            Cloner cloner;
            tc::ContextPtr pCtx = cloner.get(pCtxBase.ptr());
            tc::Formulas &part = cf.getPart(j);

            for (tc::Formulas::iterator k = part.begin(); k != part.end(); ++k)
                (*pCtx)->insert(cloner.get(k->ptr()));

            // Got cloned context, use it.
            pCtx->pParent = CS::top();
            CS::push(pCtx);

            if (infer()) {
                CS::pop();
                CS::push(tc::ContextPtr(new tc::Context()));
                context().pParent = pCtxBase;

                while (!(*pCtx)->empty()) {
                    tc::FormulaPtr pFormula = *(*pCtx)->begin();

                    (*pCtx)->erase((*pCtx)->begin());

                    if (!context()->implies(*pFormula) && !pCtxBase->implies(*pFormula))
                        context()->insert(pFormula);
                }

                if (unify()) {
                    context()->insert(context().substs->begin(), context().substs->end());
                    context().substs->clear();
                }

                if (context()->size() != part.size() ||
                        !std::equal(context()->begin(), context()->end(), part.begin(), tc::FormulaEquals()))
                {
                    part.swap(*context().fs);
                    bFormulaModified = true;
                }
            }

            CS::pop();
        }

        if (bFormulaModified) {
            formulas.push_back(&cf);
            context()->erase(i++);
            bModified = true;
        } else
            ++i;
    }

    if (bModified)
        context()->insert(formulas.begin(), formulas.end());

    return bModified;
}

bool Solver::guess() {
    bool bModified = false;
    TypeMap sups, infs;

    context().pExtrema->invalidate();

    for (tc::TypeSets::const_iterator i = context().pExtrema->sups().begin();
            i != context().pExtrema->sups().end(); ++i)
    {
        tc::FreshTypePtr pType = i->first.as<tc::FreshType>();
        const tc::TypeSet &types = i->second;

        if (pType->getFlags() != tc::FreshType::PARAM_IN)
            continue;

        if (types.size() != 1)
            continue;

        const tc::Extremum &sup = *types.begin();

        if (!sup.bStrict)
            sups[pType] = sup.pType;
    }

    for (tc::TypeSets::const_iterator i = context().pExtrema->infs().begin();
            i != context().pExtrema->infs().end(); ++i)
    {
        tc::FreshTypePtr pType = i->first.as<tc::FreshType>();
        const tc::TypeSet &types = i->second;

        if ((pType->getFlags() & tc::FreshType::PARAM_OUT) == 0)
            continue;

        if (types.size() != 1)
            continue;

        const tc::Extremum &inf = *types.begin();

        if (!inf.bStrict)
            infs[pType] = inf.pType;
    }

    for (tc::Formulas::iterator i = context()->beginCompound(); i != context()->end(); ++i) {
        tc::CompoundFormula &cf = *i->as<tc::CompoundFormula>();

        for (size_t j = 0; j < cf.size(); ++j) {
            tc::Formulas &part = cf.getPart(j);

            for (tc::Formulas::iterator k = part.begin(); k != part.end(); ++k) {
                tc::Formula &f = **k;

                for (TypeMap::iterator l = sups.begin(); l != sups.end();) {
                    tc::FreshTypePtr pType = l->first;
                    TypeMap::iterator lNext = next(l);

                    if (*f.getLhs() == *pType || f.getLhs()->contains(pType) || f.getRhs()->contains(pType) ||
                            (f.is(tc::Formula::EQUALS) && *f.getRhs() == *pType))
                        sups.erase(l);

                    l = lNext;
                }

                for (TypeMap::iterator l = infs.begin(); l != infs.end();) {
                    tc::FreshTypePtr pType = l->first;
                    TypeMap::iterator lNext = next(l);

                    if (*f.getRhs() == *pType || f.getLhs()->contains(pType) || f.getRhs()->contains(pType) ||
                            (f.is(tc::Formula::EQUALS) && *f.getLhs() == *pType))
                        infs.erase(l);

                    l = lNext;
                }
            }
        }
    }

    for (TypeMap::const_iterator j = sups.begin(); j != sups.end(); ++j)
        bModified |= context().add(new tc::Formula(tc::Formula::EQUALS, j->first, j->second));

    for (TypeMap::const_iterator j = infs.begin(); j != infs.end(); ++j)
        bModified |= context().add(new tc::Formula(tc::Formula::EQUALS, j->first, j->second));

    return bModified;
}

bool Solver::compact() {
    tc::FormulaList formulas;
    tc::Formulas::iterator iCF = context()->beginCompound();
    bool bModified = false;

    if (iCF == context()->end())
        return false;

    for (tc::Formulas::iterator i = iCF; i != context()->end();) {
        tc::CompoundFormula &cf = (tc::CompoundFormula &)**i;
        bool bFormulaModified = false;

        for (size_t j = 0; j < cf.size(); ++j) {
            tc::Formulas &part = cf.getPart(j);

            assert(!part.empty());

            for (size_t k = 0; k < cf.size(); ++k) {
                if (k == j)
                    continue;

                tc::Formulas &other = cf.getPart(k);

                if (other.size() != part.size())
                    continue;

                typedef std::vector<tc::FormulaPtr> FormulaVec;
                FormulaVec diff(part.size() + other.size());
                tc::FormulaCmp cmp;
                FormulaVec::iterator iEnd = std::set_symmetric_difference(part.begin(), part.end(),
                        other.begin(), other.end(), diff.begin(), cmp);

                if (std::distance(diff.begin(), iEnd) != 2)
                    continue;

                tc::Formulas::iterator iFormula = std::find(part.begin(), part.end(), diff[0]);

                if (iFormula == part.end()) {
                    iFormula = std::find(part.begin(), part.end(), diff[1]);
                    std::swap(diff[0], diff[1]);
                }

                tc::Formula &f = *diff[0], &g = *diff[1];

                assert((iFormula != part.end()));

                if (f.is(tc::Formula::SUBTYPE | tc::Formula::SUBTYPE_STRICT) &&
                        g.is(tc::Formula::SUBTYPE | tc::Formula::SUBTYPE_STRICT) &&
                        *f.getRhs() == *g.getLhs())
                {
                    const int ord = f.is(tc::Formula::SUBTYPE) || g.is(tc::Formula::SUBTYPE) ?
                            tc::Formula::SUBTYPE : tc::Formula::SUBTYPE_STRICT;
                    TypePtr pType;

                    if (!context().lookup(tc::Formula(ord, f.getLhs(), pType),
                            tc::Formula(tc::Formula::SUBTYPE, pType, f.getRhs())))
                        continue;

                    part.erase(iFormula);
                } else if (f.is(tc::Formula::SUBTYPE_STRICT) && g.is(tc::Formula::EQUALS) && (
                        (*f.getLhs() == *g.getLhs() && *f.getRhs() == *g.getRhs()) ||
                        (*f.getLhs() == *g.getRhs() && *f.getRhs() == *g.getLhs())))
                {
                    part.erase(iFormula);
                    part.insert(new tc::Formula(tc::Formula::SUBTYPE, f.getLhs(), f.getRhs()));
                } else
                    continue;

                bFormulaModified = true;
                cf.removePart(k);

                if (k < j)
                    --j;

                if (k > 0)
                    --k;
            }
        }

        if (bFormulaModified) {
            assert(cf.size() > 0);

            if (cf.size() == 1)
                formulas.insert(formulas.end(), cf.getPart(0).begin(), cf.getPart(0).end());
            else
                formulas.push_back(&cf);

            context()->erase(i++);
            bModified = true;
        } else
            ++i;
    }

    if (bModified)
        context()->insert(formulas.begin(), formulas.end());

    return bModified;
}

bool Solver::prune() {
    tc::Formulas::iterator iCF = context()->beginCompound();
    tc::FormulaList formulas;
    bool bModified = false;

    for (tc::Formulas::iterator i = iCF; i != context()->end();) {
        tc::CompoundFormula &cf = *i->as<tc::CompoundFormula>();
        bool bFormulaModified = false;

        assert(cf.is(tc::Formula::COMPOUND));

        for (size_t j = 0; j < cf.size();) {
            tc::Formulas &part = cf.getPart(j);
            bool bImplies = false;

            for (size_t k = 0; k < cf.size() && !bImplies; ++k) {
                if (k == j)
                    continue;

                tc::Formulas &other = cf.getPart(k);

                bImplies = true;

                for (tc::Formulas::iterator l = other.begin(); l != other.end(); ++l) {
                    tc::Formula &f = **l;

                    if (!part.implies(f) && !context().implies(f)) {
                        bImplies = false;
                        break;
                    }
                }
            }

            if (bImplies) {
                cf.removePart(j);
                bFormulaModified = true;
            } else
                ++j;
        }

        if (bFormulaModified) {
            if (cf.size() == 1)
                formulas.insert(formulas.end(), cf.getPart(0).begin(), cf.getPart(0).end());
            else if (cf.size() > 0)
                formulas.push_back(&cf);

            context()->erase(i++);
            bModified = true;
        } else
            ++i;
    }

    if (bModified)
        context()->insert(formulas.begin(), formulas.end());

    return bModified;
}

bool Solver::refute(tc::Formula &_f) {
    TypePtr a = _f.getLhs();
    TypePtr b = _f.getRhs();
    TypePtr c;

    if (_f.eval() == tc::Formula::FALSE)
        return true;

    // Check if there exists such c for which the relations P and Q hold.
#define CHECK(P,PL,PR,Q,QL,QR) \
        if (context().lookup(tc::Formula(tc::Formula::P, PL, PR), \
                tc::Formula(tc::Formula::Q, QL, QR))) \
            return true

    switch (_f.getKind()) {
        case tc::Formula::EQUALS:
            CHECK(COMPARABLE, a, c,    INCOMPARABLE, c, b);
            CHECK(COMPARABLE, b, c,    INCOMPARABLE, c, a);
            CHECK(SUBTYPE_STRICT, a, c, SUBTYPE, c, b);
            CHECK(SUBTYPE, a, c,       SUBTYPE_STRICT, c, b);
            CHECK(SUBTYPE_STRICT, c, a, SUBTYPE, b, c);
            CHECK(SUBTYPE, c, a,       SUBTYPE_STRICT, b, c);
            CHECK(SUBTYPE_STRICT, b, c, SUBTYPE, c, a);
            CHECK(SUBTYPE, b, c,       SUBTYPE_STRICT, c, a);
            CHECK(SUBTYPE_STRICT, c, b, SUBTYPE, a, c);
            CHECK(SUBTYPE, c, b,       SUBTYPE_STRICT, a, c);
            break;
        case tc::Formula::SUBTYPE_STRICT:
            CHECK(COMPARABLE, a, c,    NO_MEET, c, b);
            CHECK(COMPARABLE, b, c,    NO_JOIN, c, a);
            CHECK(SUBTYPE, c, a,       SUBTYPE_STRICT, b, c);
            CHECK(SUBTYPE_STRICT, c, a, SUBTYPE, b, c);
            CHECK(SUBTYPE_STRICT, c, a, INCOMPARABLE, b, c);  // C < A && B !~ C
            CHECK(SUBTYPE,        c, a, INCOMPARABLE, b, c);  // C <= A && B !~ C
            break;
        case tc::Formula::SUBTYPE:                           // A <= B
            CHECK(COMPARABLE, a, c,    NO_MEET, c, b);
            CHECK(COMPARABLE, b, c,    NO_JOIN, c, a);
            CHECK(SUBTYPE, c, a,       SUBTYPE_STRICT, b, c); // B < C <= A
            CHECK(SUBTYPE_STRICT, c, a, SUBTYPE, b, c);       // B <= C < A
            CHECK(SUBTYPE_STRICT, c, a, INCOMPARABLE, b, c);  // C < A && B !~ C
            CHECK(SUBTYPE,       c, a, INCOMPARABLE, b, c);  // C <= A && B !~ C

            CHECK(SUBTYPE, b, c,       SUBTYPE_STRICT, c, a); // B <= C < A
            CHECK(SUBTYPE_STRICT, b, c, SUBTYPE, c, a);       // B < C <= A
            break;
    }

#undef CHECK

    return false;
}

bool Solver::refute(int &_result) {
    tc::Formulas::iterator iBegin = context()->begin();
    tc::Formulas::iterator iCF = context()->beginCompound();

    _result = tc::Formula::UNKNOWN;

    for (tc::Formulas::iterator i = iBegin; i != iCF; ++i) {
        tc::Formula &f = **i;

        if (!refute(f))
            continue;

        _result = tc::Formula::FALSE;
        return true;
    }

    bool bModified = false;
    tc::FormulaList formulas;

    for (tc::Formulas::iterator i = iCF; i != context()->end();) {
        tc::CompoundFormula &cf = *i->as<tc::CompoundFormula>();
        bool bFormulaModified = false;

        assert(cf.is(tc::Formula::COMPOUND));

        for (size_t j = 0; j < cf.size();) {
            int result = tc::Formula::UNKNOWN;

            CS::push(cf.getPartPtr(j));

            if (refute(result)) {
                assert(result == tc::Formula::FALSE);
                cf.removePart(j);
                bFormulaModified = true;
            } else
                ++j;

            CS::pop();
        }

        if (bFormulaModified) {
            if (cf.size() == 0)
                _result = tc::Formula::FALSE;
            else if (cf.size() == 1)
                formulas.insert(formulas.end(), cf.getPart(0).begin(), cf.getPart(0).end());
            else if (cf.size() > 0)
                formulas.push_back(&cf);

            context()->erase(i++);
            bModified = true;
        } else
            ++i;
    }

    if (bModified)
        context()->insert(formulas.begin(), formulas.end());

    return bModified;
}

bool Solver::lift() {
    bool bModified = false;
    tc::FormulaList formulas;

    for (tc::Formulas::iterator i = context()->beginCompound(); i != context()->end();) {
        tc::CompoundFormula &cf = *i->as<tc::CompoundFormula>();

        assert(cf.is(tc::Formula::COMPOUND));
        assert(cf.size() > 1);

        tc::Formulas &base = cf.getPart(0);
        bool bFormulaModified = false;

        for (tc::Formulas::iterator j = base.begin(); j != base.end();) {
            tc::FormulaPtr pFormula = *j;
            bool bFound = true;

            for (size_t k = 1; bFound && k < cf.size(); ++k) {
                tc::Formulas &part = cf.getPart(k);
                bFound = (part.find(pFormula) != part.end());
            }

            if (bFound) {
                formulas.push_back(pFormula);
                base.erase(j++);
                bFormulaModified = true;

                for (size_t k = 1; k < cf.size();) {
                    cf.getPart(k).erase(pFormula);
                    if (cf.getPart(k).size() == 0)
                        cf.removePart(k);
                    else
                        ++k;
                }

                if (cf.size() == 1)
                    break;
            } else
                ++j;
        }

        if (bFormulaModified) {
            if (cf.size() == 1)
                formulas.insert(formulas.end(), cf.getPart(0).begin(), cf.getPart(0).end());
            else if (cf.size() > 0)
                formulas.push_back(&cf);

            context()->erase(i++);
            bModified = true;
        } else
            ++i;
    }

    if (bModified)
        context()->insert(formulas.begin(), formulas.end());

    return bModified;
}

bool Solver::expandPredicate(int _kind, const PredicateTypePtr &_pLhs,
        const PredicateTypePtr &_pRhs, tc::FormulaList &_formulas)
{
    if (_pLhs->getInParams().size() != _pRhs->getInParams().size())
        return false;

    if (_pLhs->getOutParams().size() != _pRhs->getOutParams().size())
        return false;

    for (size_t i = 0; i < _pLhs->getInParams().size(); ++ i) {
        Param &p = *_pLhs->getInParams().get(i);
        Param &q = *_pRhs->getInParams().get(i);

        if (p.getType()->getKind() == Type::TYPE || q.getType()->getKind() == Type::TYPE)
            _formulas.push_back(new tc::Formula(tc::Formula::EQUALS, p.getType(), q.getType()));
        else
            _formulas.push_back(new tc::Formula(_kind, q.getType(), p.getType()));
    }

    for (size_t j = 0; j < _pLhs->getOutParams().size(); ++j) {
        Branch &b = *_pLhs->getOutParams().get(j);
        Branch &c = *_pRhs->getOutParams().get(j);

        if (b.size() != c.size())
            return false;

        for (size_t i = 0; i < b.size(); ++ i) {
            Param &p = *b.get(i);
            Param &q = *c.get(i);

            _formulas.push_back(new tc::Formula(_kind, p.getType(), q.getType()));
        }
    }

    return true;
}

bool Solver::expandStruct(int _kind, const StructTypePtr &_pLhs, const StructTypePtr &_pRhs,
        tc::FormulaList & _formulas)
{
    size_t cSub = 0, cSuper = 0, cUnknown = 0;
    const size_t cOrdFieldsL = _pLhs->getNamesOrd().size() + _pLhs->getTypesOrd().size();
    const size_t cOrdFieldsR = _pRhs->getNamesOrd().size() + _pRhs->getTypesOrd().size();
    tc::CompoundFormulaPtr pStrict = _kind == tc::Formula::SUBTYPE_STRICT ? new tc::CompoundFormula() : NULL;

    for (size_t i = 0; i < cOrdFieldsL && i < cOrdFieldsR; ++i) {
        NamedValuePtr pFieldL = i < _pLhs->getNamesOrd().size() ? _pLhs->getNamesOrd().get(i) :
                _pLhs->getTypesOrd().get(i - _pLhs->getNamesOrd().size());
        NamedValuePtr pFieldR = i < _pRhs->getNamesOrd().size() ? _pRhs->getNamesOrd().get(i) :
                _pRhs->getTypesOrd().get(i - _pRhs->getNamesOrd().size());

        _formulas.push_back(new tc::Formula(_kind, pFieldL->getType(), pFieldR->getType()));

        if (pStrict)
            pStrict->addPart().insert(new tc::Formula(tc::Formula::SUBTYPE_STRICT,
                    pFieldL->getType(), pFieldR->getType()));
    }

    if (cOrdFieldsL < cOrdFieldsR)
        return false;

    typedef std::map<std::wstring, std::pair<NamedValuePtr, NamedValuePtr> > NameMap;
    NameMap fields;

    for (size_t i = 0; i < _pLhs->getNamesSet().size(); ++i)
        fields[_pLhs->getNamesSet().get(i)->getName()].first = _pLhs->getNamesSet().get(i);

    for (size_t i = 0; i < _pRhs->getNamesSet().size(); ++i)
        fields[_pRhs->getNamesSet().get(i)->getName()].second = _pRhs->getNamesSet().get(i);

    for (size_t i = 0; i < _pLhs->getNamesOrd().size(); ++i) {
        NamedValuePtr pField = _pLhs->getNamesOrd().get(i);
        NameMap::iterator j = fields.find(pField->getName());

        if (j != fields.end() && j->second.second)
            j->second.first = pField;
    }

    for (size_t i = 0; i < _pRhs->getNamesOrd().size(); ++i) {
        NamedValuePtr pField = _pRhs->getNamesOrd().get(i);
        NameMap::iterator j = fields.find(pField->getName());

        if (j != fields.end() && j->second.first)
            j->second.second = pField;
    }

    for (NameMap::iterator i = fields.begin(); i != fields.end(); ++i) {
        NamedValuePtr pFieldL = i->second.first;
        NamedValuePtr pFieldR = i->second.second;

        if (!pFieldL)
            return false;

        if (pFieldR) {
            _formulas.push_back(new tc::Formula(_kind, pFieldL->getType(), pFieldR->getType()));

            if (pStrict)
                pStrict->addPart().insert(new tc::Formula(tc::Formula::SUBTYPE_STRICT,
                        pFieldL->getType(), pFieldR->getType()));
        }
    }

    if (pStrict) {
        if (pStrict->size() == 1)
            _formulas.push_back(*pStrict->getPart(0).begin());
        else if (pStrict->size() > 1)
            _formulas.push_back(pStrict);
    }

    return true;
}

bool Solver::expandSet(int _kind, const SetTypePtr &_pLhs, const SetTypePtr &_pRhs,
        tc::FormulaList &_formulas)
{
    _formulas.push_back(new tc::Formula(_kind, _pLhs->getBaseType(), _pRhs->getBaseType()));
    return true;
}

bool Solver::expandList(int _kind, const ListTypePtr &_pLhs, const ListTypePtr &_pRhs,
        tc::FormulaList &_formulas)
{
    _formulas.push_back(new tc::Formula(_kind, _pLhs->getBaseType(), _pRhs->getBaseType()));
    return true;
}

bool Solver::expandMap(int _kind, const MapTypePtr &_pLhs, const MapTypePtr &_pRhs,
        tc::FormulaList & _formulas)
{
    _formulas.push_back(new tc::Formula(_kind, _pRhs->getIndexType(), _pLhs->getIndexType()));
    _formulas.push_back(new tc::Formula(_kind, _pLhs->getBaseType(), _pRhs->getBaseType()));
    return true;
}

bool Solver::expandType(int _kind, const TypeTypePtr &_pLhs, const TypeTypePtr &_pRhs,
        tc::FormulaList & _formulas)
{
    if (_pLhs->getDeclaration() && _pLhs->getDeclaration()->getType() &&
            _pRhs->getDeclaration() && _pRhs->getDeclaration()->getType())
        _formulas.push_back(new tc::Formula(_kind, _pLhs->getDeclaration()->getType(),
                _pRhs->getDeclaration()->getType()));

    return true;
}

// TODO: check if SUBTYPE_STRICT can be handled likewise.
bool Solver::expand(int &_result) {
    tc::FormulaList formulas;
    bool bModified = false;

    _result = tc::Formula::UNKNOWN;

    for (tc::Formulas::iterator i = context()->begin(); i != context()->end();) {
        tc::Formula &f = **i;
        TypePtr pLhs = f.getLhs(), pRhs = f.getRhs();
        bool bFormulaModified = false;

        if (f.is(tc::Formula::EQUALS | tc::Formula::SUBTYPE | tc::Formula::SUBTYPE_STRICT)) {
            bool bResult = true;

            bFormulaModified = true;

            if (pLhs->getKind() == Type::PREDICATE && pRhs->getKind() == Type::PREDICATE)
                bResult = expandPredicate(f.getKind(), pLhs.as<PredicateType>(), pRhs.as<PredicateType>(), formulas);
            else if (pLhs->getKind() == Type::STRUCT && pRhs->getKind() == Type::STRUCT)
                bResult = expandStruct(f.getKind(), pLhs.as<StructType>(), pRhs.as<StructType>(), formulas);
            else if (pLhs->getKind() == Type::SET && pRhs->getKind() == Type::SET)
                bResult = expandSet(f.getKind(), pLhs.as<SetType>(), pRhs.as<SetType>(), formulas);
            else if (pLhs->getKind() == Type::LIST && pRhs->getKind() == Type::LIST)
                bResult = expandList(f.getKind(), pLhs.as<ListType>(), pRhs.as<ListType>(), formulas);
            else if (pLhs->getKind() == Type::TYPE && pRhs->getKind() == Type::TYPE)
                bResult = expandType(f.getKind(), pLhs.as<TypeType>(), pRhs.as<TypeType>(), formulas);
            else
                bFormulaModified = false;

            if (bFormulaModified) {
                bModified = true;
                context()->erase(i++);
            } else
                ++i;

            if (!bResult)
                _result = tc::Formula::FALSE;
        } else if (f.is(tc::Formula::COMPOUND)) {
            tc::CompoundFormula &cf = (tc::CompoundFormula &)f;

            for (size_t j = 0; j != cf.size(); ++j) {
                CS::push(cf.getPartPtr(j));
                bFormulaModified |= expand(_result);
                CS::pop();

                if (_result == tc::Formula::FALSE)
                    break;
            }

            if (bFormulaModified) {
                formulas.push_back(&cf);
                context()->erase(i++);
                bModified = true;
            } else
                ++i;
        } else
            ++i;

        if (_result == tc::Formula::FALSE)
            return true;
    }

    if (bModified)
        context()->insert(formulas.begin(), formulas.end());

    return bModified;
}

bool Solver::unify(bool _bCompound) {
    bool bResult = false;

    while (!context()->empty()) {
        tc::Formula &f = **context()->begin();

        if (!f.is(tc::Formula::EQUALS))
            break;

        if (*f.getLhs() == *f.getRhs()) {
            context()->erase(context()->begin());
            continue;
        }

        if (!f.hasFresh())
            break;

        TypePtr pOld = f.getLhs(), pNew = f.getRhs();

        if (pOld->getKind() != Type::FRESH && pNew->getKind() != Type::FRESH)
            continue;

        if (!pOld->getKind() == Type::FRESH)
            std::swap(pOld, pNew);

        context()->erase(context()->begin());

        if (!pOld->compare(*pNew, Type::ORD_EQUALS)) {
            if (context().rewrite(pOld, pNew))
                bResult = true;
            context().substs->insert(new tc::Formula(tc::Formula::EQUALS, pOld, pNew));
        }

        bResult |= !_bCompound; // Subformulas of compound formulas don't store their substs separately.
    }

    if (_bCompound)
        context()->insert(context().substs->begin(), context().substs->end());

    tc::FormulaList formulas;

    for (tc::Formulas::iterator i = context()->beginCompound(); i != context()->end();) {
        tc::CompoundFormula &cf = *i->as<tc::CompoundFormula>();
        bool bFormulaModified = false;

        assert(cf.is(tc::Formula::COMPOUND));

        for (size_t j = 0; j < cf.size(); ++j) {
            Auto<tc::Formulas> pPart = cf.getPartPtr(j);

            CS::push(pPart);
            bFormulaModified |= unify(true);
            CS::pop();
        }

        if (bFormulaModified) {
            formulas.push_back(&cf);
            context()->erase(i++);
            bResult = true;
        } else
            ++i;
    }

    if (bResult)
        context()->insert(formulas.begin(), formulas.end());

    return bResult;
}

bool Solver::eval(int & _result) {
    tc::Formulas::iterator iCF = context()->beginCompound();
    bool bModified = false;

    _result = tc::Formula::TRUE;

    for (tc::Formulas::iterator i = context()->begin(); i != iCF;) {
        tc::Formula &f = **i;
        const int r = f.eval();

        if (r == tc::Formula::FALSE) {
            _result = tc::Formula::FALSE;
            break;
        }

        if (r == tc::Formula::UNKNOWN) {
            _result = tc::Formula::UNKNOWN;
            ++i;
            continue;
        }

        // TRUE.
        context()->erase(i++);
        bModified = true;
    }

    tc::FormulaList formulas;

    for (tc::Formulas::iterator i = iCF; i != context()->end();) {
        tc::CompoundFormula &cf = *i->as<tc::CompoundFormula>();
        bool bFormulaModified = false;
        int r = tc::Formula::UNKNOWN;

        for (size_t j = 0; j < cf.size();) {
            int result = tc::Formula::UNKNOWN;

            CS::push(cf.getPartPtr(j));
            bFormulaModified |= eval(result);
            CS::pop();

            if (result == tc::Formula::FALSE || cf.getPart(j).empty()) {
                bFormulaModified = true;
                cf.removePart(j);
            } else
                ++j;

            if (result == tc::Formula::TRUE)
                r = tc::Formula::TRUE;
        }

        if (r == tc::Formula::UNKNOWN && _result == tc::Formula::TRUE)
            _result = tc::Formula::UNKNOWN;

        if (bFormulaModified) {
            if (cf.size() == 0)
                _result = tc::Formula::FALSE;
            else if (cf.size() == 1)
                formulas.insert(formulas.end(), cf.getPart(0).begin(), cf.getPart(0).end());
            else if (cf.size() > 0)
                formulas.push_back(&cf);

            context()->erase(i++);
            bModified = true;
        } else
            ++i;
    }

    if (bModified)
        context()->insert(formulas.begin(), formulas.end());

    return bModified;
}

bool Solver::sequence(int &_result) {
    bool bModified = false;
    bool bIterationModified;
    size_t cStep = 0;

    _result = tc::Formula::UNKNOWN;

    do {
        bIterationModified = false;

        if (unify()) {
            if (Options::instance().bVerbose) {
                std::wcout << std::endl << L"Unify [" << cStep << L"]:" << std::endl;
                prettyPrint(context(), std::wcout);
            }

            bIterationModified = true;
        }

        if (lift()) {
            if (Options::instance().bVerbose) {
                std::wcout << std::endl << L"Lift [" << cStep << L"]:" << std::endl;
                prettyPrint(context(), std::wcout);
            }

            bIterationModified = true;
        }

        if (prune()) {
            if (Options::instance().bVerbose) {
                std::wcout << std::endl << L"Prune [" << cStep << L"]:" << std::endl;
                prettyPrint(context(), std::wcout);
            }

            bIterationModified = true;
        }

        if (compact()) {
            if (Options::instance().bVerbose) {
                std::wcout << std::endl << L"Compact [" << cStep << L"]:" << std::endl;
                prettyPrint(context(), std::wcout);
            }

            bIterationModified = true;
        }

        if (eval(_result)) {
            if (Options::instance().bVerbose) {
                std::wcout << std::endl << L"Eval [" << cStep << L"]:" << std::endl;
                prettyPrint(context(), std::wcout);
            }

            bIterationModified = true;
        }

        if (_result == tc::Formula::FALSE)
            break;

        if (expand(_result)) {
            if (Options::instance().bVerbose) {
                std::wcout << std::endl << L"Expand [" << cStep << L"]:" << std::endl;
                prettyPrint(context(), std::wcout);
            }

            bIterationModified = true;
        }

        if (_result == tc::Formula::FALSE)
            break;

        if (!bIterationModified && refute(_result)) {
            if (Options::instance().bVerbose) {
                std::wcout << std::endl << L"Refute [" << cStep << L"]:" << std::endl;
                prettyPrint(context(), std::wcout);
            }

            bIterationModified = true;
        }

        if (_result == tc::Formula::FALSE)
            break;

        if (!bIterationModified && infer()) {
            if (Options::instance().bVerbose) {
                std::wcout << std::endl << L"Infer [" << cStep << L"]:" << std::endl;
                prettyPrint(context(), std::wcout);
            }

            bIterationModified = true;
        }

        if (!bIterationModified && inferCompound()) {
            if (Options::instance().bVerbose) {
                std::wcout << std::endl << L"Infer Compound [" << cStep << L"]:" << std::endl;
                prettyPrint(context(), std::wcout);
            }

            bIterationModified = true;
        }

        if (!bIterationModified && guess()) {
            if (Options::instance().bVerbose) {
                std::wcout << std::endl << L"Guess [" << cStep << L"]:" << std::endl;
                prettyPrint(context(), std::wcout);
            }

            bIterationModified = true;
        }

        bModified |= bIterationModified;
        ++cStep;
    } while (bIterationModified && _result != tc::Formula::FALSE);

    return bIterationModified || bModified;
}

bool Solver::run() {
    int result = tc::Formula::UNKNOWN;
    bool bChanged;
    std::list<tc::ContextPtr> processed;

    do {
        const bool bChanged = sequence(result);

        if (!bChanged && result != tc::Formula::FALSE && fork()) {
            if (Options::instance().bVerbose) {
                std::wcout << std::endl << L"Fork:" << std::endl;
                prettyPrint(context(), std::wcout);
            }

            continue;
        }

        if (bChanged && result != tc::Formula::FALSE)
            continue;

        if (result != tc::Formula::FALSE)
            processed.push_back(CS::top());

        if (result == tc::Formula::FALSE)
            std::wcout << std::endl << L"Refuted" << std::endl;

        CS::pop();
    } while (!CS::empty());

    if (processed.empty())
        result = tc::Formula::FALSE;
    else {
        if (next(processed.begin()) == processed.end()) {
            CS::push(processed.front());
        } else {
            // Recombine all results into a compound formula and simplify it.
            tc::CompoundFormulaPtr pCF = new tc::CompoundFormula();

            for (std::list<tc::ContextPtr>::iterator i = processed.begin(); i != processed.end(); ++i) {
                tc::Context &ctx = **i;
                tc::Formulas &part = pCF->addPart();

                part.insert(ctx.fs->begin(), ctx.fs->end());
                part.insert(ctx.substs->begin(), ctx.substs->end());
            }

            CS::push(ptr(new tc::Context()));
            context()->insert(pCF);
            sequence(result);
            assert(result != tc::Formula::FALSE);
        }

        result = context()->empty() ? tc::Formula::TRUE : tc::Formula::UNKNOWN;

        if (Options::instance().bVerbose) {
            std::wcout << std::endl << L"Solution:" << std::endl;
            prettyPrint(context(), std::wcout);
        }
    }

    if (Options::instance().bVerbose) {
        switch (result) {
            case tc::Formula::UNKNOWN:
                std::wcout << std::endl << L"Inference incomplete" << std::endl;
                break;
            case tc::Formula::TRUE:
                std::wcout << std::endl << L"Inference successful" << std::endl;
                break;
            case tc::Formula::FALSE:
                std::wcout << std::endl << L"Type error" << std::endl;
                break;
        }
    }

    return result != tc::Formula::FALSE;
}

bool tc::solve(tc::Formulas &_formulas, tc::Formulas &_result) {
    CS::clear();
    CS::push(ContextPtr(new Context(ref(&_formulas), ref(&_result))));

    if (Options::instance().bVerbose) {
        std::wcout << std::endl << L"Solving:" << std::endl;
        prettyPrint(*CS::top(), std::wcout);
    }

    return Solver().run();
}
