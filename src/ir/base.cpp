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

bool Label::less(const Node& _other) const {
    if (!Node::equals(_other))
        return Node::less(_other);
    return getName() < ((const Label&)_other).getName();
}

bool Label::equals(const Node& _other) const {
    if (!Node::equals(_other))
        return false;
    return getName() == ((const Label&)_other).getName();
}

bool Statement::less(const Node& _other) const {
    if (!Node::equals(_other))
        return Node::less(_other);
    const Statement& other = (const Statement&)_other;
    if (getKind() != other.getKind())
        return getKind() < other.getKind();
    return _less(getLabel(), other.getLabel());
}

bool Statement::equals(const Node& _other) const {
    if (!Node::equals(_other))
        return false;
    const Statement& other = (const Statement&)_other;
    return getKind() == other.getKind() && _equals(getLabel(), other.getLabel());
}

bool NamedValue::less(const Node& _other) const {
    if (!Node::equals(_other))
        return Node::less(_other);
    const NamedValue& other = (const NamedValue&)_other;
    if (getKind() != other.getKind())
        return getKind() < other.getKind();
    if (*getType() != *other.getType())
        return *getType() < *other.getType();
    return getName() < other.getName();
}

bool NamedValue::equals(const Node& _other) const {
    if (!Node::equals(_other))
        return false;
    const NamedValue& other = (const NamedValue&)_other;
    return getKind() == other.getKind()
        && *getType() == *other.getType()
        && getName() == other.getName();
}

}
