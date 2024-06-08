/// \file llir_process.cpp
///

#include "llir_process.h"

#include <iostream>

namespace llir {

void _assertContains(Instructions & _lst, Instructions::iterator _iter) {
    if (_iter == _lst.end())
        return;

    bool bFound = false;
    for (Instructions::iterator iInstr = _lst.begin(); iInstr != _lst.end(); ++ iInstr)
        if (iInstr == _iter) {
            bFound = true;
            break;
        }

    if (! bFound) {
        std::wcerr << L"Instr. count: " << _lst.size() << /*L"; code: "  << (* _iter)->getKind() <<*/ std::endl;
    }

    assert(bFound);
}

void ProcessLL::process() {
    m_pInstr = NULL;
    m_pNext = NULL;
    m_pPrev = NULL;
    m_iNext = m_func.instructions().begin();
    m_pInstructions = & m_func.instructions();

    processInstructions(m_func.instructions());
}

void ProcessLL::processInstructions(Instructions & _instrs) {
    Instructions * pSaved = m_pInstructions;

    if (_instrs.empty())
        return;

    m_pInstructions = & _instrs;

    for (Instructions::iterator iInstr = _instrs.begin(); iInstr != _instrs.end(); ++ iInstr)
        if (*iInstr)
            processInstructionIter(iInstr);

    m_pInstructions = pSaved;
}

void ProcessLL::processInstructionIter(Instructions::iterator _iInstr) {
    m_iInstr = _iInstr;
    m_iNext = ::next(_iInstr);
    m_pNext = m_iNext == m_pInstructions->end() ? NULL : m_iNext->get();
    m_pInstr = * _iInstr;
    processInstruction(** _iInstr);
    m_pPrev = _iInstr->get();
}

void ProcessLL::processInstruction(Instruction & _instr) {
    switch (_instr.getKind()) {
        case Instruction::UNARY:
            processUnary((Unary & ) _instr);
            break;
        case Instruction::BINARY:
            processBinary((Binary & ) _instr);
            break;
        case Instruction::CALL:
            processCall((Call &) _instr);
            break;
        case Instruction::IF:
            processIf((If &) _instr);
            break;
        case Instruction::WHILE:
            processWhile((While &) _instr);
            break;
        case Instruction::SWITCH:
            processSwitch((Switch &) _instr);
            break;
        case Instruction::SELECT:
            processSelect((Select & ) _instr);
            break;
        case Instruction::FIELD:
            processField((Field &) _instr);
            break;
        case Instruction::CAST:
            processCast((Cast &) _instr);
            break;
        case Instruction::COPY:
            processCopy((Copy &) _instr);
            break;
    }
}

void ProcessLL::processUnary(Unary & _instr) {
    processOperand(_instr.getOp());
}

void ProcessLL::processBinary(Binary & _instr) {
    processOperand(_instr.getOp1());
    processOperand(_instr.getOp2());
}

void ProcessLL::processCall(Call & _instr) {
    processOperands(_instr.args());
}

void ProcessLL::processIf(If & _instr) {
    processOperand(_instr.getCondition());
    processInstructions(_instr.brTrue());
    processInstructions(_instr.brFalse());
}

void ProcessLL::processWhile(While & _instr) {
    processOperand(_instr.getCondition());
    processInstructions(_instr.getBlock());
}

void ProcessLL::processSwitch(Switch & _instr) {
    processOperand(_instr.getArg());

    for (SwitchCases::iterator iCase = _instr.cases().begin(); iCase != _instr.cases().end(); ++ iCase)
        processInstructions(iCase->body);

    processInstructions(_instr.deflt());
}

void ProcessLL::processSelect(Select & _instr) {
    processOperand(_instr.getCondition());
    processOperand(_instr.getTrue());
    processOperand(_instr.getFalse());
}

void ProcessLL::processField(Field & _instr) {
    processOperand(_instr.getOp());
}

void ProcessLL::processCast(Cast & _instr) {
    processOperand(_instr.getOp());
}

void ProcessLL::processCopy(Copy & _instr) {
    processOperand(_instr.getDest());
    processOperand(_instr.getSrc());
    processOperand(_instr.getSize());
}

void ProcessLL::processOperands(Operands & _ops) {
    for (Operands::iterator iOp = _ops.begin(); iOp != _ops.end(); ++ iOp)
        processOperand(* iOp);
}

void ProcessLL::processOperand(Operand & _op) {
}

//
// MarkEOLs
//

void MarkEOLs::processOperand(Operand & _op) {
    ProcessLL::processOperand(_op);

    if (_op.getKind() == Operand::VARIABLE)
        _op.getVariable()->setLastUse(getInstruction());
}

//
// RecycleVars
//

void RecycleVars::process()
{
    m_varPool.clear();
    m_rewriteVars.clear();
    ProcessLL::process();
}

void RecycleVars::processInstruction(Instruction & _instr)
{
    ProcessLL::processInstruction(_instr);

    const auto oldVar = _instr.getResult();

    if (oldVar) {
        typedef RecyclePool::iterator I;
        const auto type = oldVar->getType();
        std::pair<I, I> bounds = m_varPool.equal_range(type);

        if (bounds.first != bounds.second) {
            const auto newVar = bounds.first->second;

            newVar->setLastUse(oldVar->getLastUse());
            newVar->setAlive(true);
            _instr.setResult(newVar);
            m_varPool.erase(bounds.first);

            if (oldVar)
                m_rewriteVars[oldVar] = newVar;

            //newVar->setUsed(true);
            newVar->setLastInit(getInstruction());
        } else {
            getFunction().locals().push_back(oldVar);
            //oldVar->setUsed(true);
            oldVar->setLastInit(getInstruction());
        }
    }
}

void RecycleVars::processOperand(Operand & _op) {
    ProcessLL::processOperand(_op);

    if (_op.getKind() != Operand::VARIABLE || ! _op.getVariable()->isAlive())
        return;

    RewriteMap::iterator iVar = m_rewriteVars.find(_op.getVariable());

    if (iVar != m_rewriteVars.end())
        _op.setVariable(iVar->second);

    if (_op.getVariable()->getLastInit())
        _op.getVariable()->getLastInit()->incResultUsageCount();

    _op.getVariable()->setUsed(true);

    if (_op.getVariable()->getLastUse() == getInstruction()) {
        _op.getVariable()->setAlive(false);
        m_varPool.insert(std::make_pair(_op.getType(), _op.getVariable()));
    }
}

//
// CountLabels
//

void CountLabels::processInstruction(Instruction & _instr) {
    m_target = Operand();

    ProcessLL::processInstruction(_instr);

    if (! m_target.empty())
        m_target.getLabel()->incUsageCount();
}

void CountLabels::processUnary(Unary & _instr) {
    ProcessLL::processUnary(_instr);

    if (_instr.getUnaryKind() == Unary::JMP)
        m_target = _instr.getOp();
}

void CountLabels::processBinary(Binary & _instr) {
    ProcessLL::processBinary(_instr);

    if (_instr.getBinaryKind() == Binary::JMZ || _instr.getBinaryKind() == Binary::JNZ)
        m_target = _instr.getOp2();
}


//
// PruneJumps
//

void PruneJumps::process() {
    m_bFirstPass = true;
    m_rewriteLabels.clear();
    ProcessLL::process();

    // 2nd pass, rewrite labels only.
    m_bFirstPass = false;
    ProcessLL::process();
}

/*
 * Remove sequences like:
 * L0: nop
 *     jmp L1
 * L1: jmp L2
 * L2: nop
 * Ln: <any>
 *
 * ( [ Li: ] ( nop | j{mp,mz,nz} Lj ) )* [ Ln: ]    where j comes after current instruction
 * will be replaced with:
 * L0:
 */

void PruneJumps::collapse(Instructions::iterator _iInstr, Instructions & _instrs) {
    if (m_iStart == _instrs.end())
        return;

    m_bFirstPass = false;
    m_target = Operand();

    if (_iInstr != m_iStart) {
        LabelPtr pLabel;

        for (Instructions::iterator iInstr = m_iStart; iInstr != _iInstr; ++ iInstr) {
            ProcessLL::processInstructionIter(iInstr);
            if (! m_target.empty())
                m_target.getLabel()->decUsageCount();
            if ((* iInstr)->getLabel() && (* iInstr)->getLabel()->getUsageCount() > 0) {
                if (!pLabel)
                    pLabel = (* iInstr)->getLabel();
                else
                    m_rewriteLabels[(* iInstr)->getLabel()] = pLabel;
            }
        }

        if (pLabel) {
            if (_iInstr != _instrs.end()) {
                if ((* _iInstr)->getLabel())
                    m_rewriteLabels[(* _iInstr)->getLabel()] = pLabel;
                (* _iInstr)->setLabel(pLabel);
            } else {
                _instrs.push_back(std::make_shared<Instruction>()); // nop
                _instrs.back()->setLabel(pLabel);
                _iInstr = ::prev(_instrs.end());
            }
        }

        _instrs.erase(m_iStart, _iInstr);
    }

    if (_iInstr != _instrs.end())
        m_iStart = ::next(_iInstr);
}

void PruneJumps::processInstructions(Instructions & _instrs) {
    if (! m_bFirstPass) {
        ProcessLL::processInstructions(_instrs);
        return;
    }

    m_iStart = _instrs.end();
    ProcessLL::processInstructions(_instrs);

    _assertContains(_instrs, m_iStart);

    if (m_iStart != _instrs.end()) {
        collapse(_instrs.end(), _instrs);
    }

    if (getInstructions() != NULL)
        m_iStart = getInstructions()->end();
}

void PruneJumps::processInstruction(Instruction & _instr) {
    m_target = Operand();

    ProcessLL::processInstruction(_instr);

    if (! m_bFirstPass)
        return;

    // Detect start and end of collapsable instruction sequence.

    const auto pLabel = _instr.getLabel();
    bool bBreak = true;

    if (pLabel) {
        m_labels.insert(pLabel.get());
        m_labelsFwd.erase(pLabel.get());
        if (_instr.getKind() == Instruction::NOP)
            bBreak = false;
    }

    if (! m_target.empty()) {
        if (m_labels.find(m_target.getLabel().get()) == m_labels.end()) {
            bBreak = false;
            m_labelsFwd.insert(m_target.getLabel().get());
        } else
            bBreak = true;
    }

    // Process the sequence.
    m_bFirstPass = false;

    if (bBreak && m_iStart != getInstructions()->end()) {
        _assertContains(* getInstructions(), m_iStart);

        Instructions::iterator iEnd = getIter();

        for (Instructions::iterator iInstr = m_iStart; iInstr != iEnd; ++ iInstr) {
            ProcessLL::processInstructionIter(iInstr);
            if (! m_target.empty()) {
                if (m_labelsFwd.find(m_target.getLabel().get()) != m_labelsFwd.end())
                    collapse(iInstr, * getInstructions());
            }
        }

        if (m_iStart != iEnd)
            collapse(iEnd, * getInstructions());

        m_iStart = getInstructions()->end();
    } else if (! bBreak && m_iStart == getInstructions()->end()) {
        m_iStart = getIter();
        _assertContains(* getInstructions(), m_iStart);
    }

    m_bFirstPass = true;

    /*if (m_target.getKind() == Operand::Label) {
        return;
    }*/









    /*if (m_target.getKind() == Operand::Empty)
        return;

    assert(m_target.getKind() == Operand::Label);

    //Auto<Label> pLabel = m_target.getLabel();

    if (getNext() && ! getNext()->getLabel().empty() &&
            getNext()->getLabel().ptr() == pLabel.ptr())
    {
        setInstruction(new Instruction());
        return;
    }

    pLabel->incUsageCount();*/
}

void PruneJumps::processUnary(Unary & _instr) {
    ProcessLL::processUnary(_instr);

    if (_instr.getUnaryKind() == Unary::JMP) {
        RewriteMap::iterator iLabel = m_rewriteLabels.find(_instr.getOp().getLabel());
        if (iLabel != m_rewriteLabels.end())
            _instr.getOp().setLabel(iLabel->second);
        m_target = _instr.getOp();
    }
}

void PruneJumps::processBinary(Binary & _instr) {
    ProcessLL::processBinary(_instr);

    if (_instr.getBinaryKind() == Binary::JMZ || _instr.getBinaryKind() == Binary::JNZ) {
        RewriteMap::iterator iLabel = m_rewriteLabels.find(_instr.getOp2().getLabel());
        if (iLabel != m_rewriteLabels.end())
            _instr.getOp2().setLabel(iLabel->second);
        m_target = _instr.getOp2();
    }
}

//
// CollapseReturns
//

void CollapseReturns::processBinary(Binary & _instr) {
    ProcessLL::processBinary(_instr);

    if (_instr.getBinaryKind() != Binary::SET)
        return;

    if (getNext() == NULL || getNext()->getKind() != Instruction::UNARY)
        return;

    Unary & nextInstr = (Unary &) * getNext();

    if (nextInstr.getUnaryKind() != Unary::RETURN || nextInstr.getOp().getKind() != Operand::VARIABLE)
        return;

    if (nextInstr.getLabel())
        return;

    if (nextInstr.getOp().getVariable().get() != _instr.getOp1().getVariable().get())
        return;

    nextInstr.getOp() = _instr.getOp2();
    setInstruction(std::make_shared<Instruction>()); // nop
}

//
// AddRefCounting
//
/*
void AddRefCounting::process() {
    m_ptrs.clear();

    ProcessLL::process();

    for (args_t::iterator iVar = getFunction().locals().begin(); iVar != getFunction().locals().end(); ++ iVar) {
        Auto<Variable> pVar = * iVar;
        const Type & type = * pVar->getType();

        switch (type.getKind()) {
            case Type::Pointer:
                getFunction().instructions().push_back(
                        new Unary(Unary::Unref, Operand(pVar)));
                break;
        }
    }

    for (args_t::iterator iVar = m_ptrs.begin(); iVar != m_ptrs.end(); ++ iVar)
        getFunction().instructions().push_back(new Unary(Unary::Unref, Operand(* iVar)));
}

void AddRefCounting::processInstruction(Instruction & _instr) {
    m_op = Operand();

    ProcessLL::processInstruction(_instr);

    if (! _instr.getResult().empty() && ! _instr.getResult()->getType().empty()
            && _instr.getResult()->getType()->getKind() == Type::Pointer)
    {
        m_ptrs.push_back(_instr.getResult());
        getInstructions()->insert(getNextIter(), new Unary(Unary::Ref, Operand(_instr.getResult())));
    }

    if (m_op.getKind() != Operand::Empty)
        getInstructions()->insert(getNextIter(), new Unary(Unary::Ref, m_op));
}

void AddRefCounting::processUnary(Unary & _instr) {
}

void AddRefCounting::processBinary(Binary & _instr) {
}

void AddRefCounting::processField(Field & _instr) {
}

void AddRefCounting::processCast(Cast & _instr) {
}
*/

};

 // namespace llir
