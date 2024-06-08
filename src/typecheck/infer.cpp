/// \file infer.cpp
///

#include "operations.h"
#include "node_analysis.h"

using namespace ir;

namespace tc {

class Infer : public Operation {
public:
    Infer() : Operation(L"Infer", true) {}

protected:
    virtual bool _run(int & _nResult);
};

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
    if (_lattice.relations().find(std::make_shared<tc::Relation>(_pRelation->getRhs(), _pRelation->getLhs(), true)) != _lattice.relations().end())
        return false;

    // Check occurrences of A <= B <= A (or A < B <= A).
    if (_lattice.relations().find(std::make_shared<tc::Relation>(_pRelation->getRhs(), _pRelation->getLhs(), false)) != _lattice.relations().end()) {
        if (_pRelation->isStrict())
            return false;

        if (tc::FormulaList *pSubsts = (tc::FormulaList *)_pParam)
            pSubsts->push_back(std::make_shared<tc::Formula>(tc::Formula::EQUALS, _pRelation->getLhs(), _pRelation->getRhs()));
    }

    return true;
}

bool Infer::_run(int & _nResult) {
    bool bModified = false;
    tc::FormulaList substs;

    _context()->pTypes->update(&_validateRelation, &substs);

    if (!_context()->pTypes->isValid()) {
        _nResult = tc::Formula::FALSE;
        return true;
    }

    // Substs were added, need to run unify.
    if (!substs.empty()) {
        for (tc::FormulaList::iterator i = substs.begin(); i != substs.end(); ++i)
            bModified |= _context()->add(*i);

        assert(bModified);
        return bModified;
    }

    _context()->pTypes->reduce();

    const tc::Relations &relations = _context()->pTypes->relations();

    // Remove formulas that are no longer used.
    for (tc::Formulas::iterator i = _context()->formulas()->begin(); i != _context()->formulas()->end();) {
        tc::Formula &f = **i;

        if (!f.is(tc::Formula::SUBTYPE | tc::Formula::SUBTYPE_STRICT)) {
            ++i;
            continue;
        }

        const auto j = relations.find(std::make_shared<tc::Relation>(f));

        // Missing formula could have been replaced by Lattice::update() as a result of applying substs.
        if (j == relations.end() || !(*j)->bUsed) {
            i = _context()->formulas()->erase(i);
            bModified = true;
        } else {
            ++i;
        }
    }

    // Add inferred formulas.
    for (tc::Relations::iterator i = relations.begin(); i != relations.end(); ++i) {
        tc::Relation &f = **i;

        if (f.bUsed)
            bModified |= _context()->add(std::make_shared<tc::Formula>(f));
    }

    bModified |= _runCompound(_nResult);

    // Check if simple top-level formula is implied by some compound formula.
    tc::Formulas::iterator iCF = _context()->formulas()->beginCompound();

    if (iCF == _context()->formulas()->end())
        return bModified;

    for (tc::Formulas::iterator i = _context()->formulas()->begin(); i != iCF;) {
        tc::FormulaPtr pTest = *i;
        bool bIsImplied = false;

        i = _context()->formulas()->erase(i);

        for (tc::Formulas::iterator j = iCF; j != _context()->formulas()->end(); ++j) {
            tc::CompoundFormula &cf = (tc::CompoundFormula &)**j;

            bIsImplied = true;

            for (size_t k = 0; bIsImplied && k < cf.size(); ++k) {
                tc::ContextStack::push(cf.getPartPtr(k));
                bIsImplied &= _context()->implies(*pTest);
                tc::ContextStack::pop();
            }

            if (bIsImplied)
                break;
        }

        if (bIsImplied)
            bModified = true;
        else
            _context()->formulas()->insert(pTest);
    }

    return bModified;
}

OperationPtr Operation::infer() {
    return std::make_shared<Infer>();
}

}
