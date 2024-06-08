/// \file types.h
/// Internal structures representing types.
///


#ifndef TYPES_H_
#define TYPES_H_

#include <list>
#include "base.h"
#include "expressions.h"

#include <assert.h>

namespace ir {

/// Type with parameters.
/// Anonymous, can be referenced by NamedReferenceType or TypeDeclaration.
class ParameterizedType : public Type {
public:
    /// Default constructor.
    /// \param _pActualType Referenced type.
    ParameterizedType(const TypePtr &_pActualType = NULL) : m_pActualType(_pActualType) {}

    /// Get type kind.
    /// \returns #Parameterized.
    virtual int getKind() const { return PARAMETERIZED; }

    /// Get list of formal type parameters.
    /// \return List of variables.
    NamedValues &getParams() { return m_params; }
    const NamedValues &getParams() const { return m_params; }

    /// Get actual type.
    /// \return Referenced type.
    const TypePtr &getActualType() const { return m_pActualType; }

    /// Set actual type.
    /// \param _pType Referenced type.
    void setActualType(const TypePtr &_pType) { m_pActualType = _pType; }

    bool contains(const Type &_type) const override { return m_pActualType->contains(_type); }

    virtual bool hasParameters() const { return true; }

    NodePtr clone(Cloner &_cloner) const override;

    int getMonotonicity(const Type &_var) const override;

private:
    NamedValues m_params;
    TypePtr m_pActualType;
};

class TypeDeclaration;

/// User-defined type referenced by name.
class NamedReferenceType : public Type {
public:
    /// Default constructor.
    /// \param _pDeclaration Target type declaration.
    NamedReferenceType(const TypeDeclarationPtr &_pDeclaration = NULL) : m_pDecl(_pDeclaration) {}

    /// Get type kind.
    /// \returns #NamedReference.
    int getKind() const override { return NAMED_REFERENCE; }

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
    const Collection<Expression> &getArgs() const { return m_args; }

    bool rewrite(const TypePtr &_pOld, const TypePtr &_pNew, bool _bRewriteFlags = true) override;
    bool less(const Type &_other) const override;
    bool equals(const Node &_other) const override;

    NodePtr clone(Cloner &_cloner) const override;

private:
    Collection<Expression> m_args;
    TypeDeclarationPtr m_pDecl;
};

/// Parameter type. Used in a predicate or type declaration only.
class TypeType : public Type {
public:
    /// Default constructor.
    /// \param _pDeclaration Target type declaration.
    TypeType(const TypeDeclarationPtr &_pDeclaration = NULL);

    /// Get type kind.
    /// \returns #Type.
    virtual int getKind() const { return TYPE; }

    const TypeDeclarationPtr &getDeclaration() const { return m_pDecl; }

    void setDeclaration(const TypeDeclarationPtr &_pDecl) { m_pDecl = _pDecl; }

    virtual bool rewrite(const TypePtr &_pOld, const TypePtr &_pNew, bool _bRewriteFlags = true);
    virtual int compare(const Type &_other) const;
    virtual bool less(const Type &_other) const;

    NodePtr clone(Cloner &_cloner) const override;

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
    const NamedValuesPtr &getNamesOrd() const { return m_fields[0]; }

    // Ordered fields known by type.
    const NamedValuesPtr &getTypesOrd() const { return m_fields[1]; }

    // Unordered fields known by name.
    const NamedValuesPtr &getNamesSet() const { return m_fields[2]; }

    /// All fields. Guaranteed to have 3 elements.
    /// \return List of fields.
    NamedValuesPtr *getAllFields() { return m_fields; }
    const NamedValuesPtr *getAllFields() const { return m_fields; }

    size_t size() const { return m_fields[0]->size() + m_fields[1]->size() + m_fields[2]->size(); }

    NamedValuesPtr mergeFields() const;

    virtual bool hasFresh() const;
    virtual bool rewrite(const TypePtr &_pOld, const TypePtr &_pNew, bool _bRewriteFlags = true);
    virtual int compare(const Type &_other) const;
    virtual TypePtr getMeet(const TypePtr &_other);
    virtual TypePtr getJoin(const TypePtr &_other);
    virtual bool less(const Type &_other) const;
    bool contains(const Type &_type) const override;
    virtual int getMonotonicity(const Type &_var) const;

    bool empty() const;

    NodePtr clone(Cloner &_cloner) const override;

private:
    NamedValuesPtr m_fields[3] = {
        std::make_shared<NamedValues>(),
        std::make_shared<NamedValues>(),
        std::make_shared<NamedValues>()
    };
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

    NodePtr clone(Cloner &_cloner) const override;

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
    const Collection<EnumValue> &getValues() const { return m_values; }

    NodePtr clone(Cloner &_cloner) const override;

private:
    Collection<EnumValue> m_values;
};

class UnionType;

class UnionConstructorDeclaration : public Node {
public:
    UnionConstructorDeclaration(const std::wstring &_strName, size_t _ord = 0,
        const UnionTypePtr &_pUnion = NULL, const TypePtr _pFields = NULL) :
        m_strName(_strName), m_pUnion(_pUnion), m_ord(_ord)
    {
        m_pFields = !_pFields ? std::make_shared<StructType>()->as<Type>() : _pFields;
    }

    virtual int getNodeKind() const { return Node::UNION_CONSTRUCTOR_DECLARATION; }

    /// Get constructor's type. Only Fresh and Struct types are allowed.
    /// \return Type of constructor.
    TypePtr &getFields();
    const TypePtr &getFields() const;

    /// Get constructor's Struct type, if valid.
    /// \return Struct type of constructor
    StructTypePtr getStructFields() const;

    /// Set constructor's type. Only Fresh and Struct types are allowed.
    /// \param _pField Field type.
    void setFields(const TypePtr& _pFields);

    const UnionTypePtr &getUnion() const { return m_pUnion; }
    void setUnion(const UnionTypePtr &_pUnion) { m_pUnion = _pUnion; }

    const std::wstring &getName() const { return m_strName; }
    void setName(const std::wstring &_strName) { m_strName = _strName; }

    size_t getOrdinal() const { return m_ord; }
    void setOrdinal(size_t _ord) { m_ord = _ord; }

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;

    NodePtr clone(Cloner &_cloner) const override;

private:
    std::wstring m_strName;
    UnionTypePtr m_pUnion;
    TypePtr m_pFields;
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

    virtual int compare(const Type &_other) const;

    virtual bool hasFresh() const;
    virtual bool rewrite(const TypePtr &_pOld, const TypePtr &_pNew, bool _bRewriteFlags = true);
    virtual TypePtr getMeet(const TypePtr &_other);
    virtual TypePtr getJoin(const TypePtr &_other);
    virtual bool less(const Type &_other) const;
    virtual int getMonotonicity(const Type &_var) const;

    bool contains(const Type &_type) const override;

    NodePtr clone(Cloner &_cloner) const override;

private:
    UnionConstructorDeclarations m_constructors;
};

/// Common base type for arrays, sets, etc.
class DerivedType : public Type {
public:
    /// Initialize with base type.
    /// \param _pType Base type.
    DerivedType(const TypePtr &_pType = NULL) : m_pBaseType(_pType) {}

    /// Get base type.
    /// \return Base type.
    virtual TypePtr getBaseType() const { return m_pBaseType; }

    /// Set base type.
    /// \param _pType Base type.
    virtual void setBaseType(const TypePtr &_pType) { m_pBaseType = _pType; }

    virtual bool hasParameters() const { return true; }

    virtual bool hasFresh() const { return m_pBaseType->hasFresh(); }
    virtual bool rewrite(const TypePtr &_pOld, const TypePtr &_pNew, bool _bRewriteFlags = true);
    virtual int compare(const Type &_other) const;
    virtual bool less(const Type &_other) const;
    virtual bool rewriteFlags(int _flags) { return m_pBaseType->rewriteFlags(_flags); }
    virtual int getMonotonicity(const Type &_var) const;

    bool contains(const Type &_type) const override {
        return *m_pBaseType == _type || m_pBaseType->contains(_type);
    }

private:
    TypePtr m_pBaseType;
};

using OptionalTypePtr = std::shared_ptr<class OptionalType>;

/// Optional type.
/// Values of optional type are either of it's base type or \c nil.
class OptionalType : public DerivedType {
public:
    /// Initialize with base type.
    /// \param _pType Base type.
    OptionalType(const TypePtr &_pType = NULL) : DerivedType(_pType) {}

    /// Get type kind.
    /// \returns #Optional.
    virtual int getKind() const { return OPTIONAL; }

    NodePtr clone(Cloner &_cloner) const override;
};

using SeqTypePtr = std::shared_ptr<class SeqType>;

/// Sequence type.
class SeqType : public DerivedType {
public:
    /// Initialize with base type.
    /// \param _pType Base type.
    SeqType(const TypePtr &_pType = NULL) : DerivedType(_pType) {}

    /// Get type kind.
    /// \returns #Seq.
    virtual int getKind() const { return SEQ; }

    NodePtr clone(Cloner &_cloner) const override;
};

/// Subtype.
class Subtype : public Type {
public:
    /// Initialize with parameter and expression.
    /// \param _pParam Parameter.
    /// \param _pExpression Boolean expression.
    Subtype(const NamedValuePtr &_pParam = NULL, const ExpressionPtr &_pExpression = NULL)
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

    RangePtr asRange();

    NodePtr clone(Cloner &_cloner) const override;

    virtual bool less(const Type &_other) const;
    virtual TypePtr getMeet(const TypePtr &_other);
    virtual TypePtr getJoin(const TypePtr &_other);
    virtual bool rewrite(const TypePtr &_pOld, const TypePtr &_pNew, bool _bRewriteFlags = true);
    virtual int compare(const Type &_other) const;
    virtual int getMonotonicity(const Type &_var) const;

    virtual bool hasFresh() const {
        return m_pParam && m_pParam->getType()->hasFresh();
    }

    bool contains(const Type &_type) const override {
        return m_pParam && ((*m_pParam->getType() == _type) || m_pParam->getType()->contains(_type));
    }

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
    Range() {}
    Range(const ExpressionPtr &_pMin)
        : m_pMin(_pMin) {}
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

    SubtypePtr asSubtype() const;

    NodePtr clone(Cloner &_cloner) const override;

private:
    ExpressionPtr m_pMin, m_pMax;
};

/// Array type.
class ArrayType : public DerivedType {
public:
    /// Initialize with base type.
    /// \param _pType Base type.
    ArrayType() {}

    ArrayType(const TypePtr &_pType) :
        DerivedType(_pType)
    {}
    ArrayType(const TypePtr &_pType, const TypePtr &_pDimensionType) :
        DerivedType(_pType), m_pDimensionType(_pDimensionType)
    {}

    /// Get type kind.
    /// \returns #Array.
    virtual int getKind() const { return ARRAY; }

    TypePtr &getDimensionType() { return m_pDimensionType; }
    const TypePtr &getDimensionType() const { return m_pDimensionType; }

    void setDimensionType(const TypePtr &_pType) { m_pDimensionType = _pType; }

    TypePtr getRootType() const {
        return getBaseType()->getKind() == ARRAY
            ? getBaseType()->as<ArrayType>()->getRootType()
            : getBaseType();
    }

    /// Get array dimensions as list of types.
    /// \return List of ranges.
    void getDimensions(Collection<Type> &_dimensions) const;

    size_t getDimensionsCount() const {
        return getBaseType()->getKind() == ARRAY
            ? getBaseType()->as<ArrayType>()->getDimensionsCount() + 1
            : 1;
    }

    virtual bool less(const Type &_other) const;
    virtual TypePtr getMeet(const TypePtr &_other);
    virtual TypePtr getJoin(const TypePtr &_other);
    virtual bool rewrite(const TypePtr &_pOld, const TypePtr &_pNew, bool _bRewriteFlags = true);
    virtual int compare(const Type &_other) const;
    virtual int getMonotonicity(const Type &_var) const;

    virtual bool hasFresh() const {
        return getBaseType()->hasFresh();
    }

    bool contains(const Type &_type) const override {
        return *getBaseType() == _type
            || getBaseType()->contains(_type)
            || *getDimensionType() == _type
            || getDimensionType()->contains(_type);
    }

    NodePtr clone(Cloner &_cloner) const override;

private:
    TypePtr m_pDimensionType;
};

/// Set type.
class SetType : public DerivedType {
public:
    SetType() {}
    /// Initialize with base type.
    /// \param _pType Base type.
    SetType(const TypePtr &_pType) : DerivedType(_pType) {}

    /// Get type kind.
    /// \returns #Set.
    virtual int getKind() const { return SET; }

    virtual TypePtr getMeet(const TypePtr &_other);
    virtual TypePtr getJoin(const TypePtr &_other);

    NodePtr clone(Cloner &_cloner) const override;
};

/// Map type.
/// Base type (defined in DerivedType) is the type of values contained in a map.
class MapType : public DerivedType {
public:
    /// Initialize with base type.
    /// \param _pIndexType Index type.
    /// \param _pBaseType Base type.
    MapType(const TypePtr &_pIndexType, const TypePtr &_pBaseType)
        : DerivedType(_pBaseType), m_pIndexType(_pIndexType) {}
    MapType(const TypePtr &_pIndexType)
        : DerivedType(), m_pIndexType(_pIndexType) {}
    MapType()
        : DerivedType() {}

    /// Get type kind.
    /// \returns #Map.
    virtual int getKind() const { return MAP; }

    /// Get index type.
    /// \return Index type.
    const TypePtr &getIndexType() const { return m_pIndexType; }

    /// Set index type.
    /// \param _pType Index type.
    void setIndexType(const TypePtr &_pType) { m_pIndexType = _pType; }

    bool contains(const Type &_type) const override {
        return DerivedType::contains(_type) || *m_pIndexType == _type || m_pIndexType->contains(_type);
    }

    virtual bool rewriteFlags(int _flags) {
        const bool b = DerivedType::rewriteFlags(_flags);
        return m_pIndexType->rewriteFlags(_flags) || b;
    }

    virtual bool hasFresh() const { return DerivedType::hasFresh() || m_pIndexType->hasFresh(); }
    virtual bool rewrite(const TypePtr &_pOld, const TypePtr &_pNew, bool _bRewriteFlags = true);
    virtual int compare(const Type &_other) const;
    virtual bool less(const Type &_other) const;
    virtual TypePtr getJoin(const TypePtr &_other);
    virtual TypePtr getMeet(const TypePtr &_other);
    virtual int getMonotonicity(const Type &_var) const;

    NodePtr clone(Cloner &_cloner) const override;

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

    virtual TypePtr getJoin(const TypePtr &_other);
    virtual TypePtr getMeet(const TypePtr &_other);

    NodePtr clone(Cloner &_cloner) const override;
};

/// Reference type.
class RefType : public DerivedType {
public:
    /// Initialize with base type.
    /// \param _pType Base type.
    RefType(const TypePtr &_pType = NULL) : DerivedType(_pType) {}

    /// Get type kind.
    /// \returns #Reference.
    virtual int getKind() const { return REFERENCE; }

    virtual TypePtr getMeet(const TypePtr &_other);
    virtual TypePtr getJoin(const TypePtr &_other);

    NodePtr clone(Cloner &_cloner) const override;
};

/// Predicate type.
class PredicateType : public Type {
public:
    /// Default constructor.
    /// \param _pPreCond Precondition.
    /// \param _pPostCond Postcondition.
    PredicateType(const FormulaPtr &_pPreCond = NULL, const FormulaPtr &_pPostCond = NULL) :
        m_pPreCond(_pPreCond), m_pPostCond(_pPostCond) {}

    /// Make predicate type from anonymous predicate.
    /// \param _pPred Predicate.
    PredicateType(const AnonymousPredicatePtr &_pPred) {
        Cloner cloner;
        m_paramsIn.appendClones(_pPred->getInParams(), cloner);
        m_paramsOut.appendClones(_pPred->getOutParams(), cloner);
        m_pPreCond = cloner.get(_pPred->getPreCondition());
        m_pPostCond = cloner.get(_pPred->getPostCondition());
    }

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
    virtual bool rewrite(const TypePtr &_pOld, const TypePtr &_pNew, bool _bRewriteFlags = true);

    virtual bool hasParameters() const { return true; }

    bool contains(const Type &_type) const override;

    virtual int compare(const Type &_other) const;
    virtual bool less(const Type &_other) const;
    virtual TypePtr getJoin(const TypePtr &_other);
    virtual TypePtr getMeet(const TypePtr &_other);
    virtual int getMonotonicity(const Type &_var) const;

    NodePtr clone(Cloner &_cloner) const override;

private:
    Params m_paramsIn;
    Branches m_paramsOut;
    FormulaPtr m_pPreCond, m_pPostCond;
};

} // namespace ir

#endif /* TYPES_H_ */
