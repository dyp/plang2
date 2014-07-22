/// \file prune.cpp
///

#include "operations.h"

using namespace ir;

namespace tc {

class Prune : public Operation {
public:
    Prune() : Operation(L"Prune", false) {}

protected:
    virtual bool _run(int & _nResult);
};

// Lift has to be run before Prune.
bool Prune::_run(int & _nResult) {
    if (m_nCurrentCFPart < 0)
        return _runCompound(_nResult);

    tc::CompoundFormula &cf = (tc::CompoundFormula &)**m_iCurrentCF;

    for (size_t k = 0; k < cf.size(); ++k) {
        if ((int)k != m_nCurrentCFPart) {
            tc::Formulas &other = cf.getPart(k);
            bool bRedundant = true;

            for (tc::Formulas::iterator l = other.begin(); l != other.end(); ++l)
                if (!_context().implies(**l)) {
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

Auto<Operation> Operation::prune() {
    return new Prune();
}

}
