/// \file base.cpp
///

#include "ir/base.h"
#include "ir/types.h"
#include "ir/declarations.h"

namespace ir {

bool isTypeVariable(const NamedValuePtr &_pVar) {
    if (!_pVar || !_pVar->getType())
        return false;

    return _pVar->getType()->getKind() == Type::TYPE;
}

TypePtr resolveBaseType(const TypePtr &_pType) {
    if (!_pType)
        return NULL;

    TypePtr pType = _pType;

    while (pType) {
        if (pType->getKind() == Type::NAMED_REFERENCE) {
            NamedReferenceTypePtr pRef(pType.as<NamedReferenceType>());

            if (pRef->getDeclaration() && pRef->getDeclaration()->getType())
                pType = pRef->getDeclaration()->getType();
            else
                break;
        } else if (pType->getKind() == Type::PARAMETERIZED) {
            pType = pType.as<ParameterizedType>()->getActualType();
        } else
            break;
    }

    return pType;
}

}
