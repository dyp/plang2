/// \file explode.cpp
///

#include "operations.h"

using namespace ir;

namespace tc {

class Explode : public Operation {
public:
    Explode() : Operation(L"Explode", true) {}

protected:
    virtual bool _run(int & _nResult);
};

bool Explode::_run(int & /* _nResult */) {
    tc::FormulaList formulas;
    tc::Formulas::iterator iCF = _context()->beginCompound();
    std::set<TypePtr, PtrLess<Type> > processed;

    for (tc::Formulas::iterator iFormula = _context()->begin();
            iFormula != iCF; ++iFormula)
    {
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

    _context()->insert(formulas.begin(), formulas.end());

    // Don't process compound formulas to limit excessive introduction of new fresh types.
    return !formulas.empty();
}

Auto<Operation> Operation::explode() {
    return new Explode();
}

}
