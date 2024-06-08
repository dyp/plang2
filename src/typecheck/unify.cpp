/// \file unify.cpp
///

#include "operations.h"

using namespace ir;

namespace tc {

class Unify : public Operation {
public:
    Unify() : Operation(L"Unify", false) {}

protected:
    virtual bool _run(int & _nResult);
};

bool Unify::_run(int & _nResult) {
    bool bModified = false;
    const bool bCompound = m_nCurrentCFPart >= 0;

    while (!_context()->formulas()->empty()) {
        tc::Formula &f = **_context()->formulas()->begin();

        if (!f.is(tc::Formula::EQUALS))
            break;

        if (*f.getLhs() == *f.getRhs()) {
            _context()->formulas()->erase(_context()->formulas()->begin());
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

        _context()->formulas()->erase(_context()->formulas()->begin());

        if (!pOld->compare(*pNew, Type::ORD_EQUALS)) {
            if (_context()->rewrite(pOld, pNew))
                bModified = true;

            _context()->pSubsts->insert(std::make_shared<tc::Formula>(tc::Formula::EQUALS, pOld, pNew));
            bModified |= !bCompound; // Subformulas of compound formulas don't store their substs separately.
        }
    }

    if (bCompound)
        _context()->formulas()->insert(_context()->pSubsts->begin(), _context()->pSubsts->end());

    return _runCompound(_nResult) || bModified;
}

OperationPtr Operation::unify() {
    return std::make_shared<Unify>();
}

}
