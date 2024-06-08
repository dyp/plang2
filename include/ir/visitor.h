
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
        virtual void set(const NodePtr& _pValue) {}
    };

template<typename _Node, typename _Member, void (_Node::*_Method)(const std::shared_ptr<_Member> &)>
class NodeSetterImpl : public NodeSetter {
public:
    NodeSetterImpl(const std::shared_ptr<_Node> &_pNode) : m_pNode(_pNode) {}

    void set(const NodePtr &_pValue) override {
        ((*m_pNode).*(_Method))(_pValue->as<_Member>());
    }

protected:
    const std::shared_ptr<_Node> m_pNode;
};

template<class _Node, class _Base>
class ItemSetterImpl : public NodeSetter {
public:
    ItemSetterImpl(Collection<_Node, _Base> &_pNodes, size_t _cIndex) : m_pNodes(_pNodes), m_cIndex(_cIndex) {}

    virtual void set(const NodePtr &_pValue) {
        m_pNodes.set(m_cIndex, _pValue->as<_Node>());
    }

protected:
    Collection<_Node, _Base> &m_pNodes;
    size_t m_cIndex;
};

class Visitor {
public:
    using RoleHandler = int (Visitor::*)(NodePtr &_node);
    using NodeWalkUp = bool (Visitor::*)(NodePtr &_node);

    enum { 
        PARENTS_FIRST,
        CHILDREN_FIRST,
    };

        Visitor(int _order = PARENTS_FIRST) : m_bStopped(false), m_order(_order) {}
        virtual ~Visitor() {}

    struct Loc {
        NodePtr pNode; // Never NULL.
        NodeType type;
        NodeRole role;
        RoleHandler roleHandler;
        RoleHandler roleHandlerPost;
        NodeSetter *pSetter;
        NodeWalkUp walkUp;
        bool bPartOfCollection, bLastInCollection;
        size_t cPosInCollection;

        Loc(const NodePtr &_pNode, NodeType _type, NodeRole _role, RoleHandler _roleHandler = nullptr,
                RoleHandler _roleHandlerPost = nullptr, NodeSetter *_pSetter = nullptr, NodeWalkUp _walkUp = nullptr) :
            pNode(_pNode), type(_type), role(_role), roleHandler(_roleHandler), roleHandlerPost(_roleHandlerPost),
            pSetter(_pSetter), walkUp(_walkUp), bPartOfCollection(false),
            bLastInCollection(false), cPosInCollection(0)
        {
        }
    };

    virtual bool traverseNode(const NodePtr &_node);
    template<class _Node, class _Base> bool traverseCollection(Collection<_Node, _Base> &_pNodes);
    virtual bool visitNode(const NodePtr &_node);
    bool walkUpFromNode(const NodePtr &_node);

#define NODE(_NODE, _PARENT)                            \
        bool walkUpFrom##_NODE(NodePtr &_pNode) {           \
            if (!walkUpFrom##_PARENT(_pNode))            \
                return false;                           \
            return visit##_NODE(_pNode->as<_NODE>());        \
        }                                               \
        virtual bool visit##_NODE(const std::shared_ptr<_NODE> &_pNode) {       \
            return true;                                \
        }                                               \
        virtual bool traverse##_NODE(const std::shared_ptr<_NODE> &_pNode);
#include "nodes.inl"
#undef NODE

#define ROLE(_ROLE) \
    virtual int handle##_ROLE(NodePtr &_node) { return 0; } \
    virtual int handle##_ROLE##Post(NodePtr &_node) { return 0; }
#include "roles.inl"
#undef ROLE

        bool isStopped() const { return m_bStopped; }
        void reset() { m_bStopped = false; }

    protected:
        struct Ctx {
            Visitor* pVisitor;

        Ctx(Visitor *_pVisitor, const NodePtr &_pNode, NodeType _type, NodeRole _role,
                RoleHandler _roleHandler = nullptr, RoleHandler _roleHandlerPost = nullptr,
                NodeSetter *_pSetter = nullptr, NodeWalkUp _walkUp = nullptr)
            : pVisitor(_pVisitor)
        {
            pVisitor->m_path.push_back(Loc(_pNode, _type, _role, _roleHandler, _roleHandlerPost,
                    _pSetter, _walkUp));
            }

            ~Ctx() {
                pVisitor->m_path.pop_back();
            }
        };

        friend struct Ctx;
        std::list<Loc> m_path;

    size_t getDepth() const { return m_path.size(); }
    NodePtr &getNode() { return m_path.back().pNode; }
    RoleHandler getRoleHandler(bool _bPreVisit) { return m_path.empty() ? nullptr : (_bPreVisit ? m_path.back().roleHandler : m_path.back().roleHandlerPost); }
    NodeWalkUp getWalkUp() { return m_path.empty() ? nullptr : m_path.back().walkUp; }
    NodeSetter *getNodeSetter() { return m_path.empty() ? nullptr : m_path.back().pSetter; }
    NodeRole getRole() { return m_path.empty() ? R_TopLevel : m_path.back().role; }
    Loc &getLoc() { return m_path.back(); }

    int callRoleHandler(bool _bPreVisit) {
        return getRoleHandler(_bPreVisit) == nullptr ? 0 : (this->*getRoleHandler(_bPreVisit))(getNode());
    }

    NodePtr getParent();

    bool callWalkUp() {
        return getWalkUp() == nullptr ? true : (this->*getWalkUp())(getNode());
    }

    void callSetter(const NodePtr &_pNewNode) {
        if (getNodeSetter() != nullptr)
            getNodeSetter()->set(_pNewNode);
    }

        void stop() { m_bStopped = true; }

        void setOrder(int _order) { m_order = _order; }
        int getOrder() const { return m_order; }

    private:
        bool m_bStopped;
        int m_order;

protected:
    virtual bool _traverseAnonymousPredicate(const std::shared_ptr<AnonymousPredicate> &_pDecl);
    virtual bool _traverseDeclarationGroup(const std::shared_ptr<DeclarationGroup> &_pDecl);
};

template<class _Node, class _Base>
bool Visitor::traverseCollection(Collection<_Node, _Base> &_pNodes) {

    for (size_t i = 0; i < _pNodes.size(); ++i) {
        if (_pNodes.get(i)) {
            ItemSetterImpl<_Node, _Base> setter(_pNodes, i); 
            Loc &loc = m_path.back();

            loc = Loc(_pNodes.get(i), loc.type, loc.role, loc.roleHandler, loc.roleHandlerPost,
                    &setter, loc.walkUp);
            loc.cPosInCollection = i;
            loc.bPartOfCollection = true;
            loc.bLastInCollection = i + 1 == _pNodes.size();

            if (!traverseNode(_pNodes.get(i)))
                return false;
        }
    }

        return true;
    }

#define VISITOR_ENTER(_TYPE, _PARAM)                                \
    do {                                                            \
        if (isStopped())                                            \
            return false;                                           \
        auto nodePtr = (_PARAM)->as<ir::Node>();     \
        if (m_path.empty())                                         \
            m_path.push_back(Loc(nodePtr, ir::N_##_TYPE, ir::R_TopLevel));   \
        else                                                        \
            getLoc().type = ir::N_##_TYPE;                          \
        if (getOrder() == PARENTS_FIRST) {                          \
            callRoleHandler(true);                                  \
            if (!walkUpFrom##_TYPE(nodePtr))                         \
                return !isStopped();                                \
            callRoleHandler(false);                                 \
        } else                                                      \
            getLoc().walkUp = &Visitor::walkUpFrom##_TYPE;          \
    } while (0)

#define VISITOR_EXIT_INLINE()               \
    do {                                    \
        if (getOrder() == CHILDREN_FIRST) { \
            callRoleHandler(true);          \
            if (!callWalkUp())              \
                return !isStopped();        \
            callRoleHandler(false);         \
        }                                   \
    } while (0)

#define VISITOR_EXIT()                      \
    do {                                    \
        VISITOR_EXIT_INLINE();              \
        return true;                        \
    } while (0)

#define VISITOR_TRAVERSE(_TYPE, _ROLE, _PARAM, _PARENT, _PTYPE, _SETTER)            \
    do {                                                                            \
        if (isStopped())                                                            \
            return false;                                                           \
        if (_PARAM) {                                                               \
            const auto nodePtr = (_PARAM)->as<ir::Node>();                      \
            ir::NodeSetterImpl< ir::_PTYPE, ir::_TYPE, &ir::_PTYPE::_SETTER > setter(_PARENT);  \
            Ctx ctx(this, nodePtr, ir::N_##_TYPE, ir::R_##_ROLE, &Visitor::handle##_ROLE,     \
                &Visitor::handle##_ROLE##Post, &setter);                            \
            const auto typePtr = (_PARAM)->as<ir::_TYPE>();            \
            if (!traverse##_TYPE(typePtr))                                        \
                return false;                                                      \
        }                                                                           \
    } while (0)

#define VISITOR_TRAVERSE_NS(_TYPE, _ROLE, _PARAM)                                   \
    do {                                                                            \
        if (isStopped())                                                            \
            return false;                                                           \
        if (_PARAM) {                                                               \
            ir::NodeSetter setter;                                                  \
            Ctx ctx(this, _PARAM, ir::N_##_TYPE, ir::R_##_ROLE, &Visitor::handle##_ROLE, \
                &Visitor::handle##_ROLE##Post, &setter);                            \
            if (!traverse##_TYPE(_PARAM))                                        \
                return false;                                                       \
        }                                                                           \
    } while (0)

#define VISITOR_TRAVERSE_INLINE(_TYPE, _ROLE, _PARAM, _PARENT, _PTYPE, _SETTER)                     \
        assert(_PARAM);                                                                             \
        ir::NodeSetterImpl< ir::_PTYPE, ir::_TYPE, &ir::_PTYPE::_SETTER > __setter##LINE(_PARENT);  \
        Ctx __ctx##LINE(this, _PARAM, ir::N_##_TYPE, ir::R_##_ROLE, &Visitor::handle##_ROLE,     \
            &Visitor::handle##_ROLE##Post, &__setter##LINE)

#define VISITOR_TRAVERSE_INLINE_NS(_TYPE, _ROLE, _PARAM)                                            \
        assert(_PARAM);                                                                             \
        ir::NodeSetter __setter##LINE;                                                              \
        Ctx __ctx##LINE(this, _PARAM, ir::N_##_TYPE, ir::R_##_ROLE, &Visitor::handle##_ROLE,     \
            &Visitor::handle##_ROLE##Post, &__setter##LINE)

#define VISITOR_TRAVERSE_COL(_TYPE, _ROLE, _PARAM)                                  \
    do {                                                                            \
        if (isStopped())                                                            \
            return false;                                                           \
        if (!(_PARAM).empty()) {                                                    \
            auto nodePtr = std::make_shared<ir::Node>(_PARAM);                         \
            Ctx ctx(this, nodePtr, ir::N_##_TYPE, ir::R_##_ROLE, &Visitor::handle##_ROLE,    \
                &Visitor::handle##_ROLE##Post, nullptr);                               \
            if (!traverseCollection(_PARAM))                                        \
                return false;                                                     \
        }                                                                           \
    } while (0)

#define VISITOR_TRAVERSE_ITEM_NS(_TYPE, _ROLE, _PARAM, _INDEX)                      \
    do {                                                                            \
        if (isStopped())                                                            \
            return false;                                                           \
        const size_t cIndex = (_INDEX);                                             \
        if ((_INDEX) < (_PARAM).size() && (_PARAM).get(cIndex)) {                   \
            const auto pNode = (_PARAM).get(cIndex);                               \
            Ctx ctx(this, pNode, ir::N_##_TYPE, ir::R_##_ROLE, &Visitor::handle##_ROLE,    \
                &Visitor::handle##_ROLE##Post, nullptr);                               \
            getLoc().cPosInCollection = cIndex;                                     \
            getLoc().bPartOfCollection = true;                                      \
            getLoc().bLastInCollection = cIndex + 1 == (_PARAM).size();             \
            if (!traverse##_TYPE(pNode))                                           \
                return false;                                                       \
        }                                                                           \
    } while (0)

}

#endif // VISITOR_H_
