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

bool FormulaCmp::operator()(const FormulaPtr &_lhs, const FormulaPtr &_rhs) const {
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

            Formulas::const_iterator jl = l.begin();
            Formulas::const_iterator jr = r.begin();

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

Auto<Context> Context::clone(Cloner &_cloner) const {
    Auto<Context> pNew = ptr(NEW_CLONE(this, _cloner, Context()));

    for (Formulas::const_iterator i = fs->begin(); i != fs->end(); ++i)
        pNew->fs->insert(_cloner.get(i->ptr()));

    for (Formulas::const_iterator i = substs->begin(); i != substs->end(); ++i)
        pNew->substs->insert(_cloner.get(i->ptr()));

    pNew->pParent = pParent; // No clone().

    return pNew;
}

FormulaPtr CompoundFormula::clone(Cloner &_cloner) const {
    CompoundFormulaPtr pCF = NEW_CLONE(this, _cloner, CompoundFormula());

    for (size_t i = 0; i < size(); ++i) {
        Formulas &part = pCF->addPart();

        for (Formulas::iterator j = getPart(i).begin(); j != getPart(i).end(); ++j)
            part.insert(_cloner.get(j->ptr()));
    }

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

    _pType = clone(*_pType);

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

bool CompoundFormula::operator ==(const Formula &_other) const {
    if (_other.getKind() != COMPOUND)
        return false;

    CompoundFormula &other = (CompoundFormula &)_other;

    if (size() != other.size())
        return false;

    for (size_t i = 0; i < size(); ++i) {
        const Formulas &l = getPart(i);
        const Formulas &r = other.getPart(i);

        if (l.size() != r.size())
            return false;

        Formulas::const_iterator jl = l.begin();
        Formulas::const_iterator jr = r.begin();

        for (; jl != l.end(); ++jl, ++jr)
            if (**jl != **jr)
                return false;
    }

    return true;
}

static
void _check(Context &_fs) {
    if (_fs->size() > 1) {
        Formulas::iterator i = _fs->begin();
        FormulaCmp cmp;
        size_t c = 0;

        for (Formulas::iterator j = ::next(i); j != _fs->end(); ++i, ++j, ++c) {
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

static
bool rewrite(Formulas &_formulas, const ir::TypePtr &_pOld, const ir::TypePtr &_pNew) {
    Formulas reorder;
    bool bModified = false;

    for (Formulas::iterator i = _formulas.begin(); i != _formulas.end();) {
        Formulas::iterator j = next(i);
        Formulas::iterator k = i != _formulas.begin() ? prev(i) : i;
        FormulaPtr pFormula = *i;

        bModified |= pFormula->rewrite(_pOld, _pNew);

        if ((i != _formulas.begin() && !FormulaCmp()(*k, pFormula)) ||
                (j != _formulas.end() && !FormulaCmp()(pFormula, *j)))
        {
            // Order has changed.
            _formulas.erase(i);
            reorder.insert(pFormula);
        }

        i = j;
    }

    _formulas.insert(reorder.begin(), reorder.end());

    return bModified;
}

bool Formulas::rewrite(const ir::TypePtr &_pOld, const ir::TypePtr &_pNew) {
    return ::rewrite(*this, _pOld, _pNew);
}

bool Context::rewrite(const ir::TypePtr &_pOld, const ir::TypePtr &_pNew) {
    bool bModified = false;
    Formulas reorder;

    bModified |= substs->rewrite(_pOld, _pNew);

    for (Formulas::iterator i = fs->begin(); i != fs->end();) {
        Formulas::iterator j = next(i);
        FormulaPtr pFormula = *i;

        if (pFormula->rewrite(_pOld, _pNew)) {
            fs->erase(i);
            reorder.insert(pFormula);
        }

        i = j;
    }

    bModified |= !reorder.empty();

    for (Formulas::iterator i = reorder.begin(); i != reorder.end(); ++i)
        add(*i);

    _check(*this);

    return bModified;
}

bool CompoundFormula::rewrite(const ir::TypePtr &_pOld, const ir::TypePtr &_pNew) {
    bool bResult = false;

    for (size_t i = 0; i < size(); ++ i)
        bResult |= ::rewrite(getPart(i), _pOld, _pNew);

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

bool Formulas::implies(Formula &_f) const {
    for (const_iterator i = begin(); i != end(); ++i)
        if ((**i == _f) || (*i)->implies(_f))
            return true;

    return false;
}

bool Context::implies(Formula &_f) {
    if (fs->implies(_f) || substs->implies(_f))
        return true;

    if (pParent && pParent->implies(_f))
        return true;

    ir::TypePtr a = _f.getLhs();
    ir::TypePtr b = _f.getRhs();
    ir::TypePtr c;

#define CHECK(P,PL,PR,Q,QL,QR)                                                  \
        if (lookup(Formula(Formula::P, PL, PR), Formula(Formula::Q, QL, QR)))   \
            return true

    switch (_f.getKind()) {
        case Formula::EQUALS:
            break;
        case Formula::SUBTYPE_STRICT:
            CHECK(SUBTYPE, a, c, SUBTYPE_STRICT, c, b);
            CHECK(SUBTYPE_STRICT, a, c, SUBTYPE, c, b);
            CHECK(SUBTYPE_STRICT, a, c, EQUALS, c, b);
            CHECK(EQUALS, a, c, SUBTYPE_STRICT, c, b);
            break;
        case Formula::SUBTYPE:
            CHECK(SUBTYPE, a, c, SUBTYPE, c, b);
            CHECK(SUBTYPE, a, c, EQUALS, c, b);
            CHECK(EQUALS, a, c, SUBTYPE, c, b);
            CHECK(EQUALS, c, a, SUBTYPE, c, b);
            break;
    }

    return false;
}

bool Context::add(const FormulaPtr &_pFormula) {
    if (_pFormula->eval() == Formula::TRUE)
        return false;

    if (implies(*_pFormula))
        return false;

    return fs->insert(_pFormula).second;
}

bool Context::add(int _kind, const ir::TypePtr &_pLhs, const ir::TypePtr &_pRhs) {
    return add(new Formula(_kind, _pLhs, _pRhs));
}

void tc::apply(tc::Formulas &_constraints, tc::FreshTypes &_types) {
    for (Formulas::iterator i = _constraints.begin(); i != _constraints.end(); ++i) {
        Formula &f = **i;

        assert(f.is(Formula::EQUALS));
        assert(f.getLhs()->getKind() == ir::Type::FRESH);

        typedef tc::FreshTypes::iterator I;
        std::pair<I, I> bounds = _types.equal_range(f.getLhs().as<FreshType>());

        for (I j = bounds.first; j != bounds.second; ++j)
            j->second->setType(f.getRhs());
    }
}

Context::Context() :
        fs(ptr(new Formulas())),
        substs(ptr(new Formulas())),
        pExtrema(new Extrema(this))
{
}

Context::Context(const Auto<Formulas> &_fs, const Auto<Formulas> &_substs) :
        fs(_fs),
        substs(_substs),
        pExtrema(new Extrema(this))
{
}

Context::Context(const Auto<Formulas> &_fs, const Auto<Context> &_pParent) :
    fs(_fs),
    substs(ptr(new Formulas())),
    pParent(_pParent),
    pExtrema(new Extrema(this))
{
}

typedef std::list<ContextPtr> ContextList;

ContextList g_ctxs;

ContextPtr ContextStack::top() {
    return g_ctxs.empty() ? ContextPtr() : g_ctxs.back();
}

ContextPtr ContextStack::root() {
    for (ContextPtr pCtx = top(); pCtx; pCtx = pCtx->pParent)
        if (!pCtx->pParent)
            return pCtx;

    return NULL;
}

void ContextStack::push(const ContextPtr &_ctx) {
    g_ctxs.push_back(_ctx);
}

void ContextStack::push(const Auto<Formulas> &_fs) {
    g_ctxs.push_back(ptr(new Context(_fs, top())));
}

void ContextStack::pop() {
    if (!g_ctxs.empty())
        g_ctxs.pop_back();
}

bool ContextStack::empty() {
    return g_ctxs.empty();
}

void ContextStack::clear() {
    g_ctxs.clear();
}

Formulas::iterator Formulas::beginCompound() {
    CompoundFormulaPtr pEmpty = new CompoundFormula();
    return lower_bound(pEmpty);
}

Formulas::iterator Formulas::findSubst(const ir::TypePtr &_pType) {
    FormulaPtr pEmpty = new Formula(Formula::EQUALS, _pType, new ir::Type(ir::Type::BOTTOM));
    Formulas::iterator i = lower_bound(pEmpty);

    if (i == end() || !(*i)->is(Formula::EQUALS) || *(*i)->getLhs() != *_pType)
        return end();

    return i;
}

ir::TypePtr Context::lookup(const Formula &_f, const Formula &_cond) {
    for (ContextIterator it(this, true); !it.eof(); it.next()) {
        Formulas::iterator i(it.getIter());
        Formula &g = **i;

        if (!_f.is(tc::Formula::COMPARABLE) && !_f.is(g.getKind()))
            continue;

        ir::TypePtr pLhs = g.getLhs();
        ir::TypePtr pRhs = g.getRhs();

        if (_f.isSymmetric()) {
            if (_f.getLhs() && *_f.getLhs() != *pLhs)
                std::swap(pLhs, pRhs);

            if (_f.getRhs() && *_f.getRhs() != *pRhs)
                std::swap(pLhs, pRhs);
        }

        if (_f.getLhs() && *_f.getLhs() != *pLhs)
            continue;

        if (_f.getRhs() && *_f.getRhs() != *pRhs)
            continue;

        tc::Formula h = _cond;
        ir::TypePtr p = _f.getLhs() ? pRhs : pLhs;

        if (!h.getLhs())
            h.setLhs(p);
        else
            h.setRhs(p);

        if (h.eval() == tc::Formula::TRUE)
            return p;

        pLhs = h.getLhs();
        pRhs = h.getRhs();

        if (!it.find(&h).eof())
            return p;

        if (h.isSymmetric()) {
            h.setLhs(pRhs);
            h.setRhs(pLhs);

            if (!it.find(&h).eof())
                return p;
        }
    }

    return NULL;
}

ContextIterator::ContextIterator(const ContextIterator &_other) :
        m_pCtx(_other.m_pCtx),
        m_pCurrent(_other.m_pCurrent),
        m_pFormulas(_other.m_pFormulas),
        m_bSkipCompound(_other.m_bSkipCompound),
        m_bSkipTopSubsts(_other.m_bSkipTopSubsts),
        m_iter(_other.m_iter)
{
}

ContextIterator::ContextIterator(Context *_pCtx, bool _bSkipCompound, bool _bSkipTopSubsts) :
        m_pCtx(_pCtx),
        m_pCurrent(_pCtx),
        m_pFormulas(_pCtx->fs),
        m_bSkipCompound(_bSkipCompound),
        m_bSkipTopSubsts(_bSkipTopSubsts),
        m_iter(_pCtx->fs->begin())
{
    if (eof())
        next();
}

bool ContextIterator::eof() {
    return m_iter == m_pFormulas->end() ||
            (m_bSkipCompound == get()->is(Formula::COMPOUND));
}

bool ContextIterator::start() {
    m_pCurrent = m_pCtx;
    m_pFormulas = m_pCtx->fs;
    m_iter = m_pCtx->fs->begin();

    return eof() ? !next() : true;
}

bool ContextIterator::next() {
    if (eof()) {
        if (m_pFormulas == m_pCurrent->fs && (!m_bSkipTopSubsts || m_pCurrent->pParent))
            m_pFormulas = m_pCurrent->substs;
        else if (m_pCurrent->pParent) {
            m_pCurrent = m_pCurrent->pParent.ptr();
            m_pFormulas = m_pCurrent->fs;
        } else
            return false;

        m_iter = m_pFormulas->begin();

        return eof() ? !next() : true;
    }

    ++m_iter;

    return !eof();
}

ContextIterator ContextIterator::find(const FormulaPtr &_f) {
    ContextIterator it(m_pCtx, m_bSkipCompound);

    while (!it.eof()) {
        it.m_iter = it.getFormulas().find(_f);

        if (!it.eof())
            break;

        it.next();
    }

    return it;
}

static const TypeSet g_emptyTypeSet;

const TypeSets &Extrema::infs() {
    if (!m_bValid)
        update();

    return m_lowers;
}

const TypeSets &Extrema::sups() {
    if (!m_bValid)
        update();

    return m_uppers;
}

const TypeSet &Extrema::inf(const ir::TypePtr &_pType) {
    if (!m_bValid)
        update();

    TypeSets::const_iterator iTypes = m_lowers.find(_pType);

    return iTypes == m_lowers.end() ? g_emptyTypeSet : iTypes->second;
}

const TypeSet &Extrema::sup(const ir::TypePtr &_pType) {
    if (!m_bValid)
        update();

    TypeSets::const_iterator iTypes = m_uppers.find(_pType);

    return iTypes == m_lowers.end() ? g_emptyTypeSet : iTypes->second;
}

typedef std::multimap<ir::TypePtr, FormulaPtr, TypePtrCmp> FormulaMap;

static
void _updateBounds(std::vector<Extremum> &_bounds, const ir::TypePtr &_pType, bool _bStrict,
        ir::TypePtr (ir::Type::*_merge)(ir::Type &))
{
    std::list<ir::TypePtr> types;

    types.push_back(_pType);

    while (!types.empty()) {
        ir::TypePtr pType = types.back();
        bool bMerged = false;

        types.pop_back();

        for (size_t i = 0; i != _bounds.size();) {
            Extremum &old = _bounds[i];

            if (ir::TypePtr pNew = (old.pType.ptr()->*_merge)(*pType)) {
                const bool bCurrentMerged = *pNew != *pType;

                bMerged |= bCurrentMerged;

                // Put modified bounds to the processing queue.
                if (*pNew != *old.pType) {
                    std::swap(_bounds[i], _bounds.back());
                    _bounds.pop_back();
                    types.push_back(pNew);
                    continue;
                } else if (!bCurrentMerged) {
                    old.bStrict |= _bStrict;
                    bMerged = true;
                }
            }

            ++i;
        }

        if (!bMerged)
            _bounds.push_back(Extremum(pType, _bStrict));
    }
}

void Extrema::update() {
    FormulaMap order;
    std::pair<FormulaMap::iterator, FormulaMap::iterator> bounds;
    FormulaList relations;
    bool bModified = false;

    m_lowers.clear();
    m_uppers.clear();

    for (ContextIterator it(m_pCtx, true, true); !it.eof(); it.next()) {
        Formula &f = *it.get();

        if (!f.is(Formula::SUBTYPE | Formula::SUBTYPE_STRICT | Formula::EQUALS))
            continue;

        if (!f.getLhs()->hasFresh() && !f.getRhs()->hasFresh())
            continue;

        if (f.is(Formula::EQUALS)) {
            relations.push_back(new Formula(Formula::SUBTYPE, f.getLhs(), f.getRhs()));
            relations.push_back(new Formula(Formula::SUBTYPE, f.getRhs(), f.getLhs()));
        } else
            relations.push_back(it.get());
    }

    // Generate lattice based on constraints.
    for (FormulaList::iterator i = relations.begin(); i != relations.end(); ++i) {
        Formula &f = **i;
        FormulaMap added;

        // A <= ...
        bounds = order.equal_range(f.getLhs());

        for (FormulaMap::iterator j = bounds.first; j != bounds.second; ++j) {
            Formula &g = *j->second;

            // ... <= A
            if (*f.getLhs() == *g.getRhs()) {
                const int k = f.is(Formula::SUBTYPE_STRICT) || g.is(Formula::SUBTYPE_STRICT) ?
                        Formula::SUBTYPE_STRICT : Formula::SUBTYPE;
                FormulaPtr p = new Formula(k, g.getLhs(), f.getRhs());

                if (p->getLhs()->hasFresh())
                    added.insert(std::make_pair(p->getLhs(), p));

                if (p->getRhs()->hasFresh())
                    added.insert(std::make_pair(p->getRhs(), p));
            }
        }

        // ... <= B
        bounds = order.equal_range(f.getRhs());

        for (FormulaMap::iterator j = bounds.first; j != bounds.second; ++j) {
            Formula &g = *j->second;

            // B <= ...
            if (*f.getRhs() == *g.getLhs()) {
                const int k = f.is(Formula::SUBTYPE_STRICT) || g.is(Formula::SUBTYPE_STRICT) ?
                        Formula::SUBTYPE_STRICT : Formula::SUBTYPE;
                Formula *p = new Formula(k, f.getLhs(), g.getRhs());

                if (p->getLhs()->hasFresh())
                    added.insert(std::make_pair(p->getLhs(), p));

                if (p->getRhs()->hasFresh())
                    added.insert(std::make_pair(p->getRhs(), p));
            }
        }

        order.insert(added.begin(), added.end());

        if (f.getLhs()->hasFresh())
            order.insert(std::make_pair(f.getLhs(), &f));

        if (f.getRhs()->hasFresh())
            order.insert(std::make_pair(f.getRhs(), &f));
    }

    for (bounds.first = order.begin(); bounds.first != order.end(); bounds.first = bounds.second) {
        ir::TypePtr pType = bounds.first->first;
        std::vector<Extremum> uppers, lowers;

        bounds.second = order.upper_bound(pType);
        assert(pType->hasFresh());

        for (FormulaMap::iterator i = bounds.first; i != bounds.second; ++i) {
            Formula &f = *i->second;
            const bool bStrict = f.getKind() == Formula::SUBTYPE_STRICT;

            if (f.getLhs() == pType) // A <= ...
                _updateBounds(uppers, f.getRhs(), bStrict, &ir::Type::getMeet);
            else // ... <= A
                _updateBounds(lowers, f.getLhs(), bStrict, &ir::Type::getJoin);
        }

        // Takes care of removing duplicates.
        m_lowers[pType].insert(lowers.begin(), lowers.end());
        m_uppers[pType].insert(uppers.begin(), uppers.end());
    }

    m_bValid = true;
}

bool Formula::contains(const ir::TypePtr &_pType) const {
    return (m_pLhs && (*m_pLhs == *_pType || m_pLhs->contains(_pType))) ||
            (m_pRhs && (*m_pRhs == *_pType || m_pRhs->contains(_pType)));
}

bool CompoundFormula::contains(const ir::TypePtr &_pType) const {
    for (size_t i = 0; i < size(); ++i) {
        const Formulas &part = getPart(i);

        for (Formulas::iterator j = part.begin(); j != part.end(); ++j)
            if ((*j)->contains(_pType))
                return true;
    }

    return false;
}
