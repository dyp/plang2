/// \file typecheck.cpp
///

#include <iostream>

#include "utils.h"
#include "typecheck.h"
#include "collect_constraints.h"
#include "prettyprinter.h"

using namespace tc;

size_t FreshType::g_cOrdMax = 0;

ir::NodePtr FreshType::clone(Cloner &_cloner) const {
    return NEW_CLONE(this, _cloner, FreshType(*this));
}

bool FreshType::less(const Type &_other) const {
    return m_cOrd < ((const FreshType &)_other).m_cOrd;
}

bool FormulaCmp::operator()(const FormulaCmp::T &_lhs, const FormulaCmp::T &_rhs) const {
    if (_lhs->getKind() < _rhs->getKind())
        return true;

    if (_rhs->getKind() < _lhs->getKind())
        return false;

    if (_lhs->is(Formula::COMPOUND)) {
        const CompoundFormula &lhs = *_lhs.as<CompoundFormula>();
        const CompoundFormula &rhs = *_rhs.as<CompoundFormula>();

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

    if (!_lhs->getLhs() && _rhs->getLhs())
        return true;

    if (!_rhs->getLhs())
        return false;

    if (!_lhs->getRhs() && _rhs->getRhs())
        return true;

    if (!_rhs->getRhs())
        return false;

    if (_lhs->hasFresh() && !_rhs->hasFresh())
        return true;

    if (_rhs->hasFresh() && !_lhs->hasFresh())
        return false;

    if ((*_lhs->getLhs()) < (*_rhs->getLhs()))
        return true;

    if (!((*_rhs->getLhs()) < (*_lhs->getLhs())) && (*_lhs->getRhs()) < (*_rhs->getRhs()))
        return true;

    return false;
}

bool Formula::hasFresh() const {
    return (m_pLhs && m_pLhs->getKind() == ir::Type::FRESH) ||
            (m_pRhs && m_pRhs->getKind() == ir::Type::FRESH);
}

FormulaPtr Formula::clone(Cloner &_cloner) const {
    return NEW_CLONE(this, _cloner, Formula(m_kind, _cloner.get(m_pLhs.ptr()), _cloner.get(m_pRhs.ptr())));
}

Auto<Formulas> Formulas::clone(Cloner &_cloner) const {
    Auto<Formulas> pNew = ptr(NEW_CLONE(this, _cloner, Formulas()));

    for (FormulaSet::const_iterator i = begin(); i != end(); ++i)
        pNew->insert(_cloner.get(i->ptr()));

    return pNew;
}

FormulaPtr CompoundFormula::clone(Cloner &_cloner) const {
    CompoundFormulaPtr pCF = NEW_CLONE(this, _cloner, CompoundFormula());

    for (size_t i = 0; i < size(); ++i)
        pCF->addPart(getPart(i).clone(_cloner));

    return pCF;
}

// Use clone() here in future.
bool tc::rewriteType(ir::TypePtr &_pType, const ir::TypePtr &_pOld, const ir::TypePtr &_pNew) {
    if (*_pOld == *_pNew)
        return false;

    if (*_pType == *_pOld) {
        if (_pOld->getKind() == ir::Type::FRESH)
            _pNew->rewriteFlags(_pOld.as<FreshType>()->getFlags());
        _pType = _pNew;
        return true;
    }

    return _pType->rewrite(_pOld, _pNew);
}

bool Formula::rewrite(const ir::TypePtr &_pOld, const ir::TypePtr &_pNew) {
    bool bResult = false;

    bResult |= rewriteType(m_pLhs, _pOld, _pNew);
    bResult |= rewriteType(m_pRhs, _pOld, _pNew);

    return bResult;
}

bool Formula::isSymmetric() const {
    switch (getKind()) {
        case EQUALS:
        case COMPARABLE:
        case INCOMPARABLE:
            return true;
        default:
            return false;
    }
}

int Formula::eval() const {
    assert(getKind() != COMPOUND);

    const int nCmp = getLhs()->compare(*getRhs());

    if (nCmp == ir::Type::ORD_UNKNOWN /*|| nCmp == ir::Type::ORD_NONE*/)
        return UNKNOWN;

    switch (getKind()) {
        case EQUALS:
            return nCmp == ir::Type::ORD_EQUALS ? TRUE : FALSE;
        case SUBTYPE:
            return (nCmp == ir::Type::ORD_SUB || nCmp == ir::Type::ORD_EQUALS) ? TRUE : FALSE;
        case SUBTYPE_STRICT:
            return nCmp == ir::Type::ORD_SUB ? TRUE : FALSE;
        case COMPARABLE:
            return nCmp == ir::Type::ORD_NONE ? FALSE : TRUE;
        case INCOMPARABLE:
            return nCmp == ir::Type::ORD_NONE ? TRUE : FALSE;
        case NO_JOIN:
        case HAS_JOIN:
            if (ir::TypePtr pJoin = getLhs()->getJoin(*getRhs()))
                return (pJoin->getKind() == ir::Type::TOP) == (getKind() == NO_JOIN) ? TRUE : FALSE;
            break;
        case NO_MEET:
        case HAS_MEET:
            if (ir::TypePtr pMeet = getLhs()->getMeet(*getRhs()))
                return (pMeet->getKind() == ir::Type::BOTTOM) == (getKind() == NO_MEET) ? TRUE : FALSE;
            break;
    }

    return UNKNOWN;
}

bool Formula::implies(const Formula &_other) {
    switch (getKind()) {
        case SUBTYPE_STRICT:
            return (_other.is(SUBTYPE_STRICT) || _other.is(SUBTYPE)) &&
                getLhs()->compare(*_other.getLhs(), ir::Type::ORD_SUPER | ir::Type::ORD_EQUALS) &&
                getRhs()->compare(*_other.getRhs(), ir::Type::ORD_SUB | ir::Type::ORD_EQUALS);

        case SUBTYPE:
            if (_other.is(SUBTYPE)) {
                return getLhs()->compare(*_other.getLhs(), ir::Type::ORD_SUPER | ir::Type::ORD_EQUALS) &&
                        getRhs()->compare(*_other.getRhs(), ir::Type::ORD_SUB | ir::Type::ORD_EQUALS);
            } else if (_other.is(SUBTYPE_STRICT)) {
                return (getLhs()->compare(*_other.getLhs(), ir::Type::ORD_SUPER) &&
                            getRhs()->compare(*_other.getRhs(), ir::Type::ORD_SUB | ir::Type::ORD_EQUALS)) ||
                        (getLhs()->compare(*_other.getLhs(), ir::Type::ORD_SUPER | ir::Type::ORD_EQUALS) &&
                            getRhs()->compare(*_other.getRhs(), ir::Type::ORD_SUB));
            } else
                return false;

        case EQUALS:
            if (_other.is(EQUALS) || _other.is(SUBTYPE)) {
                return (getLhs()->compare(*_other.getLhs(), ir::Type::ORD_EQUALS) &&
                            getRhs()->compare(*_other.getRhs(), ir::Type::ORD_EQUALS)) ||
                        (getLhs()->compare(*_other.getRhs(), ir::Type::ORD_EQUALS) &&
                            getRhs()->compare(*_other.getLhs(), ir::Type::ORD_EQUALS));
            } else
                return false;

        default:
            return false;
    }
}

void CompoundFormula::addPart(const Auto<Formulas> &_pFormulas) {
    m_parts.push_back(_pFormulas);
}

Formulas &CompoundFormula::addPart() {
    m_parts.push_back(ptr(new Formulas()));
    return *m_parts.back();
}

static
void _check(Formulas &_fs) {
    if (_fs.size() > 1) {
        Formulas::iterator i = _fs.begin();
        FormulaCmp cmp;
        size_t c = 0;

        for (Formulas::iterator j = ::next(i); j != _fs.end(); ++i, ++j, ++c) {
            FormulaPtr p1 = *i;
            FormulaPtr p2 = *j;

            bool b1 = cmp(p1, p2);
            bool b2 = cmp(p2, p1);

            if (!b1 || b2) {
                std::wcout << std::endl << L"Inconsistency at " << c << ":" << std::endl;
                prettyPrint(_fs, std::wcout);
                b1 = cmp(p1, p2);
                b2 = cmp(p2, p1);
            }

            assert(b1);
            assert(!b2);
        }
    }
}

bool Formulas::rewrite(const ir::TypePtr &_pOld, const ir::TypePtr &_pNew) {
    Formulas fs(*this);

    // We have to delete and then reinsert everything since we have'n got proper clone() yet.
    clear();
    substs.clear();

    for (iterator j = fs.begin(); !fs.empty(); j = fs.begin()) {
        FormulaPtr pFormula = *j;

        fs.erase(j);
        pFormula->rewrite(_pOld, _pNew);
        insert(pFormula);
    }

    for (iterator j = fs.substs.begin(); !fs.substs.empty(); j = fs.substs.begin()) {
        FormulaPtr pFormula = *j;
        const bool bSubst = !pFormula->getLhs()->compare(*_pOld, ir::Type::ORD_EQUALS);

        fs.substs.erase(j);
        pFormula->rewrite(_pOld, _pNew);

        if (bSubst)
            substs.insert(pFormula);
        else
            insert(pFormula);
    }

    insert(fs.begin(), fs.end());
    substs.insert(fs.substs.begin(), fs.substs.end());
    _check(*this);

    return !fs.empty() || !fs.substs.empty();
}

bool CompoundFormula::rewrite(const ir::TypePtr &_pOld, const ir::TypePtr &_pNew) {
    bool bResult = false;

    for (size_t i = 0; i < size(); ++ i)
        bResult |= getPart(i).rewrite(_pOld, _pNew);

    return bResult;
}

int CompoundFormula::eval() const {
    int result = FALSE;

    for (size_t i = 0; i < size(); ++i) {
        const Formulas &part = getPart(i);
        int r = TRUE;

        for (Formulas::iterator j = part.begin(); j != part.end(); ++j) {
            switch (const int cmp = (*j)->eval()) {
                case UNKNOWN:
                    if (r == FALSE)
                        break;
                    // no break;
                case FALSE:
                    r = cmp;
            }
        }

        if (r == TRUE)
            return TRUE;

        if (r == UNKNOWN)
            result = UNKNOWN;
    }

    return result;
}

size_t CompoundFormula::count() const {
    size_t result = 0;

    for (size_t i = 0; i < size(); ++ i)
        result += m_parts[i]->size();

    return result;
}

bool Formulas::implies(Formula & _f) const {
    for (const_iterator i = begin(); i != end(); ++i)
        if ((*i)->implies(_f))
            return true;
    return false;
}

void tc::apply(tc::Formulas &_constraints, tc::FreshTypes &_types) {
    for (FormulaSet::iterator i = _constraints.substs.begin(); i != _constraints.substs.end(); ++i) {
        Formula &f = **i;

        assert(f.is(Formula::EQUALS));
        assert(f.getLhs()->getKind() == ir::Type::FRESH);

        typedef tc::FreshTypes::iterator I;
        std::pair<I, I> bounds = _types.equal_range(f.getLhs().as<FreshType>());

        for (I j = bounds.first; j != bounds.second; ++j)
            j->second->setType(f.getRhs());
    }
}
