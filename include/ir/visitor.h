
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
    N_Node,
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
    virtual ~NodeSetter() {}
    virtual void set(const NodePtr &_pValue) {}
};

template<typename _Node, typename _Member, void (_Node::*_Method)(const Auto<_Member> &)>
class NodeSetterImpl : public NodeSetter {
public:
    NodeSetterImpl(_Node &_node) : m_node(_node) {}

    virtual void set(const NodePtr &_pValue) {
        (m_node.*(_Method))(_pValue.as<_Member>());
    }

protected:
    _Node &m_node;
};

template<class _Node, class _Base>
class ItemSetterImpl : public NodeSetter {
public:
    ItemSetterImpl(Collection<_Node, _Base> &_nodes, size_t _cIndex) : m_nodes(_nodes), m_cIndex(_cIndex) {}

    virtual void set(const NodePtr &_pValue) {
        m_nodes.set(m_cIndex, _pValue.as<_Node>());
    }

protected:
    Collection<_Node, _Base> &m_nodes;
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
    virtual ~Visitor() {}

    struct Loc {
        Node *pNode; // Never NULL.
        NodeType type;
        NodeRole role;
        RoleHandler roleHandler;
        RoleHandler roleHandlerPost;
        NodeSetter *pSetter;
        NodeWalkUp walkUp;
        bool bPartOfCollection;
        size_t cPosInCollection;

        Loc(Node &_node, NodeType _type, NodeRole _role, RoleHandler _roleHandler = NULL,
                RoleHandler _roleHandlerPost = NULL, NodeSetter *_pSetter = NULL, NodeWalkUp _walkUp = NULL) :
            pNode(&_node), type(_type), role(_role), roleHandler(_roleHandler), roleHandlerPost(_roleHandlerPost),
            pSetter(_pSetter), walkUp(_walkUp), bPartOfCollection(false), cPosInCollection(0)
        {
        }
    };

    virtual bool traverseNode(Node &_node);
    template<class _Node, class _Base> bool traverseCollection(Collection<_Node, _Base> &_nodes);
    virtual bool visitNode(Node &_node);
    bool walkUpFromNode(Node &_node);

#define NODE(_NODE, _PARENT)                            \
        bool walkUpFrom##_NODE(Node &_node) {           \
            if (!walkUpFrom##_PARENT(_node))            \
                return false;                           \
            return visit##_NODE((_NODE &)_node);        \
        }                                               \
        virtual bool visit##_NODE(_NODE &_node) {       \
            return true;                                \
        }                                               \
        virtual bool traverse##_NODE(_NODE &_node);
#include "nodes.inl"
#undef NODE

#define ROLE(_ROLE) \
    virtual int handle##_ROLE(Node &_node) { return 0; } \
    virtual int handle##_ROLE##Post(Node &_node) { return 0; }
#include "roles.inl"
#undef ROLE

    bool isStopped() const { return m_bStopped; }
    void reset() { m_bStopped = false; }

protected:
    struct Ctx {
        Visitor *pVisitor;

        Ctx(Visitor *_pVisitor, Node &_node, NodeType _type, NodeRole _role,
                RoleHandler _roleHandler = NULL, RoleHandler _roleHandlerPost = NULL,
                NodeSetter *_pSetter = NULL, NodeWalkUp _walkUp = NULL)
            : pVisitor(_pVisitor)
        {
            pVisitor->m_path.push_back(Loc(_node, _type, _role, _roleHandler, _roleHandlerPost,
                    _pSetter, _walkUp));
        }

        ~Ctx() {
            pVisitor->m_path.pop_back();
        }
    };

    friend struct Ctx;
    std::list<Loc> m_path;

    size_t getDepth() const { return m_path.size(); }
    Node &getNode() { return *m_path.back().pNode; }
    RoleHandler getRoleHandler(bool _bPreVisit) { return m_path.empty() ? NULL : (_bPreVisit ? m_path.back().roleHandler : m_path.back().roleHandlerPost); }
    NodeWalkUp getWalkUp() { return m_path.empty() ? NULL : m_path.back().walkUp; }
    NodeSetter *getNodeSetter() { return m_path.empty() ? NULL : m_path.back().pSetter; }
    NodeRole getRole() { return m_path.empty() ? R_TopLevel : m_path.back().role; }
    Loc &getLoc() { return m_path.back(); }

    int callRoleHandler(bool _bPreVisit) {
        return getRoleHandler(_bPreVisit) == NULL ? 0 : (this->*getRoleHandler(_bPreVisit))(getNode());
    }

    Node* getParent();

    bool callWalkUp() {
        return getWalkUp() == NULL ? true : (this->*getWalkUp())(getNode());
    }

    void callSetter(const NodePtr &_pNewNode) {
        if (getNodeSetter() != NULL)
            getNodeSetter()->set(_pNewNode);
    }

    void stop() { m_bStopped = true; }

    void setOrder(int _order) { m_order = _order; }
    int getOrder() const { return m_order; }

private:
    bool m_bStopped;
    int m_order;

protected:
    virtual bool _traverseAnonymousPredicate(AnonymousPredicate &_decl);
    virtual bool _traverseDeclarationGroup(DeclarationGroup &_decl);
};

template<class _Node, class _Base>
bool Visitor::traverseCollection(Collection<_Node, _Base> &_nodes) {

    for (size_t i = 0; i < _nodes.size(); ++i) {
        if (_nodes.get(i)) {
            ItemSetterImpl<_Node, _Base> setter(_nodes, i);
            Loc &loc = m_path.back();

            loc = Loc(*_nodes.get(i), loc.type, loc.role, loc.roleHandler, loc.roleHandlerPost,
                    &setter, loc.walkUp);
            loc.cPosInCollection = i;
            loc.bPartOfCollection = true;

            if (!traverseNode(*_nodes.get(i)))
                return false;
        }
    }

    return true;
}

#define VISITOR_ENTER(_TYPE, _PARAM)                                \
    do {                                                            \
        if (isStopped())                                            \
            return false;                                           \
        if (m_path.empty())                                         \
            m_path.push_back(Loc(_PARAM, ir::N_##_TYPE, ir::R_TopLevel));   \
        else                                                        \
            getLoc().type = ir::N_##_TYPE;                              \
        if (getOrder() == PARENTS_FIRST) {                          \
            callRoleHandler(true);                                  \
            if (!walkUpFrom##_TYPE(_PARAM))                         \
                return !isStopped();                                \
            callRoleHandler(false);                                 \
        } else                                                      \
            getLoc().walkUp = &Visitor::walkUpFrom##_TYPE;          \
    } while (0)

#define VISITOR_EXIT()                      \
    do {                                    \
        if (getOrder() == CHILDREN_FIRST) { \
            callRoleHandler(true);          \
            if (!callWalkUp())              \
                return !isStopped();        \
            callRoleHandler(false);         \
        }                                   \
        return true;                        \
    } while (0)

#define VISITOR_TRAVERSE(_TYPE, _ROLE, _PARAM, _PARENT, _PTYPE, _SETTER)            \
    do {                                                                            \
        if (isStopped())                                                            \
            return false;                                                           \
        if (_PARAM) {                                                    \
            ir::NodeSetterImpl< ir::_PTYPE, ir::_TYPE, &ir::_PTYPE::_SETTER > setter(_PARENT);      \
            Ctx ctx(this, *(_PARAM), ir::N_##_TYPE, ir::R_##_ROLE, &Visitor::handle##_ROLE, \
                &Visitor::handle##_ROLE##Post, &setter);                            \
            if (!traverse##_TYPE(*(_PARAM)))                                        \
                return false;                                                       \
        }                                                                           \
    } while (0)

#define VISITOR_TRAVERSE_COL(_TYPE, _ROLE, _PARAM)                                  \
    do {                                                                            \
        if (isStopped())                                                            \
            return false;                                                           \
        if (!(_PARAM).empty()) {                                                    \
            Ctx ctx(this, _PARAM, ir::N_##_TYPE, ir::R_##_ROLE, &Visitor::handle##_ROLE,    \
                &Visitor::handle##_ROLE##Post, NULL);                               \
            if (!traverseCollection(_PARAM))                                        \
                return false;                                                       \
        }                                                                           \
    } while (0)

}

#endif // VISITOR_H_
