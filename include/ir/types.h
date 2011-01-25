/// \file types.h
/// Internal structures representing types.
///


#ifndef TYPES_H_
#define TYPES_H_

#include "base.h"
#include "expressions.h"

#include <assert.h>

namespace ir {

/// Type with parameters.
/// Anonymous, can be referenced by NamedReferenceType or TypeDeclaration.
class ParameterizedType : public Type {
public:
    /// Default constructor.
    ParameterizedType() : m_pActualType(NULL) {}

    /// Destructor.
    virtual ~ParameterizedType() {
        _delete(m_pActualType);
    }

    /// Get type kind.
    /// \returns #Parameterized.
    virtual int getKind() const { return PARAMETERIZED; }

    /// Get list of formal type parameters.
    /// \return List of variables.
    NamedValues & getParams() { return m_params; }

    /// Get actual type.
    /// \return Referenced type.
    Type * getActualType() const { return m_pActualType; }

    /// Set actual type.
    /// \param _pType Referenced type.
    /// \param _bReparent If specified (default) also sets parent of _pType to this node.
    void setActualType(Type * _pType, bool _bReparent = true) {
        _assign(m_pActualType, _pType, _bReparent);
    }

    virtual bool contains(const ir::Type *_pType) const { return m_pActualType->contains(_pType); }

    virtual bool hasParameters() const { return true; }

private:
    NamedValues m_params;
    Type * m_pActualType;
};

class TypeDeclaration;

/// User-defined type referenced by name.
class NamedReferenceType : public Type {
public:
    /// Default constructor.
    NamedReferenceType() : m_pDecl(NULL) {}

    /// Initialize with type declaration.
    /// \param _pDecl Target type declaration.
    NamedReferenceType(const TypeDeclaration * _pDecl) : m_pDecl(_pDecl) {}

//    /// Initialize with variable or type declaration.
//    /// \param _pVar Target variable or parameter declaration. Must be of \c type type.
//    NamedReferenceType(const NamedValue * _pVar) : m_pDecl(NULL), m_pVar(_pVar) {}

    /// Get type kind.
    /// \returns #NamedReference.
    virtual int getKind() const { return NAMED_REFERENCE; }

    /// Get pointer to target type declaration.
    /// \return Target type declaration or NULL if it wasn't declared as a type.
    const TypeDeclaration * getDeclaration() const { return m_pDecl; }

    /// Set pointer to target type declaration.
    /// \param _pDecl Target type declaration (NULL if it wasn't declared as a type.)
    void setDeclaration(const TypeDeclaration * _pDecl) { m_pDecl = _pDecl; }
//
//    /// Get pointer to variable or parameter declaration.
//    /// \return Target variable or parameter declaration or NULL if it wasn't declared as a variable or a parameter.)
//    const NamedValue * getVariable() const { return m_pVar; }
//
//    /// Set pointer to variable or parameter declaration. Must be of \c type type.
//    /// \param _pVar Target variable or parameter declaration (NULL if it wasn't declared as a variable or a parameter.)
//    void setVariable(const NamedValue * _pVar) { m_pVar = _pVar; }

    std::wstring getName() const;
    /// Get list of actual parameters.
    /// \return List of expressions.
    Collection<Expression> & getArgs() { return m_args; }

private:
    Collection<Expression> m_args;
    const TypeDeclaration * m_pDecl;
};

/// Parameter type. Used in a predicate or type declaration only.
class TypeType : public Type {
public:
    /// Default constructor.
    TypeType();

    /// Destructor.
    virtual ~TypeType();

    /// Get type kind.
    /// \returns #Type.
    virtual int getKind() const { return TYPE; }

    TypeDeclaration * getDeclaration() const { return m_pDecl; }

    void setDeclaration(TypeDeclaration *_pDecl, bool _bReparent = true) {
        _assign(m_pDecl, _pDecl, _bReparent);
    }

    virtual bool rewrite(ir::Type * _pOld, ir::Type * _pNew);
    virtual int compare(const Type & _other) const;
    virtual bool less(const Type & _other) const;

private:
    TypeDeclaration *m_pDecl;
};

/// Struct type.
/// Contains list of fields.
class StructType : public Type {
public:
    /// Default constructor.
    StructType() {}

    /// Get type kind.
    /// \returns #Struct.
    virtual int getKind() const { return STRUCT; }

    /// Get list of struct fields.
    /// \return List of fields.
    NamedValues & getFields() { return m_fields; }
    const NamedValues & getFields() const { return m_fields; }

    virtual bool hasFresh() const;
    virtual bool rewrite(ir::Type * _pOld, ir::Type * _pNew);
    virtual int compare(const Type & _other) const;
    virtual Extremum getMeet(ir::Type & _other);
    virtual Extremum getJoin(ir::Type & _other);
    virtual bool less(const Type & _other) const;

    bool allFieldsNamed() const;
    bool allFieldsUnnamed() const;

    virtual bool contains(const ir::Type *_pType) const {
        for (size_t i = 0; i < m_fields.size(); ++i)
            if (*m_fields.get(i)->getType() == *_pType || m_fields.get(i)->getType()->contains(_pType))
                return true;
        return false;
    }

private:
    NamedValues m_fields;
    mutable std::map<std::wstring, size_t> m_mapNames;

    void _fillNames() const;
};

/// Identifier belonging to an enumeration.
class EnumValue : public NamedValue {
public:
    /// Default constructor.
    EnumValue() : m_nOrdinal(-1) {}

    /// Constructor for initializing using name.
    /// \param _strName Identifier.
    /// \param _pType Associated enumeration.
    /// \param _nOrdinal Ordinal corresponding to the value.
    EnumValue(const std::wstring & _strName, int _nOrdinal = -1, Type * _pEnumType = NULL)
        : NamedValue(_strName, _pEnumType, false), m_nOrdinal(_nOrdinal) {}

    /// Destructor.
    virtual ~EnumValue() {}

    /// Get value kind.
    /// \returns #EnumValue.
    virtual int getKind() const { return ENUM_VALUE; }

    /// Get ordinal.
    /// \returns Ordinal corresponding to the value.
    int getOrdinal() const { return m_nOrdinal; }

    /// Set ordinal.
    /// \param _nOrdinal Ordinal corresponding to the value.
    void setOrdinal(int _nOrdinal) { m_nOrdinal = _nOrdinal; }

private:
    int m_nOrdinal;
};

/// Enum type.
/// Contains list of values.
class EnumType : public Type {
public:
    /// Default constructor.
    EnumType() {}

    /// Get type kind.
    /// \returns #Enum.
    virtual int getKind() const { return ENUM; }

    /// Get list of values.
    /// \return List of values.
    Collection<EnumValue> & getValues() { return m_values; }

private:
    Collection<EnumValue> m_values;
};

class UnionType;

class UnionConstructorDeclaration : public Node {
public:
    UnionConstructorDeclaration(const std::wstring & _strName, size_t _ord = 0) : m_strName(_strName), m_pUnion(NULL), m_ord(_ord) {}

    virtual int getNodeKind() const { return Node::UNION_CONSTRUCTOR_DECLARATION; }

    /// Get list of constructor fields.
    /// \return List of fields.
    StructType & getStruct() { return m_struct; }
    const StructType & getStruct() const { return m_struct; }

    UnionType * getUnion() const { return m_pUnion; }
    void setUnion(UnionType * _pUnion) { m_pUnion = _pUnion; }

    const std::wstring & getName() const { return m_strName; }
    void setName(const std::wstring & _strName) { m_strName = _strName; }

    size_t getOrdinal() const { return m_ord; }
    void setOrdinal(size_t _ord) { m_ord = _ord; }

private:
    std::wstring m_strName;
    StructType m_struct;
    UnionType * m_pUnion;
    size_t m_ord;
};

typedef Collection<UnionConstructorDeclaration> UnionConstructorDeclarations;

/// Union type.
/// Contains list of constructors.
class UnionType : public Type {
public:
    /// Default constructor.
    UnionType() {}

    /// Get type kind.
    /// \returns #Union.
    virtual int getKind() const { return UNION; }

    /// Get list of union constructors.
    /// \return List of constructors.
    UnionConstructorDeclarations & getConstructors() { return m_constructors; }
    const UnionConstructorDeclarations & getConstructors() const { return m_constructors; }

    UnionFieldIdx findField(const std::wstring & _strName) const;

    virtual int compare(const Type & _other) const;

    virtual bool contains(const ir::Type *_pType) const {
        for (size_t i = 0; i < m_constructors.size(); ++i)
            if (!m_constructors.get(i)->getStruct().contains(_pType))
                return false;
        return !m_constructors.empty();
    }

private:
    UnionConstructorDeclarations m_constructors;
};

/// Common base type for arrays, sets, etc.
class DerivedType : public Type {
public:
    /// Default constructor.
    DerivedType() : m_pBaseType(NULL) {}

    /// Initialize with base type.
    /// \param _pType Base type.
    /// \param _bReparent If specified (default) also sets parent of _pType to this node.
    DerivedType(Type * _pType, bool _bReparent = true) : m_pBaseType(NULL) {
        _assign(m_pBaseType, _pType, _bReparent);
    }

    /// Get base type.
    /// \return Base type.
    virtual Type * getBaseType() const { return m_pBaseType; }

    /// Set base type.
    /// \param _pType Base type.
    /// \param _bReparent If specified (default) also sets parent of _pType to this node.
    virtual void setBaseType(Type * _pType, bool _bReparent = true) {
        _assign(m_pBaseType, _pType, _bReparent);
    }

    virtual bool hasParameters() const { return true; }

    virtual bool hasFresh() const { return m_pBaseType->hasFresh(); }
    virtual bool rewrite(Type * _pOld, Type * _pNew);
    virtual int compare(const Type & _other) const;
    virtual bool less(const Type & _other) const;
    virtual bool rewriteFlags(int _flags) { return m_pBaseType->rewriteFlags(_flags); }

    virtual bool contains(const ir::Type *_pType) const {
        return *m_pBaseType == *_pType || m_pBaseType->contains(_pType);
    }

private:
    Type * m_pBaseType;
};

/// Optional type.
/// Values of optional type are either of it's base nype or \c nil.
class OptionalType : public DerivedType {
public:
    /// Default constructor.
    OptionalType() {}

    /// Initialize with base type.
    /// \param _pType Base type.
    /// \param _bReparent If specified (default) also sets parent of _pType to this node.
    OptionalType(Type * _pType, bool _bReparent = true) : DerivedType(_pType, _bReparent) {}

    /// Get type kind.
    /// \returns #Optional.
    virtual int getKind() const { return OPTIONAL; }
};

/// Sequence type.
class SeqType : public DerivedType {
public:
    /// Default constructor.
    SeqType() {}

    /// Initialize with base type.
    /// \param _pType Base type.
    /// \param _bReparent If specified (default) also sets parent of _pType to this node.
    SeqType(Type * _pType, bool _bReparent = true) : DerivedType(_pType, _bReparent) {}

    /// Get type kind.
    /// \returns #Seq.
    virtual int getKind() const { return SEQ; }
};

/// Subtype.
class Subtype : public Type {
public:
    /// Default constructor.
    Subtype() : m_pParam(NULL), m_pExpression(NULL) {}

    /// Initialize with parameter and expression.
    /// \param _pParam Parameter.
    /// \param _pExpression Boolean expression.
    /// \param _bReparent If specified (default) also sets parent of _pParam and _pExpression to this node.
    Subtype(NamedValue * _pParam, Expression * _pExpression, bool _bReparent = true) : m_pParam(NULL), m_pExpression(NULL) {
        _assign(m_pParam, _pParam, _bReparent);
        _assign(m_pExpression, _pExpression, _bReparent);
    }

    /// Destructor.
    virtual ~Subtype() {
        _delete(m_pParam);
        _delete(m_pExpression);
    }

    /// Get type kind.
    /// \returns #Subtype.
    virtual int getKind() const { return SUBTYPE; }

    /// Get parameter declaration.
    /// \return Subtype parameter variable.
    NamedValue * getParam() const { return m_pParam; }

    /// Set parameter declaration.
    /// \param _pParam Subtype parameter variable.
    /// \param _bReparent If specified (default) also sets parent of _pParam to this node.
    void setParam(NamedValue * _pParam, bool _bReparent = true) {
        _assign(m_pParam, _pParam, _bReparent);
    }

    /// Get logical expression which should evaluate to \c true for all values of type.
    /// \return Boolean expression.
    Expression * getExpression() const {
        return m_pExpression;
    }

    /// Set logical expression which should evaluate to \c true for all values of type.
    /// \param _pExpression Boolean expression.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setExpression(Expression * _pExpression, bool _bReparent = true) {
        _assign(m_pExpression, _pExpression, _bReparent);
    }

private:
    NamedValue * m_pParam;
    Expression * m_pExpression;
};

/// Range type.
/// A subtype defined as a MIN..MAX continuous range. Should be roughly equivalent
/// to types defined like:
/// \code subtype (T t : t >= tMin & t <= tMax) \endcode
class Range : public Type {
public:
    /// Default constructor.
    Range() : m_pMin(NULL), m_pMax(NULL) {}

    /// Initialize with bounds.
    /// \param _pMin Expression corresponding to the lower bound of the type.
    /// \param _pMax Expression corresponding to the upper bound of the type.
    /// \param _bReparent If specified (default) also sets parent of _pMin and _pMax to this node.
    Range(Expression * _pMin = NULL, Expression * _pMax = NULL, bool _bReparent = true)
        : m_pMin(NULL), m_pMax(NULL)
    {
        _assign(m_pMin, _pMin, _bReparent);
        _assign(m_pMax, _pMax, _bReparent);
    }

    /// Destructor.
    virtual ~Range() {
        _delete(m_pMin);
        _delete(m_pMax);
    }

    /// Get type kind.
    /// \returns #Range.
    virtual int getKind() const { return RANGE; }

    /// Get minimal value contained in the type.
    /// \return Expression corresponding to the lower bound of the type.
    Expression * getMin() const { return m_pMin; }

    /// Set minimal value contained in the type.
    /// \param _pExpression Expression corresponding to the lower bound of the type.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setMin(Expression * _pExpression, bool _bReparent = true) {
        _assign(m_pMin, _pExpression, _bReparent);
    }

    /// Get maximal value contained in the type.
    /// \return Expression corresponding to the upper bound of the type.
    Expression * getMax() const { return m_pMax; }

    /// Set maximal value contained in the type.
    /// \param _pExpression Expression corresponding to the upper bound of the type.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setMax(Expression * _pExpression, bool _bReparent = true) {
        _assign(m_pMax, _pExpression, _bReparent);
    }

private:
    Expression * m_pMin, * m_pMax;
};

/// Array type.
class ArrayType : public DerivedType {
public:
    /// Default constructor.
    ArrayType() {}

    /// Initialize with base type.
    /// \param _pType Base type.
    /// \param _bReparent If specified (default) also sets parent of _pType to this node.
    ArrayType(Type * _pType, bool _bReparent = true) : DerivedType(_pType, _bReparent) {}

    /// Get type kind.
    /// \returns #Array.
    virtual int getKind() const { return ARRAY; }

    /// Get array dimensions as list of ranges.
    /// \return List of ranges.
    Collection<Range> & getDimensions() { return m_dimensions; }

private:
    Collection<Range> m_dimensions;
};

/// Set type.
class SetType : public DerivedType {
public:
    /// Default constructor.
    SetType() {}

    /// Initialize with base type.
    /// \param _pType Base type.
    /// \param _bReparent If specified (default) also sets parent of _pType to this node.
    SetType(Type * _pType, bool _bReparent = true) : DerivedType(_pType, _bReparent) {}

    /// Get type kind.
    /// \returns #Set.
    virtual int getKind() const { return SET; }

    virtual Extremum getMeet(Type & _other);
    virtual Extremum getJoin(Type & _other);
};

/// Map type.
/// Base type (defined in DerivedType) is the type of values contained in a map.
class MapType : public DerivedType {
public:
    /// Default constructor.
    MapType() : m_pIndexType(NULL) {}

    /// Initialize with base type.
    /// \param _pIndexType Index type.
    /// \param _pBaseType Base type.
    /// \param _bReparent If specified (default) also sets parent of _pIndexType and _pBaseType to this node.
    MapType(Type * _pIndexType, Type * _pBaseType, bool _bReparent = true)
        : DerivedType(_pBaseType, _bReparent), m_pIndexType(NULL)
    {
        _assign(m_pIndexType, _pIndexType, _bReparent);
    }

    /// Get type kind.
    /// \returns #Map.
    virtual int getKind() const { return MAP; }

    /// Get index type.
    /// \return Index type.
    Type * getIndexType() const { return m_pIndexType; }

    /// Set index type.
    /// \param _pType Index type.
    /// \param _bReparent If specified (default) also sets parent of _pType to this node.
    void setIndexType(Type * _pType, bool _bReparent = true) {
        _assign(m_pIndexType, _pType, _bReparent);
    }

    virtual bool contains(const ir::Type *_pType) const {
        return DerivedType::contains(_pType) || *m_pIndexType == *_pType || m_pIndexType->contains(_pType);
    }

    virtual bool rewriteFlags(int _flags) {
        const bool b = DerivedType::rewriteFlags(_flags);
        return m_pIndexType->rewriteFlags(_flags) || b;
    }

    virtual bool hasFresh() const { return DerivedType::hasFresh() || m_pIndexType->hasFresh(); }
    virtual bool rewrite(Type * _pOld, Type * _pNew);
    virtual int compare(const Type & _other) const;
    virtual bool less(const Type & _other) const;
    virtual Extremum getJoin(ir::Type &_other);
    virtual Extremum getMeet(ir::Type &_other);

private:
    Type * m_pIndexType;
};

/// List type.
class ListType : public DerivedType {
public:
    /// Default constructor.
    ListType() {}

    /// Initialize with base type.
    /// \param _pType Base type.
    /// \param _bReparent If specified (default) also sets parent of _pType to this node.
    ListType(Type * _pType, bool _bReparent = true) : DerivedType(_pType, _bReparent) {}

    /// Get type kind.
    /// \returns #List.
    virtual int getKind() const { return LIST; }

    virtual Extremum getJoin(ir::Type &_other);
    virtual Extremum getMeet(ir::Type &_other);
};

/// Predicate type.
class PredicateType : public Type {
public:
    /// Default constructor.
    PredicateType() : m_pPreCond(NULL), m_pPostCond(NULL) {}

    /// Destructor.
    virtual ~PredicateType() {
        _delete(m_pPreCond);
        _delete(m_pPostCond);
    }

    /// Get type kind.
    /// \returns #Predicate.
    virtual int getKind() const { return PREDICATE; }

    /// Get list of formal input parameters.
    /// \return List of parameters.
    Params & getInParams() { return m_paramsIn; }
    const Params & getInParams() const { return m_paramsIn; }

    /// Get list of output branches. Each branch can contain a list of parameters,
    /// a precondition and a postcondition.
    /// \return List of branches.
    Branches & getOutParams() { return m_paramsOut; }
    const Branches & getOutParams() const { return m_paramsOut; }

    /// Get precondition common for all branches.
    /// \return Precondition.
    Formula * getPreCondition() const { return m_pPreCond; }

    /// Set precondition common for all branches.
    /// \param _pCondition Precondition.
    /// \param _bReparent If specified (default) also sets parent of _pCondition to this node.
    void setPreCondition(Formula * _pCondition, bool _bReparent = true) {
        _assign(m_pPreCond, _pCondition, _bReparent);
    }

    /// Get postcondition common for all branches.
    /// \return Postcondition.
    Formula * getPostCondition() const { return m_pPostCond; }

    /// Set postcondition common for all branches.
    /// \param _pCondition Postcondition.
    /// \param _bReparent If specified (default) also sets parent of _pCondition to this node.
    void setPostCondition(Formula * _pCondition, bool _bReparent = true) {
        _assign(m_pPostCond, _pCondition, _bReparent);
    }

    /// Check if the predicate is a hyperfunction.
    /// \return True if the predicate has more than one branch or it's branch has
    ///   a handler assigned.
    bool isHyperFunction() const {
        return m_paramsOut.size() > 1 ||
            (m_paramsOut.size() == 1 && m_paramsOut.get(0)->getLabel() != NULL);
    }

    virtual bool hasFresh() const;
    virtual bool rewrite(ir::Type * _pOld, ir::Type * _pNew);

    virtual bool hasParameters() const { return true; }

    virtual bool contains(const ir::Type *_pType) const {
        // TODO: implement.
        return false;
    }

    virtual int compare(const Type &_other) const;

    // TODO implement join/meet/less/etc.

private:
    Params m_paramsIn;
    Branches m_paramsOut;
    Formula * m_pPreCond, * m_pPostCond;
};

} // namespace ir

#endif /* TYPES_H_ */
