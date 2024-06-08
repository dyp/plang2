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
    const auto iCF = _context()->formulas()->beginCompound();
    std::set<TypePtr, PtrLess<Type> > processed;

    for (tc::Formulas::iterator iFormula = _context()->formulas()->begin();
            iFormula != iCF; ++iFormula)
    {
        tc::Formula &f = **iFormula;
        TypePtr pLhs = f.getLhs(), pRhs = f.getRhs();
        TypePtr pType;

        if (pLhs->getKind() != Type::FRESH && pRhs->getKind() == Type::FRESH)
            std::swap(pLhs, pRhs);

        if (processed.find(pLhs) != processed.end())
            continue;

        tc::FreshTypePtr pFresh = pLhs->as<tc::FreshType>();
        const int nFlags = pFresh->getFlags();
        const int nReversed = ((nFlags & tc::FreshType::PARAM_IN) ? tc::FreshType::PARAM_OUT : 0) |
                ((nFlags & tc::FreshType::PARAM_OUT) ? tc::FreshType::PARAM_IN : 0);

        switch (pRhs->getKind()) {
            case Type::PREDICATE: {
                const auto pOld = pRhs->as<PredicateType>();
                const auto pNew = std::make_shared<PredicateType>(pOld->getPreCondition(), pOld->getPostCondition());

                for (size_t i = 0; i < pOld->getInParams().size(); ++i)
                    pNew->getInParams().add(std::make_shared<Param>(L"", std::make_shared<tc::FreshType>(nReversed)));

                for (size_t j = 0; j < pOld->getOutParams().size(); ++j) {
                    Branch &b = *pOld->getOutParams().get(j);
                    Branch &c = *pNew->getOutParams().add(std::make_shared<Branch>(
                            b.getLabel(), b.getPreCondition(), b.getPostCondition()));

                    for (size_t i = 0; i < b.size(); ++i)
                        c.add(std::make_shared<Param>(L"", std::make_shared<tc::FreshType>(nFlags)));
                }

                pType = pNew;
                break;
            }

            case Type::SET:
                pType = std::make_shared<SetType>(std::make_shared<tc::FreshType>(nFlags));
                break;
            case Type::LIST:
                pType = std::make_shared<ListType>(std::make_shared<tc::FreshType>(nFlags));
                break;
            case Type::MAP:
                pType = std::make_shared<MapType>(std::make_shared<tc::FreshType>(nReversed), std::make_shared<tc::FreshType>(nFlags));
                break;
        }

        if (pType) {
            processed.insert(pLhs);
            formulas.push_back(std::make_shared<tc::Formula>(tc::Formula::EQUALS, pLhs, pType));
        }
    }

    _context()->insert(formulas.begin(), formulas.end());

    // Don't process compound formulas to limit excessive introduction of std::make_shared<fresh types.
    return !formulas.empty();
}

OperationPtr Operation::explode() {
    return std::make_shared<Explode>();
}

}
