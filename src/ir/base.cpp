/// \file base.cpp
///

#include "ir/base.h"
#include "ir/types.h"
#include "ir/declarations.h"

namespace ir {

bool isTypeVariable(const NamedValue *_pVar, const Type *&_pType) {
    if (!_pVar || !_pVar->getType())
        return false;

    return _pVar->getType()->getKind() == Type::TYPE;
}

const Type *resolveBaseType(const Type *_pType) {
    if (! _pType)
        return NULL;

    while (_pType) {
        if (_pType->getKind() == Type::NAMED_REFERENCE) {
            const NamedReferenceType *pRef = (NamedReferenceType *)_pType;

            if (pRef->getDeclaration() != NULL && pRef->getDeclaration()->getType() != NULL)
                _pType = pRef->getDeclaration()->getType();
            else
                break;
        } else if (_pType->getKind() == Type::PARAMETERIZED) {
            _pType = ((ParameterizedType *)_pType)->getActualType();
        } else
            break;
    }

    return _pType;
}

}
