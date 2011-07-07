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

namespace ir {

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
class Node {
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
    Node() : m_pParent(NULL) /*, m_pPragmas(NULL)*/ {}

    /// Destructor.
    virtual ~Node() {}

    virtual int getNodeKind() const { return NONE; }

    /// Get pointer to parent.
    /// \return Parent of the parent node.
    Node * getParent() const { return m_pParent; }

    /// Set new parent for the node. Removes the node from the list children of
    /// it's former parent.
    /// \param _pParent New parent.
    void setParent(Node * _pParent) const { m_pParent = _pParent; }
/*
    /// Get assosciated pragmas.
    /// \return Pointer to pragma context relevant for the node.
    const PragmaGroup * getPragmas() const {
        return (m_pPragmas || ! m_pParent) ? m_pPragmas : m_pParent->getPragmas();
    }

    /// Set pragma context relevant for the node.
    /// \param _pPragmas Pointer to the list of pragmas. Should point to the
    ///   object stored in Module.
    /// \see Module
    void setPragmas(const PragmaGroup * _pPragmas) { m_pPragmas = _pPragmas; }
*/
protected:
    mutable Node * m_pParent;
//    const PragmaGroup * m_pPragmas;

    /// Delete _pChild if it's parent is this node or NULL.
    /// \param _pChild Node to delete.
    void _delete(Node * _pChild) const {
//        if (_pChild != NULL && _pChild->getParent() == this)
//            delete _pChild;
    }

    /// Assign a subnode performing necessary checks and deleting old subnode if needed.
    /// \param _pMember Pointer to a subnode to assign to.
    /// \param _pNode Pointer to the new subnode.
    /// \param _bReparent If specified (default) also sets parent of _pNode to this node.
    template<class _Node>
    void _assign(_Node * & _pMember, _Node * _pNode, bool _bReparent) {
        if (_pMember != _pNode) {
            _delete(_pMember);
            _pMember = _pNode;
        }

        if (_bReparent && _pMember != NULL && ! _pMember->getParent())
            _pMember->setParent(this);
    }
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
template<class _Node, class _Base = Node>
class Collection : public _Base {
public:
    /// Default constructor.
    Collection() {}

    virtual ~Collection() {
        for (size_t i = size(); i > 0; -- i)
            _delete(get(i - 1));
    }

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
    _Node * get(size_t _c) const {
        return _c < m_nodes.size() ? (_Node *) (m_nodes[_c]) : NULL;
    }

    /// Add element to the collection.
    /// \param _pNode Pointer to node to add.
    /// \param _bReparent If specified (default) also sets parent of _pNode to this node.
    void add(_Node * _pNode, bool _bReparent = true) {
        m_nodes.push_back((void *) _pNode);
        if (_pNode && _bReparent)
            _pNode->setParent(this);
    }

    /// Append elements from another collection.
    /// \param _other Other collection.
    /// \param _bReparent If specified (default) also sets parent of new nodes to this node.
    template<typename _OtherNode>
    void append(const Collection<_OtherNode> & _other, bool _bReparent = true) {
        for (size_t i = 0; i < _other.size(); ++ i)
            add(_other.get(i), _bReparent);
    }

    /// Replace element by index.
    /// \param _c Index of element (zero-based).
    /// \param _pNode Pointer to new element.
    /// \param _bReparent If specified (default) also sets parent of _pNode to this node.
    void set(size_t _c, _Node * _pNode, bool _bReparent = true) {
        if (_c < m_nodes.size()) {
            _delete((_Node *) (m_nodes[_c]));
            m_nodes.push_back((void *) _pNode);
            if (_pNode && _bReparent)
                _pNode->setParent(this);
        }
    }

    /// Remove element.
    /// \param _pNode Pointer to element to remove.
    /// \param _bDeleteIfOwned Also delete _pNode if it is owned by the collection.
    /// \return True if node was successfully removed, false if not found.
    bool remove(_Node * _pNode, bool _bDeleteIfOwned = false) {
        std::vector<void *>::iterator iNode =
            std::find(m_nodes.begin(), m_nodes.end(), (void *) _pNode);
        if (iNode == m_nodes.end())
            return false;
        if (_bDeleteIfOwned)
            _delete(_pNode);
        m_nodes.erase(iNode);
        return true;
    }

    size_t findByNameIdx(const std::wstring & _name) const {
        for (size_t i = 0; i < size(); ++ i)
            if (get(i)->getName() == _name)
                return i;

        return (size_t) -1;
    }

private:
    std::vector<void *> m_nodes;
};

class Type;

typedef std::map<Type *, Type *> TypeSubst;

/// Virtual ancestor of all types.
class Type : public Node {
public:
    /// Type kind.
    enum {
        /// Fresh type (for typechecking purposes).
        FRESH,
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
    };

    /// Initialize with kind.
    /// \param _kind One of built-in types (#Unit, #Int, #Nat, #Real, #Bool, #Char, #String, #Type or #Generic).
    Type(int _kind, int _bits = 0) : m_kind(_kind), m_nBits(_bits) {}

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

    virtual int compare(const Type & _other) const;
    bool compare(const Type & _other, int _order) const;

    // For comparison/sorting only, no subtyping relation is implied.
    bool operator <(const Type & _other) const;
    bool operator ==(const Type & _other) const;
    bool operator !=(const Type & _other) const;
    virtual bool less(const Type & _other) const;

    /*bool operator ==(const Type & _other) const { return compare(_other) == OrdEquals; }
    bool operator <(const Type & _other) const { return compare(_other) == OrdSub; }
    bool operator <=(const Type & _other) const {
        const int n = compare(_other);
        return n == OrdSub || n == OrdEquals;
    }*/

    // .second  is true if extremum doesn't exist.
    typedef std::pair<Type *, bool> Extremum;
    virtual Extremum getJoin(ir::Type & _other); // Supremum.
    virtual Extremum getMeet(ir::Type & _other); // Infinum.

    virtual ir::Type * clone() const;
    virtual bool hasFresh() const;
    virtual bool rewrite(ir::Type * _pOld, ir::Type * _pNew) { return false; }
    virtual bool rewriteFlags(int _flags) { return false; }

    // Check if _pType is structurally contained.
    virtual bool contains(const ir::Type *_pType) const { return false; }

    virtual bool hasParameters() const { return m_kind == INT || m_kind == NAT || m_kind == REAL; }

protected:
    /// Default constructor.
    /// Only descendant classes should use this.
    Type() : m_kind (0), m_nBits(0) {}

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
    /// \param _bReparent If specified (default) also sets parent of _pType to this node.
    NamedValue(const std::wstring & _strName, Type * _pType = NULL, bool _bReparent = true)
        : m_pType(NULL), m_strName(_strName)
    {
        _assign(m_pType, _pType, _bReparent);
    }

    /// Destructor.
    virtual ~NamedValue() { _delete(m_pType); }

    virtual int getNodeKind() const { return Node::NAMED_VALUE; }

    /// Get value kind.
    /// \returns Value kind (#Generic, #PredicateParameter, #Local or #Global).
    virtual int getKind() const { return GENERIC; }

    /// Get name of the value.
    /// \returns Identifier.
    const std::wstring & getName() const { return m_strName; }

    /// Set name of the value.
    /// \param _strName Identifier.
    void setName(const std::wstring & _strName) { m_strName = _strName; }

    /// Get type of the value.
    /// \returns Type associated with value.
    Type * getType() const { return m_pType; }

    /// Set type of the value.
    /// \param _pType Type associated with value.
    /// \param _bReparent If specified (default) also sets parent of _pType to this node.
    void setType(Type * _pType, bool _bReparent = true) {
        _assign(m_pType, _pType, _bReparent);
    }

private:
    Type * m_pType;
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
    /// \param _bReparent If specified (default) also sets parent of _pType to this node.
    Param(const std::wstring & _strName, Type * _pType = NULL, bool _bReparent = true)
        : NamedValue(_strName, _pType, _bReparent), m_pLinkedParam(NULL), m_bOutput(false) {}

    /// Destructor.
    virtual ~Param() {}

    /// Get value kind.
    /// \returns #PredicateParameter.
    virtual int getKind() const { return PREDICATE_PARAMETER; }

    /// Get pointer to (constant) linked parameter.
    /// \returns Linked parameter.
    const Param * getLinkedParam() const { return m_pLinkedParam; }

    /// Set linked parameter pointer.
    /// \param _pParam Linked parameter.
    void setLinkedParam(const Param * _pParam) { m_pLinkedParam = _pParam; }

    /// Check if a linked parameter is needed.
    /// \returns True if a linked parameter is needed, false otherwise.
    bool isOutput() const { return m_bOutput; }

    /// Set to true if a linked parameter is needed.
    /// \param _bValue True if a linked parameter is needed, false otherwise.
    void setOutput(bool _bValue) { m_bOutput = _bValue; }

private:
    const Param * m_pLinkedParam;
    bool m_bOutput;
};

/// Collection of predicate parameters.
/// \extends Node
class Params : public Collection<Param> {
};

/// Collection of generic named values (e.g. type parameters).
/// \extends Node
class NamedValues : public Collection<NamedValue> {
};

/// Named label used to specify return branch.
/// \code foo (int x : #ok : #error) \endcode
/// In the above declaration parameters \c \#ok and \c \#error are labels.
class Label : public Node {
public:
    /// Default constructor.
    Label() : m_strName(L"") {}

    /// Constructor for initializing using name.
    /// \param _strName Label name.
    Label(const std::wstring & _strName) : m_strName(_strName) {}

    /// Destructor.
    virtual ~Label() {}

    virtual int getNodeKind() const { return Node::LABEL; }

    /// Get name of the value.
    /// \returns Label name.
    const std::wstring & getName() const { return m_strName; }

    /// Set name of the value.
    /// \param _strName Label name.
    void setName(const std::wstring & _strName) { m_strName = _strName; }

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
    };

    /// Default constructor.
    Statement() : m_pLabel(NULL) {}

    virtual ~Statement() { _delete(m_pLabel); }

    virtual int getNodeKind() const { return Node::STATEMENT; }

    /// Get statement kind.
    /// \returns Statement kind.
    virtual int getKind() const { return NOP; }

    /// Get optional label that can be associated with the statement.
    /// \return Label pointer (possibly NULL).
    Label * getLabel() const { return m_pLabel; }

    /// Associated a label with the statement.
    /// \param _pLabel Label pointer (possibly NULL).
    /// \param _bReparent If specified (default) also sets parent of _pLabel to this node.
    void setLabel(Label * _pLabel, bool _bReparent = true) {
        _assign(m_pLabel, _pLabel, _bReparent);
    }

    // Check if the statement ends like a block (i.e. separating semicolon is not needed).
    // \return True if the statement ends like a block, false otherwise.
    virtual bool isBlockLike() const { return false; }

private:
    Label * m_pLabel;
};

/// Block of statements (statements surrounded by curly braces in source code).
/// Use Collection methods to access statements inside of block.
/// \extends Statement
class Block : public Collection<Statement, Statement> {
public:
    /// Default constructor.
    Block() {}

    /// Get statement kind.
    /// \returns #Block.
    virtual int getKind() const { return BLOCK; }

    // Check if the statement ends like a block (i.e. separating semicolon is not needed).
    // \return True.
    virtual bool isBlockLike() const { return true; }
};

/// Collection of statements that can be executed simultaneously.
class ParallelBlock : public Block {
public:
    /// Default constructor.
    ParallelBlock() {}

    /// Get statement kind.
    /// \returns #ParallelBlock.
    virtual int getKind() const { return PARALLEL_BLOCK; }

    // Check if the statement ends like a block (i.e. separating semicolon is not needed).
    // \return False.
    virtual bool isBlockLike() const { return false; }
};

bool isTypeVariable(const NamedValue *_pVar, const Type *&_pType);

const Type *resolveBaseType(const Type *_pType);

} // namespace ir

#endif /* BASE_H_ */
