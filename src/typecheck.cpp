/// \file typecheck.cpp
///

#include <iostream>

#include "utils.h"
#include "typecheck.h"
#include "prettyprinter.h"
#include "ir/visitor.h"
#include "type_lattice.h"

using namespace tc;

size_t FreshType::g_cOrdMax = 0;
std::vector<int> FreshType::m_flags;

int Flags::get(size_t _cIdx) const {
    return _cIdx < m_flags.size() ? m_flags[_cIdx] : 0;
}

void Flags::set(size_t _cIdx, int _flags) {
    if (_flags != get(_cIdx)) {
        if (_cIdx >= m_flags.size())
            m_flags.resize(_cIdx + 1);
        m_flags[_cIdx] = _flags;
    }
}

int Flags::add(size_t _cIdx, int _flags) {
    const int flags = get(_cIdx) | _flags;

    set(_cIdx, flags);

    return flags;
}

void Flags::mergeTo(Flags &_to) {
    for (size_t i = 0; i < m_flags.size(); ++i)
        _to.add(i, m_flags[i]);
}

class FlagsCollector : public ir::Visitor {
public:
    FlagsCollector(Flags &_to, Flags &_from) : m_to(_to), m_from(_from) {}

    virtual bool visitType(ir::Type &_type) {
        if (_type.getKind() == ir::Type::FRESH) {
            const size_t cOrd = ((FreshType &)_type).getOrdinal();
            m_to.set(cOrd, m_from.get(cOrd));
        }

        return true;
    }

private:
    Flags &m_to;
    Flags &m_from;
};

void Flags::filterTo(Flags &_to, const class Formula &_from) {
    if (_from.is(Formula::COMPOUND)) {
        CompoundFormula &cf = (CompoundFormula &)_from;

        for (size_t i = 0; i < cf.size(); ++i)
            for (Formulas::iterator j = cf.getPart(i).begin(); j != cf.getPart(i).end(); ++j)
                filterTo(_to, **j);
    } else {
        FlagsCollector(_to, *this).traverseNode(*_from.getLhs());
        FlagsCollector(_to, *this).traverseNode(*_from.getRhs());
    }
}

void Flags::filterTo(Flags &_to, const class Formulas &_from) {
    for (Formulas::iterator j = _from.begin(); j != _from.end(); ++j)
        filterTo(_to, **j);
}

int FreshType::getFlags() const {
    if (ContextStack::empty())
        return 0;

    return ContextStack::top()->flags().get(m_cOrd) |
            (ContextStack::top()->pParent ? ContextStack::top()->pParent->flags().get(m_cOrd) : 0);
}

void FreshType::setFlags(int _flags) {
    assert(_flags == 0 || !ContextStack::empty());
    if (!ContextStack::empty())
        ContextStack::top()->flags().set(m_cOrd, _flags);
}

int FreshType::addFlags(int _flags) {
    assert(_flags == 0 || !ContextStack::empty());
    return ContextStack::empty() ? 0 : ContextStack::top()->flags().add(m_cOrd, _flags);
}

ir::NodePtr FreshType::clone(Cloner &_cloner) const {
    return NEW_CLONE(this, _cloner, FreshType(*this));
}

bool FreshType::less(const Type &_other) const {
    return m_cOrd < ((const FreshType &)_other).m_cOrd;
}

int FreshType::compare(const ir::Type &_other) const {
    if (_other.getKind() == FRESH && getOrdinal() == ((const FreshType &)_other).getOrdinal())
        return ORD_EQUALS;

    return ORD_UNKNOWN;
}

bool FormulaCmp::operator()(const FormulaPtr &_lhs, const FormulaPtr &_rhs) const {
    if (_lhs->getKind() != _rhs->getKind())
        return _lhs->getKind() < _rhs->getKind();

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

    if (_lhs->hasFresh() != _rhs->hasFresh())
        return _lhs->hasFresh() && !_rhs->hasFresh();

    if (!_lhs->getLhs() ^ !_rhs->getLhs())
        return !_lhs->getLhs();
    if (_lhs->getLhs() && _rhs->getLhs() && *_lhs->getLhs() != *_rhs->getLhs())
        return *_lhs->getLhs() < *_rhs->getLhs();

    if (!_lhs->getRhs() || !_rhs->getRhs())
        return !_lhs->getRhs() && _rhs->getRhs();
    if (*_lhs->getRhs() != *_rhs->getRhs())
       return *_lhs->getRhs() < *_rhs->getRhs();

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

    for (Formulas::const_iterator i = pFormulas->begin(); i != pFormulas->end(); ++i)
        pNew->pFormulas->insert(_cloner.get(i->ptr()));

    pNew->pFormulas->pFlags = new Flags(*pFormulas->pFlags);

    for (Formulas::const_iterator i = pSubsts->begin(); i != pSubsts->end(); ++i)
        pNew->pSubsts->insert(_cloner.get(i->ptr()));

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

bool tc::rewriteType(ir::TypePtr &_pType, const ir::TypePtr &_pOld, const ir::TypePtr &_pNew, bool _bRewriteFlags) {
    if (*_pOld == *_pNew)
        return false;

    if (*_pType == *_pOld) {
        if (_pOld->getKind() == ir::Type::FRESH && _bRewriteFlags)
            _pNew->rewriteFlags(_pOld.as<FreshType>()->getFlags());
        _pType = _pNew;
        return true;
    }

    _pType = clone(*_pType);

    return _pType->rewrite(_pOld, _pNew, _bRewriteFlags);
}

bool Formula::rewrite(const ir::TypePtr &_pOld, const ir::TypePtr &_pNew, bool _bRewriteFlags) {
    bool bResult = false;

    bResult |= rewriteType(m_pLhs, _pOld, _pNew, _bRewriteFlags);
    bResult |= rewriteType(m_pRhs, _pOld, _pNew, _bRewriteFlags);

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
                std::wcerr << std::endl << L"Inconsistency at " << c << ":" << std::endl;
                prettyPrint(_fs, std::wcerr);
                b1 = cmp(p1, p2);
                b2 = cmp(p2, p1);
            }

            assert(b1);
            assert(!b2);
        }
    }
}

static
bool rewrite(Formulas &_formulas, const ir::TypePtr &_pOld, const ir::TypePtr &_pNew, bool _bRewriteFlags = true) {
    Formulas reorder;
    bool bModified = false;

    for (Formulas::iterator i = _formulas.begin(); i != _formulas.end();) {
        Formulas::iterator j = ::next(i);
        Formulas::iterator k = i != _formulas.begin() ? ::prev(i) : i;
        FormulaPtr pFormula = *i;

        bModified |= pFormula->rewrite(_pOld, _pNew, _bRewriteFlags);

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

bool Formulas::rewrite(const ir::TypePtr &_pOld, const ir::TypePtr &_pNew, bool _bRewriteFlags) {
    return ::rewrite(*this, _pOld, _pNew, _bRewriteFlags);
}

bool Context::rewrite(const ir::TypePtr &_pOld, const ir::TypePtr &_pNew, bool _bRewriteFlags) {
    bool bModified = false;
    Formulas reorder;

    bModified |= pSubsts->rewrite(_pOld, _pNew, _bRewriteFlags);

    for (Formulas::iterator i = pFormulas->begin(); i != pFormulas->end();) {
        Formulas::iterator j = ::next(i);
        FormulaPtr pFormula = *i;

        if (pFormula->rewrite(_pOld, _pNew, _bRewriteFlags)) {
            pFormulas->erase(i);
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

bool CompoundFormula::rewrite(const ir::TypePtr &_pOld, const ir::TypePtr &_pNew, bool _bRewriteFlags) {
    bool bResult = false;

    for (size_t i = 0; i < size(); ++ i)
        bResult |= ::rewrite(getPart(i), _pOld, _pNew, _bRewriteFlags);

    return bResult;
}

int CompoundFormula::eval() const {
    int result = FALSE;

    for (size_t i = 0; i < size(); ++i) {
        const Formulas &part = getPart(i);
        int r = TRUE;

        for (Formulas::const_iterator j = part.begin(); j != part.end(); ++j) {
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

void Formulas::swap(Formulas &_other) {
    std::set<FormulaPtr, FormulaCmp>::swap(_other);
    pFlags.swap(_other.pFlags);
}

bool Formulas::implies(Formula &_f) const {
    for (const_iterator i = begin(); i != end(); ++i)
        if ((**i == _f) || (*i)->implies(_f))
            return true;

    return false;
}

bool Context::implies(Formula &_f) {
    if (pFormulas->implies(_f) || pSubsts->implies(_f))
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
            CHECK(EQUALS, a, c, EQUALS, c, b);    // A = C && C = B  |-  A = B
            CHECK(EQUALS, b, c, EQUALS, c, a);    // B = C && C = A  |-  A = B
            break;

        case Formula::SUBTYPE_STRICT:
            CHECK(SUBTYPE_STRICT, c, b, SUBTYPE, a, c);    // C < B && A <= C  |-  A < B
            CHECK(SUBTYPE_STRICT, a, c, SUBTYPE, c, b);    // A < C && C <= B  |-  A < B
            CHECK(SUBTYPE, c, b, SUBTYPE_STRICT, a, c);    // C <= B && A < C  |-  A < B
            CHECK(SUBTYPE, a, c, SUBTYPE_STRICT, c, b);    // A <= C && C < B  |-  A < B
            CHECK(SUBTYPE_STRICT, c, b, EQUALS, a, c);     // C < B && A = C   |-  A < B
            CHECK(SUBTYPE_STRICT, a, c, EQUALS, c, b);     // A < C && B = C   |-  A < B
            CHECK(EQUALS, a, c, SUBTYPE_STRICT, c, b);     // A = C && C < B   |-  A < B
            CHECK(EQUALS, b, c, SUBTYPE_STRICT, a, c);     // B = C && A < C   |-  A < B
            break;

        case Formula::SUBTYPE:
            CHECK(SUBTYPE, c, b, SUBTYPE, a, c);    // C <= B && A <= C  |-  A <= B
            CHECK(SUBTYPE, a, c, SUBTYPE, c, b);    // A <= C && C <= B  |-  A <= B
            CHECK(SUBTYPE, c, b, EQUALS, a, c);     // C <= B && A = C   |-  A <= B
            CHECK(SUBTYPE, a, c, EQUALS, c, b);     // A <= C && B = C   |-  A <= B
            CHECK(EQUALS, a, c, SUBTYPE, c, b);     // A = C && C <= B   |-  A <= B
            CHECK(EQUALS, b, c, SUBTYPE, a, c);     // B = C && A <= C   |-  A <= B
            break;
    }

    return false;
}

bool Context::implies(Formulas &_fs) {
    for (tc::Formulas::iterator i = _fs.begin(); i != _fs.end(); ++i)
        if (!implies(**i))
            return false;

    return true;
}

bool Context::add(const FormulaPtr &_pFormula) {
    if (_pFormula->eval() == Formula::TRUE)
        return false;

    if (implies(*_pFormula))
        return false;

    return pFormulas->insert(_pFormula).second;
}

bool Context::add(int _kind, const ir::TypePtr &_pLhs, const ir::TypePtr &_pRhs) {
    return add(new Formula(_kind, _pLhs, _pRhs));
}

void Context::applySubsts() {
    for (auto& i: m_conditions)
        apply(*pSubsts, *i);
}

class PredicateLinker : public ir::Visitor {
public:
    PredicateLinker(ir::Context &_ctx) : Visitor(CHILDREN_FIRST), m_ctx(_ctx) {}

    virtual bool visitPredicateReference(ir::PredicateReference &_ref) {
        ir::Predicates predicetes;

        if (_ref.getTarget() && _ref.getTarget()->isBuiltin())
            return true;

        m_ctx.getPredicates(_ref.getName(), predicetes);
        if (predicetes.empty())
            return true;

        if (_ref.getTarget())
            _ref.setTarget(NULL);

        for (size_t i=0; i<predicetes.size(); ++i) {
            ir::PredicatePtr pPredicate = predicetes.get(i);
            ir::TypePtr pType = pPredicate->getType();

            const size_t szOrd = _ref.getType()->compare(*pType);
            if (szOrd != ir::Type::ORD_EQUALS && szOrd != ir::Type::ORD_SUPER)
                continue;

            if (!_ref.getTarget()
                || _ref.getTarget()->getType()->compare(*pType) == ir::Type::ORD_SUB)
                _ref.setTarget(pPredicate);
        }
        return true;
    }

private:
    ir::Context &m_ctx;
};

void tc::linkPredicates(ir::Context &_ctx, ir::Node &_node) {
    PredicateLinker(_ctx).traverseNode(_node);
}

class FreshTypeRewriter : public ir::Visitor {
public:
    FreshTypeRewriter(const Formulas &_substs) : Visitor(CHILDREN_FIRST), m_substs(_substs) {}

    virtual bool visitExpression(ir::Expression &_expr) {
        VISITOR_TRAVERSE(Type, ExprType, _expr.getType(), _expr, Expression, setType);
        return true;
    }

    virtual bool visitType(ir::Type &_type) {
        if (_type.getKind() == ir::Type::FRESH) {
            Formulas::const_iterator iSubst = m_substs.findSubst(&_type);

            if (iSubst != m_substs.end())
                callSetter((*iSubst)->getRhs());
        }
        return true;
    }

private:
    const Formulas &m_substs;
};

void tc::apply(tc::Formulas &_constraints, ir::Node &_node) {
    FreshTypeRewriter ftr(_constraints);
    ftr.traverseNode(_node);
}

Context::Context() :
        pFormulas(ptr(new Formulas())),
        pSubsts(ptr(new Formulas())),
        pTypes(new Lattice(this))
{
}

Context::Context(const Auto<Formulas> &_pFormulas, const Auto<Formulas> &_pSubsts) :
        pFormulas(_pFormulas),
        pSubsts(_pSubsts),
        pTypes(new Lattice(this))
{
}

Context::Context(const Auto<Formulas> &_pFormulas, const Auto<Context> &_pParent) :
    pFormulas(_pFormulas),
    pSubsts(ptr(new Formulas())),
    pParent(_pParent),
    pTypes(new Lattice(this)),
    m_conditions(_pParent->m_conditions)
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

Formulas::iterator Formulas::findSubst(const ir::TypePtr &_pType) const {
    FormulaPtr pEmpty = new Formula(Formula::EQUALS, _pType, new ir::Type(ir::Type::BOTTOM));
    Formulas::const_iterator i = lower_bound(pEmpty);

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
        m_pFormulas(_pCtx->pFormulas),
        m_bSkipCompound(_bSkipCompound),
        m_bSkipTopSubsts(_bSkipTopSubsts),
        m_iter(_pCtx->pFormulas->begin())
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
    m_pFormulas = m_pCtx->pFormulas;
    m_iter = m_pCtx->pFormulas->begin();

    return eof() ? !next() : true;
}

bool ContextIterator::next() {
    if (!eof())
        ++m_iter;

    if (eof()) {
        if (m_pFormulas == m_pCurrent->pFormulas && (!m_bSkipTopSubsts || m_pCurrent->pParent))
            m_pFormulas = m_pCurrent->pSubsts;
        else if (m_pCurrent->pParent) {
            m_pCurrent = m_pCurrent->pParent.ptr();
            m_pFormulas = m_pCurrent->pFormulas;
        } else
            return false;

        m_iter = m_pFormulas->begin();

        return eof() ? !next() : true;
    }

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
