/// \file operations.h
///

#ifndef OPERATIONS_H_
#define OPERATIONS_H_

#include "autoptr.h"
#include "typecheck.h"
#include "type_lattice.h"

namespace tc {

typedef std::set<tc::FreshTypePtr, PtrLess<tc::FreshType> > FreshTypeSet;
typedef std::map<tc::FreshTypePtr, std::pair<size_t, size_t>, PtrLess<tc::FreshType> > ExtraBoundsCount;

using OperationPtr = std::shared_ptr<class Operation>;

class Operation {
public:
    virtual ~Operation() {}

    bool run(int & _result);
    const std::wstring & getName() const { return m_strName; }
    bool getRestartIteration() const { return m_bRestartIteration; }

    // Operations.
    static OperationPtr unify();
    static OperationPtr lift();
    static OperationPtr eval();
    static OperationPtr prune();
    static OperationPtr refute();
    static OperationPtr compact();
    static OperationPtr infer();
    static OperationPtr expand();
    static OperationPtr explode();
    static OperationPtr guess();

protected:
    const std::wstring m_strName;
    const bool m_bRestartIteration;
    Formulas::iterator m_iCurrentCF, m_iLastCF;
    int m_nCurrentCFPart;
    std::set<std::pair<size_t, size_t> > m_redundantParts;

    Operation(const std::wstring & _strName = L"", bool _bRestartIteration = false) :
        m_strName(_strName), m_bRestartIteration(_bRestartIteration), m_nCurrentCFPart(-1)
    {
    }

    bool _runCompound(int & _result);
    virtual bool _run(int & _result) = 0;
    ContextPtr _context();
    void _enumerateFreshTypes(const CompoundFormulaPtr &_cf, FreshTypeSet &_types);

protected:
    virtual void _clear();
};

class OperationOnLattice : public Operation {
protected:
    OperationOnLattice(const std::wstring & _strName, bool _bRestartIteration) :
        Operation(_strName, _bRestartIteration)
    {
    }

    virtual bool _handler(const ir::TypePtr& _pType, const tc::Relations& _lowers, const tc::Relations& _uppers) = 0;

private:
    FreshTypeSet m_ignored;
    ExtraBoundsCount m_extraBounds;

    bool _run(int & _result);
    void _findRestrictions(FreshTypeSet& _ignored);

protected:
    size_t _getExtraLowerBoundsCount(const ir::TypePtr& _pType);
    size_t _getExtraUpperBoundsCount(const ir::TypePtr& _pType);
};

} // namespace tc

#endif // OPERATIONS_H_
