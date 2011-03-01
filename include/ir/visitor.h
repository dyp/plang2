
#ifndef VISITOR_H_
#define VISITOR_H_

#include <list>

#include "base.h"
#include "types.h"
#include "declarations.h"
#include "expressions.h"
#include "statements.h"

namespace ir {

enum NodeType {
#define NODE(_Node, ...) N_##_Node,
#include "nodes.inl"
#undef NODE
};

enum NodeRole {
#define ROLE(_Role) R_##_Role,
#include "roles.inl"
#undef ROLE
};

class NodeSetter {
public:
    virtual void set(Node *_pValue, bool _bReparent = true) {}
};

template<typename _Node, typename _Member, void (_Node::*_Method)(_Member *, bool)>
class NodeSetterImpl : public NodeSetter {
public:
    NodeSetterImpl(_Node *_pNode) : m_pNode(_pNode) {}

    virtual void set(Node *_pValue, bool _bReparent = true) {
        ((*m_pNode).*(_Method))((_Member *)_pValue, _bReparent);
    }

protected:
    _Node *m_pNode;
};

template<class _Node, class _Base>
class ItemSetterImpl : public NodeSetter {
public:
    ItemSetterImpl(Collection<_Node, _Base> *_pNodes, size_t _cIndex) : m_pNodes(_pNodes), m_cIndex(_cIndex) {}

    virtual void set(Node *_pValue, bool _bReparent = true) {
        m_pNodes->set(m_cIndex, (_Node *)_pValue, _bReparent);
    }

protected:
    Collection<_Node, _Base> *m_pNodes;
    size_t m_cIndex;
};

class Visitor {
public:
    typedef int (Visitor::*RoleHandler)(Node &_node);
    typedef bool (Visitor::*NodeWalkUp)(Node &_node);

    enum {
        PARENTS_FIRST,
        CHILDREN_FIRST,
    };

    Visitor(int _order = PARENTS_FIRST) : m_bStopped(false), m_order(_order) {}

    struct Loc {
        Node *pNode;
        NodeType type;
        NodeRole role;
        RoleHandler roleHandler;
        NodeSetter *pSetter;
        NodeWalkUp walkUp;
        bool bPartOfCollection;
        bool bFirstInCollection;

        Loc(Node *_pNode, NodeType _type, NodeRole _role, RoleHandler _roleHandler = NULL,
                NodeSetter *_pSetter = NULL, NodeWalkUp _walkUp = NULL) :
            pNode(_pNode), type(_type), role(_role), roleHandler(_roleHandler), pSetter(_pSetter),
            walkUp(_walkUp), bPartOfCollection(false), bFirstInCollection(false)
        {
        }
    };

    virtual bool traverseNode(Node &_node);
    template<class _Node, class _Base> bool traverseCollection(Collection<_Node, _Base> &_nodes);
    virtual bool visitNode(Node &_node);
    bool walkUpFromNode(Node &_node);

#define NODE(_NODE, _PARENT)                            \
        bool walkUpFrom##_NODE(Node &_node) {       \
            if (!walkUpFrom##_PARENT(_node))            \
                return false;                           \
            return visit##_NODE((_NODE &)_node);                 \
        }                                               \
        virtual bool visit##_NODE(_NODE &_node) {    \
            return true;                                \
        }                                               \
        virtual bool traverse##_NODE(_NODE &_node);
#include "nodes.inl"
#undef NODE

#define ROLE(_ROLE) \
    virtual int handle##_ROLE(Node &_node) { return 0; }
#include "roles.inl"
#undef ROLE

    bool isStopped() const { return m_bStopped; }
    void reset() { m_bStopped = false; }

protected:
    struct Ctx {
        Visitor *pVisitor;

        Ctx(Visitor *_pVisitor, Node *_pNode, NodeType _type, NodeRole _role,
                RoleHandler _roleHandler = NULL, NodeSetter *_pSetter = NULL, NodeWalkUp _walkUp = NULL)
            : pVisitor(_pVisitor)
        {
            pVisitor->m_path.push_back(Loc(_pNode, _type, _role, _roleHandler, _pSetter, _walkUp));
        }

        ~Ctx() {
            pVisitor->m_path.pop_back();
        }
    };

    friend struct Ctx;
    std::list<Loc> m_path;

    size_t getDepth() const { return m_path.size(); }
    Node *getNode() { return m_path.empty() ? NULL : m_path.back().pNode; }
    RoleHandler getRoleHandler() { return m_path.empty() ? NULL : m_path.back().roleHandler; }
    NodeWalkUp getWalkUp() { return m_path.empty() ? NULL : m_path.back().walkUp; }
    NodeSetter *getNodeSetter() { return m_path.empty() ? NULL : m_path.back().pSetter; }
    NodeRole getRole() { return m_path.empty() ? R_TopLevel : m_path.back().role; }
    Loc & getLoc() { return m_path.back(); }

    int callRoleHandler() {
        return getRoleHandler() == NULL ? 0 : (this->*getRoleHandler())(*getNode());
    }

    bool callWalkUp() {
        return getWalkUp() == NULL ? true : (this->*getWalkUp())(*getNode());
    }

    void callSetter(Node *_pNewNode, bool _bReparent = true) {
        if (getNodeSetter() != NULL)
            getNodeSetter()->set(_pNewNode, _bReparent);
    }

    void stop() { m_bStopped = true; }

    void setOrder(int _order) { m_order = _order; }

private:
    bool m_bStopped;
    int m_order;

    bool _traverseAnonymousPredicate(AnonymousPredicate &_decl);
    bool _traverseDeclarationGroup(DeclarationGroup &_decl);
};

template<class _Node, class _Base>
bool Visitor::traverseCollection(Collection<_Node, _Base> &_nodes) {
    m_path.back().bFirstInCollection = true;

    for (size_t i = 0; i < _nodes.size(); ++i) {
        if (_nodes.get(i) != NULL) {
            ItemSetterImpl<_Node, _Base> setter(&_nodes, i);
            Loc &loc = m_path.back();

            loc.pNode = _nodes.get(i);
            loc.pSetter = &setter;
            loc.bFirstInCollection = !loc.bPartOfCollection;
            loc.bPartOfCollection = true;

            if (!traverseNode(*_nodes.get(i)))
                return false;
            if (m_order == CHILDREN_FIRST) {
                callRoleHandler();
                if (!callWalkUp())
                    return !isStopped();
            }
        }
    }

    return true;
}

}

#endif // VISITOR_H_
