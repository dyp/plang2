/// \file ir.cpp
///

#include "ir/expressions.h"
#include "ir/types.h"
#include "ir/declarations.h"
#include "ir/statements.h"
#include "typecheck.h"

using namespace ir;

std::wstring NamedReferenceType::getName() const {
    return m_pDecl != NULL ? m_pDecl->getName() : L"";
}

std::wstring FormulaCall::getName() const {
    return m_pTarget != NULL ? m_pTarget->getName() : L"";
}

const std::wstring &PredicateReference::getName() const {
    return m_pTarget != NULL ? m_pTarget->getName() : m_strName;
}

bool UnionConstructor::isComplete() const {
    return m_decls.empty() && m_pProto && size() == m_pProto->getStruct().getFields().size();
}

AnonymousPredicate::~AnonymousPredicate() {
    _delete(m_pPreCond);
    _delete(m_pPostCond);
    _delete(m_pBlock);
    _delete(m_pType);
}

void AnonymousPredicate::updateType() const {
    PredicateType * pType = new PredicateType();

    pType->getInParams().append(m_paramsIn, false);
    pType->getOutParams().append(m_paramsOut, false);
    pType->setPreCondition(m_pPreCond, false);
    pType->setPostCondition(m_pPostCond, false);

    const_cast<AnonymousPredicate *>(this)->_assign(m_pType, pType, true);
}

std::wstring VariableDeclaration::getName() const {
    return m_pVar != NULL ? m_pVar->getName() : L"";
}

UnionFieldIdx UnionType::findField(const std::wstring & _strName) const {
    for (size_t i = 0; i < m_constructors.size(); ++ i) {
        size_t cIdx = m_constructors.get(i)->getStruct().getFields().findByNameIdx(_strName);
        if (cIdx != (size_t) -1)
            return UnionFieldIdx(i, cIdx);
    }

    return UnionFieldIdx((size_t) -1, (size_t) -1);
}

UnionAlternativeExpr::UnionAlternativeExpr(const UnionType * _pType, const UnionFieldIdx & _idx) :
    m_strName(_pType->getConstructors().get(_idx.first)->getStruct().getFields().get(_idx.second)->getName()), m_pType(_pType), m_idx(_idx)
{
}

const UnionConstructorDeclaration * UnionAlternativeExpr::getConstructor() const {
    return getUnionType()->getConstructors().get(m_idx.first);
}

const NamedValue * UnionAlternativeExpr::getField() const {
    return getConstructor()->getStruct().getFields().get(m_idx.second);
}

Type * FunctionCall::getType() const {
    FunctionCall * pThis = const_cast<FunctionCall *>(this);

    if (Expression::getType())
        return Expression::getType();

    if (! m_pPredicate || ! m_pPredicate->getType() || m_pPredicate->getType()->getKind() != Type::PREDICATE)
        return NULL;

    PredicateType * pType = (PredicateType *) m_pPredicate->getType();
    Branches & branches = pType->getOutParams();

    if (branches.empty()) {
        pThis->setType(new Type(Type::UNIT));
        return Expression::getType ();
    }

    if (branches.size() > 1)
        return NULL;

    Branch * pBranch = branches.get(0);

    if (pBranch->empty()) {
        pThis->setType(new Type(Type::UNIT));
    } else if (pBranch->size() == 1) {
        pThis->setType(pBranch->get(0)->getType());
    } else {
        StructType * pReturnType = new StructType();

        pReturnType->getFields().append(* pBranch, false);
        pThis->setType(pReturnType);
    }

    return Expression::getType ();
}
