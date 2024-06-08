/// \file lift.cpp
///

#include "operations.h"

using namespace ir;

namespace tc {

class Lift : public Operation {
public:
    Lift() : Operation(L"Lift", false) {}

protected:
    virtual bool _run(int & _nResult);
};

bool Lift::_run(int & _nResult) {
    bool bModified = false;
    tc::FormulaList formulas;
    tc::Flags flags;

    for (tc::Formulas::iterator iCF = _context()->formulas()->beginCompound();
            iCF != _context()->formulas()->end();)
    {
        auto cf = (*iCF)->as<tc::CompoundFormula>();
        bool bFormulaModified = false;

        assert(cf->size() > 1);

        for (size_t cTestPart = 0; cTestPart < cf->size(); ++cTestPart) {
            tc::Formulas &part = cf->getPart(cTestPart);

            for (tc::Formulas::iterator iTest = part.begin(); iTest != part.end();) {
                bool bLift = true;
                tc::Formulas::iterator iNext = std::next(iTest);
                tc::FormulaPtr pTest = *iTest;

                // Test whether pTest can be lifted.
                for (size_t i = 0; bLift && i < cf->size(); ++i) {
                    if (i == cTestPart)
                        continue;

                    tc::ContextStack::push(cf->getPartPtr(i));
                    bLift &= _context()->implies(*pTest);
                    tc::ContextStack::pop();
                }

                if (bLift) {
                    part.pFlags->filterTo(flags, *pTest);
                    formulas.push_back(pTest);
                    bFormulaModified = true;

                    // Iterate over all parts and erase the lifted formula.
                    for (size_t i = 0; i < cf->size(); ++i) {
                        if (i == cTestPart)
                            part.erase(iTest);
                        else
                            cf->getPart(i).erase(pTest);

                        if (cf->getPart(i).size() == 0)
                            cf = std::make_shared<tc::CompoundFormula>(); // Clear formula, no need for other parts anymore.
                    }
                }

                if (cf->size() == 0)
                    break;

                iTest = iNext;
            }
        }

        if (bFormulaModified) {
            if (cf->size() > 0)
                formulas.push_back(cf);

            iCF = _context()->formulas()->erase(iCF);
            bModified = true;
        } else
            ++iCF;
    }

    if (bModified) {
        _context()->insert(formulas.begin(), formulas.end());
        flags.mergeTo(*_context()->formulas()->pFlags);
    }

    return bModified;
}

OperationPtr Operation::lift() {
    return std::make_shared<Lift>();
}

}
