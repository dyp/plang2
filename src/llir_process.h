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
class ProcessLL {
public:
    ProcessLL(Function & _func) : m_func(_func) {}
    virtual ~ProcessLL() {}

    virtual void process();
    virtual void processInstructions(Instructions &_instrs);

    virtual void processInstruction(Instruction & _instr);
    virtual void processUnary(Unary & _instr);
    virtual void processBinary(Binary & _instr);
    virtual void processCall(Call & _instr);
    virtual void processIf(If & _instr);
    virtual void processWhile(While & _instr);
    virtual void processSwitch(Switch & _instr);
    virtual void processSelect(Select & _instr);
    virtual void processField(Field & _instr);
    virtual void processCast(Cast & _instr);
    virtual void processCopy(Copy & _instr);


    virtual void processOperands(Operands &_ops);
    virtual void processOperand(Operand & _op);

    Function & getFunction() { return m_func; }
    Auto<Instruction> getInstruction() const { return m_pInstr; }
    void setInstruction(Auto<Instruction> _instr) { * m_iInstr = _instr; }
    Instructions * getInstructions() const { return m_pInstructions; }
    const Instruction * getNext() const { return m_pNext; }
    Instructions::iterator getNextIter() const { return m_iNext; }
    Instructions::iterator getIter() { return m_iInstr; }
    const Instruction * getPrev() const { return m_pPrev; }

protected:
    void processInstructionIter(Instructions::iterator _iInstr);

private:
    Function & m_func;
    Auto<Instruction> m_pInstr;
    const Instruction * m_pNext, * m_pPrev;
    Instructions::iterator m_iInstr, m_iNext;
    Instructions * m_pInstructions;
};

class MarkEOLs : public ProcessLL {
public:
    MarkEOLs(Function & _func) : ProcessLL(_func) {}

    virtual void processOperand(Operand & _op);
};

class RecycleVars : public ProcessLL {
public:
    RecycleVars(Function & _func) : ProcessLL(_func) {}

    virtual void process();
    virtual void processInstruction(Instruction & _instr);
    virtual void processOperand(Operand & _op);

private:
    typedef std::multimap<Auto<Type>, Auto<Variable> > RecyclePool;
    typedef std::map<Auto<Variable>, Auto<Variable> > RewriteMap;

    RecyclePool m_varPool;
    RewriteMap m_rewriteVars;
};

class CountLabels : public ProcessLL {
public:
    CountLabels(Function & _func) : ProcessLL(_func) {}

    virtual void processInstruction(Instruction & _instr);
    virtual void processUnary(Unary & _instr);
    virtual void processBinary(Binary & _instr);

private:
    Operand m_target;
};

class PruneJumps : public ProcessLL {
public:
    PruneJumps(Function & _func) : ProcessLL(_func) {}

    virtual void process();
    virtual void processInstruction(Instruction & _instr);
    virtual void processUnary(Unary & _instr);
    virtual void processBinary(Binary & _instr);
    virtual void processInstructions(Instructions & _instrs);

private:
    bool m_bFirstPass;
    Operand m_target;
    Instructions::iterator m_iStart;
    typedef std::set<Label *> Labels;
    Labels m_labels, m_labelsFwd;
    typedef std::map<Auto<Label>, Auto<Label> > RewriteMap;
    RewriteMap m_rewriteLabels;

    void collapse(Instructions::iterator _iInstr, Instructions & _instrs);
};

class CollapseReturns : public ProcessLL {
public:
    CollapseReturns(Function & _func) : ProcessLL(_func) {}

    virtual void processBinary(Binary & _instr);
};

/*class AddRefCounting : public ProcessLL {
public:
    AddRefCounting(Function & _func) : ProcessLL(_func) {}

    virtual void process();
    virtual void processInstruction(Instruction & _instr);
    virtual void processUnary(Unary & _instr);
    virtual void processBinary(Binary & _instr);
    virtual void processField(Field & _instr);
    virtual void processCast(Cast & _instr);

private:
    args_t m_ptrs;
    Operand m_op;
};*/

class CollapseLabels : public ProcessLL {
public:
    CollapseLabels(Function & _func) : ProcessLL(_func) {}

    virtual void processBinary(Binary & _instr);
};

template<class Processor>
void processLL(Function & _func) {
    Processor(_func).process();
}

}; // namespace llir

#endif /* LLIR_PROCESS_H_ */
