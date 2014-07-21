/// \file solve_constraints.cpp
///

#include <iostream>
#include "typecheck.h"
#include "prettyprinter.h"
#include "utils.h"
#include "options.h"
#include "type_lattice.h"
#include "typecheck/operations.h"

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
    return tc::Operation::guess()->run(_result);
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

bool Solver::expand(int &_result) {
    return tc::Operation::expand()->run(_result);
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

bool tc::solve(const tc::ContextPtr &_pContext) {
    CS::clear();
    CS::push(_pContext);

    if (Options::instance().bVerbose) {
        std::wcout << std::endl << L"Solving:" << std::endl;
        prettyPrint(*CS::top(), std::wcout);
    }

    const bool bResult = Solver().run();

    CS::pop();

    return bResult;
}
