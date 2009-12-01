/// \file ir.cpp
///

#include "ir/expressions.h"
#include "ir/types.h"

namespace ir {

CAnonymousPredicate::~CAnonymousPredicate() {
    _delete(m_pPreCond);
    _delete(m_pPostCond);
    _delete(m_pBlock);
    _delete(m_pType);
}

void CAnonymousPredicate::updateType() const {
    CPredicateType * pType = new CPredicateType();

    pType->getInParams().append(m_paramsIn, false);
    pType->getOutParams().append(m_paramsOut, false);
    pType->setPreCondition(m_pPreCond, false);
    pType->setPostCondition(m_pPostCond, false);

    const_cast<CAnonymousPredicate *>(this)->_assign(m_pType, pType, true);
}

const CNamedValue * CStructFieldExpr::getField() const {
    return getStructType()->getFields().get(m_cFieldIdx);
}

CUnionAlternativeExpr::CUnionAlternativeExpr(const CUnionType * _pType, size_t _cIdx) :
    m_strName(_pType->getAlternatives().get(_cIdx)->getName()), m_pType(_pType), m_cIdx(_cIdx)
{
}

const CNamedValue * CUnionAlternativeExpr::getAlternative() const {
    return getUnionType()->getAlternatives().get(m_cIdx);
}

CType * CFunctionCall::getType() const {
    CFunctionCall * pThis = const_cast<CFunctionCall *>(this);

    if (CExpression::getType())
        return CExpression::getType();

    if (! m_pPredicate || ! m_pPredicate->getType() || m_pPredicate->getType()->getKind() != CType::Predicate)
        return NULL;

    CPredicateType * pType = (CPredicateType *) m_pPredicate->getType();
    CBranches & branches = pType->getOutParams();

    if (branches.empty()) {
        pThis->setType(new CType(CType::Unit));
        return CExpression::getType ();
    }

    if (branches.size() > 1)
        return NULL;

    CBranch * pBranch = branches.get(0);

    if (pBranch->empty()) {
        pThis->setType(new CType(CType::Unit));
    } else if (pBranch->size() == 1) {
        pThis->setType(pBranch->get(0)->getType());
    } else {
        CStructType * pReturnType = new CStructType();

        pReturnType->getFields().append(* pBranch, false);
        pThis->setType(pReturnType);
    }

    return CExpression::getType ();
}

};
