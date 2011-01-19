/// \file solve_constraints.cpp
///

#include <iostream>
#include "typecheck.h"
#include "prettyprinter.h"

using namespace ir;

class Solver {
public:
    Solver(tc::Formulas & _formulas) : m_formulas(_formulas) {}

    bool unify(tc::Formulas & _formulas);
    bool lift();
    bool run();
    bool eval(int & _result);
    bool prune();
    bool refute(tc::Formulas & _formulas, int & _result);
    bool compact();
    bool guess();
    bool expand(tc::Formulas & _formulas, int & _result);
    bool infere(tc::Formulas & _formulas);

protected:
    typedef std::map<tc::FreshType *, std::list<ir::CType *> > MergeMap;
    typedef std::map<tc::FreshType *, ir::CType *> TypeMap;

    tc::FormulaSet::iterator beginCompound(tc::FormulaSet & _formulas);
    bool lookup(tc::FormulaSet & _formulas, CType * _pLhs, CType * _pRhs, int _op);

    bool refute(tc::Formula & _f, tc::Formulas & _formulas);
    CType * find(tc::Formulas & _formulas, const tc::Formula & _f, const tc::Formula & _cond);
    void collectGuessesIn(tc::Formulas & _formulas, TypeMap & _dest);
    void collectGuessesOut(tc::Formulas & _formulas, TypeMap & _dest);
    bool expandPredicate(int _kind, CPredicateType * _pLhs, CPredicateType * _pRhs, tc::FormulaList & _formulas);
    bool expandStruct(int _kind, CStructType * _pLhs, CStructType * _pRhs, tc::FormulaList & _formulas);
    bool expandSet(int _kind, CSetType * _pLhs, CSetType * _pRhs, tc::FormulaList & _formulas);
    bool expandList(int _kind, CListType * _pLhs, CListType * _pRhs, tc::FormulaList & _formulas);
    bool expandType(int _kind, CTypeType * _pLhs, CTypeType * _pRhs, tc::FormulaList & _formulas);

private:
    tc::Formulas & m_formulas;
};

tc::FormulaSet::iterator Solver::beginCompound(tc::FormulaSet & _formulas) {
    tc::CompoundFormula cfEmpty;
    return _formulas.lower_bound(& cfEmpty);
}

bool Solver::lookup(tc::FormulaSet & _formulas, CType * _pLhs, CType * _pRhs, int _op) {
    tc::Formula f(_op, _pLhs, _pRhs);

    if (_formulas.find(& f) != _formulas.end())
        return true;

    if (& _formulas != & m_formulas && m_formulas.find(& f) != m_formulas.end())
        return true;

    return false;
}

void cloneFormulas(tc::Formulas & _dest, tc::Formulas::iterator _begin,
        tc::Formulas::iterator _end, tc::FormulaSet & _substs)
{
    for (tc::Formulas::iterator i = _begin; i != _end; ++ i) {
        tc::Formula & f = ** i;
        tc::Formula * g = new tc::Formula(f.getKind(), f.getLhs(), f.getRhs());
        for (tc::Formulas::iterator j = _substs.begin(); j != _substs.end(); ++ j)
            g->rewrite((* j)->getLhs(), (* j)->getRhs());
        _dest.insert(g);
    }
}

void Solver::collectGuessesIn(tc::Formulas & _formulas, TypeMap & _dest) {
    for (tc::FormulaSet::iterator i = _formulas.begin(); i != _formulas.end(); ++ i) {
        tc::Formula & f = ** i;

        if (f.is(tc::Formula::Compound)) {
            tc::CompoundFormula & cf = (tc::CompoundFormula &) f;
            TypeMap tm = _dest;
            for (size_t j = 0; j < cf.size(); ++ j) {
                TypeMap part;

                collectGuessesIn(cf.getPart(j), part);

                for (TypeMap::iterator j = part.begin(); j != part.end(); ++ j) {
                    tc::FreshType * p = j->first;
                    TypeMap::iterator k = _dest.find(p);
                    TypeMap::iterator l = tm.find(p);
                    CType * q = j->second;

                    if (q != NULL && k != _dest.end()) {
                        if (k->second == NULL)
                            continue;
                        q = q->getJoin(* k->second).first;
                    }

                    if (l == tm.end() || l->second == NULL ||
                            (q != NULL && q->compare(* l->second, CType::OrdSuper)))
                        tm[p] = q;
                }
            }

            for (TypeMap::iterator j = tm.begin(); j != tm.end(); ++ j) {
                TypeMap::iterator k = _dest.find(j->first);
                if (k != _dest.end() && k->second != NULL)
                    _dest[j->first] = j->second;
            }
        }

        if (! f.is(tc::Formula::Subtype | tc::Formula::SubtypeStrict))
            continue;

        if (f.getLhs()->getKind() == ir::CType::Fresh &&
                (((tc::FreshType *) f.getLhs())->getFlags() & tc::FreshType::ParamIn) != 0)
        {
            tc::FreshType * pLhs = (tc::FreshType *) f.getLhs();

            if (f.is(tc::Formula::SubtypeStrict)) {
                _dest[pLhs] = NULL;
            } else {
                TypeMap::iterator iType = _dest.find(pLhs);

                if (iType != _dest.end()) {
                    if (iType->second != NULL)
                        iType->second = f.getRhs()->getMeet(* iType->second).first;
                } else
                    _dest[pLhs] = f.getRhs();
            }
        }

        /*if (f.getRhs()->getKind() == ir::CType::Fresh &&
                (((tc::FreshType *) f.getRhs())->getFlags() & tc::FreshType::ParamIn) != 0)
        {
            tc::FreshType * pRhs = (tc::FreshType *) f.getRhs();

            if (f.is(tc::Formula::SubtypeStrict)) {
                _dest[pRhs] = NULL;
            } else {
                TypeMap::iterator iType = _dest.find(pRhs);

                if (iType != _dest.end()) {
                    if (iType->second != NULL)
                        iType->second = f.getLhs()->getMeet(* iType->second).first;
                } else
                    _dest[pRhs] = f.getLhs();
            }
        }*/
    }
}

void Solver::collectGuessesOut(tc::Formulas & _formulas, TypeMap & _dest) {
    for (tc::FormulaSet::iterator i = _formulas.begin(); i != _formulas.end(); ++ i) {
        tc::Formula & f = ** i;

        if (f.is(tc::Formula::Compound)) {
            tc::CompoundFormula & cf = (tc::CompoundFormula &) f;
            TypeMap tm = _dest;
            for (size_t j = 0; j < cf.size(); ++ j) {
                TypeMap part;

                collectGuessesOut(cf.getPart(j), part);

                for (TypeMap::iterator j = part.begin(); j != part.end(); ++ j) {
                    tc::FreshType * p = j->first;
                    TypeMap::iterator k = _dest.find(p);
                    TypeMap::iterator l = tm.find(p);
                    CType * q = j->second;

                    if (q != NULL && k != _dest.end()) {
                        if (k->second == NULL)
                            continue;
                        q = q->getJoin(* k->second).first;
                    }

                    if (l == tm.end() || l->second == NULL ||
                            (q != NULL && q->compare(* l->second, CType::OrdSub)))
                        tm[p] = q;
                }
            }

            for (TypeMap::iterator j = tm.begin(); j != tm.end(); ++ j) {
                TypeMap::iterator k = _dest.find(j->first);
                if (k != _dest.end() && k->second != NULL)
                    _dest[j->first] = j->second;
            }
        }

        if (! f.is(tc::Formula::Subtype | tc::Formula::SubtypeStrict))
            continue;

        if (f.getRhs()->getKind() == ir::CType::Fresh &&
                (((tc::FreshType *) f.getRhs())->getFlags() & tc::FreshType::ParamOut) != 0)
        {
            tc::FreshType * pRhs = (tc::FreshType *) f.getRhs();

            if (f.is(tc::Formula::SubtypeStrict)) {
                _dest[pRhs] = NULL;
            } else {
                TypeMap::iterator iType = _dest.find(pRhs);

                if (iType != _dest.end()) {
                    if (iType->second != NULL) {
                        CType * p = f.getLhs()->getJoin(* iType->second).first;

                        if (p == NULL) {
                            if (f.getLhs()->getKind() == CType::Fresh)
                                p = f.getLhs();
                            else if (iType->second->getKind() == CType::Fresh)
                                p = iType->second;
                        }

                        if (p != NULL)
                            iType->second = p;
                    }
                } else
                    _dest[pRhs] = f.getLhs();
            }
        }

        /*if (f.getLhs()->getKind() == ir::CType::Fresh &&
                (((tc::FreshType *) f.getLhs())->getFlags() & tc::FreshType::ParamOut) != 0)
        {
            tc::FreshType * pLhs = (tc::FreshType *) f.getLhs();

            if (f.is(tc::Formula::Subtype)) {
                TypeMap::iterator iType = _dest.find(pLhs);

                if (iType != _dest.end()) {
                    if (iType->second != NULL) {
                        CType * p = f.getRhs()->getMeet(* iType->second).first;

                        if (p == NULL) {
                            if (f.getRhs()->getKind() == CType::Fresh)
                                p = f.getRhs();
                            else if (iType->second->getKind() == CType::Fresh)
                                p = iType->second;
                        }

                        if (p != NULL)
                            iType->second = p;
                    }
                } else
                    _dest[pLhs] = f.getRhs();
            }
        }*/

    }
}

bool Solver::guess() {
    TypeMap tm;

    collectGuessesOut(m_formulas, tm);

    for (TypeMap::iterator i = tm.begin(); i != tm.end(); ++ i) {
        if (i->second == NULL)
            continue;

        if (m_formulas.insert(new tc::Formula(tc::Formula::Equals, i->first, i->second)).second)
            return true;
    }

    tm.clear();

    collectGuessesIn(m_formulas, tm);

    for (TypeMap::iterator i = tm.begin(); i != tm.end(); ++ i) {
        if (i->second == NULL)
            continue;

        if (m_formulas.insert(new tc::Formula(tc::Formula::Equals, i->first, i->second)).second)
            return true;
    }

    return false;
}

bool Solver::compact() {
    tc::FormulaSet::iterator iCF = beginCompound(m_formulas);
    bool bModified = false;

    if (iCF == m_formulas.end())
        return false;

    for (tc::FormulaSet::iterator i = iCF; i != m_formulas.end();) {
        tc::Formula * pFormula = NULL;
        tc::CompoundFormula & cf = (tc::CompoundFormula &) ** i;

        for (size_t j = 0; j < cf.size(); ++ j) {
            tc::Formulas & part = cf.getPart(j);
            tc::Formula * p = NULL;

            if (part.empty()) {
                if (part.substs.empty())
                    continue;

                if (part.substs.size() > 1) {
                    pFormula = NULL;
                    break;
                }

                p = * part.substs.begin();
            } else if (part.empty()) {
                continue;
            } else {
                for (tc::FormulaSet::iterator k = part.begin(); k != part.end(); ++ k) {
                    if (p == NULL) {
                        p = * k;
                        continue;
                    }

                    p = p->mergeAnd(** k);

                    if (p == NULL)
                        break;
                }
            }

            if (p == NULL)
                break;

            if (pFormula == NULL)
                pFormula = p;
            else
                pFormula = pFormula->mergeOr(* p);
        }

        if (pFormula != NULL) {
            m_formulas.erase(i ++);
            m_formulas.insert(pFormula);
            bModified = true;
        } else
            ++ i;
    }

    return bModified;
}

bool Solver::prune() {
    tc::FormulaSet::iterator iCF = beginCompound(m_formulas);
    tc::FormulaList formulas;
    bool bModified = false;

    for (tc::Formulas::iterator i = iCF; i != m_formulas.end();) {
        tc::CompoundFormula & cf = * (tc::CompoundFormula *) * i;
        bool bFormulaModified = false;

        assert(cf.is(tc::Formula::Compound));

        for (size_t j = 0; j < cf.size();) {
            tc::Formulas & part = cf.getPart(j);
            bool bImplies = false;

            for (size_t k = 0; k < cf.size() && ! bImplies; ++ k) {
                if (k == j)
                    continue;

                tc::Formulas & other = cf.getPart(k);

                bImplies = true;

                for (tc::Formulas::iterator l = other.begin(); l != other.end(); ++ l) {
                    tc::Formula & f = ** l;

                    if (! part.implies(f) && ! m_formulas.implies(f)) {
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

            m_formulas.erase(i++);
            bModified = true;
        } else
            ++i;
    }

    if (bModified)
        m_formulas.insert(formulas.begin(), formulas.end());

    return bModified;
}

CType * Solver::find(tc::Formulas & _formulas, const tc::Formula & _f,
        const tc::Formula & _cond)
{
    tc::FormulaSet::iterator iCF = beginCompound(m_formulas);

    assert((_f.getLhs() == NULL && _f.getRhs() != NULL) || (_f.getLhs() != NULL && _f.getRhs() == NULL));
    assert((_cond.getLhs() == NULL && _cond.getRhs() != NULL) || (_cond.getLhs() != NULL && _cond.getRhs() == NULL));

    for (tc::FormulaSet::iterator i = _formulas.begin(); i != iCF; ++ i) {
        if (i == _formulas.end()) {
            if (! m_formulas.empty() && m_formulas.begin() != iCF)
                i = m_formulas.begin();
            else
                break;
        }

        tc::Formula & g = ** i;

        if (! _f.is(tc::Formula::Comparable) && ! _f.is(g.getKind()))
            continue;

        ir::CType * pLhs = g.getLhs();
        ir::CType * pRhs = g.getRhs();

        if (_f.isSymmetric()) {
            if (_f.getLhs() != NULL && * _f.getLhs() != * pLhs)
                std::swap(pLhs, pRhs);

            if (_f.getRhs() != NULL && * _f.getRhs() != * pRhs)
                std::swap(pLhs, pRhs);
        }

        if (_f.getLhs() != NULL && * _f.getLhs() != * pLhs)
            continue;

        if (_f.getRhs() != NULL && * _f.getRhs() != * pRhs)
            continue;

        tc::Formula h = _cond;
        ir::CType * p = (_f.getLhs() == NULL) ? pLhs : pRhs;

        if (h.getLhs() == NULL)
            h.setLhs(p);
        else
            h.setRhs(p);

        if (h.eval() == tc::Formula::True)
            return p;

        pLhs = h.getLhs();
        pRhs = h.getRhs();

        if (_formulas.find(& h) != _formulas.end())
            return p;

        if (& _formulas != & m_formulas && m_formulas.find(& h) != m_formulas.end())
            return p;

        if (! _cond.is(tc::Formula::Equals))
            continue;

//        if (! _cond.isSymmetric())
//            continue;

        if (m_formulas.substs.find(& h) != m_formulas.substs.end())
            return p;

        h.setLhs(pRhs);
        h.setRhs(pLhs);

        if (_formulas.find(& h) != _formulas.end())
            return p;

        if (& _formulas != & m_formulas && m_formulas.find(& h) != m_formulas.end())
            return p;

        if (m_formulas.substs.find(& h) != m_formulas.substs.end())
            return p;
    }

    return NULL;
}

bool Solver::refute(tc::Formula & _f, tc::Formulas & _formulas) {
    CType * a = _f.getLhs();
    CType * b = _f.getRhs();
    CType * c = NULL;

    if (_f.eval() == tc::Formula::False)
        return true;

    // Check if there exists such c for which the relations P and Q hold.
#define CHECK(P,PL,PR,Q,QL,QR) \
        if (find(_formulas, tc::Formula(tc::Formula::P, PL, PR), \
                tc::Formula(tc::Formula::Q, QL, QR)) != NULL) \
            return true

    switch (_f.getKind()) {
        case tc::Formula::Equals:
            CHECK(Comparable, a, c,    Incomparable, c, b);
            CHECK(Comparable, b, c,    Incomparable, c, a);
            CHECK(SubtypeStrict, a, c, Subtype, c, b);
            CHECK(Subtype, a, c,       SubtypeStrict, c, b);
            CHECK(SubtypeStrict, c, a, Subtype, b, c);
            CHECK(Subtype, c, a,       SubtypeStrict, b, c);
            CHECK(SubtypeStrict, b, c, Subtype, c, a);
            CHECK(Subtype, b, c,       SubtypeStrict, c, a);
            CHECK(SubtypeStrict, c, b, Subtype, a, c);
            CHECK(Subtype, c, b,       SubtypeStrict, a, c);
            break;
        case tc::Formula::SubtypeStrict:
            CHECK(Comparable, a, c,    NoMeet, c, b);
            CHECK(Comparable, b, c,    NoJoin, c, a);
            CHECK(Subtype, c, a,       SubtypeStrict, b, c);
            CHECK(SubtypeStrict, c, a, Subtype, b, c);
            CHECK(SubtypeStrict, c, a, Incomparable, b, c);  // C < A && B !~ C
            CHECK(Subtype,       c, a, Incomparable, b, c);  // C <= A && B !~ C
            break;
        case tc::Formula::Subtype:                           // A <= B
            CHECK(Comparable, a, c,    NoMeet, c, b);
            CHECK(Comparable, b, c,    NoJoin, c, a);
            CHECK(Subtype, c, a,       SubtypeStrict, b, c); // B < C <= A
            CHECK(SubtypeStrict, c, a, Subtype, b, c);       // B <= C < A
            CHECK(Subtype, b, c,       SubtypeStrict, c, a); // B <= C < A
            CHECK(SubtypeStrict, b, c, Subtype, c, a);       // B < C <= A
            CHECK(SubtypeStrict, c, a, Incomparable, b, c);  // C < A && B !~ C
            CHECK(Subtype,       c, a, Incomparable, b, c);  // C <= A && B !~ C
            break;
    }

#undef CHECK

    return false;
}

bool Solver::refute(tc::Formulas & _formulas, int & _result) {
    tc::FormulaSet::iterator iCF = beginCompound(_formulas);

    _result = tc::Formula::Unknown;

    for (tc::FormulaSet::iterator i = _formulas.begin(); i != iCF; ++ i) {
        tc::Formula & f = ** i;

        if (! refute(f, _formulas))
            continue;

        _result = tc::Formula::False;
        return true;
    }

    bool bModified = false;
    tc::FormulaList formulas;

    for (tc::FormulaSet::iterator i = iCF; i != _formulas.end();) {
        tc::CompoundFormula & cf = * (tc::CompoundFormula *) * i;
        bool bFormulaModified = false;

        assert(cf.is(tc::Formula::Compound));

        for (size_t j = 0; j < cf.size();) {
            tc::Formulas & part = cf.getPart(j);
            int result = tc::Formula::Unknown;

            if (refute(part, result)) {
                assert(result == tc::Formula::False);
                cf.removePart(j);
                bFormulaModified = true;
            } else
                ++j;
        }

        if (bFormulaModified) {
            if (cf.size() == 0)
                _result = tc::Formula::False;
            else if (cf.size() == 1)
                formulas.insert(formulas.end(), cf.getPart(0).begin(), cf.getPart(0).end());
            else if (cf.size() > 0)
                formulas.push_back(&cf);

            m_formulas.erase(i++);
            bModified = true;
        } else
            ++i;
    }

    if (bModified)
        m_formulas.insert(formulas.begin(), formulas.end());

    return bModified;
}

bool Solver::infere(tc::Formulas & _formulas) {
    tc::FormulaList formulas;
    bool bModified = false;

#define CHECK(R, P,PL,PR,Q,QL,QR) \
    (((R) = find(_formulas, tc::Formula(tc::Formula::P, PL, PR), \
            tc::Formula(tc::Formula::Q, QL, QR))) != NULL)

    for (tc::Formulas::iterator i = _formulas.begin(); i != _formulas.end(); ++ i) {
        tc::Formula & f = ** i;
        CType * a = f.getLhs();
        CType * b = f.getRhs();
        CType * c = NULL;

        if (f.is(tc::Formula::Subtype) && f.hasFresh()) {
            if (CHECK(c, Subtype, b, c, Equals, c, a))
                formulas.push_back(new tc::Formula(tc::Formula::Equals, a, b));
            else if (CHECK(c, Subtype, a, c, Incomparable, b, c)) {
                CType::Extremum meet = b->getMeet(* c);
                if (meet.first != NULL)
                    formulas.push_back(new tc::Formula(tc::Formula::Subtype, a, meet.first));
            }
            c = NULL;
        } else if (f.is(tc::Formula::Compound)) {
            tc::CompoundFormula & cf = * (tc::CompoundFormula *) * i;
            for (size_t j = 0; j < cf.size(); ++ j)
                bModified |= infere(cf.getPart(j));
        }
    }

#undef CHECK

    const size_t c = _formulas.size();

    _formulas.insert(formulas.begin(), formulas.end());

    return bModified || (c != _formulas.size());
}

bool Solver::lift() {
    bool bModified = false;
    tc::FormulaList formulas;

    for (tc::FormulaSet::iterator i = beginCompound(m_formulas);
            i != m_formulas.end();)
    {
        tc::CompoundFormula & cf = * (tc::CompoundFormula *) * i;

        assert(cf.is(tc::Formula::Compound));
        assert(cf.size() > 1);

        tc::FormulaSet & base = cf.getPart(0);
        bool bFormulaModified = false;

        for (tc::FormulaSet::iterator j = base.begin(); j != base.end();) {
            tc::Formula *pFormula = *j;
            bool bFound = true;

            for (size_t k = 1; bFound && k < cf.size(); ++ k) {
                tc::FormulaSet & part = cf.getPart(k);
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

            m_formulas.erase(i++);
            bModified = true;
        } else
            ++i;
    }

    if (bModified)
        m_formulas.insert(formulas.begin(), formulas.end());

    return bModified;
}

bool Solver::expandPredicate(int _kind, CPredicateType * _pLhs, CPredicateType * _pRhs, tc::FormulaList & _formulas) {
    if (_pLhs->getInParams().size() != _pRhs->getInParams().size())
        return false;

    if (_pLhs->getOutParams().size() != _pRhs->getOutParams().size())
        return false;

    for (size_t i = 0; i < _pLhs->getInParams().size(); ++ i) {
        CParam & p = * _pLhs->getInParams().get(i);
        CParam & q = * _pRhs->getInParams().get(i);

        if (p.getType()->getKind() == CType::Type || q.getType()->getKind() == CType::Type)
            _formulas.push_back(new tc::Formula(tc::Formula::Equals, p.getType(), q.getType()));
        else
            _formulas.push_back(new tc::Formula(_kind, p.getType(), q.getType()));
    }

    for (size_t j = 0; j < _pLhs->getOutParams().size(); ++ j) {
        CBranch & b = * _pLhs->getOutParams().get(j);
        CBranch & c = * _pRhs->getOutParams().get(j);

        if (b.size() != c.size())
            return false;

        for (size_t i = 0; i < b.size(); ++ i) {
            CParam & p = * b.get(i);
            CParam & q = * c.get(i);
            _formulas.push_back(new tc::Formula(_kind, q.getType(), p.getType()));
        }
    }

    return true;
}

// It should be more sophisticated than that really. Field names and such..
bool Solver::expandStruct(int _kind, CStructType * _pLhs, CStructType * _pRhs, tc::FormulaList & _formulas) {
    if (_pLhs->getFields().size() != _pRhs->getFields().size())
        return false;

    for (size_t i = 0; i < _pLhs->getFields().size(); ++ i) {
        CNamedValue & p = * _pLhs->getFields().get(i);
        CNamedValue & q = * _pRhs->getFields().get(i);
        _formulas.push_back(new tc::Formula(_kind, p.getType(), q.getType()));
    }

    return true;
}

bool Solver::expandSet(int _kind, CSetType * _pLhs, CSetType * _pRhs, tc::FormulaList & _formulas) {
    _formulas.push_back(new tc::Formula(_kind, _pLhs->getBaseType(), _pRhs->getBaseType()));
    return true;
}

bool Solver::expandList(int _kind, CListType * _pLhs, CListType * _pRhs, tc::FormulaList & _formulas) {
    _formulas.push_back(new tc::Formula(_kind, _pLhs->getBaseType(), _pRhs->getBaseType()));
    return true;
}

bool Solver::expandType(int _kind, CTypeType * _pLhs, CTypeType * _pRhs, tc::FormulaList & _formulas) {
    if (_pLhs->getDeclaration() != NULL && _pLhs->getDeclaration()->getType() != NULL &&
            _pRhs->getDeclaration() != NULL && _pRhs->getDeclaration()->getType() != NULL)
        _formulas.push_back(new tc::Formula(_kind, _pLhs->getDeclaration()->getType(),
                _pRhs->getDeclaration()->getType()));
    return true;
}

bool Solver::expand(tc::Formulas &_formulas, int & _result) {
    tc::FormulaList formulas;
    bool bModified = false;

    _result = tc::Formula::Unknown;

    for (tc::Formulas::iterator i = _formulas.begin(); i != _formulas.end();) {
        tc::Formula &f = **i;
        CType *pLhs = f.getLhs(), *pRhs = f.getRhs();
        bool bFormulaModified = false;

        if (f.is(tc::Formula::Equals | tc::Formula::Subtype | tc::Formula::SubtypeStrict)) {
            bool bResult = true;

            bFormulaModified = true;

            if (pLhs->getKind() == CType::Predicate && pRhs->getKind() == CType::Predicate)
                bResult = expandPredicate(f.getKind(), (CPredicateType *)pLhs, (CPredicateType *)pRhs, formulas);
            else if (pLhs->getKind() == CType::Struct && pRhs->getKind() == CType::Struct)
                bResult = expandStruct(f.getKind(), (CStructType *)pLhs, (CStructType *)pRhs, formulas);
            else if (pLhs->getKind() == CType::Set && pRhs->getKind() == CType::Set)
                bResult = expandSet(f.getKind(), (CSetType *)pLhs, (CSetType *)pRhs, formulas);
            else if (pLhs->getKind() == CType::List && pRhs->getKind() == CType::List)
                bResult = expandList(f.getKind(), (CListType *)pLhs, (CListType *)pRhs, formulas);
            else if (pLhs->getKind() == CType::Type && pRhs->getKind() == CType::Type)
                bResult = expandType(f.getKind(), (CTypeType *)pLhs, (CTypeType *)pRhs, formulas);
            else
                bFormulaModified = false;

            if (bFormulaModified) {
                bModified = true;
                _formulas.erase(i++);
            } else
                ++i;

            if (!bResult)
                _result = tc::Formula::False;
        } else if (f.is(tc::Formula::Compound)) {
            tc::CompoundFormula &cf = (tc::CompoundFormula &)f;

            for (size_t j = 0; j != cf.size(); ++j) {
                bFormulaModified |= expand(cf.getPart(j), _result);

                if (_result == tc::Formula::False)
                    break;
            }

            if (bFormulaModified) {
                formulas.push_back(&cf);
                _formulas.erase(i++);
                bModified = true;
            } else
                ++i;
        } else
            ++i;

        if (_result == tc::Formula::False)
            return true;
    }

    if (bModified)
        m_formulas.insert(formulas.begin(), formulas.end());

    return bModified;
}

bool Solver::unify(tc::Formulas & _formulas) {
    bool bResult = false;
    const bool bCompound = & _formulas != & m_formulas;

    while (! _formulas.empty()) {
        tc::Formula & f = ** _formulas.begin();

        if (! f.is(tc::Formula::Equals))
            break;

        if (* f.getLhs() == * f.getRhs()) {
            _formulas.erase(_formulas.begin());
            continue;
        }

        if (! f.hasFresh())
            break;

        CType * pOld = f.getLhs(), * pNew = f.getRhs();

        if (pOld->getKind() != CType::Fresh && pNew->getKind() != CType::Fresh)
            continue;

        if (! pOld->getKind() == CType::Fresh)
            std::swap(pOld, pNew);

        _formulas.erase(_formulas.begin());
        if (! pOld->compare(* pNew, CType::OrdEquals)) {
            if (_formulas.rewrite(pOld, pNew, bCompound))
                bResult = true;
            _formulas.substs.insert(new tc::Formula(tc::Formula::Equals, pOld, pNew));
        }

        if (& _formulas == & m_formulas)
            bResult = true;
    }

    if (bCompound) {
        _formulas.insert(_formulas.substs.begin(), _formulas.substs.end());
        _formulas.substs.clear();
    }

//    std::wcout << L"Found " << std::distance(beginCompound(_formulas), _formulas.end()) << " compound formulas" << std::endl;

    tc::FormulaList formulas;

    for (tc::FormulaSet::iterator i = beginCompound(_formulas); i != _formulas.end();) {
        tc::CompoundFormula &cf = *(tc::CompoundFormula *)*i;
        bool bFormulaModified = false;

        assert(cf.is(tc::Formula::Compound));

        for (size_t j = 0; j < cf.size(); ++j)
            bFormulaModified |= unify(cf.getPart(j));

        if (bFormulaModified) {
            formulas.push_back(&cf);
            _formulas.erase(i++);
            bResult = true;
        } else
            ++i;
    }

    if (bResult)
        m_formulas.insert(formulas.begin(), formulas.end());

    return bResult;
}

bool Solver::eval(int & _result) {
    bool bChanged = false;

    _result = tc::Formula::True;

    for (tc::Formulas::iterator i = m_formulas.begin(); i != m_formulas.end();) {
        tc::Formula & f = ** i;
        const int r = f.eval();

        if (r == tc::Formula::False) {
            _result = tc::Formula::False;
            break;
        }

        if (r == tc::Formula::Unknown) {
            _result = tc::Formula::Unknown;
            ++ i;
            continue;
        }

        // True.
        m_formulas.erase(i ++);
        bChanged = true;
    }

    return bChanged;
}

bool Solver::run() {
    int result = tc::Formula::Unknown;
    bool bChanged;
    size_t cStep = 0;

    do {
        bChanged = false;

        if (unify(m_formulas)) {
            std::wcout << std::endl << L"Unify [" << cStep << L"]:" << std::endl;
            prettyPrint(m_formulas, std::wcout);
            bChanged = true;
        }

        if (lift()) {
            std::wcout << std::endl << L"Lift [" << cStep << L"]:" << std::endl;
            prettyPrint(m_formulas, std::wcout);
            bChanged = true;
        }

        if (prune()) {
            std::wcout << std::endl << L"Prune [" << cStep << L"]:" << std::endl;
            prettyPrint(m_formulas, std::wcout);
            bChanged = true;
        }

        if (infere(m_formulas)) {
            std::wcout << std::endl << L"Infere [" << cStep << L"]:" << std::endl;
            prettyPrint(m_formulas, std::wcout);
            bChanged = true;
        }

        if (eval(result)) {
            std::wcout << std::endl << L"Eval [" << cStep << L"]:" << std::endl;
            prettyPrint(m_formulas, std::wcout);
            bChanged = true;
        }

        if (result == tc::Formula::False)
            break;

        if (expand(m_formulas, result)) {
            std::wcout << std::endl << L"Expand [" << cStep << L"]:" << std::endl;
            prettyPrint(m_formulas, std::wcout);
            bChanged = true;
        }

        if (result == tc::Formula::False)
            break;

        if (! bChanged && refute(m_formulas, result)) {
            std::wcout << std::endl << L"Refute [" << cStep << L"]:" << std::endl;
            prettyPrint(m_formulas, std::wcout);
            bChanged = true;
        }

        if (result == tc::Formula::False)
            break;

        /*if (! bChanged && compact()) {
            std::wcout << std::endl << L"Compact [" << cStep << L"]:" << std::endl;
            prettyPrint(m_formulas, std::wcout);
            bChanged = true;
        }*/

        if (! bChanged && guess()) {
            std::wcout << std::endl << L"Guess [" << cStep << L"]:" << std::endl;
            prettyPrint(m_formulas, std::wcout);
            bChanged = true;
        }

/*
        bChanged |= unify(m_formulas);
        bChanged |= lift();
        bChanged |= eval(result);
*/
        ++ cStep;
    } while (bChanged && result != tc::Formula::False);

    if (m_formulas.empty())
        result = tc::Formula::True;

    switch (result) {
        case tc::Formula::Unknown:
            std::wcout << std::endl << L"Inference incomplete" << std::endl;
            break;
        case tc::Formula::True:
            std::wcout << std::endl << L"Inference successful" << std::endl;
            break;
        case tc::Formula::False:
            std::wcout << std::endl << L"Type error" << std::endl;
            break;
    }

    return result != tc::Formula::False;
}

bool tc::solve(tc::Formulas & _formulas) {
    Solver solver(_formulas);
    return solver.run();
}
