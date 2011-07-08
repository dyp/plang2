/// \file ir.cpp
///

#include "ir/expressions.h"
#include "ir/types.h"
#include "ir/declarations.h"
#include "ir/statements.h"
#include "typecheck.h"

using namespace ir;

std::wstring NamedReferenceType::getName() const {
    return m_pDecl ? m_pDecl->getName() : L"";
}

std::wstring FormulaCall::getName() const {
    return m_pTarget ? m_pTarget->getName() : L"";
}

const std::wstring &PredicateReference::getName() const {
    return m_pTarget ? m_pTarget->getName() : m_strName;
}

bool UnionConstructor::isComplete() const {
    return m_decls.empty() && m_pProto && size() == m_pProto->getFields().size();
}

void AnonymousPredicate::updateType() const {
    PredicateTypePtr pType = new PredicateType();

    pType->getInParams().append(m_paramsIn);
    pType->getOutParams().append(m_paramsOut);
    pType->setPreCondition(m_pPreCond);
    pType->setPostCondition(m_pPostCond);

    const_cast<AnonymousPredicate *>(this)->m_pType = pType;
}

std::wstring VariableDeclaration::getName() const {
    return m_pVar ? m_pVar->getName() : L"";
}

UnionFieldIdx UnionType::findField(const std::wstring &_strName) const {
    for (size_t i = 0; i < m_constructors.size(); ++i) {
        size_t cIdx = m_constructors.get(i)->getFields().findByNameIdx(_strName);
        if (cIdx != (size_t) -1)
            return UnionFieldIdx(i, cIdx);
    }

    return UnionFieldIdx((size_t) -1, (size_t) -1);
}

UnionAlternativeExpr::UnionAlternativeExpr(const UnionTypePtr &_pType, const UnionFieldIdx &_idx) :
    m_strName(_pType->getConstructors().get(_idx.first)->getFields().get(_idx.second)->getName()), m_pType(_pType), m_idx(_idx)
{
}

UnionConstructorDeclarationPtr UnionAlternativeExpr::getConstructor() const {
    return getUnionType()->getConstructors().get(m_idx.first);
}

NamedValuePtr UnionAlternativeExpr::getField() const {
    return getConstructor()->getFields().get(m_idx.second);
}
