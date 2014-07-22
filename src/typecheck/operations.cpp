/// \file operations.cpp
///

#include "operations.h"
#include "ir/visitor.h"
#include "utils.h"

#include <functional>

using namespace ir;

bool tc::Operation::run(int & _result) {
    const bool bResult = _run(_result);
    _clear();
    return bResult;
}

tc::Context& tc::Operation::_context() {
    return *tc::ContextStack::top();
}

bool tc::Operation::_runCompound(int & _result) {
    tc::FormulaList formulas;
    tc::Flags flags = _context().flags();
    tc::Formulas::iterator iCF = _context()->beginCompound();
    std::list<tc::Formulas::iterator> replaced;
    bool bModified = false;

    if (iCF == _context()->end())
        return false;

    m_iLastCF = _context()->end();

    for (tc::Formulas::iterator i = iCF; i != _context()->end(); ++i) {
        tc::CompoundFormula &cf = (tc::CompoundFormula &)**i;
        bool bFormulaModified = false;

        m_iCurrentCF = i;
        m_redundantParts.clear();

        for (size_t j = 0; j < cf.size();) {
            Auto<tc::Formulas> pPart = cf.getPartPtr(j);
            int result = tc::Formula::UNKNOWN;

            tc::ContextStack::push(pPart);
            m_nCurrentCFPart = j;

            if (_run(result)) {
                if (result == tc::Formula::FALSE) {
                    cf.removePart(j);
                } else {
                    pPart->swap(*_context().pFormulas);
                    ++j;
                }

                bFormulaModified = true;
            } else
                ++j;

            m_nCurrentCFPart = -1;
            tc::ContextStack::pop();
        }

        if (!m_redundantParts.empty()) {
            std::set<size_t> partsToDelete;

            for (std::set<std::pair<size_t, size_t> >::iterator j = m_redundantParts.begin();
                    j != m_redundantParts.end(); ++j)
                partsToDelete.insert(j->second);

            for (std::set<size_t>::reverse_iterator j = partsToDelete.rbegin(); j != partsToDelete.rend(); ++j)
                if (!tc::ContextStack::top()->implies(cf.getPart(*j))) {
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
        _context()->erase(*i);

    if (bModified) {
        _context().insert(formulas.begin(), formulas.end());
        flags.mergeTo(_context().flags());
    }

    return bModified;
}

void tc::Operation::_clear() {
    m_nCurrentCFPart = -1;
    m_iCurrentCF = m_iLastCF = _context()->end();
    m_redundantParts.clear();
}

class FreshTypeEnumerator : public ir::Visitor {
public:
    FreshTypeEnumerator(tc::FreshTypeSet &_types, const TypePtr &_pRoot = NULL,
            const tc::TypeNode *_pCurrentBounds = NULL, tc::ExtraBoundsCount *_pExtraBounds = NULL) :
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
    tc::FreshTypeSet &m_types;
    TypePtr m_pRoot;
    const tc::TypeNode *m_pCurrentBounds;
    tc::ExtraBoundsCount *m_pExtraBounds;
};

void tc::Operation::_enumerateFreshTypes(CompoundFormula &_cf, FreshTypeSet &_types) {
    for (size_t i = 0; i < _cf.size(); ++i) {
        tc::Formulas &part = _cf.getPart(i);

        for (tc::Formulas::iterator j = part.begin(); j != part.end(); ++j) {
            FreshTypeEnumerator(_types).traverseNode(*(*j)->getLhs());
            FreshTypeEnumerator(_types).traverseNode(*(*j)->getRhs());
        }
    }
}

bool tc::OperationOnLattice::_run(int & _result) {
    _context().pTypes->update();
    _context().pTypes->reduce();

    tc::FreshTypeSet ignored;
    _findRestrictions(ignored);

    auto boundHandler =
        [&](const Auto<Type>& _pType, const tc::Relations& _lowers, const tc::Relations& _uppers) {
            return this->_handler(_pType, _lowers, _uppers);
        };

    const bool bModifed =
        _context().pTypes->traverse(boundHandler, true, tc::Lattice::Types(ignored.begin(), ignored.end()));

    if (m_nCurrentCFPart >= 0)
        m_iLastCF = m_iCurrentCF; // Track change of compound formulas.

    return bModifed || _runCompound(_result);
}

void tc::OperationOnLattice::_findRestrictions(FreshTypeSet& _ignored) {
    const bool bCompound = m_nCurrentCFPart >= 0;
    tc::Formulas::iterator iBegin, iEnd;

    // Clear ignored list if processing any normal or the first compound formula in context.
    if (!bCompound || m_iLastCF == _context().pParent->pFormulas->end())
        m_ignored.clear();

    if (!bCompound) {
        iBegin = _context()->beginCompound();
        iEnd = _context()->end();
    } else {
        iBegin = ::next(m_iCurrentCF);
        iEnd = _context().pParent->pFormulas->end();
    }

    // Compound formula changed: update saved ignored fresh types with ones from the _previous_ compound formula.
    if (bCompound && m_iCurrentCF != m_iLastCF && m_iLastCF != _context().pParent->pFormulas->end())
        _enumerateFreshTypes(*m_iLastCF->as<tc::CompoundFormula>(), m_ignored);

    _ignored = m_ignored;

    // Ignore all fresh types used in compound formulas _after_current_ one.
    for (tc::Formulas::iterator i = iBegin; i != iEnd; ++i)
        _enumerateFreshTypes(*i->as<tc::CompoundFormula>(), _ignored);

    if (!bCompound)
        m_ignored = _ignored; // Remember for use inside of compound formulas.

    // Mark all non-monotonically contained types as ignored.
    const tc::TypeNodes &types = _context().pTypes->nodes();
    for (tc::TypeNodes::const_iterator i = types.begin(); i != types.end(); ++i)
        if (i->pType->getKind() != Type::FRESH)
            FreshTypeEnumerator(_ignored, i->pType, &*i, &m_extraBounds).traverseNode(*i->pType);
}

size_t tc::OperationOnLattice::_getExtraLowerBoundsCount(const TypePtr& _pType) {
    return m_extraBounds[_pType.as<tc::FreshType>()].first;
}

size_t tc::OperationOnLattice::_getExtraUpperBoundsCount(const TypePtr& _pType) {
    return m_extraBounds[_pType.as<tc::FreshType>()].second;
}
