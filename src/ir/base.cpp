/// \file base.cpp
///

#include "ir/base.h"
#include "ir/types.h"
#include "ir/declarations.h"
#include "ir/visitor.h"

class ChildrenCollector : public ir::Visitor {
public:
    ChildrenCollector(const ir::NodesPtr _pContainer) :
        m_pContainer(_pContainer), m_bRoot(true)
    {}

    bool visitNode(ir::Node& _node) {
        if (m_bRoot) {
            m_bRoot = false;
            return true;
        }
        m_pContainer->add(&_node);
        return false;
    }

    ir::NodesPtr run(const ir::NodePtr _pNode) {
        if (_pNode && m_pContainer)
            traverseNode(*_pNode);
        return m_pContainer;
    }

private:
    ir::NodesPtr m_pContainer;
    bool m_bRoot;
};

namespace ir {

NodesPtr Node::getChildren() const {
    return ChildrenCollector(new Nodes()).run(this);
}

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
