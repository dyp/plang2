/// \file solve_constraints.cpp
///

#include <iostream>
#include "typecheck.h"
#include "prettyprinter.h"
#include "utils.h"
#include "options.h"
#include "type_lattice.h"

using namespace ir;

typedef std::set<tc::FreshTypePtr, PtrLess<tc::FreshType> > FreshTypeSet;
typedef tc::ContextStack CS;

class Solver {
public:
    typedef bool (Solver::*Operation)(int & /* _result */);

    Solver() : m_nCurrentCFPart(-1) {}

    bool run();
    bool sequence(int &_result);
    bool fork();
    tc::Context &context() { return *CS::top(); }

    // Operations.
    bool unify(int & /* _result */);
    bool lift(int & /* _result */);
    bool eval(int &_result);
    bool prune(int & /* _result */);
    bool refute(int &_result);
    bool compact(int & /* _result */);
    bool infer(int &_result);
    bool expand(int &_result);
    bool explode(int & /* _result */);
    bool guess(int & /* _result */);

protected:
    bool expandPredicate(int _kind, const PredicateTypePtr &_pLhs,
            const PredicateTypePtr &_pRhs, tc::FormulaList &_formulas);
    bool expandStruct(int _kind, const StructTypePtr &_pLhs, const StructTypePtr &_pRhs, tc::FormulaList &_formulas, bool _bAllowCompound);
    bool expandSet(int _kind, const SetTypePtr &_pLhs, const SetTypePtr &_pRhs, tc::FormulaList &_formulas);
    bool expandList(int _kind, const ListTypePtr &_pLhs, const ListTypePtr &_pRhs, tc::FormulaList &_formulas);
    bool expandMap(int _kind, const MapTypePtr &_pLhs, const MapTypePtr &_pRhs, tc::FormulaList &_formulas);
    bool expandArray(int _kind, const ArrayTypePtr &_pLhs, const ArrayTypePtr &_pRhs, tc::FormulaList &_formulas);
    bool expandType(int _kind, const TypeTypePtr &_pLhs, const TypeTypePtr &_pRhs, tc::FormulaList &_formulas);
    bool expandSubtype(int _kind, const SubtypePtr &_pLhs, const SubtypePtr &_pRhs, tc::FormulaList &_formulas);
    bool expandSubtypeWithType(int _kind, const TypePtr &_pLhs, const TypePtr &_pRhs, tc::FormulaList &_formulas);
    bool runCompound(Operation _op, int &_result);

private:
    tc::Formulas::iterator m_iCurrentCF, m_iLastCF;
    int m_nCurrentCFPart;
    FreshTypeSet m_guessIgnored;
    std::set<std::pair<size_t, size_t> > m_redundantParts;
};

bool Solver::fork() {
    tc::Formulas::iterator iCF = context()->beginCompound();

    if (iCF == context()->end())
        return false;

    tc::CompoundFormulaPtr pCF = iCF->as<tc::CompoundFormula>();

    context()->erase(iCF);

    tc::ContextPtr pCopy = clone(context());

    context().insertFormulas(pCF->getPart(0));
    pCF->getPart(0).pFlags->mergeTo(context().flags());

    if (pCF->size() > 2) {
        pCF->removePart(0);
        (*pCopy)->insert(pCF);
    } else {
        pCopy->insertFormulas(pCF->getPart(1));
        pCF->getPart(1).pFlags->mergeTo(pCopy->flags());
    }

    CS::push(pCopy);

    return true;
}

static
bool _validateRelation(const tc::RelationPtr &_pRelation, tc::Lattice &_lattice, void *_pParam) {
    const int lk = _pRelation->getLhs()->getKind();
    const int rk = _pRelation->getRhs()->getKind();

    // No strict supertypes of TOP exist.
    if (lk == ir::Type::TOP && rk != ir::Type::TOP && _pRelation->isStrict())
        return false;

    // No strict subtypes of BOTTOM exist.
    if (lk != ir::Type::BOTTOM && rk == ir::Type::BOTTOM && _pRelation->isStrict())
        return false;

    if (_pRelation->eval() == tc::Formula::FALSE)
        return false;

    // Check occurrences of A <= B < A (or A < B < A).
    if (_lattice.relations().find(new tc::Relation(_pRelation->getRhs(), _pRelation->getLhs(), true)) != _lattice.relations().end())
        return false;

    // Check occurrences of A <= B <= A (or A < B <= A).
    if (_lattice.relations().find(new tc::Relation(_pRelation->getRhs(), _pRelation->getLhs(), false)) != _lattice.relations().end()) {
        if (_pRelation->isStrict())
            return false;

        if (tc::FormulaList *pSubsts = (tc::FormulaList *)_pParam)
            pSubsts->push_back(new tc::Formula(tc::Formula::EQUALS, _pRelation->getLhs(), _pRelation->getRhs()));
    }

    return true;
}

bool Solver::infer(int & _result) {
    bool bModified = false;
    tc::Formulas::iterator iNext = context()->empty() ? context()->end() : ::next(context()->begin());
    tc::FormulaList substs;

    context().pTypes->update(&_validateRelation, &substs);

    if (!context().pTypes->isValid()) {
        _result = tc::Formula::FALSE;
        return true;
    }

    // Substs were added, need to run unify.
    if (!substs.empty()) {
        for (tc::FormulaList::iterator i = substs.begin(); i != substs.end(); ++i)
            bModified |= context().add(*i);

        assert(bModified);
        return bModified;
    }

    context().pTypes->reduce();

    const tc::Relations &relations = context().pTypes->relations();

    // Remove formulas that are no longer used.
    for (tc::Formulas::iterator i = context()->begin(); i != context()->end(); i = iNext) {
        tc::Formula &f = **i;

        iNext = ::next(i);

        if (!f.is(tc::Formula::SUBTYPE | tc::Formula::SUBTYPE_STRICT))
            continue;

        tc::Relations::iterator j = relations.find(new tc::Relation(f));

        // Missing formula could have been replaced by Lattice::update() as a result of applying substs.
        if (j == relations.end() || !(*j)->bUsed) {
            context()->erase(i);
            bModified = true;
        }
    }

    // Add inferred formulas.
    for (tc::Relations::iterator i = relations.begin(); i != relations.end(); ++i) {
        tc::Relation &f = **i;

        if (f.bUsed)
            bModified |= context().add(new tc::Formula(f));
    }

    bModified |= runCompound(&Solver::infer, _result);

    // Check if simple top-level formula is implied by some compound formula.
    tc::Formulas::iterator iCF = context()->beginCompound();

    if (iCF == context()->end())
        return bModified;

    for (tc::Formulas::iterator i = context()->begin(); i != iCF; i = iNext) {
        tc::FormulaPtr pTest = *i;
        bool bIsImplied = false;

        iNext = ::next(i);
        context()->erase(i);

        for (tc::Formulas::iterator j = iCF; j != context()->end(); ++j) {
            tc::CompoundFormula &cf = (tc::CompoundFormula &)**j;

            bIsImplied = true;

            for (size_t k = 0; bIsImplied && k < cf.size(); ++k) {
                CS::push(cf.getPartPtr(k));
                bIsImplied &= context().implies(*pTest);
                CS::pop();
            }

            if (bIsImplied)
                break;
        }

        if (bIsImplied)
            bModified = true;
        else
            context()->insert(pTest);
    }

    return bModified;
}

bool Solver::runCompound(Operation _operation, int &_result) {
    // We assume that unify() and infer() were already run.
    tc::FormulaList formulas;
    tc::Flags flags = context().flags();
    tc::Formulas::iterator iCF = context()->beginCompound();
    std::list<tc::Formulas::iterator> replaced;
    bool bModified = false;

    if (iCF == context()->end())
        return false;

    m_iLastCF = context()->end();

    for (tc::Formulas::iterator i = iCF; i != context()->end(); ++i) {
        tc::CompoundFormula &cf = (tc::CompoundFormula &)**i;
        bool bFormulaModified = false;

        m_iCurrentCF = i;
        m_redundantParts.clear();

        for (size_t j = 0; j < cf.size();) {
            Auto<tc::Formulas> pPart = cf.getPartPtr(j);
            int result = tc::Formula::UNKNOWN;

            CS::push(pPart);
            m_nCurrentCFPart = j;

            if ((this->*_operation)(result)) {
                if (result == tc::Formula::FALSE) {
                    cf.removePart(j);
                } else {
                    pPart->swap(*context().pFormulas);
                    ++j;
                }

                bFormulaModified = true;
            } else
                ++j;

            m_nCurrentCFPart = -1;
            CS::pop();
        }

        if (!m_redundantParts.empty()) {
            std::set<size_t> partsToDelete;

            for (std::set<std::pair<size_t, size_t> >::iterator j = m_redundantParts.begin();
                    j != m_redundantParts.end(); ++j)
                partsToDelete.insert(j->second);

            for (std::set<size_t>::reverse_iterator j = partsToDelete.rbegin(); j != partsToDelete.rend(); ++j)
                if (!CS::top()->implies(cf.getPart(*j))) {
                    bFormulaModified = true;
                    cf.removePart(*j);
                }
        }

        if (bFormulaModified) {
            if (cf.size() == 0)
                _result = tc::Formula::FALSE;
            else if (cf.size() == 1) {
                formulas.insert(formulas.end(), cf.getPart(0).begin(), cf.getPart(0).end());
                cf.getPart(0).pFlags->filterTo(flags, cf.getPart(0));
            } else if (cf.size() > 0)
                formulas.push_back(&cf);

            replaced.push_back(i);
            bModified = true;
        }

        m_iLastCF = i;
    }

    for (std::list<tc::Formulas::iterator>::iterator i = replaced.begin(); i != replaced.end(); ++i)
        context()->erase(*i);

    if (bModified) {
        context().insert(formulas.begin(), formulas.end());
        flags.mergeTo(context().flags());
    }

    return bModified;
}

typedef std::map<tc::FreshTypePtr, std::pair<size_t, size_t>, PtrLess<tc::FreshType> > ExtraBoundsCount;

class FreshTypeEnumerator : public ir::Visitor {
public:
    FreshTypeEnumerator(FreshTypeSet &_types, const TypePtr &_pRoot = NULL,
            const tc::TypeNode *_pCurrentBounds = NULL, ExtraBoundsCount *_pExtraBounds = NULL) :
        m_types(_types), m_pRoot(_pRoot), m_pCurrentBounds(_pCurrentBounds), m_pExtraBounds(_pExtraBounds) {}

    virtual bool visitType(ir::Type &_type) {
        if (_type.getKind() == Type::FRESH) {
            if (m_pRoot) {
                const int mt = m_pRoot->getMonotonicity(_type);

                assert(m_pCurrentBounds != NULL);
                assert(m_pExtraBounds != NULL);

                if (mt == Type::MT_NONE)
                    m_types.insert(ptr(&(tc::FreshType &)_type));
                else if (mt != Type::MT_CONST) {
                    const tc::Relations *pLowers = &m_pCurrentBounds->lowers;
                    const tc::Relations *pUppers = &m_pCurrentBounds->uppers;

                    if (mt == Type::MT_ANTITONE)
                        std::swap(pLowers, pUppers);

                    (*m_pExtraBounds)[&_type].first += pLowers ? pLowers->size() : 0;
                    (*m_pExtraBounds)[&_type].second += pUppers ? pUppers->size() : 0;
                }
            } else
                m_types.insert(ptr(&(tc::FreshType &)_type));
        }

        return true;
    }

private:
    FreshTypeSet &m_types;
    TypePtr m_pRoot;
    const tc::TypeNode *m_pCurrentBounds;
    ExtraBoundsCount *m_pExtraBounds;
};

bool Solver::guess(int & _result) {
    bool bModified = false;
    const bool bCompound = m_nCurrentCFPart >= 0;
    tc::Formulas::iterator iBegin, iEnd;

    // Clear ignored list if processing any normal or the first compound formula in context.
    if (!bCompound || m_iLastCF == context().pParent->pFormulas->end())
        m_guessIgnored.clear();

    if (!bCompound) {
        iBegin = context()->beginCompound();
        iEnd = context()->end();
    } else {
        iBegin = ::next(m_iCurrentCF);
        iEnd = context().pParent->pFormulas->end();
    }

    // Compound formula changed: update saved ignored fresh types with ones from the _previous_ compound formula.
    if (bCompound && m_iCurrentCF != m_iLastCF && m_iLastCF != context().pParent->pFormulas->end()) {
        tc::CompoundFormula &cf = *m_iLastCF->as<tc::CompoundFormula>();

        for (size_t j = 0; j < cf.size(); ++j) {
            tc::Formulas &part = cf.getPart(j);

            for (tc::Formulas::iterator k = part.begin(); k != part.end(); ++k) {
                FreshTypeEnumerator(m_guessIgnored).traverseNode(*(*k)->getLhs());
                FreshTypeEnumerator(m_guessIgnored).traverseNode(*(*k)->getRhs());
            }
        }
    }

    FreshTypeSet ignored = m_guessIgnored;

    // Ignore all fresh types used in compound formulas _after_current_ one.
    for (tc::Formulas::iterator i = iBegin; i != iEnd; ++i) {
        tc::CompoundFormula &cf = *i->as<tc::CompoundFormula>();

        for (size_t j = 0; j < cf.size(); ++j) {
            tc::Formulas &part = cf.getPart(j);

            for (tc::Formulas::iterator k = part.begin(); k != part.end(); ++k) {
                FreshTypeEnumerator(ignored).traverseNode(*(*k)->getLhs());
                FreshTypeEnumerator(ignored).traverseNode(*(*k)->getRhs());
            }
        }
    }

    if (bCompound)
        m_iLastCF = m_iCurrentCF; // Track change of compound formulas.
    else
        m_guessIgnored = ignored; // Remember for use inside of compound formulas.

    context().pTypes->update();
    context().pTypes->reduce();

    const tc::TypeNodes &types = context().pTypes->nodes();
    ExtraBoundsCount extraBounds; // For cases like A <= [ B ] (A is actually bounded above).

    // Mark all non-monotonically contained types as ignored.
    for (tc::TypeNodes::const_iterator i = types.begin(); i != types.end(); ++i)
        if (i->pType->getKind() != Type::FRESH)
            FreshTypeEnumerator(ignored, i->pType, &*i, &extraBounds).traverseNode(*i->pType);

    for (tc::TypeNodes::const_iterator i = types.begin(); i != types.end(); ++i) {
        if (i->pType->getKind() != Type::FRESH)
            continue;

        tc::FreshTypePtr pType = i->pType.as<tc::FreshType>();

        if (ignored.find(pType) != ignored.end())
            continue;

        const tc::Relations &infs = i->lowers;
        const tc::Relations &sups = i->uppers;
        const std::pair<size_t, size_t> &ebCount = extraBounds[pType];
        // Need to preserve zero count to ensure that calling e.g. sups.begin() is valid if count == 1.
        const size_t infCount = infs.size() > 0 ? ebCount.first + infs.size() : 0;
        const size_t supCount = sups.size() > 0 ? ebCount.second + sups.size() : 0;

        // Matching the following patterns:
        //    A1,k(IN) <= *n,m   ->  A*n,m+k(IN)
        //    Bp,k(IN) <= An,1   ->  BAn+p,k(IN)
        //    *p,k <= An,1(OUT)  ->  *An+p,k(OUT)
        //    A1,k <= Bn,m(OUT)  ->  ABn,m+k(OUT)
        // where A, B are fresh types; 1, k, m, n, p are infs/sups count; * is any type.
        TypePtr pUpper, pLower;

        if ((pType->getFlags() & tc::FreshType::PARAM_IN) != 0 && supCount == 1 && !(*sups.begin())->isStrict())
            pUpper = sups.getType(sups.begin());
        else if (supCount == 1 && !(*sups.begin())->isStrict() && sups.getType(sups.begin())->getKind() == Type::FRESH &&
                (sups.getType(sups.begin()).as<tc::FreshType>()->getFlags() & tc::FreshType::PARAM_OUT) != 0)
            pUpper = sups.getType(sups.begin());

        if (infCount == 1 && !(*infs.begin())->isStrict() && infs.getType(infs.begin())->getKind() == Type::FRESH &&
                (infs.getType(infs.begin()).as<tc::FreshType>()->getFlags() & tc::FreshType::PARAM_IN) != 0)
            pLower = infs.getType(infs.begin());
        else if ((pType->getFlags() & tc::FreshType::PARAM_OUT) != 0 && infCount == 1 && !(*infs.begin())->isStrict())
            pLower = infs.getType(infs.begin());

        // Types with both flags set cannot choose between non-fresh bounds.
        if (pType->getFlags() == (tc::FreshType::PARAM_IN | tc::FreshType::PARAM_OUT)) {
            if (pUpper && pUpper->getKind() != Type::FRESH && infCount > 0)
                pUpper = NULL;

            if (pLower && pLower->getKind() != Type::FRESH && supCount > 0)
                pLower = NULL;
        }

        if (TypePtr pOther = pUpper ? pUpper : pLower)
            bModified |= context().add(new tc::Formula(tc::Formula::EQUALS, pType, pOther));
    }

    // We need other strategies to run first if base context was modified.
    return bModified || runCompound(&Solver::guess, _result);
}

bool Solver::compact(int &_result) {
    bool bModified = false;

    if (m_nCurrentCFPart < 0)
        return runCompound(&Solver::compact, _result);

    tc::CompoundFormula &cf = (tc::CompoundFormula &)**m_iCurrentCF;

    if (context()->size() != 1)
        return false;

    for (size_t k = m_nCurrentCFPart + 1; k < cf.size(); ++k) {
        tc::Formulas &other = cf.getPart(k);

        if (other.size() != 1 || context()->size() != 1)
            continue;

        tc::FormulaPtr pSub = *context()->begin(), pEq = *other.begin();

        if (!pEq->is(tc::Formula::EQUALS))
            std::swap(pSub, pEq);

        if (pSub->is(tc::Formula::SUBTYPE_STRICT | tc::Formula::SUBTYPE) && pEq->is(tc::Formula::EQUALS) && (
                (*pSub->getLhs() == *pEq->getLhs() && *pSub->getRhs() == *pEq->getRhs()) ||
                (*pSub->getLhs() == *pEq->getRhs() && *pSub->getRhs() == *pEq->getLhs())))
        {
            context()->clear();
            context()->insert(new tc::Formula(tc::Formula::SUBTYPE, pSub->getLhs(), pSub->getRhs()));
            other.pFlags->filterTo(*context()->pFlags, *pSub);
            bModified = true;
            cf.removePart(k);
            --k;
        }
    }

    return bModified;
}

// lift() has to be run before prune().
bool Solver::prune(int &_result) {
    if (m_nCurrentCFPart < 0)
        return runCompound(&Solver::prune, _result);

    tc::CompoundFormula &cf = (tc::CompoundFormula &)**m_iCurrentCF;

    for (size_t k = 0; k < cf.size(); ++k) {
        if ((int)k != m_nCurrentCFPart) {
            tc::Formulas &other = cf.getPart(k);
            bool bRedundant = true;

            for (tc::Formulas::iterator l = other.begin(); l != other.end(); ++l)
                if (!context().implies(**l)) {
                    bRedundant = false;
                    break;
                }

            if (bRedundant)
                if (m_redundantParts.find(std::make_pair(m_nCurrentCFPart, k)) == m_redundantParts.end())
                    // Causes runCompound() to delete part k.
                    m_redundantParts.insert(std::make_pair(k, m_nCurrentCFPart));
        }
    }

    return false;
}

bool Solver::refute(int &_result) {
    tc::Formulas::iterator iCF = context()->beginCompound();

    _result = tc::Formula::UNKNOWN;

    for (tc::Formulas::iterator i = context()->begin(); i != iCF; ++i) {
        TypePtr a = (*i)->getLhs();
        TypePtr b = (*i)->getRhs();
        TypePtr c;

        // Check if there exists such c for which the relations P and Q hold.
#define CHECK(P,PL,PR,Q,QL,QR) \
        if (context().lookup(tc::Formula(tc::Formula::P, PL, PR),   \
                tc::Formula(tc::Formula::Q, QL, QR))) {             \
            _result = tc::Formula::FALSE;                           \
            return true;                                            \
        }

        if ((*i)->getKind() == tc::Formula::EQUALS) {
            CHECK(COMPARABLE, a, c, INCOMPARABLE, c, b);
            CHECK(COMPARABLE, b, c, INCOMPARABLE, c, a);
        } else {
            CHECK(COMPARABLE, a, c, NO_MEET, c, b);
            CHECK(COMPARABLE, b, c, NO_JOIN, c, a);
        }
#undef CHECK
    }

    return runCompound(&Solver::refute, _result);
}

bool Solver::lift(int &_result) {
    bool bModified = false;
    tc::FormulaList formulas;
    tc::Flags flags;

    for (tc::Formulas::iterator iCF = context()->beginCompound(); iCF != context()->end();) {
        tc::CompoundFormula &cf = *iCF->as<tc::CompoundFormula>();
        bool bFormulaModified = false;

        assert(cf.size() > 1);

        for (size_t cTestPart = 0; cTestPart < cf.size(); ++cTestPart) {
            tc::Formulas &part = cf.getPart(cTestPart);

            for (tc::Formulas::iterator iTest = part.begin(); iTest != part.end();) {
                bool bLift = true;
                tc::Formulas::iterator iNext = ::next(iTest);
                tc::FormulaPtr pTest = *iTest;

                // Test whether pTest can be lifted.
                for (size_t i = 0; bLift && i < cf.size(); ++i) {
                    if (i == cTestPart)
                        continue;

                    CS::push(cf.getPartPtr(i));
                    bLift &= context()->implies(*pTest);
                    CS::pop();
                }

                if (bLift) {
                    part.pFlags->filterTo(flags, *pTest);
                    formulas.push_back(pTest);
                    bFormulaModified = true;

                    // Iterate over all parts and erase the lifted formula.
                    for (size_t i = 0; i < cf.size(); ++i) {
                        if (i == cTestPart)
                            part.erase(iTest);
                        else
                            cf.getPart(i).erase(pTest);

                        if (cf.getPart(i).size() == 0)
                            cf = tc::CompoundFormula(); // Clear formula, no need for other parts anymore.
                    }
                }

                if (cf.size() == 0)
                    break;

                iTest = iNext;
            }
        }

        if (bFormulaModified) {
            if (cf.size() > 0)
                formulas.push_back(&cf);

            context()->erase(iCF++);
            bModified = true;
        } else
            ++iCF;
    }

    if (bModified) {
        context()->insert(formulas.begin(), formulas.end());
        flags.mergeTo(*context()->pFlags);
    }

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
        tc::FormulaList & _formulas, bool _bAllowCompound)
{
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
        else if (_bAllowCompound && pStrict->size() > 1)
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

bool Solver::expandArray(int _kind, const ArrayTypePtr &_pLhs, const ArrayTypePtr &_pRhs, tc::FormulaList &_formulas) {
    _formulas.push_back(new tc::Formula(_kind, _pLhs->getBaseType(), _pRhs->getBaseType()));
    _formulas.push_back(new tc::Formula(_kind, _pRhs->getDimensionType(), _pLhs->getDimensionType()));
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

bool Solver::expandSubtype(int _kind, const SubtypePtr &_pLhs, const SubtypePtr &_pRhs, tc::FormulaList &_formulas) {
    const tc::FreshTypePtr
        pMinType = new tc::FreshType(tc::FreshType::PARAM_IN);

    const NamedValuePtr
        pParam = new NamedValue(L"", pMinType);

    Cloner cloner;
    cloner.inject(pParam, _pLhs->getParam());
    cloner.inject(pParam, _pRhs->getParam());

    const ExpressionPtr
        pImplication = new Binary(Binary::IMPLIES,
            _pLhs->getExpression(), _pRhs->getExpression()),
        pCondition = na::generalize(cloner.get(pImplication));

    const ExpressionPtr
        pImplGen = na::generalize(pImplication),
        pE1 = cloner.get(pImplGen);

    _formulas.push_back(new tc::Formula(tc::Formula::SUBTYPE,
        pMinType, _pLhs->getParam()->getType(), pCondition));
    _formulas.push_back(new tc::Formula(tc::Formula::SUBTYPE,
        pMinType, _pRhs->getParam()->getType(), pCondition));

    return true;
}

bool Solver::expandSubtypeWithType(int _kind, const TypePtr &_pLhs, const TypePtr &_pRhs, tc::FormulaList &_formulas) {
    assert(_pLhs->getKind() == Type::SUBTYPE
        || _pRhs->getKind() == Type::SUBTYPE);

    if (_pLhs->getKind() == Type::SUBTYPE) {
        _formulas.push_back(new tc::Formula(tc::Formula::SUBTYPE,
            _pLhs.as<Subtype>()->getParam()->getType(), _pRhs));
        return true;
    }

    const SubtypePtr& pSubtype = _pRhs.as<Subtype>();
    const TypePtr& pType = _pLhs;

    Cloner cloner;

    const NamedValuePtr
        pParam = new NamedValue(L"", pType);

    cloner.inject(pParam, pSubtype->getParam());

    const ExpressionPtr pCondition = cloner.get(pSubtype->getExpression());

    _formulas.push_back(new tc::Formula(tc::Formula::SUBTYPE,
        pType, pSubtype->getParam()->getType(), na::generalize(pCondition)));

    return true;

}

bool Solver::expand(int &_result) {
    tc::FormulaList formulas;
    bool bModified = false;
    tc::Formulas::iterator iCF = context()->beginCompound();

    _result = tc::Formula::UNKNOWN;

    for (tc::Formulas::iterator i = context()->begin(); i != iCF;) {
        tc::Formula &f = **i;
        TypePtr pLhs = f.getLhs(), pRhs = f.getRhs();
        bool bFormulaModified = false;

        if (f.is(tc::Formula::EQUALS | tc::Formula::SUBTYPE | tc::Formula::SUBTYPE_STRICT)) {
            bool bResult = true;

            bFormulaModified = true;

            if (pLhs->getKind() == Type::PREDICATE && pRhs->getKind() == Type::PREDICATE)
                bResult = expandPredicate(f.getKind(), pLhs.as<PredicateType>(), pRhs.as<PredicateType>(), formulas);
            else if (pLhs->getKind() == Type::STRUCT && pRhs->getKind() == Type::STRUCT)
                bResult = expandStruct(f.getKind(), pLhs.as<StructType>(), pRhs.as<StructType>(),
                        formulas, !context().pParent);
            else if (pLhs->getKind() == Type::SET && pRhs->getKind() == Type::SET)
                bResult = expandSet(f.getKind(), pLhs.as<SetType>(), pRhs.as<SetType>(), formulas);
            else if (pLhs->getKind() == Type::LIST && pRhs->getKind() == Type::LIST)
                bResult = expandList(f.getKind(), pLhs.as<ListType>(), pRhs.as<ListType>(), formulas);
            else if (pLhs->getKind() == Type::MAP && pRhs->getKind() == Type::MAP)
                bResult = expandMap(f.getKind(), pLhs.as<MapType>(), pRhs.as<MapType>(), formulas);
            else if (pLhs->getKind() == Type::ARRAY && pRhs->getKind() == Type::ARRAY)
                bResult = expandArray(f.getKind(), pLhs.as<ArrayType>(), pRhs.as<ArrayType>(), formulas);
            else if (pLhs->getKind() == Type::TYPE && pRhs->getKind() == Type::TYPE)
                bResult = expandType(f.getKind(), pLhs.as<TypeType>(), pRhs.as<TypeType>(), formulas);
            else if (f.getKind() == tc::Formula::SUBTYPE && pLhs->getKind() == Type::SUBTYPE && pRhs->getKind() == Type::SUBTYPE)
                bResult = expandSubtype(f.getKind(), pLhs.as<Subtype>(), pRhs.as<Subtype>(), formulas);
            else if (f.getKind() == tc::Formula::SUBTYPE && (pLhs->getKind() == Type::SUBTYPE || pRhs->getKind() == Type::SUBTYPE))
                bResult = expandSubtypeWithType(f.getKind(), pLhs, pRhs, formulas);
            else
                bFormulaModified = false;

            if (bFormulaModified) {
                bModified = true;
                context()->erase(i++);
            } else
                ++i;

            if (!bResult) {
                _result = tc::Formula::FALSE;
                return true;
            }
        } else
            ++i;
    }

    if (bModified)
        context()->insert(formulas.begin(), formulas.end());

    return runCompound(&Solver::expand, _result) || bModified;
}

bool Solver::explode(int & /* _result */) {
    tc::FormulaList formulas;
    tc::Formulas::iterator iCF = context()->beginCompound();
    std::set<TypePtr, PtrLess<Type> > processed;

    for (tc::Formulas::iterator iFormula = context()->begin(); iFormula != iCF; ++iFormula) {
        tc::Formula &f = **iFormula;
        TypePtr pLhs = f.getLhs(), pRhs = f.getRhs();
        TypePtr pType;

        if (pLhs->getKind() != Type::FRESH && pRhs->getKind() == Type::FRESH)
            std::swap(pLhs, pRhs);

        if (processed.find(pLhs) != processed.end())
            continue;

        tc::FreshTypePtr pFresh = pLhs.as<tc::FreshType>();
        const int nFlags = pFresh->getFlags();
        const int nReversed = ((nFlags & tc::FreshType::PARAM_IN) ? tc::FreshType::PARAM_OUT : 0) |
                ((nFlags & tc::FreshType::PARAM_OUT) ? tc::FreshType::PARAM_IN : 0);

        switch (pRhs->getKind()) {
            case Type::PREDICATE: {
                PredicateTypePtr pOld = pRhs.as<PredicateType>();
                PredicateTypePtr pNew = new PredicateType(pOld->getPreCondition(), pOld->getPostCondition());

                for (size_t i = 0; i < pOld->getInParams().size(); ++i)
                    pNew->getInParams().add(new Param(L"", new tc::FreshType(nReversed)));

                for (size_t j = 0; j < pOld->getOutParams().size(); ++j) {
                    Branch &b = *pOld->getOutParams().get(j);
                    Branch &c = *pNew->getOutParams().add(new Branch(
                            b.getLabel(), b.getPreCondition(), b.getPostCondition()));

                    for (size_t i = 0; i < b.size(); ++i)
                        c.add(new Param(L"", new tc::FreshType(nFlags)));
                }

                pType = pNew;
                break;
            }

            case Type::SET:
                pType = new SetType(new tc::FreshType(nFlags));
                break;
            case Type::LIST:
                pType = new ListType(new tc::FreshType(nFlags));
                break;
            case Type::MAP:
                pType = new MapType(new tc::FreshType(nReversed), new tc::FreshType(nFlags));
                break;
        }

        if (pType) {
            processed.insert(pLhs);
            formulas.push_back(new tc::Formula(tc::Formula::EQUALS, pLhs, pType));
        }
    }

    context()->insert(formulas.begin(), formulas.end());

    // Don't process compound formulas to limit excessive introduction of new fresh types.
    return !formulas.empty();
}

bool Solver::unify(int &_result) {
    bool bModified = false;
    const bool bCompound = m_nCurrentCFPart >= 0;

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

        // Normalize: ensure that lhs is fresh / reorder fresh type rewrite to propagate types with lower ordinals.
        if (pOld->getKind() != Type::FRESH || (pNew->getKind() == Type::FRESH && *pOld < *pNew)) {
            std::swap(pOld, pNew);
            bModified = true;
        }

        context()->erase(context()->begin());

        if (!pOld->compare(*pNew, Type::ORD_EQUALS)) {
            if (context().rewrite(pOld, pNew))
                bModified = true;

            context().pSubsts->insert(new tc::Formula(tc::Formula::EQUALS, pOld, pNew));
            bModified |= !bCompound; // Subformulas of compound formulas don't store their substs separately.
        }
    }

    if (bCompound)
        context()->insert(context().pSubsts->begin(), context().pSubsts->end());

    return runCompound(&Solver::unify, _result) || bModified;
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
            bModified = true;
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

    return runCompound(&Solver::eval, _result) || bModified;
}

bool Solver::sequence(int &_result) {
    bool bModified = false;
    bool bIterationModified;
    size_t cStep = 0;
    struct { Operation op; bool bFreshIteration; std::wstring title; } ops[] = {
            &Solver::unify,     false, L"Unify",
            &Solver::lift,      false, L"Lift",
            &Solver::prune,     false, L"Prune",
            &Solver::compact,   false, L"Compact",
            &Solver::eval,      false, L"Eval",
            &Solver::expand,    true,  L"Expand",
            &Solver::refute,    true,  L"Refute",
            &Solver::infer,     true,  L"Infer",
            &Solver::explode,   true,  L"Explode",
            &Solver::guess,     true,  L"Guess",
            NULL,               false, L""
    };

    _result = tc::Formula::UNKNOWN;

    do {
        bIterationModified = false;

        for (size_t i = 0; ops[i].op; ++i) {
            if (bIterationModified && ops[i].bFreshIteration)
                break;

            if ((this->*ops[i].op)(_result)) {
                if (Options::instance().bVerbose) {
                    std::wcout << std::endl << ops[i].title << L" [" << cStep << L"]:" << std::endl;
                    prettyPrint(context(), std::wcout);
                }

                bIterationModified = true;

                if (_result == tc::Formula::FALSE)
                    break;
            }
        }

        bModified |= bIterationModified;
        ++cStep;
    } while (bIterationModified && _result != tc::Formula::FALSE);

    return bModified;
}

bool Solver::run() {
    int result = tc::Formula::UNKNOWN;
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

        if (Options::instance().bVerbose)
            std::wcout << std::endl << (result == tc::Formula::FALSE ? L"Refuted" : L"Affirmed") << std::endl;

        CS::pop();
    } while (!CS::empty());

    if (processed.empty())
        result = tc::Formula::FALSE;
    else {
        if (::next(processed.begin()) == processed.end())
            CS::push(processed.front());
        else {
            // Recombine all results into a compound formula and simplify it.
            tc::CompoundFormulaPtr pCF = new tc::CompoundFormula();

            for (std::list<tc::ContextPtr>::iterator i = processed.begin(); i != processed.end(); ++i) {
                tc::Context &ctx = **i;
                tc::Formulas &part = pCF->addPart();

                CS::push(*i);
                part.insertFormulas(*ctx.pFormulas);

                for (tc::Formulas::iterator j = ctx.pSubsts->begin(); j != ctx.pSubsts->end(); ++j) {
                    tc::FormulaPtr pSubst = *j;

                    if (pSubst->getLhs().as<tc::FreshType>()->getFlags() == tc::FreshType::PARAM_IN)
                        part.insert(new tc::Formula(tc::Formula::SUBTYPE, pSubst->getLhs(), pSubst->getRhs()));
                    else if (pSubst->getLhs().as<tc::FreshType>()->getFlags() == tc::FreshType::PARAM_OUT)
                        part.insert(new tc::Formula(tc::Formula::SUBTYPE, pSubst->getRhs(), pSubst->getLhs()));
                    else
                        part.insert(pSubst);
                }

                part.pFlags = ctx.pFormulas->pFlags;
                CS::pop();
            }

            CS::push(ptr(new tc::Context()));
            context()->insert(pCF);

            if (Options::instance().bVerbose) {
                std::wcout << std::endl << L"Recombine:" << std::endl;
                prettyPrint(context(), std::wcout);
            }

            sequence(result);
            assert(result != tc::Formula::FALSE);
        }

        result = context()->empty() ? tc::Formula::TRUE : tc::Formula::UNKNOWN;

        if (Options::instance().bVerbose) {
            std::wcout << std::endl << L"Solution:" << std::endl;
            prettyPrint(context(), std::wcout);
        }
    }

    context().applySubsts();

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
    CS::push(ContextPtr(new Context(::ref(&_formulas), ::ref(&_result))));

    if (Options::instance().bVerbose) {
        std::wcout << std::endl << L"Solving:" << std::endl;
        prettyPrint(*CS::top(), std::wcout);
    }

    const bool bResult = Solver().run();

    CS::pop();

    return bResult;
}
