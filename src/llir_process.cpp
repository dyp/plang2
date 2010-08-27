/// \file llir_process.cpp
///

#include "llir_process.h"

#include <iostream>

namespace llir {

void _assertContains(instructions_t & _lst, instructions_t::iterator _iter) {
    if (_iter == _lst.end())
        return;

    bool bFound = false;
    for (instructions_t::iterator iInstr = _lst.begin(); iInstr != _lst.end(); ++ iInstr)
        if (iInstr == _iter) {
            bFound = true;
            break;
        }

    if (! bFound) {
        std::wcerr << L"Instr. count: " << _lst.size() << /*L"; code: "  << (* _iter)->getKind() <<*/ std::endl;
    }

    assert(bFound);
}

void CProcessLL::process() {
    m_pInstr = NULL;
    m_pNext = NULL;
    m_pPrev = NULL;
    m_iNext = m_func.instructions().begin();
    m_pInstructions = & m_func.instructions();

    processInstructions(m_func.instructions());
}

void CProcessLL::processInstructions(instructions_t & _instrs) {
    instructions_t * pSaved = m_pInstructions;

    if (_instrs.empty())
        return;

    m_pInstructions = & _instrs;

    for (instructions_t::iterator iInstr = _instrs.begin(); iInstr != _instrs.end(); ++ iInstr)
        if (! iInstr->empty())
            processInstructionIter(iInstr);

    m_pInstructions = pSaved;
}

void CProcessLL::processInstructionIter(instructions_t::iterator _iInstr) {
    m_iInstr = _iInstr;
    m_iNext = next(_iInstr);
    m_pNext = m_iNext == m_pInstructions->end() ? NULL : m_iNext->ptr();
    m_pInstr = * _iInstr;
    processInstruction(** _iInstr);
    m_pPrev = _iInstr->ptr();
}

void CProcessLL::processInstruction(CInstruction & _instr) {
    switch (_instr.getKind()) {
        case CInstruction::Unary:
            processUnary((CUnary & ) _instr);
            break;
        case CInstruction::Binary:
            processBinary((CBinary & ) _instr);
            break;
        case CInstruction::Call:
            processCall((CCall &) _instr);
            break;
        case CInstruction::If:
            processIf((CIf &) _instr);
            break;
        case CInstruction::Switch:
            processSwitch((CSwitch &) _instr);
            break;
        case CInstruction::Select:
            processSelect((CSelect & ) _instr);
            break;
        case CInstruction::Field:
            processField((CField &) _instr);
            break;
        case CInstruction::Cast:
            processCast((CCast &) _instr);
            break;
        case CInstruction::Copy:
            processCopy((CCopy &) _instr);
            break;
    }
}

void CProcessLL::processUnary(CUnary & _instr) {
    processOperand(_instr.getOp());
}

void CProcessLL::processBinary(CBinary & _instr) {
    processOperand(_instr.getOp1());
    processOperand(_instr.getOp2());
}

void CProcessLL::processCall(CCall & _instr) {
    processOperands(_instr.args());
}

void CProcessLL::processIf(CIf & _instr) {
    processOperand(_instr.getCondition());
    processInstructions(_instr.brTrue());
    processInstructions(_instr.brFalse());
}

void CProcessLL::processSwitch(CSwitch & _instr) {
    processOperand(_instr.getArg());

    for (switch_cases_t::iterator iCase = _instr.cases().begin(); iCase != _instr.cases().end(); ++ iCase)
        processInstructions(iCase->body);

    processInstructions(_instr.deflt());
}

void CProcessLL::processSelect(CSelect & _instr) {
    processOperand(_instr.getCondition());
    processOperand(_instr.getTrue());
    processOperand(_instr.getFalse());
}

void CProcessLL::processField(CField & _instr) {
    processOperand(_instr.getOp());
}

void CProcessLL::processCast(CCast & _instr) {
    processOperand(_instr.getOp());
}

void CProcessLL::processCopy(CCopy & _instr) {
    processOperand(_instr.getDest());
    processOperand(_instr.getSrc());
    processOperand(_instr.getSize());
}

void CProcessLL::processOperands(operands_t & _ops) {
    for (operands_t::iterator iOp = _ops.begin(); iOp != _ops.end(); ++ iOp)
        processOperand(* iOp);
}

void CProcessLL::processOperand(COperand & _op) {
}

//
// CMarkEOLs
//

void CMarkEOLs::processOperand(COperand & _op) {
    CProcessLL::processOperand(_op);

    if (_op.getKind() == COperand::Variable)
        _op.getVariable()->setLastUse(getInstruction());
}

//
// CRecycleVars
//

void CRecycleVars::process()
{
    m_varPool.clear();
    m_rewriteVars.clear();
    CProcessLL::process();
}

void CRecycleVars::processInstruction(CInstruction & _instr)
{
    CProcessLL::processInstruction(_instr);

    Auto<CVariable> oldVar = _instr.getResult();

    if (! oldVar.empty()) {
        typedef recycle_pool_t::iterator I;
        const Auto<CType> & type = oldVar->getType();
        std::pair<I, I> bounds = m_varPool.equal_range(type);

        if (bounds.first != bounds.second) {
            Auto<CVariable> newVar = bounds.first->second;

            newVar->setLastUse(oldVar->getLastUse());
            newVar->setAlive(true);
            _instr.setResult(newVar);
            m_varPool.erase(bounds.first);

            if (! oldVar.empty())
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

void CRecycleVars::processOperand(COperand & _op) {
    CProcessLL::processOperand(_op);

    if (_op.getKind() != COperand::Variable || ! _op.getVariable()->isAlive())
        return;

    rewrite_map_t::iterator iVar = m_rewriteVars.find(_op.getVariable());

    if (iVar != m_rewriteVars.end())
        _op.setVariable(iVar->second);

    if (! _op.getVariable()->getLastInit().empty())
        _op.getVariable()->getLastInit()->incResultUsageCount();

    _op.getVariable()->setUsed(true);

    if (_op.getVariable()->getLastUse() == getInstruction()) {
        _op.getVariable()->setAlive(false);
        m_varPool.insert(std::make_pair(_op.getType(), _op.getVariable()));
    }
}

//
// CCountLabels
//

void CCountLabels::processInstruction(CInstruction & _instr) {
    m_target = COperand();

    CProcessLL::processInstruction(_instr);

    if (! m_target.empty())
        m_target.getLabel()->incUsageCount();
}

void CCountLabels::processUnary(CUnary & _instr) {
    CProcessLL::processUnary(_instr);

    if (_instr.getUnaryKind() == CUnary::Jmp)
        m_target = _instr.getOp();
}

void CCountLabels::processBinary(CBinary & _instr) {
    CProcessLL::processBinary(_instr);

    if (_instr.getBinaryKind() == CBinary::Jmz || _instr.getBinaryKind() == CBinary::Jnz)
        m_target = _instr.getOp2();
}


//
// CPruneJumps
//

void CPruneJumps::process() {
    m_bFirstPass = true;
    m_rewriteLabels.clear();
    CProcessLL::process();

    // 2nd pass, rewrite labels only.
    m_bFirstPass = false;
    CProcessLL::process();
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

void CPruneJumps::collapse(instructions_t::iterator _iInstr, instructions_t & _instrs) {
    if (m_iStart == _instrs.end())
        return;

    m_bFirstPass = false;
    m_target = COperand();

    if (_iInstr != m_iStart) {
        Auto<CLabel> pLabel;

        for (instructions_t::iterator iInstr = m_iStart; iInstr != _iInstr; ++ iInstr) {
            CProcessLL::processInstructionIter(iInstr);
            if (! m_target.empty())
                m_target.getLabel()->decUsageCount();
            if (! (* iInstr)->getLabel().empty() && (* iInstr)->getLabel()->getUsageCount() > 0) {
                if (pLabel.empty())
                    pLabel = (* iInstr)->getLabel();
                else
                    m_rewriteLabels[(* iInstr)->getLabel()] = pLabel;
            }
        }

        if (! pLabel.empty()) {
            if (_iInstr != _instrs.end()) {
                if (! (* _iInstr)->getLabel().empty())
                    m_rewriteLabels[(* _iInstr)->getLabel()] = pLabel;
                (* _iInstr)->setLabel(pLabel);
            } else {
                _instrs.push_back(new CInstruction()); // nop
                _instrs.back()->setLabel(pLabel);
                _iInstr = prev(_instrs.end());
            }
        }

        _instrs.erase(m_iStart, _iInstr);
    }

    if (_iInstr != _instrs.end())
        m_iStart = next(_iInstr);
}

void CPruneJumps::processInstructions(instructions_t & _instrs) {
    if (! m_bFirstPass) {
        CProcessLL::processInstructions(_instrs);
        return;
    }

    m_iStart = _instrs.end();
    CProcessLL::processInstructions(_instrs);

    _assertContains(_instrs, m_iStart);

    if (m_iStart != _instrs.end()) {
        collapse(_instrs.end(), _instrs);
    }

    if (getInstructions() != NULL)
        m_iStart = getInstructions()->end();
}

void CPruneJumps::processInstruction(CInstruction & _instr) {
    m_target = COperand();

    CProcessLL::processInstruction(_instr);

    if (! m_bFirstPass)
        return;

    // Detect start and end of collapsable instruction sequence.

    Auto<CLabel> pLabel = _instr.getLabel();
    bool bBreak = true;

    if (! pLabel.empty()) {
        m_labels.insert(pLabel.ptr());
        m_labelsFwd.erase(pLabel.ptr());
        if (_instr.getKind() == CInstruction::Nop)
            bBreak = false;
    }

    if (! m_target.empty()) {
        if (m_labels.find(m_target.getLabel().ptr()) == m_labels.end()) {
            bBreak = false;
            m_labelsFwd.insert(m_target.getLabel().ptr());
        } else
            bBreak = true;
    }

    // Process the sequence.
    m_bFirstPass = false;

    if (bBreak && m_iStart != getInstructions()->end()) {
        _assertContains(* getInstructions(), m_iStart);

        instructions_t::iterator iEnd = getIter();

        for (instructions_t::iterator iInstr = m_iStart; iInstr != iEnd; ++ iInstr) {
            CProcessLL::processInstructionIter(iInstr);
            if (! m_target.empty()) {
                if (m_labelsFwd.find(m_target.getLabel().ptr()) != m_labelsFwd.end())
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

    /*if (m_target.getKind() == COperand::Label) {
        return;
    }*/









    /*if (m_target.getKind() == COperand::Empty)
        return;

    assert(m_target.getKind() == COperand::Label);

    //Auto<CLabel> pLabel = m_target.getLabel();

    if (getNext() && ! getNext()->getLabel().empty() &&
            getNext()->getLabel().ptr() == pLabel.ptr())
    {
        setInstruction(new CInstruction());
        return;
    }

    pLabel->incUsageCount();*/
}

void CPruneJumps::processUnary(CUnary & _instr) {
    CProcessLL::processUnary(_instr);

    if (_instr.getUnaryKind() == CUnary::Jmp) {
        rewrite_map_t::iterator iLabel = m_rewriteLabels.find(_instr.getOp().getLabel());
        if (iLabel != m_rewriteLabels.end())
            _instr.getOp().setLabel(iLabel->second);
        m_target = _instr.getOp();
    }
}

void CPruneJumps::processBinary(CBinary & _instr) {
    CProcessLL::processBinary(_instr);

    if (_instr.getBinaryKind() == CBinary::Jmz || _instr.getBinaryKind() == CBinary::Jnz) {
        rewrite_map_t::iterator iLabel = m_rewriteLabels.find(_instr.getOp2().getLabel());
        if (iLabel != m_rewriteLabels.end())
            _instr.getOp2().setLabel(iLabel->second);
        m_target = _instr.getOp2();
    }
}

//
// CCollapseReturns
//

void CCollapseReturns::processBinary(CBinary & _instr) {
    CProcessLL::processBinary(_instr);

    if (_instr.getBinaryKind() != CBinary::Set)
        return;

    if (getNext() == NULL || getNext()->getKind() != CInstruction::Unary)
        return;

    CUnary & nextInstr = (CUnary &) * getNext();

    if (nextInstr.getUnaryKind() != CUnary::Return || nextInstr.getOp().getKind() != COperand::Variable)
        return;

    if (! nextInstr.getLabel().empty())
        return;

    if (nextInstr.getOp().getVariable().ptr() != _instr.getOp1().getVariable().ptr())
        return;

    nextInstr.getOp() = _instr.getOp2();
    setInstruction(new CInstruction()); // nop
}

//
// CAddRefCounting
//
/*
void CAddRefCounting::process() {
    m_ptrs.clear();

    CProcessLL::process();

    for (args_t::iterator iVar = getFunction().locals().begin(); iVar != getFunction().locals().end(); ++ iVar) {
        Auto<CVariable> pVar = * iVar;
        const CType & type = * pVar->getType();

        switch (type.getKind()) {
            case CType::Pointer:
                getFunction().instructions().push_back(
                        new CUnary(CUnary::Unref, COperand(pVar)));
                break;
        }
    }

    for (args_t::iterator iVar = m_ptrs.begin(); iVar != m_ptrs.end(); ++ iVar)
        getFunction().instructions().push_back(new CUnary(CUnary::Unref, COperand(* iVar)));
}

void CAddRefCounting::processInstruction(CInstruction & _instr) {
    m_op = COperand();

    CProcessLL::processInstruction(_instr);

    if (! _instr.getResult().empty() && ! _instr.getResult()->getType().empty()
            && _instr.getResult()->getType()->getKind() == CType::Pointer)
    {
        m_ptrs.push_back(_instr.getResult());
        getInstructions()->insert(getNextIter(), new CUnary(CUnary::Ref, COperand(_instr.getResult())));
    }

    if (m_op.getKind() != COperand::Empty)
        getInstructions()->insert(getNextIter(), new CUnary(CUnary::Ref, m_op));
}

void CAddRefCounting::processUnary(CUnary & _instr) {
}

void CAddRefCounting::processBinary(CBinary & _instr) {
}

void CAddRefCounting::processField(CField & _instr) {
}

void CAddRefCounting::processCast(CCast & _instr) {
}
*/

};

 // namespace llir
