/// \file refute.cpp
///

#include "operations.h"

using namespace ir;

namespace tc {

class Refute : public Operation {
public:
    Refute() : Operation(L"Refute", true) {}

protected:
    virtual bool _run(int & _nResult);
};

bool Refute::_run(int & _nResult) {
    const auto iCF = _context()->formulas()->beginCompound();

    _nResult = tc::Formula::UNKNOWN;

    for (tc::Formulas::iterator i = _context()->formulas()->begin(); i != iCF; ++i) {
        TypePtr a = (*i)->getLhs();
        TypePtr b = (*i)->getRhs();
        TypePtr c;

        // Check if there exists such c for which the relations P and Q hold.
#define CHECK(P,PL,PR,Q,QL,QR) \
        if (_context()->lookup(std::make_shared<tc::Formula>(tc::Formula::P, PL, PR),   \
                std::make_shared<tc::Formula>(tc::Formula::Q, QL, QR))) {             \
            _nResult = tc::Formula::FALSE;                           \
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

    return _runCompound(_nResult);
}

OperationPtr Operation::refute() {
    return std::make_shared<Refute>();
}

}
