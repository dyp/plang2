/// \file base.h
/// Common classes that are used in IR structures.

/// \mainpage Internal representation reference
///
/// \section sec_root_classes Root classes
/// \subsection sec_CNode CNode : Base class for all internal representation objects
/// \copydetails ir::CNode
///
/// \subsection sec_CPragma CPragma : Compiler directive
/// \copydetails ir::CPragma
///
/// \subsection sec_CNumber CNumber : Representation of numeric literals
/// \copydetails CNumber
///
/// \section sec_other_classes Other classes
/// Please follow Classes or Files link to see complete documentation.

#ifndef BASE_H_
#define BASE_H_

#include <vector>
#include <algorithm>
#include <string>

namespace ir {

/// Base class for all internal representation objects.
///
/// Purpose of CNode class is to provide management of deallocation of IR objects
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
/// CNode class also contains a (possibly NULL) pointer to list of pragmas
/// relevant to the node's location in source code.
///
class CNode {
public:
    /// Default constructor.
    CNode() : m_pParent(NULL) /*, m_pPragmas(NULL)*/ {}

    /// Destructor.
    virtual ~CNode() {}

    /// Get pointer to parent.
    /// \return Parent of the parent node.
    CNode * getParent() const { return m_pParent; }

    /// Set new parent for the node. Removes the node from the list children of
    /// it's former parent.
    /// \param _pParent New parent.
    void setParent(CNode * _pParent) { m_pParent = _pParent; }
/*
    /// Get assosciated pragmas.
    /// \return Pointer to pragma context relevant for the node.
    const CPragmaGroup * getPragmas() const {
        return (m_pPragmas || ! m_pParent) ? m_pPragmas : m_pParent->getPragmas();
    }

    /// Set pragma context relevant for the node.
    /// \param _pPragmas Pointer to the list of pragmas. Should point to the
    ///   object stored in CModule.
    /// \see CModule
    void setPragmas(const CPragmaGroup * _pPragmas) { m_pPragmas = _pPragmas; }
*/
protected:
    CNode * m_pParent;
//    const CPragmaGroup * m_pPragmas;

    /// Delete _pChild if it's parent is this node or NULL.
    /// \param _pChild Node to delete.
    void _delete(CNode * _pChild) const {
        if (_pChild != NULL && _pChild->getParent() == this)
            delete _pChild;
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
/// \param _Base Base class of the result (default is CNode).
///
/// The elements are stored in separate list than children and are not required
/// to be the children of the collection.
template<class _Node, class _Base = CNode>
class CCollection : public _Base {
public:
    /// Default constructor.
    CCollection() {}

    virtual ~CCollection() {
        for (size_t i = size(); i > 0; -- i)
            _delete(get(i - 1));
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
    void append(const CCollection<_OtherNode> & _other, bool _bReparent = true) {
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

private:
    std::vector<void *> m_nodes;
};

/// Virtual ancestor of all types.
class CType : public CNode {
public:
    /// Type kind.
    enum {
        /// Unit type (\c nil, (), [], etc.)
        Unit,
        /// \c int type. Use getBits() and setBits() to access bitness.
        Int,
        /// \c nat type. Use getBits() and setBits() to access bitness.
        Nat,
        /// \c real type. Use getBits() and setBits() to access bitness.
        Real,
        /// \c bool type.
        Bool,
        /// \c char type.
        Char,
        /// \c string type.
        String,
        /// \c type type. Used as predicate or type parameter only.
        Type,
        /// \c var type. Actual type should be determined at typechecking phase.
        Generic,
        /// \c enum type. Can be cast to CEnumType.
        Enum,
        /// Struct type. Can be cast to CStructType.
        Struct,
        /// Union type. Can be cast to CUnionType.
        Union,
        /// Optional type. Can be cast to COptionalType.
        Optional,
        /// Sequence type. Can be cast to CSeqType.
        Seq,
        /// Array type. Can be cast to CArrayType.
        Array,
        /// Set type. Can be cast to CSetType.
        Set,
        /// Map type. Can be cast to CMapType.
        Map,
        /// List type. Can be cast to CListType.
        List,
        /// Subtype. Can be cast to CSubtype.
        Subtype,
        /// Range. Can be cast to CRange.
        Range,
        /// Predicate type. Can be cast to CPredicateType.
        Predicate,
        /// Parameterized type. Can be cast to CParameterizedType.
        Parameterized,
        /// User-defined type referenced by name. Can be cast to CNamedReferenceType.
        NamedReference,
    };

    /// Initialize with kind.
    /// \param _kind One of built-in types (#Unit, #Int, #Nat, #Real, #Bool, #Char, #String, #Type or #Generic).
    CType(int _kind) : m_kind(_kind), m_nBits(0) {}

    /// Destructor.
    virtual ~CType() {}

    /// Get type kind.
    /// \return Specific kind.
    virtual int getKind() const { return m_kind; }

    /// Get bitness (for numeric types only).
    /// \return #Native, #Unbounded or number of bits.
    int getBits() const { return m_nBits; }

    /// Set bitness (for numeric types only).
    /// \param _nBits #Native, #Unbounded or number of bits.
    void setBits(int _nBits) { m_nBits = _nBits; }

protected:
    /// Default constructor.
    /// Only descendant classes should use this.
    CType() : m_kind (0), m_nBits(0) {}

private:
    int m_kind;
    int m_nBits;
};

/// Simple typed and named value.
///
/// CNamedValue represents local and global variables, predicate parameters,
/// type parameters, iterators, etc. Particular kind of the value can be
/// determined by getKind.
class CNamedValue : public CNode {
public:
    /// Kind of the value.
    enum {
        /// Simple combination of type and identifier (type parameters,
        /// iterators, etc.)
        Generic,
        /// Predicate parameter. The object can be cast to CParam if needed.
        EnumValue,
        /// Predicate parameter. The object can be cast to CParam if needed.
        PredicateParameter,
        /// Local variable.
        Local,
        /// Global variable.
        Global
     };

    /// Default constructor.
    CNamedValue() : m_pType(NULL), m_strName(L"") {}

    /// Constructor for initializing using name.
    /// \param _strName Identifier.
    /// \param _pType Type associated with value.
    /// \param _bReparent If specified (default) also sets parent of _pType to this node.
    CNamedValue(const std::wstring & _strName, CType * _pType = NULL, bool _bReparent = true)
        : m_pType(NULL), m_strName(_strName)
    {
        _assign(m_pType, _pType, _bReparent);
    }

    /// Destructor.
    virtual ~CNamedValue() { _delete(m_pType); }

    /// Get value kind.
    /// \returns Value kind (#Generic, #PredicateParameter, #Local or #Global).
    virtual int getKind() const { return Generic; }

    /// Get name of the value.
    /// \returns Identifier.
    const std::wstring & getName() const { return m_strName; }

    /// Set name of the value.
    /// \param _strName Identifier.
    void setName(const std::wstring & _strName) { m_strName = _strName; }

    /// Get type of the value.
    /// \returns Type associated with value.
    CType * getType() const { return m_pType; }

    /// Set type of the value.
    /// \param _pType Type associated with value.
    /// \param _bReparent If specified (default) also sets parent of _pType to this node.
    void setType(CType * _pType, bool _bReparent = true) {
        _assign(m_pType, _pType, _bReparent);
    }

private:
    CType * m_pType;
    std::wstring m_strName;
};

/// Predicate parameter.
///
/// Offers a possibility to specify linked parameter. Consider:
/// \code foo (int x : int x') \endcode
/// In the above declaration parameters \c x and \c x' are linked.
class CParam : public CNamedValue {
public:
    /// Default constructor.
    CParam() : m_pLinkedParam(NULL), m_bOutput(false) {}

    /// Constructor for initializing using name.
    /// \param _strName Identifier.
    /// \param _pType Type associated with value.
    /// \param _bReparent If specified (default) also sets parent of _pType to this node.
    CParam(const std::wstring & _strName, CType * _pType = NULL, bool _bReparent = true)
        : CNamedValue(_strName, _pType, _bReparent), m_pLinkedParam(NULL), m_bOutput(false) {}

    /// Destructor.
    virtual ~CParam() {}

    /// Get value kind.
    /// \returns #PredicateParameter.
    virtual int getKind() const { return PredicateParameter; }

    /// Get pointer to (constant) linked parameter.
    /// \returns Linked parameter.
    const CParam * getLinkedParam() const { return m_pLinkedParam; }

    /// Set linked parameter pointer.
    /// \param _pParam Linked parameter.
    void setLinkedParam(const CParam * _pParam) { m_pLinkedParam = _pParam; }

    /// Check if a linked parameter is needed.
    /// \returns True if a linked parameter is needed, false otherwise.
    bool isOutput() const { return m_bOutput; }

    /// Set to true if a linked parameter is needed.
    /// \param _bValue True if a linked parameter is needed, false otherwise.
    void setOutput(bool _bValue) { m_bOutput = _bValue; }

private:
    const CParam * m_pLinkedParam;
    bool m_bOutput;
};

/// Collection of predicate parameters.
/// \extends CNode
class CParams : public CCollection<CParam> {
};

/// Collection of generic named values (e.g. type parameters).
/// \extends CNode
class CNamedValues : public CCollection<CNamedValue> {
};

/// Named label used to specify return branch.
/// \code foo (int x : #ok : #error) \endcode
/// In the above declaration parameters \c \#ok and \c \#error are labels.
class CLabel : public CNode {
public:
    /// Default constructor.
    CLabel() : m_strName(L"") {}

    /// Constructor for initializing using name.
    /// \param _strName Label name.
    CLabel(const std::wstring & _strName) : m_strName(_strName) {}

    /// Destructor.
    virtual ~CLabel() {}

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
class CStatement : public CNode {
public:
    /// Statement kind.
    enum {
        /// Statement that does nothing (appears if a label is placed at the end of block).
        Nop,
        /// A block of statements. Can be cast to CBlock.
        Block,
        /// Collection of statements that can be executed simultaneously. Can be cast to CParallelBlock.
        ParallelBlock,
        /// Jump statement. Can be cast to CJump.
        Jump,
        /// Assignment. Can be cast to CAssignment.
        Assignment,
        /// Multiassignment. Can be cast to CMultiassignment.
        Multiassignment,
        /// Predicate call. Can be cast to CCall.
        Call,
        /// Switch statement. Can be cast to CSwitch.
        Switch,
        /// Conditional statement. Can be cast to CIf.
        If,
        /// Imperative for-loop. Can be cast to CFor.
        For,
        /// Imperative while-loop. Can be cast to CWhile.
        While,
        /// Imperative break statement. Can be cast to CBreak.
        Break,
        /// Synchronized statement header. Can be cast to CWith.
        With,
        /// Receive message statement. Can be cast to CReceive.
        Receive,
        /// Send message statement. Can be cast to CSend.
        Send,
        /// Declaration of a user-defined type. Can be cast to CTypeDeclaration.
        TypeDeclaration,
        /// Declaration of a variable. Can be cast to CVariableDeclaration.
        VariableDeclaration,
        /// Declaration of a formula. Can be cast to CFormulaDeclaration.
        FormulaDeclaration,
        /// Declaration of a (nested) predicate. Can be cast to CPredicate.
        PredicateDeclaration,
    };

    /// Default constructor.
    CStatement() : m_pLabel(NULL) {}

    virtual ~CStatement() { _delete(m_pLabel); }

    /// Get statement kind.
    /// \returns Statement kind.
    virtual int getKind() const { return Nop; }

    /// Get optional label that can be associated with the statement.
    /// \return Label pointer (possibly NULL).
    CLabel * getLabel() const { return m_pLabel; }

    /// Associated a label with the statement.
    /// \param _pLabel Label pointer (possibly NULL).
    /// \param _bReparent If specified (default) also sets parent of _pLabel to this node.
    void setLabel(CLabel * _pLabel, bool _bReparent = true) {
        _assign(m_pLabel, _pLabel, _bReparent);
    }

    // Check if the statement ends like a block (i.e. separating semicolon is not needed).
    // \return True if the statement ends like a block, false otherwise.
    virtual bool isBlockLike() const { return false; }

private:
    CLabel * m_pLabel;
};

/// Block of statements (statements surrounded by curly braces in source code).
/// Use CCollection methods to access statements inside of block.
/// \extends CStatement
class CBlock : public CCollection<CStatement, CStatement> {
public:
    /// Default constructor.
    CBlock() {}

    /// Get statement kind.
    /// \returns #Block.
    virtual int getKind() const { return Block; }

    // Check if the statement ends like a block (i.e. separating semicolon is not needed).
    // \return True.
    virtual bool isBlockLike() const { return true; }
};

/// Collection of statements that can be executed simultaneously.
class CParallelBlock : public CBlock {
public:
    /// Default constructor.
    CParallelBlock() {}

    /// Get statement kind.
    /// \returns #ParallelBlock.
    virtual int getKind() const { return ParallelBlock; }

    // Check if the statement ends like a block (i.e. separating semicolon is not needed).
    // \return False.
    virtual bool isBlockLike() const { return false; }
};

} // namespace ir

#endif /* BASE_H_ */
