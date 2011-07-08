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

    /// Get type kind.
    /// \returns #Parameterized.
    virtual int getKind() const { return PARAMETERIZED; }

    /// Get list of formal type parameters.
    /// \return List of variables.
    NamedValues &getParams() { return m_params; }

    /// Get actual type.
    /// \return Referenced type.
    const TypePtr &getActualType() const { return m_pActualType; }

    /// Set actual type.
    /// \param _pType Referenced type.
    void setActualType(const TypePtr &_pType) { m_pActualType = _pType; }

    virtual bool contains(const TypePtr &_pType) const { return m_pActualType->contains(_pType); }

    virtual bool hasParameters() const { return true; }

private:
    NamedValues m_params;
    TypePtr m_pActualType;
};

class TypeDeclaration;

/// User-defined type referenced by name.
class NamedReferenceType : public Type {
public:
    /// Default constructor.
    NamedReferenceType() : m_pDecl(NULL) {}

    /// Initialize with type declaration.
    /// \param _pDecl Target type declaration.
    NamedReferenceType(const TypeDeclarationPtr &_pDecl) : m_pDecl(_pDecl) {}

    /// Get type kind.
    /// \returns #NamedReference.
    virtual int getKind() const { return NAMED_REFERENCE; }

    /// Get pointer to target type declaration.
    /// \return Target type declaration or NULL if it wasn't declared as a type.
    const TypeDeclarationPtr &getDeclaration() const { return m_pDecl; }

    /// Set pointer to target type declaration.
    /// \param _pDecl Target type declaration (NULL if it wasn't declared as a type.)
    void setDeclaration(const TypeDeclarationPtr &_pDecl) { m_pDecl = _pDecl; }

    std::wstring getName() const;

    /// Get list of actual parameters.
    /// \return List of expressions.
    Collection<Expression> &getArgs() { return m_args; }

private:
    Collection<Expression> m_args;
    TypeDeclarationPtr m_pDecl;
};

/// Parameter type. Used in a predicate or type declaration only.
class TypeType : public Type {
public:
    /// Default constructor.
    TypeType();

    /// Get type kind.
    /// \returns #Type.
    virtual int getKind() const { return TYPE; }

    const TypeDeclarationPtr &getDeclaration() const { return m_pDecl; }

    void setDeclaration(const TypeDeclarationPtr &_pDecl) { m_pDecl = _pDecl; }

    virtual bool rewrite(const TypePtr &_pOld, const TypePtr &_pNew);
    virtual int compare(const Type &_other) const;
    virtual bool less(const Type &_other) const;

    TypePtr clone() const;

private:
    TypeDeclarationPtr m_pDecl;
};

/// Struct type.
/// Contains list of fields.
class StructType : public Type {
public:
    /// Default constructor.
    StructType();

    /// Get type kind.
    /// \returns #Struct.
    virtual int getKind() const { return STRUCT; }

    /// Ordered fields known by name.
    /// \return List of fields.
    NamedValues &getNamesOrd() { return m_namesOrd; }
    const NamedValues &getNamesOrd() const { return m_namesOrd; }

    // Ordered fields known by type.
    NamedValues &getTypesOrd() { return m_typesOrd; }
    const NamedValues &getTypesOrd() const { return m_typesOrd; }

    // Unordered fields known by name.
    NamedValues &getNamesSet() { return m_namesSet; }
    const NamedValues &getNamesSet() const { return m_namesSet; }

    /// All fields. Guaranteed to have 3 elements.
    /// \return List of fields.
    NamedValues *getAllFields() { return m_fields; }
    const NamedValues *getAllFields() const { return m_fields; }

    virtual bool hasFresh() const;
    virtual bool rewrite(const TypePtr &_pOld, const TypePtr &_pNew);
    virtual int compare(const Type &_other) const;
    virtual TypePtr getMeet(Type &_other);
    virtual TypePtr getJoin(Type &_other);
    virtual bool less(const Type &_other) const;
    virtual bool contains(const TypePtr &_pType) const;

    bool empty() const;

private:
    NamedValues m_fields[3];
    NamedValues &m_namesOrd, &m_typesOrd, &m_namesSet;
};

/// Identifier belonging to an enumeration.
class EnumValue : public NamedValue {
public:
    /// Default constructor.
    EnumValue() : m_nOrdinal(-1) {}

    /// Constructor for initializing using name.
    /// \param _strName Identifier.
    /// \param _nOrdinal Ordinal corresponding to the value.
    /// \param _pEnumType Associated enumeration.
    EnumValue(const std::wstring &_strName, int _nOrdinal = -1, const TypePtr &_pEnumType = NULL)
        : NamedValue(_strName, _pEnumType), m_nOrdinal(_nOrdinal) {}

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
    Collection<EnumValue> &getValues() { return m_values; }

private:
    Collection<EnumValue> m_values;
};

class UnionType;

class UnionConstructorDeclaration : public Node {
public:
    UnionConstructorDeclaration(const std::wstring &_strName, size_t _ord = 0) : m_strName(_strName), m_pUnion(NULL), m_ord(_ord) {}

    virtual int getNodeKind() const { return Node::UNION_CONSTRUCTOR_DECLARATION; }

    /// Get list of constructor fields.
    /// \return List of fields.
    NamedValues &getFields() { return m_fields; }
    const NamedValues &getFields() const { return m_fields; }

    const UnionTypePtr &getUnion() const { return m_pUnion; }
    void setUnion(const UnionTypePtr &_pUnion) { m_pUnion = _pUnion; }

    const std::wstring &getName() const { return m_strName; }
    void setName(const std::wstring &_strName) { m_strName = _strName; }

    size_t getOrdinal() const { return m_ord; }
    void setOrdinal(size_t _ord) { m_ord = _ord; }

private:
    std::wstring m_strName;
    NamedValues m_fields;
    UnionTypePtr m_pUnion;
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
    UnionConstructorDeclarations &getConstructors() { return m_constructors; }
    const UnionConstructorDeclarations &getConstructors() const { return m_constructors; }

    UnionFieldIdx findField(const std::wstring &_strName) const;

    virtual int compare(const Type &_other) const;

    virtual bool hasFresh() const;
    virtual bool rewrite(const TypePtr &_pOld, const TypePtr &_pNew);
    virtual TypePtr getMeet(Type &_other);
    virtual TypePtr getJoin(Type &_other);
    virtual bool less(const Type &_other) const;

    virtual bool contains(const TypePtr &_pType) const;

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
    DerivedType(const TypePtr &_pType) : m_pBaseType(_pType) {}

    /// Get base type.
    /// \return Base type.
    virtual TypePtr getBaseType() const { return m_pBaseType; }

    /// Set base type.
    /// \param _pType Base type.
    virtual void setBaseType(const TypePtr &_pType) { m_pBaseType = _pType; }

    virtual bool hasParameters() const { return true; }

    virtual bool hasFresh() const { return m_pBaseType->hasFresh(); }
    virtual bool rewrite(const TypePtr &_pOld, const TypePtr &_pNew);
    virtual int compare(const Type &_other) const;
    virtual bool less(const Type &_other) const;
    virtual bool rewriteFlags(int _flags) { return m_pBaseType->rewriteFlags(_flags); }

    virtual bool contains(const TypePtr &_pType) const {
        return *m_pBaseType == *_pType || m_pBaseType->contains(_pType);
    }

private:
    TypePtr m_pBaseType;
};

/// Optional type.
/// Values of optional type are either of it's base nype or \c nil.
class OptionalType : public DerivedType {
public:
    /// Default constructor.
    OptionalType() {}

    /// Initialize with base type.
    /// \param _pType Base type.
    OptionalType(const TypePtr &_pType) : DerivedType(_pType) {}

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
    SeqType(const TypePtr &_pType) : DerivedType(_pType) {}

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
    Subtype(const NamedValuePtr &_pParam, const ExpressionPtr &_pExpression)
        : m_pParam(_pParam), m_pExpression(_pExpression) {}

    /// Get type kind.
    /// \returns #Subtype.
    virtual int getKind() const { return SUBTYPE; }

    /// Get parameter declaration.
    /// \return Subtype parameter variable.
    const NamedValuePtr &getParam() const { return m_pParam; }

    /// Set parameter declaration.
    /// \param _pParam Subtype parameter variable.
    void setParam(const NamedValuePtr &_pParam) { m_pParam = _pParam; }

    /// Get logical expression which should evaluate to \c true for all values of type.
    /// \return Boolean expression.
    const ExpressionPtr &getExpression() const { return m_pExpression; }

    /// Set logical expression which should evaluate to \c true for all values of type.
    /// \param _pExpression Boolean expression.
    void setExpression(const ExpressionPtr &_pExpression) { m_pExpression = _pExpression; }

private:
    NamedValuePtr m_pParam;
    ExpressionPtr m_pExpression;
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
    Range(const ExpressionPtr &_pMin = NULL, const ExpressionPtr &_pMax = NULL)
        : m_pMin(_pMin), m_pMax(_pMax) {}

    /// Get type kind.
    /// \returns #Range.
    virtual int getKind() const { return RANGE; }

    /// Get minimal value contained in the type.
    /// \return Expression corresponding to the lower bound of the type.
    const ExpressionPtr &getMin() const { return m_pMin; }

    /// Set minimal value contained in the type.
    /// \param _pExpression Expression corresponding to the lower bound of the type.
    void setMin(const ExpressionPtr &_pExpression) { m_pMin = _pExpression; }

    /// Get maximal value contained in the type.
    /// \return Expression corresponding to the upper bound of the type.
    const ExpressionPtr &getMax() const { return m_pMax; }

    /// Set maximal value contained in the type.
    /// \param _pExpression Expression corresponding to the upper bound of the type.
    void setMax(const ExpressionPtr &_pExpression) { m_pMax = _pExpression; }

private:
    ExpressionPtr m_pMin, m_pMax;
};

/// Array type.
class ArrayType : public DerivedType {
public:
    /// Default constructor.
    ArrayType() {}

    /// Initialize with base type.
    /// \param _pType Base type.
    ArrayType(const TypePtr &_pType) : DerivedType(_pType) {}

    /// Get type kind.
    /// \returns #Array.
    virtual int getKind() const { return ARRAY; }

    /// Get array dimensions as list of ranges.
    /// \return List of ranges.
    Collection<Range> &getDimensions() { return m_dimensions; }

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
    SetType(const TypePtr &_pType) : DerivedType(_pType) {}

    /// Get type kind.
    /// \returns #Set.
    virtual int getKind() const { return SET; }

    virtual TypePtr getMeet(Type &_other);
    virtual TypePtr getJoin(Type &_other);
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
    MapType(const TypePtr &_pIndexType, const TypePtr &_pBaseType)
        : DerivedType(_pBaseType), m_pIndexType(_pIndexType) {}

    /// Get type kind.
    /// \returns #Map.
    virtual int getKind() const { return MAP; }

    /// Get index type.
    /// \return Index type.
    const TypePtr &getIndexType() const { return m_pIndexType; }

    /// Set index type.
    /// \param _pType Index type.
    void setIndexType(const TypePtr &_pType) { m_pIndexType = _pType; }

    virtual bool contains(const TypePtr &_pType) const {
        return DerivedType::contains(_pType) || *m_pIndexType == *_pType || m_pIndexType->contains(_pType);
    }

    virtual bool rewriteFlags(int _flags) {
        const bool b = DerivedType::rewriteFlags(_flags);
        return m_pIndexType->rewriteFlags(_flags) || b;
    }

    virtual bool hasFresh() const { return DerivedType::hasFresh() || m_pIndexType->hasFresh(); }
    virtual bool rewrite(const TypePtr &_pOld, const TypePtr &_pNew);
    virtual int compare(const Type &_other) const;
    virtual bool less(const Type &_other) const;
    virtual TypePtr getJoin(Type &_other);
    virtual TypePtr getMeet(Type &_other);

private:
    TypePtr m_pIndexType;
};

/// List type.
class ListType : public DerivedType {
public:
    /// Default constructor.
    ListType() {}

    /// Initialize with base type.
    /// \param _pType Base type.
    ListType(const TypePtr &_pType) : DerivedType(_pType) {}

    /// Get type kind.
    /// \returns #List.
    virtual int getKind() const { return LIST; }

    virtual TypePtr getJoin(Type &_other);
    virtual TypePtr getMeet(Type &_other);
};

/// Predicate type.
class PredicateType : public Type {
public:
    /// Default constructor.
    PredicateType() : m_pPreCond(NULL), m_pPostCond(NULL) {}

    /// Get type kind.
    /// \returns #Predicate.
    virtual int getKind() const { return PREDICATE; }

    /// Get list of formal input parameters.
    /// \return List of parameters.
    Params &getInParams() { return m_paramsIn; }
    const Params &getInParams() const { return m_paramsIn; }

    /// Get list of output branches. Each branch can contain a list of parameters,
    /// a precondition and a postcondition.
    /// \return List of branches.
    Branches &getOutParams() { return m_paramsOut; }
    const Branches &getOutParams() const { return m_paramsOut; }

    /// Get precondition common for all branches.
    /// \return Precondition.
    const FormulaPtr &getPreCondition() const { return m_pPreCond; }

    /// Set precondition common for all branches.
    /// \param _pCondition Precondition.
    void setPreCondition(const FormulaPtr &_pCondition) { m_pPreCond = _pCondition; }

    /// Get postcondition common for all branches.
    /// \return Postcondition.
    const FormulaPtr &getPostCondition() const { return m_pPostCond; }

    /// Set postcondition common for all branches.
    /// \param _pCondition Postcondition.
    void setPostCondition(const FormulaPtr &_pCondition) { m_pPostCond = _pCondition; }

    /// Check if the predicate is a hyperfunction.
    /// \return True if the predicate has more than one branch or it's branch has
    ///   a handler assigned.
    bool isHyperFunction() const {
        return m_paramsOut.size() > 1 ||
            (m_paramsOut.size() == 1 && m_paramsOut.get(0)->getLabel());
    }

    virtual bool hasFresh() const;
    virtual bool rewrite(const TypePtr &_pOld, const TypePtr &_pNew);

    virtual bool hasParameters() const { return true; }

    virtual bool contains(const TypePtr &_pType) const {
        // TODO: implement.
        return false;
    }

    virtual int compare(const Type &_other) const;
    virtual bool less(const Type &_other) const;

    // TODO implement join/meet/less/etc.

private:
    Params m_paramsIn;
    Branches m_paramsOut;
    FormulaPtr m_pPreCond, m_pPostCond;
};

} // namespace ir

#endif /* TYPES_H_ */
