/// \file eval.cpp
///

#include "operations.h"

using namespace ir;

namespace tc {

class Eval : public Operation {
public:
    Eval() : Operation(L"Eval", false) {}

protected:
    virtual bool _run(int & _nResult);
};

bool Eval::_run(int & _nResult) {
    tc::Formulas::iterator iCF = _context()->beginCompound();
    bool bModified = false;

    _nResult = tc::Formula::TRUE;

    for (tc::Formulas::iterator i = _context()->begin(); i != iCF;) {
        tc::Formula &f = **i;
        const int r = f.eval();

        if (r == tc::Formula::FALSE) {
            _nResult = tc::Formula::FALSE;
            bModified = true;
            break;
        }

        if (r == tc::Formula::UNKNOWN) {
            _nResult = tc::Formula::UNKNOWN;
            ++i;
            continue;
        }

        // TRUE.
        _context()->erase(i++);
        bModified = true;
    }

    return _runCompound(_nResult) || bModified;
}

Auto<Operation> Operation::eval() {
    return new Eval();
}

}
