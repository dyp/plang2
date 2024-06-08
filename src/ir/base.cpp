/// \file base.cpp
///

#include <set>

#include "ir/base.h"
#include "ir/types.h"
#include "ir/declarations.h"
#include "ir/visitor.h"

class ChildrenCollector : public ir::Visitor {
public:
    ChildrenCollector(const ir::NodesPtr _pContainer) :
        m_pContainer(_pContainer), m_bRoot(true)
    {}

    bool visitNode(const ir::NodePtr& _pNode) {
        if (m_bRoot) {
            m_bRoot = false;
            return true;
        }
        m_pContainer->add(_pNode);
        return false;
    }

    ir::NodesPtr run(const ir::NodePtr _pNode) {
        if (_pNode && m_pContainer)
            traverseNode(_pNode);
        return m_pContainer;
    }

private:
    ir::NodesPtr m_pContainer;
    bool m_bRoot;
};

namespace ir {

NodesPtr Node::getChildren() const {
    return ChildrenCollector(std::make_shared<Nodes>()).run(std::const_pointer_cast<Node>(shared_from_this()));
}

bool Node::_less(const NodeConstPtr& _pLeft, const NodeConstPtr& _pRight) {
    if (_pLeft == _pRight)
        return false;
    return (_pLeft && _pRight) ? *_pLeft < *_pRight : !_pLeft && _pRight;
}

bool Node::_equals(const NodeConstPtr& _pLeft, const NodeConstPtr& _pRight) {
    if (_pLeft == _pRight)
        return true;
    return (_pLeft && _pRight) ? *_pLeft == *_pRight : (bool)_pLeft == (bool)_pRight;
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
            auto pRef = std::static_pointer_cast<NamedReferenceType>(pType);

            if (pRef->getDeclaration() && pRef->getDeclaration()->getType())
                pType = pRef->getDeclaration()->getType();
            else
                break;
        } else if (pType->getKind() == Type::PARAMETERIZED) {
            pType = std::static_pointer_cast<ParameterizedType>(pType)->getActualType();
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
    if (getName() != other.getName())
        return getName() < other.getName();
    return _less(getType(), other.getType());
}

bool NamedValue::equals(const Node& _other) const {
    if (!Node::equals(_other))
        return false;
    const NamedValue& other = (const NamedValue&)_other;
    return getKind() == other.getKind()
        && getName() == other.getName()
        && _equals(getType(), other.getType());
}

void Param::updateUsed(const NodePtr &_pRoot) {
    struct Enumerator : public Visitor {
        std::set<NamedValuePtr> params;

        bool visitParam(const ParamPtr &_pParam) override {
            _pParam->setUsed(false);
            params.insert(_pParam);
            return true;
        }
    };

    struct Updater : public Visitor {
        Enumerator enumerator;

        void run(const NodePtr &_pRoot) {
            enumerator.traverseNode(_pRoot);
            traverseNode(_pRoot);
        }

        bool visitVariableReference(const VariableReferencePtr &_pVal) override {
            if (_pVal->getTarget() && _pVal->getTarget()->getKind() == NamedValue::PREDICATE_PARAMETER &&
                enumerator.params.find(_pVal->getTarget()) != enumerator.params.end()) {
                if (const auto paramPtr = _pVal->getTarget()->as<Param>()) {
                    paramPtr->setUsed(true);
                }
            }
            return true;
        }
    } updater;

    updater.run(_pRoot);
}


}
