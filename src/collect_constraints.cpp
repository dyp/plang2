/// \file collect_constraints.cpp
///

#include <iostream>

#include <assert.h>

#include "typecheck.h"

#include <ir/statements.h>

using namespace ir;

namespace ir {
    const CType * resolveBaseType(const CType * _pType);
};

class Collector {
public:
    Collector(tc::Formulas & _constraints, CPredicate * _pPredicate,
            CContext & _ctx, tc::FreshTypes & _types);
    ~Collector() {}

    void collectPredicate(CPredicate * _pPred);
    void collectParam(CNamedValue * _pParam);
    void collectBlock(CBlock * _pBlock);
    void collectAssignment(CAssignment * _pAssignment);
    void collectLiteral(CLiteral * _pLiteral);
    void collectVariable(CVariableReference * _pVariable);
    void collectBinary(CBinary * _pBinary);
    void collectPredicateReference(CPredicateReference * _pRef);
    void collectExpression(CExpression * _pExpression);
    void collectStructConstructor(CStructConstructor * _pCons);
    void collectConstructor(CConstructor * _pCons);
    void collectField(CStructFieldExpr * _pField);
    void collectCast(CCastExpr * _pCast);
    void collectStatement(CStatement * _pStmt);
    void collectVarDeclaration(CVariableDeclaration * _pStmt);
    void collectTypeDeclaration(CTypeDeclaration * _pStmt);
    void collectFunctionCall(CFunctionCall * _pCall);
    void collectIf(CIf * _pIf);

protected:
    tc::FreshType * createFresh(CNamedValue * _pParam);
    tc::FreshType * createFresh(CExpression * _pParam);

    void setFreshType(CExpression * _pParam, CType * _pType) {
        _pParam->setType(_pType);
        if (_pType->getKind() == CType::Fresh)
            m_types.insert(std::make_pair((tc::FreshType *) _pType, new tc::TypeSetter<CExpression>(_pParam)));
    }

    void setFreshType(CNamedValue * _pParam, CType * _pType) {
        _pParam->setType(_pType);
        if (_pType->getKind() == CType::Fresh)
            m_types.insert(std::make_pair((tc::FreshType *) _pType, new tc::TypeSetter<CNamedValue>(_pParam)));
    }

private:
    tc::Formulas & m_constraints;
    CPredicate * m_pPredicate;
    CContext & m_ctx;
    tc::FreshTypes & m_types;
};

Collector::Collector(tc::Formulas & _constraints,
        CPredicate * _pPredicate, CContext & _ctx, tc::FreshTypes & _types)
    : m_constraints(_constraints), m_pPredicate(_pPredicate), m_ctx(_ctx), m_types(_types)
{
}

tc::FreshType * Collector::createFresh(CNamedValue * _pParam) {
    tc::FreshType *pType = new tc::FreshType(/*_pParam*/);
    m_types.insert(std::make_pair(pType, new tc::TypeSetter<CNamedValue>(_pParam)));
    return pType;
}

tc::FreshType * Collector::createFresh(CExpression * _pParam) {
    tc::FreshType *pType = new tc::FreshType(/*_pParam*/);
    m_types.insert(std::make_pair(pType, new tc::TypeSetter<CExpression>(_pParam)));
    return pType;
}

/*template<typename _Node>
void*/

void Collector::collectParam(CNamedValue * _pParam) {
    CType * pFresh = createFresh(_pParam);
    CType * pType = (CType *) resolveBaseType(_pParam->getType());

    if (pType != NULL) {
        _pParam->getType()->setParent(NULL); // Suppress delete.
        if (pType->getKind() != CType::Generic)
            m_constraints.insert(new tc::Formula(tc::Formula::Equals,
                    pFresh, pType));
    }

    _pParam->setType(pFresh);
}

void Collector::collectLiteral(CLiteral * _pLiteral) {
    assert(_pLiteral->getType() != NULL);
}

void Collector::collectVariable(CVariableReference * _pVariable) {
    setFreshType(_pVariable, _pVariable->getTarget()->getType());
    /*_pVariable->setType(createFresh(_pVariable));
    m_constraints.insert(new tc::Formula(tc::Formula::Equals,
            _pVariable->getType(), _pVariable->getTarget()->getType()));*/
}

void Collector::collectPredicateReference(CPredicateReference * _pRef) {
    Predicates funcs;

    if (! m_ctx.getPredicates(_pRef->getName(), funcs))
        assert(false);

    _pRef->setType(createFresh(_pRef), false);

    tc::CompoundFormula * pConstraint = new tc::CompoundFormula();

    for (size_t i = 0; i < funcs.size(); ++ i) {
        CPredicate * pPred = funcs.get(i);
        tc::Formulas & part = pConstraint->addPart();

        part.insert(new tc::Formula(tc::Formula::Subtype, pPred->getType(), _pRef->getType()));
    }

    m_constraints.insert(pConstraint);
}

void Collector::collectFunctionCall(CFunctionCall * _pCall) {
    collectExpression(_pCall->getPredicate());
    CPredicateType * pType = new CPredicateType();

    _pCall->setType(createFresh(_pCall), false);
    pType->getOutParams().add(new CBranch(), true);
    pType->getOutParams().get(0)->add(new CParam(L"", _pCall->getType(), false), true);

    for (size_t i = 0; i < _pCall->getParams().size(); ++ i) {
        CExpression * pParam = _pCall->getParams().get(i);
        collectExpression(pParam);
        pType->getInParams().add(new CParam(L"", pParam->getType(), false), true);
    }

    m_constraints.insert(new tc::Formula(tc::Formula::Equals,
            _pCall->getPredicate()->getType(), pType));
}


void Collector::collectBinary(CBinary * _pBinary) {
    collectExpression(_pBinary->getLeftSide());
    collectExpression(_pBinary->getRightSide());
    _pBinary->setType(createFresh(_pBinary));

    switch (_pBinary->getOperator()) {
        case CBinary::Add:
        case CBinary::Subtract:
        case CBinary::Multiply:
        case CBinary::Divide:
        case CBinary::Remainder:
            //         x : A * y : B = z :C
            // -----------------------------------
            // (A <= B and C = B) or (B < A and C = A)
            {
                tc::CompoundFormula * p = new tc::CompoundFormula();
                tc::Formulas & part1 = p->addPart();
                tc::Formulas & part2 = p->addPart();

                part1.insert(new tc::Formula(tc::Formula::Subtype,
                        _pBinary->getLeftSide()->getType(),
                        _pBinary->getRightSide()->getType()));
                part1.insert(new tc::Formula(tc::Formula::Equals,
                        _pBinary->getType(),
                        _pBinary->getRightSide()->getType()));
                part2.insert(new tc::Formula(tc::Formula::SubtypeStrict,
                        _pBinary->getRightSide()->getType(),
                        _pBinary->getLeftSide()->getType()));
                part2.insert(new tc::Formula(tc::Formula::Equals,
                        _pBinary->getType(),
                        _pBinary->getLeftSide()->getType()));
                m_constraints.insert(p);
            }

            // Support only numbers for now.
            m_constraints.insert(new tc::Formula(tc::Formula::Subtype,
                    _pBinary->getLeftSide()->getType(), new CType(CType::Real, CNumber::Generic)));
            m_constraints.insert(new tc::Formula(tc::Formula::Subtype,
                    _pBinary->getRightSide()->getType(), new CType(CType::Real, CNumber::Generic)));
            m_constraints.insert(new tc::Formula(tc::Formula::Subtype,
                    _pBinary->getType(), new CType(CType::Real, CNumber::Generic)));

            break;

        /*case CBinary::Power:
            return _selectInstr(_pType->getKind(), CBinary::Pow, CBinary::FPow, CBinary::ZPow, CBinary::QPow);

        case CBinary::ShiftLeft:
            return _selectInstr(_pType->getKind(), CBinary::Shl, -1, CBinary::ZShl, -1);
        case CBinary::ShiftRight:
            return _selectInstr(_pType->getKind(), CBinary::Shr, -1, CBinary::ZShr, -1);
        */
        case CBinary::Less:
        case CBinary::LessOrEquals:
        case CBinary::Greater:
        case CBinary::GreaterOrEquals:
            m_constraints.insert(new tc::Formula(tc::Formula::Subtype,
                    _pBinary->getLeftSide()->getType(), new CType(CType::Real, CNumber::Generic)));
            m_constraints.insert(new tc::Formula(tc::Formula::Subtype,
                    _pBinary->getRightSide()->getType(), new CType(CType::Real, CNumber::Generic)));
            // no break;
        case CBinary::Equals:
        case CBinary::NotEquals:
            //       (x : A = y : B) : C
            // ----------------------------------
            //   C = bool and (A <= B or B < A)
            m_constraints.insert(new tc::Formula(tc::Formula::Equals,
                    _pBinary->getType(), new CType(CType::Bool)));

            {
                tc::CompoundFormula * p = new tc::CompoundFormula();

                p->addPart().insert(new tc::Formula(tc::Formula::Subtype,
                        _pBinary->getLeftSide()->getType(),
                        _pBinary->getRightSide()->getType()));
                p->addPart().insert(new tc::Formula(tc::Formula::SubtypeStrict,
                        _pBinary->getRightSide()->getType(),
                        _pBinary->getLeftSide()->getType()));

                //_pBinary->h

                m_constraints.insert(p);
            }
            break;

        case CBinary::BoolAnd:
        case CBinary::BitwiseAnd:
        case CBinary::BoolOr:
        case CBinary::BitwiseOr:
        case CBinary::BoolXor:
        case CBinary::BitwiseXor:
            //       (x : A and y : B) : C
            // ----------------------------------
            //   (C = bool and A = bool and B = bool)
            //     or (A <= B and C = B and B <= int)      (bitwise AND)
            //     or (B < A and C = A and A <= int)
            {
                tc::CompoundFormula * p = new tc::CompoundFormula();
                tc::Formulas & part1 = p->addPart();
                tc::Formulas & part2 = p->addPart();
                tc::Formulas & part3 = p->addPart();

                part1.insert(new tc::Formula(tc::Formula::Equals,
                        _pBinary->getLeftSide()->getType(), new CType(CType::Bool)));
                part1.insert(new tc::Formula(tc::Formula::Equals,
                        _pBinary->getRightSide()->getType(), new CType(CType::Bool)));
                part1.insert(new tc::Formula(tc::Formula::Equals,
                        _pBinary->getType(), new CType(CType::Bool)));

                part2.insert(new tc::Formula(tc::Formula::Subtype,
                        _pBinary->getLeftSide()->getType(),
                        _pBinary->getRightSide()->getType()));
                part2.insert(new tc::Formula(tc::Formula::Equals,
                        _pBinary->getType(), _pBinary->getRightSide()->getType()));
                part2.insert(new tc::Formula(tc::Formula::Subtype,
                        _pBinary->getRightSide()->getType(),
                        new CType(CType::Int, CNumber::Generic)));

                part3.insert(new tc::Formula(tc::Formula::SubtypeStrict,
                        _pBinary->getRightSide()->getType(),
                        _pBinary->getLeftSide()->getType()));
                part3.insert(new tc::Formula(tc::Formula::Equals,
                        _pBinary->getType(),
                        _pBinary->getLeftSide()->getType()));
                part3.insert(new tc::Formula(tc::Formula::Subtype,
                        _pBinary->getLeftSide()->getType(),
                        new CType(CType::Int, CNumber::Generic)));

                m_constraints.insert(p);
            }
            break;

        /*
        // TODO: use boolean instructions when type inference gets implemented.
        case CBinary::BoolAnd:
        case CBinary::BitwiseAnd:
            return _selectInstr(_pType->getKind(), CBinary::And, -1, CBinary::ZAnd, -1);
        case CBinary::BoolOr:
        case CBinary::BitwiseOr:
            return _selectInstr(_pType->getKind(), CBinary::Or, -1, CBinary::ZOr, -1);
        case CBinary::BoolXor:
        case CBinary::BitwiseXor:
            return _selectInstr(_pType->getKind(), CBinary::Xor, -1, CBinary::ZXor, -1);*/
    }
}

/*class FieldDefTypeSetter : public tc::TypeSetterBase {
public:
    FieldDefTypeSetter(CStructFieldDefinition * _pField) : m_pField(_pField) {}

    virtual void setType(CType * _pType) {
        CStructType *pStruct = (CStructType *)_pType;
        m_pField->setStructType(pStruct);
    }

private:
    CStructFieldDefinition * m_pField;
};*/

void Collector::collectStructConstructor(CStructConstructor * _pCons) {
    CStructType * pStruct = m_ctx.attach(new CStructType());

    for (size_t i = 0; i < _pCons->size(); ++i) {
        CStructFieldDefinition *pDef = _pCons->get(i);
        CNamedValue *pField = new CNamedValue(pDef->getName());

        collectExpression(pDef->getValue());

        pDef->setStructType(pStruct);
        //pField->setType(pDef->getValue()->getType());

        setFreshType(pField, pDef->getValue()->getType());

        //pField->setType(createFresh(pField));
        pStruct->getFields().add(pField);
        pDef->setField(pField);

        /*// x : A = y : B |- B <= A
        m_constraints.insert(new tc::Formula(tc::Formula::Subtype,
                pDef->getValue()->getType(), pField->getType()));*/
    }

    _pCons->setType(pStruct);

    //pStruct->getFields().add(pField);
    //CStructType *
}

void Collector::collectConstructor(CConstructor * _pCons) {
    switch (_pCons->getConstructorKind()) {
        case CConstructor::StructFields:
            collectStructConstructor((CStructConstructor *) _pCons);
            break;
    }
}

void Collector::collectExpression(CExpression * _pExpression) {
    std::wcout << L"Expression kind: " << _pExpression->getKind() << std::endl;
    switch (_pExpression->getKind()) {
        case CExpression::Literal:
            collectLiteral((CLiteral *) _pExpression);
            break;
        case CExpression::Var:
            collectVariable((CVariableReference *) _pExpression);
            break;
        case CExpression::Binary:
            collectBinary((CBinary *) _pExpression);
            break;
        case CExpression::Predicate:
            collectPredicateReference((CPredicateReference *) _pExpression);
            break;
        case CExpression::FunctionCall:
            collectFunctionCall((CFunctionCall *) _pExpression);
            break;
        case CExpression::Type:
            //collectFunctionCall((CFunctionCall *) _pExpression);
            break;
        case CExpression::Component:
            switch (((CComponent *) _pExpression)->getComponentKind()) {
                case CComponent::StructField:
                    collectField((CStructFieldExpr *) _pExpression);
                    break;
            }
            break;
        case CExpression::Constructor:
            collectConstructor((CConstructor *) _pExpression);
            break;
        case CExpression::Cast:
            collectCast((CCastExpr *) _pExpression);
            break;
        /*case CExpression::Ternary:
            return translate((CTernary &) _expr, _instrs);
        case CExpression::Component:
            return translate((CComponent &) _expr, _instrs);
        case CExpression::Constructor:
            return translate((CConstructor &) _expr, _instrs);*/
    }
}

void Collector::collectField(CStructFieldExpr * _pField) {
    collectExpression(_pField->getObject());
    _pField->setType(createFresh(_pField));

    ir::CStructType * pStruct = m_ctx.attach(new CStructType());
    ir::CNamedValue * pField = new CNamedValue(_pField->getFieldName(), createFresh(_pField));

    pStruct->getFields().add(pField);

    if (_pField->getObject()->getType()->getKind() == CType::Fresh) {
        tc::FreshType * pFresh = (tc::FreshType *) _pField->getObject()->getType();
        ((tc::FreshType *) pField->getType())->setFlags(pFresh->getFlags());
    }

    // (x : A).foo : B  |-  A <= struct(B foo)
    m_constraints.insert(new tc::Formula(tc::Formula::Subtype,
            _pField->getObject()->getType(), pStruct));
    m_constraints.insert(new tc::Formula(tc::Formula::Equals,
            _pField->getType(), pField->getType()));
}

void Collector::collectCast(CCastExpr * _pCast) {
    //collectExpression(_pCast->getToType());
    collectExpression(_pCast->getExpression());

    CType * pToType = (CType *)resolveBaseType(_pCast->getToType()->getContents());

    _pCast->setType(pToType, false);

    if (pToType->getKind() == CType::Struct &&
            _pCast->getExpression()->getKind() == CExpression::Constructor &&
            ((CConstructor *) _pCast->getExpression())->getConstructorKind() == CConstructor::StructFields)
    {
        // We can cast form tuples to structs explicitly.
        CStructType * pStruct = (CStructType *) pToType;
        CStructConstructor * pFields = (CStructConstructor *) _pCast->getExpression();
        bool bSuccess = true;

        // TODO: maybe use default values for fields.
        if (pStruct->getFields().size() == pFields->size()) {
            for (size_t i = 0; i < pFields->size(); ++i) {
                CStructFieldDefinition * pDef = pFields->get(i);
                size_t cOtherIdx = pDef->getName().empty() ? i : pStruct->getFields().findByNameIdx(pDef->getName());

                if (cOtherIdx == (size_t) -1) {
                    bSuccess = false;
                    break;
                }

                CNamedValue * pField = pStruct->getFields().get(cOtherIdx);

                m_constraints.insert(new tc::Formula(tc::Formula::Subtype,
                        pDef->getValue()->getType(), pField->getType()));
            }
        }

        if (bSuccess)
            return;
    }

    // (A)(x : B) |- B <= A
    m_constraints.insert(new tc::Formula(tc::Formula::Subtype,
            _pCast->getExpression()->getType(), pToType));
}

void Collector::collectAssignment(CAssignment * _pAssignment) {
    collectExpression(_pAssignment->getLValue());
    collectExpression(_pAssignment->getExpression());

    // x : A = y : B |- B <= A
    m_constraints.insert(new tc::Formula(tc::Formula::Subtype,
            _pAssignment->getExpression()->getType(),
            _pAssignment->getLValue()->getType()));
}

void Collector::collectVarDeclaration(CVariableDeclaration * _pStmt) {
    collectParam(& _pStmt->getVariable());

    ((tc::FreshType *) _pStmt->getVariable().getType())->addFlags(tc::FreshType::ParamOut);

    if (_pStmt->getValue() == NULL)
        return;

    collectExpression(_pStmt->getValue());

    // x : A = y : B |- B <= A
    m_constraints.insert(new tc::Formula(tc::Formula::Subtype,
            _pStmt->getValue()->getType(),
            _pStmt->getVariable().getType()));
}

void Collector::collectTypeDeclaration(CTypeDeclaration * _pDecl) {
   /* _pDecl->getType();
    collectParam(& _pStmt->getVariable());

    ((tc::FreshType *) _pStmt->getVariable().getType())->addFlags(tc::FreshType::ParamOut);

    if (_pStmt->getValue() == NULL)
        return;

    collectExpression(_pStmt->getValue());

    // x : A = y : B |- B <= A
    m_constraints.insert(new tc::Formula(tc::Formula::Subtype,
            _pStmt->getValue()->getType(),
            _pStmt->getVariable().getType()));*/
}

void Collector::collectIf(CIf * _pIf) {
    collectExpression(_pIf->getParam());

    m_constraints.insert(new tc::Formula(tc::Formula::Equals,
            _pIf->getParam()->getType(), new CType(CType::Bool)));
    collectStatement(_pIf->getBody());

    if (_pIf->getElse())
        collectStatement(_pIf->getElse());
}

void Collector::collectStatement(CStatement * _pStmt) {
    switch(_pStmt->getKind()) {
        case CStatement::Block:
            collectBlock((CBlock *) _pStmt);
            break;
        case CStatement::Assignment:
            collectAssignment((CAssignment *) _pStmt);
            break;
        case CStatement::VariableDeclaration:
            collectVarDeclaration((CVariableDeclaration *) _pStmt);
            break;
        case CStatement::TypeDeclaration:
            collectTypeDeclaration((CTypeDeclaration *) _pStmt);
            break;
        /*case CStatement::ParallelBlock:
            return print(* (CParallelBlock *) (& _stmt));
        case CStatement::Multiassignment:
            return print(* (CMultiassignment *) (& _stmt));
        case CStatement::Switch:
            return print(* (CSwitch *) (& _stmt));*/
        case CStatement::If:
            collectIf((CIf *) _pStmt);
            break;
        /*case CStatement::Jump:
            return print(* (CJump *) (& _stmt));
        case CStatement::Receive:
            return print(* (CReceive *) (& _stmt));
        case CStatement::Send:
            return print(* (CSend *) (& _stmt));
        case CStatement::With:
            return print(* (CWith *) (& _stmt));
        case CStatement::For:
            return print(* (CFor *) (& _stmt));
        case CStatement::While:
            return print(* (CWhile *) (& _stmt));
        case CStatement::Break:
            return print(* (CBreak *) (& _stmt));
        case CStatement::Call:
            return print(* (CCall *) (& _stmt));

        case CStatement::TypeDeclaration:
            return print(* (CTypeDeclaration *) (& _stmt));
        case CStatement::PredicateDeclaration:
            return print(* (CPredicate *) (& _stmt));*/
    }
}

void Collector::collectBlock(CBlock * _pBlock) {
    for (size_t i = 0; i < _pBlock->size(); ++ i)
        collectStatement(_pBlock->get(i));
}

void Collector::collectPredicate(CPredicate * _pPred) {
    for (size_t i = 0; i < _pPred->getInParams().size(); ++ i) {
        collectParam(_pPred->getInParams().get(i));
        ((tc::FreshType *) _pPred->getInParams().get(i)->getType())->addFlags(tc::FreshType::ParamIn);
    }

    for (size_t j = 0; j < _pPred->getOutParams().size(); ++ j) {
        CBranch * pBranch = _pPred->getOutParams().get(j);

        for (size_t i = 0; i < pBranch->size(); ++ i) {
            collectParam(pBranch->get(i));
            ((tc::FreshType *) pBranch->get(i)->getType())->addFlags(tc::FreshType::ParamOut);
        }
    }

    collectBlock(_pPred->getBlock());
}

void tc::collect(Formulas & _constraints, CPredicate & _pred, CContext & _ctx, FreshTypes & _types) {
    Collector collector(_constraints, & _pred, _ctx, _types);
    collector.collectPredicate(& _pred);
}


#if 0
class CCollector {
public:
    CCollector(constraints_t & _constraints, CPredicate * _pPredicate,
            CContext & _ctx);
    ~CCollector() {}

    void collectPredicate(CPredicate * _pPred);
    void collectParam(CNamedValue * _pParam);
    void collectBlock(CBlock * _pBlock);
    void collectAssignment(CAssignment * _pAssignment);
    void collectLiteral(CLiteral * _pLiteral);
    void collectVariable(CVariableReference * _pVariable);
    void collectBinary(CBinary * _pBinary);
    void collectExpression(CExpression * _pExpression);
    void collectStatement(CStatement * _pStmt);

protected:
    CFreshType * createFresh(CNamedValue * _pParam);
    CFreshType * createFresh(CExpression * _pParam);
    CConstraint & addConstraint(CType * _pLhs, CType * _pRhs);

private:
    constraints_t & m_constraints;
    CPredicate * m_pPredicate;
    CContext & m_ctx;
};

CCollector::CCollector(constraints_t & _constraints,
        CPredicate * _pPredicate, CContext & _ctx)
    : m_constraints(_constraints), m_pPredicate(_pPredicate), m_ctx(_ctx)
{
}

CFreshType * CCollector::createFresh(CNamedValue * _pParam) {
    return m_ctx.attach(new CFreshType(_pParam));
}

CFreshType * CCollector::createFresh(CExpression * _pParam) {
    return m_ctx.attach(new CFreshType(_pParam));
}

CConstraint & CCollector::addConstraint(CType * _pLhs, CType * _pRhs) {
    m_constraints.push_back(CConstraint());
    m_constraints.back().setLhs(_pLhs);
    m_constraints.back().setRhs(_pRhs);
    return m_constraints.back();
}

void CCollector::collectParam(CNamedValue * _pParam) {
    CType * pFresh = createFresh(_pParam);
    if (_pParam->getType() != NULL) {
        _pParam->getType()->setParent(NULL); // Force reparent.
        addConstraint(pFresh, _pParam->getType());
    }
    _pParam->setType(pFresh);
}

void CCollector::collectLiteral(CLiteral * _pLiteral) {
    assert(_pLiteral->getType() != NULL);
}

void CCollector::collectVariable(CVariableReference * _pVariable) {
    _pVariable->setType(createFresh(_pVariable));
    addConstraint(_pVariable->getType(), _pVariable->getTarget()->getType());
}

void CCollector::collectBinary(CBinary * _pBinary) {
    switch (_pBinary->getOperator()) {
        case ir::CBinary::Add:
        case ir::CBinary::Subtract:
        case ir::CBinary::Multiply:
        case ir::CBinary::Divide:
        case ir::CBinary::Remainder:
            collectExpression(_pBinary->getLeftSide());
            collectExpression(_pBinary->getRightSide());
            _pBinary->setType(createFresh(_pBinary));
            addConstraint(_pBinary->getType(), _pBinary->getLeftSide()->getType());
            addConstraint(_pBinary->getType(), _pBinary->getRightSide()->getType());

        /*case ir::CBinary::Power:
            return _selectInstr(_pType->getKind(), CBinary::Pow, CBinary::FPow, CBinary::ZPow, CBinary::QPow);

        case ir::CBinary::ShiftLeft:
            return _selectInstr(_pType->getKind(), CBinary::Shl, -1, CBinary::ZShl, -1);
        case ir::CBinary::ShiftRight:
            return _selectInstr(_pType->getKind(), CBinary::Shr, -1, CBinary::ZShr, -1);

        case ir::CBinary::Equals:
            return _selectInstr(_pType->getKind(), CBinary::Eq, CBinary::FEq, CBinary::ZEq, CBinary::QEq);
        case ir::CBinary::NotEquals:
            return _selectInstr(_pType->getKind(), CBinary::Ne, CBinary::FNe, CBinary::ZNe, CBinary::QNe);
        case ir::CBinary::Less:
            return _selectInstr(_pType->getKind(), CBinary::Lt, CBinary::FLt, CBinary::ZLt, CBinary::QLt);
        case ir::CBinary::LessOrEquals:
            return _selectInstr(_pType->getKind(), CBinary::Lte, CBinary::FLte, CBinary::ZLte, CBinary::QLte);
        case ir::CBinary::Greater:
            return _selectInstr(_pType->getKind(), CBinary::Gt, CBinary::FGt, CBinary::ZGt, CBinary::QGt);
        case ir::CBinary::GreaterOrEquals:
            return _selectInstr(_pType->getKind(), CBinary::Gte, CBinary::FGte, CBinary::ZGte, CBinary::QGte);

        // TODO: use boolean instructions when type inference gets implemented.
        case ir::CBinary::BoolAnd:
        case ir::CBinary::BitwiseAnd:
            return _selectInstr(_pType->getKind(), CBinary::And, -1, CBinary::ZAnd, -1);
        case ir::CBinary::BoolOr:
        case ir::CBinary::BitwiseOr:
            return _selectInstr(_pType->getKind(), CBinary::Or, -1, CBinary::ZOr, -1);
        case ir::CBinary::BoolXor:
        case ir::CBinary::BitwiseXor:
            return _selectInstr(_pType->getKind(), CBinary::Xor, -1, CBinary::ZXor, -1);*/
    }
}

void CCollector::collectExpression(CExpression * _pExpression) {
    switch (_pExpression->getKind()) {
        case ir::CExpression::Literal:
            collectLiteral((ir::CLiteral *) _pExpression);
            break;
        case ir::CExpression::Var:
            collectVariable((ir::CVariableReference *) _pExpression);
            break;
        case ir::CExpression::Binary:
            collectBinary((ir::CBinary *) _pExpression);
            break;
        /*case ir::CExpression::Predicate:
            return translate((ir::CPredicateReference &) _expr, _instrs);
        case ir::CExpression::FunctionCall:
            return translate((ir::CFunctionCall &) _expr, _instrs);
        case ir::CExpression::Ternary:
            return translate((ir::CTernary &) _expr, _instrs);
        case ir::CExpression::Component:
            return translate((ir::CComponent &) _expr, _instrs);
        case ir::CExpression::Constructor:
            return translate((ir::CConstructor &) _expr, _instrs);*/
    }
}

void CCollector::collectAssignment(CAssignment * _pAssignment) {
    collectExpression(_pAssignment->getLValue());
    collectExpression(_pAssignment->getExpression());
    addConstraint(_pAssignment->getLValue()->getType(),
            _pAssignment->getExpression()->getType());
}

void CCollector::collectStatement(CStatement * _pStmt) {
    switch(_pStmt->getKind()) {
        case CStatement::Block:
            collectBlock((CBlock *) _pStmt);
            break;
        case CStatement::Assignment:
            collectAssignment((CAssignment *) _pStmt);
            break;
        /*case CStatement::ParallelBlock:
            return print(* (CParallelBlock *) (& _stmt));
        case CStatement::Multiassignment:
            return print(* (CMultiassignment *) (& _stmt));
        case CStatement::Switch:
            return print(* (CSwitch *) (& _stmt));
        case CStatement::If:
            return print(* (CIf *) (& _stmt));
        case CStatement::Jump:
            return print(* (CJump *) (& _stmt));
        case CStatement::Receive:
            return print(* (CReceive *) (& _stmt));
        case CStatement::Send:
            return print(* (CSend *) (& _stmt));
        case CStatement::With:
            return print(* (CWith *) (& _stmt));
        case CStatement::For:
            return print(* (CFor *) (& _stmt));
        case CStatement::While:
            return print(* (CWhile *) (& _stmt));
        case CStatement::Break:
            return print(* (CBreak *) (& _stmt));
        case CStatement::Call:
            return print(* (CCall *) (& _stmt));

        case CStatement::TypeDeclaration:
            return print(* (CTypeDeclaration *) (& _stmt));
        case CStatement::VariableDeclaration:
            return print(* (CVariableDeclaration *) (& _stmt));
        case CStatement::PredicateDeclaration:
            return print(* (CPredicate *) (& _stmt));*/
    }
}

void CCollector::collectBlock(CBlock * _pBlock) {
    for (size_t i = 0; i < _pBlock->size(); ++ i)
        collectStatement(_pBlock->get(i));
}

void CCollector::collectPredicate(CPredicate * _pPred) {
    for (size_t i = 0; i < _pPred->getInParams().size(); ++ i)
        collectParam(_pPred->getInParams().get(i));

    for (size_t j = 0; j < _pPred->getOutParams().size(); ++ j) {
        CBranch * pBranch = _pPred->getOutParams().get(j);

        for (size_t i = 0; i < pBranch->size(); ++ i)
            collectParam(pBranch->get(i));
    }

    collectBlock(_pPred->getBlock());
}

void collectConstraints(constraints_t & _constraints,
        CPredicate * _pPredicate, CContext & _ctx)
{
    CCollector collector(_constraints, _pPredicate, _ctx);
    collector.collectPredicate(_pPredicate);
}
#endif
