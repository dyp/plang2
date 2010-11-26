/// \file typecheck.cpp
///

#include <iostream>

#include "utils.h"
#include "typecheck.h"
#include "collect_constraints.h"
#include "prettyprinter.h"

using namespace tc;

size_t FreshType::g_cOrdMax = 0;

ir::CType * FreshType::clone() const {
    return new FreshType(* this);
}

bool FreshType::operator <(const CType & _other) const {
    if (_other.getKind() != ir::CType::Fresh)
        return true;

    return m_cOrd < ((const FreshType &) _other).m_cOrd;
}

bool FormulaCmp::operator()(const FormulaCmp::T & _lhs,
        const FormulaCmp::T & _rhs) const
{
    if (_lhs->getKind() < _rhs->getKind())
        return true;

    if (_rhs->getKind() < _lhs->getKind())
        return false;

    if (_lhs->is(Formula::Compound)) {
        const CompoundFormula &lhs = *(const CompoundFormula *)_lhs;
        const CompoundFormula &rhs = *(const CompoundFormula *)_rhs;

        if (lhs.size() != rhs.size())
            return lhs.size() < rhs.size();

        for (size_t i = 0; i < lhs.size(); ++i) {
            const Formulas &l = lhs.getPart(i);
            const Formulas &r = rhs.getPart(i);

            if (l.size() != r.size())
                return l.size() < r.size();

            FormulaSet::const_iterator jl = l.begin();
            FormulaSet::const_iterator jr = r.begin();

            for (; jl != l.end(); ++jl, ++jr) {
                if ((*this)(*jl, *jr))
                    return true;
                if ((*this)(*jr, *jl))
                    return false;
            }
        }

        return false;
    }

    if (_lhs->getLhs() == NULL && _rhs->getLhs() != NULL)
        return true;

    if (_rhs->getLhs() == NULL)
        return false;

    if (_lhs->getRhs() == NULL && _rhs->getRhs() != NULL)
        return true;

    if (_rhs->getRhs() == NULL)
        return false;

    if (_lhs->hasFresh() && ! _rhs->hasFresh())
        return true;

    if (_rhs->hasFresh() && ! _lhs->hasFresh())
        return false;

    if ((* _lhs->getLhs()) < (* _rhs->getLhs()))
        return true;

    if (! ((* _rhs->getLhs()) < (* _lhs->getLhs())) && (* _lhs->getRhs()) < (* _rhs->getRhs()))
        return true;

    return false;
}

bool Formula::hasFresh() const {
    return (m_pLhs != NULL && m_pLhs->getKind() == ir::CType::Fresh) ||
            (m_pRhs != NULL && m_pRhs->getKind() == ir::CType::Fresh);
}

Formula *Formula::clone() const {
    return new Formula(m_kind, m_pLhs->clone(), m_pRhs->clone());
}

Formulas *Formulas::clone() const {
    Formulas *pNew = new Formulas();

    for (FormulaSet::const_iterator i = begin(); i != end(); ++i)
        pNew->insert((*i)->clone());

    return pNew;
}

Formula *CompoundFormula::clone() const {
    CompoundFormula *pCF = new CompoundFormula();

    for (size_t i = 0; i < size(); ++i)
        pCF->addPart(getPart(i).clone());

    return pCF;
}

//int Formula::invKind() const {
//    switch (getKind()) {
//        case Equals: return Equals;
//        case Equals: return Equals;
//    }
//}

// Use clone here in future.
bool tc::rewriteType(ir::CType * & _pType, ir::CType * _pOld, ir::CType * _pNew) {
    if (* _pOld == * _pNew)
        return false;

    if (* _pType == * _pOld) {
        if (_pOld->getKind() == ir::CType::Fresh)
            _pNew->rewriteFlags(((FreshType *) _pOld)->getFlags());
        _pType = _pNew;
        return true;
    }

    return _pType->rewrite(_pOld, _pNew);
}

bool Formula::rewrite(ir::CType * _pOld, ir::CType * _pNew) {
    bool bResult = false;

    bResult |= rewriteType(m_pLhs, _pOld, _pNew);
    bResult |= rewriteType(m_pRhs, _pOld, _pNew);

    return bResult;
}

bool Formula::isSymmetric() const {
    switch (getKind()) {
        case Equals:
        case Comparable:
        case Incomparable:
            return true;
        default:
            return false;
    }
}

int Formula::eval() const {
    assert(getKind() != Compound);

    const int nCmp = getLhs()->compare(* getRhs());

    if (nCmp == ir::CType::OrdUnknown /*|| nCmp == ir::CType::OrdNone*/)
        return Unknown;

    switch (getKind()) {
        case Equals:
            return nCmp == ir::CType::OrdEquals ? True : False;
        case Subtype:
            return (nCmp == ir::CType::OrdSub || nCmp == ir::CType::OrdEquals) ? True : False;
        case SubtypeStrict:
            return nCmp == ir::CType::OrdSub ? True : False;
        case Comparable:
            return nCmp == ir::CType::OrdNone ? False : True;
        case Incomparable:
            return nCmp == ir::CType::OrdNone ? True : False;
        case NoJoin:
            return getLhs()->getJoin(* getRhs()).second;
        case NoMeet:
            return getLhs()->getMeet(* getRhs()).second;
    }

    return Unknown;
}

bool Formula::implies(Formula & _other) {
    switch (getKind()) {
        case SubtypeStrict:
            return (_other.is(SubtypeStrict) || _other.is(Subtype)) &&
                getLhs()->compare(* _other.getLhs(), ir::CType::OrdSuper | ir::CType::OrdEquals) &&
                getRhs()->compare(* _other.getRhs(), ir::CType::OrdSub | ir::CType::OrdEquals);

        case Subtype:
            if (_other.is(Subtype)) {
                return getLhs()->compare(* _other.getLhs(), ir::CType::OrdSuper | ir::CType::OrdEquals) &&
                        getRhs()->compare(* _other.getRhs(), ir::CType::OrdSub | ir::CType::OrdEquals);
            } else if (_other.is(SubtypeStrict)) {
                return (getLhs()->compare(* _other.getLhs(), ir::CType::OrdSuper) &&
                            getRhs()->compare(* _other.getRhs(), ir::CType::OrdSub | ir::CType::OrdEquals)) ||
                        (getLhs()->compare(* _other.getLhs(), ir::CType::OrdSuper | ir::CType::OrdEquals) &&
                            getRhs()->compare(* _other.getRhs(), ir::CType::OrdSub));
            } else
                return false;

        case Equals:
            if (_other.is(Equals) || _other.is(Subtype)) {
                return (getLhs()->compare(* _other.getLhs(), ir::CType::OrdEquals) &&
                            getRhs()->compare(* _other.getRhs(), ir::CType::OrdEquals)) ||
                        (getLhs()->compare(* _other.getRhs(), ir::CType::OrdEquals) &&
                            getRhs()->compare(* _other.getLhs(), ir::CType::OrdEquals));
            } else
                return false;

        default:
            return false;
    }
}

Formula * Formula::mergeAnd(Formula & _other) {
    FormulaCmp cmp;

    if (cmp(this, & _other) || cmp(& _other, this))
        return NULL;

    // Currently returns non-NULL only if equal.
    return this;
}

Formula * Formula::mergeOr(Formula & _other) {
    FormulaCmp cmp;

    if (! cmp(this, & _other) && ! cmp(& _other, this))
        return this;

    if (implies(_other))
        return & _other;

    if (_other.implies(* this))
        return this;

    Formula * a = this, * b = & _other;

    // Reorder if needed.
    if (! cmp(a, b))
        std::swap(a, b);

    if (a->is(Equals) && b->is(SubtypeStrict)) {
        if ((a->getLhs()->compare(* b->getLhs(), ir::CType::OrdEquals) &&
                a->getRhs()->compare(* b->getRhs(), ir::CType::OrdEquals)) ||
            (a->getLhs()->compare(* b->getRhs(), ir::CType::OrdEquals) &&
                a->getRhs()->compare(* b->getLhs(), ir::CType::OrdEquals)))
        {
            return new Formula(Subtype, b->getLhs(), b->getRhs());
        }
    }

    if (a->is(Subtype) || b->is(Subtype | SubtypeStrict)) {
        if (a->getLhs()->compare(* b->getRhs(), ir::CType::OrdEquals) &&
                b->getLhs()->compare(* a->getRhs(), ir::CType::OrdEquals | ir::CType::OrdSub))
        {
            return NULL; // Actually it's True.
        }

        if (a->getRhs()->compare(* b->getLhs(), ir::CType::OrdEquals) &&
                a->getLhs()->compare(* b->getRhs(), ir::CType::OrdEquals | ir::CType::OrdSub))
        {
            return NULL; // Actually it's True.
        }
    }

    return NULL;
}

void CompoundFormula::addPart(Formulas *_pFormulas) {
    m_parts.push_back(_pFormulas);
}

Formulas & CompoundFormula::addPart() {
    m_parts.push_back(new Formulas());
    return * m_parts.back();
}

static
void _check(Formulas & _fs) {
    if (_fs.size() > 1) {
        Formulas::iterator i = _fs.begin();
        FormulaCmp cmp;
        size_t c = 0;

        for (Formulas::iterator j = next(i); j != _fs.end(); ++ i, ++ j, ++c) {
            Formula * p1 = * i;
            Formula * p2 = * j;

            bool b1 = cmp(p1, p2);
            bool b2 = cmp(p2, p1);

            if (!b1 || b2) {
                std::wcout << std::endl << L"Inconsistency at " << c << ":" << std::endl;
                prettyPrint(_fs, std::wcout);
            }

            assert(b1);
            assert(! b2);
        }
    }
}

bool Formulas::rewrite(ir::CType * _pOld, ir::CType * _pNew, bool _bKeepOrig) {
    bool bResult = false;
//    FormulaList keep;

    _check(* this);

    for (iterator j = begin(); j != end();) {
        Formula * pFormula = * j;
//        Formula * pKeep = NULL;
        //iterator jNext = next(j);
        //Formula * pNext = (jNext == end() ? NULL : * jNext);

//        if (_bKeepOrig && ! pFormula->is(Formula::Compound)) {
//            pKeep = new Formula(pFormula->getKind(), pFormula->getLhs()->clone(),
//                pFormula->getRhs()->clone());
//        }

        if (pFormula->rewrite(_pOld, _pNew)) {
//            if (pKeep != NULL)
//                keep.push_back(pKeep);

            // Reinsert to preserve order.
            erase(j ++);
            insert(pFormula);
            //if (pNext == NULL)
            //    break;
            //j = lower_bound(pNext);
            bResult = true;
        } else
            ++ j;
    }

    for (iterator j = substs.begin(); j != substs.end();) {
        Formula * pFormula = * j;
//        Formula * pKeep = NULL;
        //iterator jNext = next(j);
        //Formula * pNext = (jNext == end() ? NULL : * jNext);

//        if (_bKeepOrig && ! pFormula->is(Formula::Compound)) {
//            pKeep = new Formula(pFormula->getKind(), pFormula->getLhs()->clone(),
//                pFormula->getRhs()->clone());
//        }

        if (pFormula->getLhs()->compare(* _pOld, ir::CType::OrdEquals)) {
//            if (pKeep != NULL)
//                keep.push_back(pKeep);
            pFormula->rewrite(_pOld, _pNew);
            substs.erase(j ++);
            insert(pFormula);
            bResult = true;
        } else if (pFormula->rewrite(_pOld, _pNew)) {
//            if (pKeep != NULL)
//                keep.push_back(pKeep);
            // Reinsert to preserve order.
            substs.erase(j ++);
            substs.insert(pFormula);
            //if (pNext == NULL)
            //    break;
            //j = lower_bound(pNext);
            bResult = true;
        } else
            ++ j;
    }

/*    for (iterator j = substs.begin(); j != substs.end();) {
        Formula * pFormula = * j;
        //iterator jNext = next(j);
        //Formula * pNext = (jNext == substs.end() ? NULL : * jNext);
        ir::CType * pRhs = pFormula->getRhs();

        //std::wcout << L"pNext = " << fmtInt((int64_t) pNext, L"0x%016x") << std::endl;

        if (rewriteType(pRhs, _pOld, _pNew)) {
            // Reinsert to preserve order.
            substs.erase(j ++);
            pFormula->setRhs(pRhs);
            substs.insert(pFormula);
            bResult = true;
        } else
            ++ j;
    }
*/
//    insert(keep.begin(), keep.end());

    _check(* this);

    return bResult;
}

bool CompoundFormula::rewrite(ir::CType * _pOld, ir::CType * _pNew) {
    bool bResult = false;

    for (size_t i = 0; i < size(); ++ i)
        bResult |= getPart(i).rewrite(_pOld, _pNew);

    return bResult;
}

int CompoundFormula::eval() const {
    int result = False;

    for (size_t i = 0; i < size(); ++i) {
        const Formulas & part = getPart(i);
        int r = True;

        for (Formulas::iterator j = part.begin(); j != part.end(); ++j) {
            switch (int cmp = (* j)->eval()) {
                case Unknown:
                    if (r == False)
                        break;
                    // no break;
                case False:
                    r = cmp;
            }
        }

        if (r == True)
            return True;

        if (r == Unknown)
            result = Unknown;
    }

    return result;
}

size_t CompoundFormula::count() const {
    size_t result = 0;

    for (size_t i = 0; i < size(); ++ i)
        result += m_parts[i]->size();

    return result;
}

void CompoundFormula::merge(Formulas & _dest) {

}

bool Formulas::implies(Formula & _f) const {
    for (const_iterator i = begin(); i != end(); ++ i)
        if ((* i)->implies(_f))
            return true;
    return false;
}

Formula * Formulas::lookup(int _op, int _ordLhs, ir::CType * _pLhs,
        int _ordRhs, ir::CType * _pRhs)
{
/*    bool bRepeat = true;

    do {
        Formula f(_op, NULL, NULL);

        if (_ordLhs == ir::CType::OrdEquals)
            f.setLhs(_pLhs);

        if (_ordRhs == ir::CType::OrdEquals)
            f.setRhs(_pRhs);

        for (FormulaSet::iterator i = lower_bound(& f); i != end(); ++ i) {
            Formula & g = ** i;

            if (g.getKind() != f.getKind())
                break;

            if (f.getLhs() != NULL && g.getLhs() != f.getLhs())
                break;

            if (f.getRhs() != NULL && g.getRhs() != f.getRhs())
                break;

            if (g.getLhs()->compare(_pLhs, _ordLhs) && g.getRhs()->compare(_pRhs, _ordRhs))
                return & g;
        }

        if (_op != Formula::Equals)
            break;

        for (FormulaSet::iterator i = substs.lower_bound(& f); i != substs.end(); ++ i) {
            Formula & g = ** i;

            if (f.getLhs() != NULL && g.getLhs() != f.getLhs())
                break;

            if (f.getRhs() != NULL && g.getRhs() != f.getRhs())
                break;

            if (g.getLhs()->compare(_pLhs, _ordLhs) && g.getRhs()->compare(_pRhs, _ordRhs))
                return & g;
        }

        if (bRepeat) {
            bRepeat = false;
            std::swap(_pLhs, _pRhs);
            std::swap(_ordLhs, _ordRhs);
        } else
            break;
    } while (true);
*/
    return NULL;
}


void typecheck(ir::CPredicate * _pPredicate, CContext & _ctx) {
    /*ir::constraints_t constraints;
    collectConstraints(constraints, _pPredicate, _ctx);*/
}

void tc::apply(tc::Formulas & _constraints, tc::FreshTypes & _types) {
    for (FormulaSet::iterator i = _constraints.substs.begin(); i != _constraints.substs.end(); ++ i) {
        Formula & f = ** i;

        assert(f.is(Formula::Equals));
        assert(f.getLhs()->getKind() == ir::CType::Fresh);

        typedef tc::FreshTypes::iterator I;
        std::pair<I, I> bounds = _types.equal_range((FreshType *) f.getLhs());

        for (I j = bounds.first; j != bounds.second; ++j) {
            j->second->setType(f.getRhs());
        }

        //((FreshType *) f.getLhs())->replaceType(f.getRhs());
    }
}





