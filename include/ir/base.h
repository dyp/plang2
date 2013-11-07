/// \file base.h
/// Common classes that are used in IR structures.

/// \mainpage Internal representation reference
///
/// \section sec_root_classes Root classes
/// \subsection sec_Node Node : Base class for all internal representation objects
/// \copydetails ir::Node
///
/// \subsection sec_Pragma Pragma : Compiler directive
/// \copydetails ir::Pragma
///
/// \subsection sec_Number Number : Representation of numeric literals
/// \copydetails Number
///
/// \section sec_other_classes Other classes
/// Please follow Classes or Files link to see complete documentation.

#ifndef BASE_H_
#define BASE_H_

#include <vector>
#include <algorithm>
#include <string>
#include <map>
#include <functional>

#include "autoptr.h"
#include "lexer.h"

namespace ir {

// Used s/Auto<([^_][\w:]*)>(?:\s+(>))?/\1Ptr\2/g to replace "Auto<Foo>" with "FooPtr".
#define NODE(_Node, ...)    \
    class _Node;            \
    typedef Auto<_Node> _Node##Ptr;
#include "nodes.inl"
NODE(Node)
NODE(Branch)
NODE(CallBranch)
NODE(NamedValues)
#undef NODE

class Node;

template<class _Node, class _Base = Node>
class Collection;

typedef Collection<Node> Nodes;
typedef Auto<Nodes> NodesPtr;

/// Base class for all internal representation objects.
///
/// Purpose of Node class is to provide management of deallocation of IR objects
/// and to hold description of object context.
///
/// IR object can have it's parent assigned by some set*() call of another node
/// with _bReparent parameter set to true or explicitly using setParent()
/// method (if the node is already referenced by this parent). If an object has
/// a parent assigned then the parent is responsible for the object's
/// deallocation and there is no need to delete this object manually.
///
/// If you want to preserve an object after it's parent is destroyed you should
/// call setParent(NULL) on this object. That way manual deallocation management
/// of this object is assumed until it is deleted or adopted by a new parent.
///
/// Node class also contains a (possibly NULL) pointer to list of pragmas
/// relevant to the node's location in source code.
///
class Node : public Counted {
public:
    /// Node kind.
    enum {
        NONE,
        COLLECTION,
        TYPE,
        NAMED_VALUE,
        STATEMENT,
        EXPRESSION,
        MODULE,
        CLASS,
        LABEL,
        MESSAGE,
        PROCESS,
        UNION_CONSTRUCTOR_DECLARATION,
        ELEMENT_DEFINITION,
        STRUCT_FIELD_DEFINITION,
        ARRAY_PART_DEFINITION,
        SWITCH_CASE,
        MESSAGE_HANDLER,
    };

    /// Default constructor.
    Node() {}

    /// Override Counted's deleted copy constructor to allow copying of nodes
    /// preserving Counted's internal fields.
    Node(const Node &_other) {}

    /// Destructor.
    virtual ~Node() {}

    virtual int getNodeKind() const { return NONE; }

    virtual NodesPtr getChildren() const;

    void setLoc(lexer::Token *_pLoc) { m_pLoc = _pLoc; }
    const lexer::Token *getLoc() const { return m_pLoc; }

    bool operator<(const Node& _other) const { return less(_other); }
    bool operator>(const Node& _other) const { return _other < *this; }
    bool operator==(const Node& _other) const { return equals(_other); }
    bool operator!=(const Node& _other) const { return !equals(_other); }

    virtual bool less(const Node& _other) const { return getNodeKind() < _other.getNodeKind(); }
    virtual bool equals(const Node& _other) const { return getNodeKind() == _other.getNodeKind(); }

    // \returns Deep copy of the node.
    virtual NodePtr clone(Cloner &_cloner) const { return NULL; }

protected:
    static bool _less(const NodePtr& _pLeft, const NodePtr& _pRight);
    static bool _equals(const NodePtr& _pLeft, const NodePtr& _pRight);

private:
    lexer::Token *m_pLoc = nullptr;
    // TODO: assignment of m_pLoc for all Nodes.
};

/// Collection of homogeneous nodes.
///
/// Provides functionality to add, replace and remove objects of class _Node.
///
/// \param _Node Class of elements.
/// \param _Base Base class of the result (default is Node).
///
/// The elements are stored in separate list than children and are not required
/// to be the children of the collection.
template<class _Node, class _Base>
class Collection : public _Base {
public:
    /// Default constructor.
    Collection() {}

    /// Copy constructor
    Collection(const Collection &_other) : m_nodes(_other.m_nodes) {}

    virtual ~Collection() {}

    virtual int getNodeKind() const {
        if (_Base::getNodeKind() != Node::NONE)
            return _Base::getNodeKind();
        return Node::COLLECTION;
    }

    /// Get element count.
    /// \return Number of elements.
    size_t size() const { return m_nodes.size(); }

    /// Check if the collection is empty.
    /// \return True if the collection is empty, false otherwise.
    bool empty() const { return m_nodes.empty(); }

    /// Get element by index.
    /// \param _c Index of element (zero-based).
    /// \return Pointer to element or NULL if index is out of bounds.
    Auto<_Node> get(size_t _c) const {
        return _c < m_nodes.size() ? m_nodes[_c] : Auto<_Node>();
    }

    /// Add element to the collection.
    /// \param _pNode Pointer to node to add.
    Auto<_Node> add(const Auto<_Node> &_pNode) {
        m_nodes.push_back(_pNode);
        return m_nodes.back();
    }

    /// Add element to the front of collection.
    /// \param _pNode Pointer to node to add.
    Auto<_Node> prepend(const Auto<_Node> &_pNode) {
        m_nodes.insert(m_nodes.begin(), _pNode);
        return m_nodes.front();
    }

    template <class InputIterator>
    void prepend(InputIterator _first, InputIterator _last) {
        insert(m_nodes.begin(), _first, _last);
    }

    /// Append elements from another collection.
    /// \param _other Other collection.
    template<typename _OtherNode, typename _OtherBase>
    void append(const Collection<_OtherNode, _OtherBase> &_other) {
        m_nodes.reserve(m_nodes.size() + _other.size());
        for (size_t i = 0; i < _other.size(); ++i)
            add(_other.get(i));
    }

    /// Append elements from another collection.
    /// \param _other Other collection.
    template<typename _OtherNode, typename _OtherBase>
    void appendClones(const Collection<_OtherNode, _OtherBase> &_other, Cloner &_cloner) {
        m_nodes.reserve(m_nodes.size() + _other.size());
        for (size_t i = 0; i < _other.size(); ++i)
            add(_cloner.get(_other.get(i)));
    }

    void clear() {
        m_nodes.clear();
    }

    /// Assign elements from another collection.
    /// \param _other Other collection.
    template<typename _OtherNode, typename _OtherBase>
    void assign(const Collection<_OtherNode, _OtherBase> &_other) {
        clear();
        append(_other);
    }

    /// Replace element by index.
    /// \param _c Index of element (zero-based).
    /// \param _pNode Pointer to new element.
    void set(size_t _c, const Auto<_Node> &_pNode) {
        if (_c < m_nodes.size())
            m_nodes[_c] = _pNode;
    }

    /// Insert elements from another collection before element with number _c.
    /// \param _c Index of element (zero-based).
    /// \param _other Other collection.
    template<typename _OtherNode, typename _OtherBase>
    void insert(size_t _c, const Collection<_OtherNode, _OtherBase> &_other) {
        m_nodes.reserve(m_nodes.size() + _other.size());
        if (_c <= m_nodes.size())
            m_nodes.insert(m_nodes.begin() + _c, _other.m_nodes.begin(), _other.m_nodes.end());
    }

    void insert(size_t _c, const Auto<_Node> &_pNode) {
        if (_c <= m_nodes.size())
            m_nodes.insert(m_nodes.begin() + _c, _pNode);
    }

    template <class InputIterator>
    void insert(typename std::vector<Auto<_Node> >::iterator _position, InputIterator _first, InputIterator _last) {
        m_nodes.insert(_position, _first, _last);
    }

    /// Remove element.
    /// \param _pNode Pointer to element to remove.
    /// \return True if node was successfully removed, false if not found.
    bool remove(const Auto<_Node> &_pNode) {
        auto iNode = std::find(m_nodes.begin(), m_nodes.end(), _pNode);
        if (iNode == m_nodes.end())
            return false;
        m_nodes.erase(iNode);
        return true;
    }

    bool remove(size_t _index) {
        if (_index >= size())
            return false;
        m_nodes.erase(m_nodes.begin() + _index);
        return true;
    }

    template<class _Predicate>
    size_t findIdx(const _Node &_node, _Predicate _pred) const {
        for (size_t i = 0; i < size(); ++i)
            if (get(i) && _pred(_node, *get(i)))
                return i;
        return (size_t)-1;
    }

    size_t findIdx(const _Node &_node) const {
        return findIdx(_node, std::equal_to<_Node>());
    }

    size_t findByNameIdx(const std::wstring &_name) const {
        for (size_t i = 0; i < size(); ++i)
            if (get(i)->getName() == _name)
                return i;
        return (size_t)-1;
    }

    virtual bool less(const Node& _other) const {
        if (!_Base::equals(_other))
            return _Base::less(_other);
        const Collection& other = (const Collection&)_other;
        if (size() != other.size())
            return size() < other.size();
        for (size_t i=0; i<size(); ++i)
            if (!this->_equals(get(i), other.get(i)))
                return this->_less(get(i), other.get(i));
        return false;
    }

    virtual bool equals(const Node& _other) const {
        if (!_Base::equals(_other))
            return false;
        const Collection& other = (const Collection&)_other;
        if (size() != other.size())
            return false;
        for (size_t i=0; i<size(); ++i)
            if (!this->_equals(get(i), other.get(i)))
                return false;
        return true;
    }

    typename std::vector<Auto<_Node> >::iterator begin() {
        return m_nodes.begin();
    }

    typename std::vector<Auto<_Node> >::iterator end() {
        return m_nodes.end();
    }

    virtual NodePtr clone(Cloner &_cloner) const {
        Auto<Collection<_Node, _Base> > pCopy = NEW_CLONE(this, _cloner, Collection());
        pCopy->appendClones(*this, _cloner);
        return pCopy;
    }

private:
    std::vector<Auto<_Node> > m_nodes;

    template<class, class> friend class Collection;
};

class Type;

typedef std::map<TypePtr, TypePtr> TypeSubst;
typedef std::pair<TypePtr, bool> SideType;

/// Virtual ancestor of all types.
class Type : public Node {
public:
    /// Type kind.
    enum {
        /// Fresh type (for typechecking purposes).
        FRESH = 1,
        /// Bottom type (subtype of any type).
        BOTTOM,
        /// Top type (supertype of any type).
        TOP,
        /// Unit type (\c nil, (), [], etc.)
        UNIT,
        /// \c nat type. Use getBits() and setBits() to access bitness.
        NAT,
        /// \c int type. Use getBits() and setBits() to access bitness.
        INT,
        /// \c real type. Use getBits() and setBits() to access bitness.
        REAL,
        /// \c bool type.
        BOOL,
        /// \c char type.
        CHAR,
        /// \c string type.
        STRING,
        /// \c var type. Actual type should be determined at typechecking phase.
        GENERIC,
        /// \c type type. Used as predicate or type parameter only. Can be cast to TypeType.
        TYPE,
        /// \c enum type. Can be cast to EnumType.
        ENUM,
        /// Struct type. Can be cast to StructType.
        STRUCT,
        /// Union type. Can be cast to UnionType.
        UNION,
        /// Optional type. Can be cast to OptionalType. (deprecated)
        OPTIONAL,
        /// Sequence type. Can be cast to SeqType. (deprecated)
        SEQ,
        /// Array type. Can be cast to ArrayType.
        ARRAY,
        /// Set type. Can be cast to SetType.
        SET,
        /// Map type. Can be cast to MapType.
        MAP,
        /// List type. Can be cast to ListType.
        LIST,
        /// Subtype. Can be cast to Subtype.
        SUBTYPE,
        /// Range. Can be cast to Range.
        RANGE,
        /// Predicate type. Can be cast to PredicateType.
        PREDICATE,
        /// Parameterized type. Can be cast to ParameterizedType.
        PARAMETERIZED,
        /// User-defined type referenced by name. Can be cast to NamedReferenceType.
        NAMED_REFERENCE,
        /// Tuple type (for typechecking purposes). Can be cast to tc::TupleType.
        TUPLE,
    };

    /// Initialize with kind.
    /// \param _kind One of built-in types (#Unit, #Int, #Nat, #Real, #Bool, #Char, #String, #Type or #Generic).
    Type(int _kind, int _bits = 0) : m_kind(_kind), m_nBits(_bits) {}

    Type(const Type &_other) : m_kind(_other.m_kind), m_nBits(_other.m_nBits) {
        assert(m_kind > 0);
    }

    /// Destructor.
    virtual ~Type() {}

    virtual int getNodeKind() const { return Node::TYPE; }

    /// Get type kind.
    /// \return Specific kind.
    virtual int getKind() const { return m_kind; }

    /// Get bitness (for numeric types only).
    /// \return #Native, #Generic or number of bits.
    int getBits() const { return m_nBits; }

    /// Set bitness (for numeric types only).
    /// \param _nBits #Native, #Generic or number of bits.
    void setBits(int _nBits) { m_nBits = _nBits; }

    enum {
        ORD_UNKNOWN = 0x01,
        ORD_NONE    = 0x02,
        ORD_SUB     = 0x04,
        ORD_SUPER   = 0x08,
        ORD_EQUALS  = 0x10,
    };

    static int inverse(int _nOrder) {
        return _nOrder == ORD_SUB ? ORD_SUPER : (_nOrder == ORD_SUPER ? ORD_SUB : _nOrder);
    }

    // Subtyping.
    virtual int compare(const Type &_other) const;
    bool compare(const Type &_other, int _order) const;
    virtual TypePtr getJoin(Type &_other); // Supremum.
    virtual TypePtr getMeet(Type &_other); // Infinum.

    enum {
        MT_NONE     = 0x01,
        MT_CONST    = 0x02,
        MT_MONOTONE = 0x04,
        MT_ANTITONE = 0x08,
    };

    virtual int getMonotonicity(const Type &_var) const;
    bool isMonotone(const Type &_var, bool _bStrict = true) const;
    bool isAntitone(const Type &_var, bool _bStrict = true) const;

    // For comparison/sorting only, no subtyping relation is implied.
    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;
    virtual bool less(const Type &_other) const;

    // Perform deep copy.
    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, Type(m_kind, m_nBits));
    }

    virtual bool hasFresh() const;
    virtual bool rewrite(const TypePtr &_pOld, const TypePtr &_pNew, bool _bRewriteFlags = true) { return false; }
    virtual bool rewriteFlags(int _flags) { return false; }

    // Check if _pType is structurally contained (strict relation).
    virtual bool contains(const TypePtr &_pType) const { return false; }

    virtual bool hasParameters() const { return m_kind == INT || m_kind == NAT || m_kind == REAL; }

protected:
    /// Default constructor.
    /// Only descendant classes should use this.
    Type() : m_kind (0), m_nBits(0) {}

    SideType _getJoin(Type &_other); // Supremum.
    SideType _getMeet(Type &_other); // Infinum.

private:
    int m_kind;
    int m_nBits;
};

/// Simple typed and named value.
///
/// NamedValue represents local and global variables, predicate parameters,
/// type parameters, iterators, etc. Particular kind of the value can be
/// determined by getKind.
class NamedValue : public Node {
public:
    /// Kind of the value.
    enum {
        /// Simple combination of type and identifier (type parameters,
        /// iterators, etc.)
        GENERIC,
        /// Predicate parameter. The object can be cast to Param if needed.
        ENUM_VALUE,
        /// Predicate parameter. The object can be cast to Param if needed.
        PREDICATE_PARAMETER,
        /// Local variable.
        LOCAL,
        /// Global variable.
        GLOBAL
     };

    /// Default constructor.
    NamedValue() : m_pType(NULL), m_strName(L"") {}

    /// Constructor for initializing using name.
    /// \param _strName Identifier.
    /// \param _pType Type associated with value.
    NamedValue(const std::wstring &_strName, const TypePtr &_pType = NULL)
        : m_pType(_pType), m_strName(_strName) {}

    virtual int getNodeKind() const { return Node::NAMED_VALUE; }

    /// Get value kind.
    /// \returns Value kind (#Generic, #PredicateParameter, #Local or #Global).
    virtual int getKind() const { return GENERIC; }

    /// Get name of the value.
    /// \returns Identifier.
    const std::wstring &getName() const { return m_strName; }

    /// Set name of the value.
    /// \param _strName Identifier.
    void setName(const std::wstring &_strName) { m_strName = _strName; }

    /// Get type of the value.
    /// \returns Type associated with value.
    TypePtr getType() const { return m_pType; }

    /// Set type of the value.
    /// \param _pType Type associated with value.
    void setType(const TypePtr &_pType) { m_pType = _pType; }

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;
    bool operator== (const NamedValue& _other) const {
        return equals(_other);
    }

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, NamedValue(m_strName, _cloner.get(m_pType.ptr())));
    }

private:
    TypePtr m_pType;
    std::wstring m_strName;
};

/// Predicate parameter.
///
/// Offers a possibility to specify linked parameter. Consider:
/// \code foo (int x : int x') \endcode
/// In the above declaration parameters \c x and \c x' are linked.
class Param : public NamedValue {
public:
    /// Default constructor.
    Param() : m_pLinkedParam(NULL), m_bOutput(false) {}

    /// Constructor for initializing using name.
    /// \param _strName Identifier.
    /// \param _pType Type associated with value.
    Param(const std::wstring &_strName, const TypePtr &_pType = NULL, bool _bOutput = false)
        : NamedValue(_strName, _pType), m_pLinkedParam(NULL), m_bOutput(_bOutput) {}

    /// Get value kind.
    /// \returns #PredicateParameter.
    virtual int getKind() const { return PREDICATE_PARAMETER; }

    /// Get pointer to (constant) linked parameter.
    /// \returns Linked parameter.
    const ParamPtr &getLinkedParam() const { return m_pLinkedParam; }

    /// Set linked parameter pointer.
    /// \param _pParam Linked parameter.
    void setLinkedParam(const ParamPtr &_pParam) { m_pLinkedParam = _pParam; }

    /// Check if a linked parameter is needed.
    /// \returns True if a linked parameter is needed, false otherwise.
    bool isOutput() const { return m_bOutput; }

    /// Set to true if a linked parameter is needed.
    /// \param _bValue True if a linked parameter is needed, false otherwise.
    void setOutput(bool _bValue) { m_bOutput = _bValue; }

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, Param(getName(), _cloner.get(getType().ptr()), m_bOutput));
    }

private:
    ParamPtr m_pLinkedParam;
    bool m_bOutput;
};

// We need to define some collections as classes (vs. typedef'ed templates) because some uses
// require that e.g. NamedValues needs to be a class name, not a typedef name.
#define COLLECTION_CLASS(_Name, _Item)                                              \
    class _Name : public Collection<_Item> {                                        \
    public:                                                                         \
        _Name() {}                                                                  \
        _Name(Collection<_Item> &_collection) : Collection<_Item>(_collection) {}   \
        virtual NodePtr clone(Cloner &_cloner) const {                              \
            Auto<_Name> pCopy = NEW_CLONE(this, _cloner, _Name());                  \
            pCopy->appendClones(*this, _cloner);                                    \
            return pCopy;                                                           \
        }                                                                           \
    }

COLLECTION_CLASS(Params, Param);

COLLECTION_CLASS(NamedValues, NamedValue);

/// Named label used to specify return branch.
/// \code foo (int x : #ok : #error) \endcode
/// In the above declaration parameters \c \#ok and \c \#error are labels.
class Label : public Node {
public:
    /// Default constructor.
    Label() : m_strName(L"") {}

    /// Constructor for initializing using name.
    /// \param _strName Label name.
    Label(const std::wstring &_strName) : m_strName(_strName) {}

    virtual int getNodeKind() const { return Node::LABEL; }

    /// Get name of the value.
    /// \returns Label name.
    const std::wstring &getName() const { return m_strName; }

    /// Set name of the value.
    /// \param _strName Label name.
    void setName(const std::wstring &_strName) { m_strName = _strName; }

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;

    virtual NodePtr clone(Cloner &_cloner) const { return NEW_CLONE(this, _cloner, Label(m_strName)); }

private:
    std::wstring m_strName;
};

/// Virtual ancestor of all statements.
class Statement : public Node {
public:
    /// Statement kind.
    enum {
        /// Statement that does nothing (appears if a label is placed at the end of block).
        NOP,
        /// A block of statements. Can be cast to Block.
        BLOCK,
        /// Collection of statements that can be executed simultaneously. Can be cast to ParallelBlock.
        PARALLEL_BLOCK,
        /// Jump statement. Can be cast to Jump.
        JUMP,
        /// Assignment. Can be cast to Assignment.
        ASSIGNMENT,
        /// Multiassignment. Can be cast to Multiassignment.
        MULTIASSIGNMENT,
        /// Predicate call. Can be cast to Call.
        CALL,
        /// Switch statement. Can be cast to Switch.
        SWITCH,
        /// Conditional statement. Can be cast to If.
        IF,
        /// Imperative for-loop. Can be cast to For.
        FOR,
        /// Imperative while-loop. Can be cast to While.
        WHILE,
        /// Imperative break statement. Can be cast to Break.
        BREAK,
        /// Synchronized statement header. Can be cast to With.
        WITH,
        /// Receive message statement. Can be cast to Receive.
        RECEIVE,
        /// Send message statement. Can be cast to Send.
        SEND,
        /// Declaration of a user-defined type. Can be cast to TypeDeclaration.
        TYPE_DECLARATION,
        /// Declaration of a variable. Can be cast to VariableDeclaration.
        VARIABLE_DECLARATION,
        /// Declaration of a formula. Can be cast to FormulaDeclaration.
        FORMULA_DECLARATION,
        /// Declaration of a lemma. Can be cast to LemmaDeclaration.
        LEMMA_DECLARATION,
        /// Declaration of a (nested) predicate. Can be cast to Predicate.
        PREDICATE_DECLARATION,
        /// Block of variable declarations. Can be cast to VariableDeclarationGroup.
        VARIABLE_DECLARATION_GROUP,
        /// Anonymous predicate inside Lambda. Can be cast to AnonymousPredicate.
        ANONYMOUS_PREDICATE,
    };

    /// Default constructor.
    Statement(const LabelPtr &_pLabel = NULL) : m_pLabel(_pLabel) {}

    virtual int getNodeKind() const { return Node::STATEMENT; }

    /// Get statement kind.
    /// \returns Statement kind.
    virtual int getKind() const { return NOP; }

    /// Get optional label that can be associated with the statement.
    /// \return Label pointer (possibly NULL).
    const LabelPtr &getLabel() const { return m_pLabel; }

    /// Associated a label with the statement.
    /// \param _pLabel Label pointer (possibly NULL).
    void setLabel(const LabelPtr &_pLabel) { m_pLabel = _pLabel; }

    // Check if the statement ends like a block (i.e. separating semicolon is not needed).
    // \return True if the statement ends like a block, false otherwise.
    virtual bool isBlockLike() const { return false; }

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, Statement(_cloner.get(getLabel())));
    }

private:
    LabelPtr m_pLabel;
};

/// Block of statements (statements surrounded by curly braces in source code).
/// Use Collection methods to access statements inside of block.
/// \extends Statement
class Block : public Collection<Statement, Statement> {
public:
    /// Default constructor.
    Block(const LabelPtr &_pLabel = NULL) { setLabel(_pLabel); }

    /// Get statement kind.
    /// \returns #Block.
    virtual int getKind() const { return BLOCK; }

    // Check if the statement ends like a block (i.e. separating semicolon is not needed).
    // \return True.
    virtual bool isBlockLike() const { return true; }

    virtual NodePtr clone(Cloner &_cloner) const {
        Auto<Block> pCopy = NEW_CLONE(this, _cloner, Block(_cloner.get(this->getLabel())));
        pCopy->appendClones(*this, _cloner);
        return pCopy;
    }
};

/// Collection of statements that can be executed simultaneously.
class ParallelBlock : public Block {
public:
    /// Default constructor.
    ParallelBlock(const LabelPtr &_pLabel = NULL) : Block(_pLabel) {}

    /// Get statement kind.
    /// \returns #ParallelBlock.
    virtual int getKind() const { return PARALLEL_BLOCK; }

    // Check if the statement ends like a block (i.e. separating semicolon is not needed).
    // \return False.
    virtual bool isBlockLike() const { return false; }

    virtual NodePtr clone(Cloner &_cloner) const {
        Auto<ParallelBlock> pCopy = NEW_CLONE(this, _cloner, ParallelBlock(_cloner.get(this->getLabel())));
        pCopy->appendClones(*this, _cloner);
        return pCopy;
    }
};

bool isTypeVariable(const NamedValuePtr &_pVar);

TypePtr resolveBaseType(const TypePtr &_pType);

} // namespace ir

#endif /* BASE_H_ */
