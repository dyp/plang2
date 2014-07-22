/// \file compact.cpp
///

#include "operations.h"

using namespace ir;

namespace tc {

class Compact : public Operation {
public:
    Compact() : Operation(L"Compact", false) {}

protected:
    virtual bool _run(int & _nResult);
};

bool Compact::_run(int & _nResult) {
    bool bModified = false;

    if (m_nCurrentCFPart < 0)
        return _runCompound(_nResult);

    tc::CompoundFormula &cf = (tc::CompoundFormula &)**m_iCurrentCF;

    if (_context()->size() != 1)
        return false;

    for (size_t k = m_nCurrentCFPart + 1; k < cf.size(); ++k) {
        tc::Formulas &other = cf.getPart(k);

        if (other.size() != 1 || _context()->size() != 1)
            continue;

        tc::FormulaPtr pSub = *_context()->begin(), pEq = *other.begin();

        if (!pEq->is(tc::Formula::EQUALS))
            std::swap(pSub, pEq);

        if (pSub->is(tc::Formula::SUBTYPE_STRICT | tc::Formula::SUBTYPE) && pEq->is(tc::Formula::EQUALS) && (
                (*pSub->getLhs() == *pEq->getLhs() && *pSub->getRhs() == *pEq->getRhs()) ||
                (*pSub->getLhs() == *pEq->getRhs() && *pSub->getRhs() == *pEq->getLhs())))
        {
            _context()->clear();
            _context()->insert(new tc::Formula(tc::Formula::SUBTYPE, pSub->getLhs(), pSub->getRhs()));
            other.pFlags->filterTo(*_context()->pFlags, *pSub);
            bModified = true;
            cf.removePart(k);
            --k;
        }
    }

    return bModified;
}

Auto<Operation> Operation::compact() {
    return new Compact();
}

}
