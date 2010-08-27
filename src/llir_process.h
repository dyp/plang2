/// \file llir_process.h
///


#ifndef LLIR_PROCESS_H_
#define LLIR_PROCESS_H_

#include <map>
#include <set>

#include <llir.h>

#include "utils.h"

namespace llir {

// Function-level processing.
class CProcessLL {
public:
    CProcessLL(CFunction & _func) : m_func(_func) {}
    virtual ~CProcessLL() {}

    virtual void process();
    virtual void processInstructions(instructions_t & _instrs);

    virtual void processInstruction(CInstruction & _instr);
    virtual void processUnary(CUnary & _instr);
    virtual void processBinary(CBinary & _instr);
    virtual void processCall(CCall & _instr);
    virtual void processIf(CIf & _instr);
    virtual void processSwitch(CSwitch & _instr);
    virtual void processSelect(CSelect & _instr);
    virtual void processField(CField & _instr);
    virtual void processCast(CCast & _instr);
    virtual void processCopy(CCopy & _instr);


    virtual void processOperands(operands_t & _ops);
    virtual void processOperand(COperand & _op);

    CFunction & getFunction() { return m_func; }
    Auto<CInstruction> getInstruction() const { return m_pInstr; }
    void setInstruction(Auto<CInstruction> _instr) { * m_iInstr = _instr; }
    instructions_t * getInstructions() const { return m_pInstructions; }
    const CInstruction * getNext() const { return m_pNext; }
    instructions_t::iterator getNextIter() const { return m_iNext; }
    instructions_t::iterator getIter() { return m_iInstr; }
    const CInstruction * getPrev() const { return m_pPrev; }

protected:
    void processInstructionIter(instructions_t::iterator _iInstr);

private:
    CFunction & m_func;
    Auto<CInstruction> m_pInstr;
    const CInstruction * m_pNext, * m_pPrev;
    instructions_t::iterator m_iInstr, m_iNext;
    instructions_t * m_pInstructions;
};

class CMarkEOLs : public CProcessLL {
public:
    CMarkEOLs(CFunction & _func) : CProcessLL(_func) {}

    virtual void processOperand(COperand & _op);
};

class CRecycleVars : public CProcessLL {
public:
    CRecycleVars(CFunction & _func) : CProcessLL(_func) {}

    virtual void process();
    virtual void processInstruction(CInstruction & _instr);
    virtual void processOperand(COperand & _op);

private:
    typedef std::multimap<Auto<CType>, Auto<CVariable> > recycle_pool_t;
    typedef std::map<Auto<CVariable>, Auto<CVariable> > rewrite_map_t;

    recycle_pool_t m_varPool;
    rewrite_map_t m_rewriteVars;
};

class CCountLabels : public CProcessLL {
public:
    CCountLabels(CFunction & _func) : CProcessLL(_func) {}

    virtual void processInstruction(CInstruction & _instr);
    virtual void processUnary(CUnary & _instr);
    virtual void processBinary(CBinary & _instr);

private:
    COperand m_target;
};

class CPruneJumps : public CProcessLL {
public:
    CPruneJumps(CFunction & _func) : CProcessLL(_func) {}

    virtual void process();
    virtual void processInstruction(CInstruction & _instr);
    virtual void processUnary(CUnary & _instr);
    virtual void processBinary(CBinary & _instr);
    virtual void processInstructions(instructions_t & _instrs);

private:
    bool m_bFirstPass;
    COperand m_target;
    instructions_t::iterator m_iStart;
    typedef std::set<CLabel *> labels_t;
    labels_t m_labels, m_labelsFwd;
    typedef std::map<Auto<CLabel>, Auto<CLabel> > rewrite_map_t;
    rewrite_map_t m_rewriteLabels;

    void collapse(instructions_t::iterator _iInstr, instructions_t & _instrs);
};

class CCollapseReturns : public CProcessLL {
public:
    CCollapseReturns(CFunction & _func) : CProcessLL(_func) {}

    virtual void processBinary(CBinary & _instr);
};

/*class CAddRefCounting : public CProcessLL {
public:
    CAddRefCounting(CFunction & _func) : CProcessLL(_func) {}

    virtual void process();
    virtual void processInstruction(CInstruction & _instr);
    virtual void processUnary(CUnary & _instr);
    virtual void processBinary(CBinary & _instr);
    virtual void processField(CField & _instr);
    virtual void processCast(CCast & _instr);

private:
    args_t m_ptrs;
    COperand m_op;
};*/

class CCollapseLabels : public CProcessLL {
public:
    CCollapseLabels(CFunction & _func) : CProcessLL(_func) {}

    virtual void processBinary(CBinary & _instr);
};

template<class Processor>
void processLL(CFunction & _func) {
    Processor(_func).process();
}

}; // namespace llir

#endif /* LLIR_PROCESS_H_ */
