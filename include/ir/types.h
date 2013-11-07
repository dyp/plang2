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

    virtual bool contains(const TypePtr &_pType) const { return m_pActualType->contains(_pType); }

    virtual bool hasParameters() const { return true; }

    virtual NodePtr clone(Cloner &_cloner) const {
        ParameterizedTypePtr pCopy = NEW_CLONE(this, _cloner, ParameterizedType(_cloner.get(getActualType())));
        pCopy->getParams().appendClones(getParams(), _cloner);
        return pCopy;
    }

    virtual int getMonotonicity(const Type &_var) const;

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
    const Collection<Expression> &getArgs() const { return m_args; }

    virtual bool less(const Type &_other) const;
    virtual bool equals(const Type &_other) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        NamedReferenceTypePtr pCopy = NEW_CLONE(this, _cloner, NamedReferenceType(_cloner.get(getDeclaration(), true)));
        pCopy->getArgs().appendClones(getArgs(), _cloner);
        return pCopy;
    }

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

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, TypeType(_cloner.get(getDeclaration())));
    }

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
    virtual bool rewrite(const TypePtr &_pOld, const TypePtr &_pNew, bool _bRewriteFlags = true);
    virtual int compare(const Type &_other) const;
    virtual TypePtr getMeet(Type &_other);
    virtual TypePtr getJoin(Type &_other);
    virtual bool less(const Type &_other) const;
    virtual bool contains(const TypePtr &_pType) const;
    virtual int getMonotonicity(const Type &_var) const;

    bool empty() const;

    virtual NodePtr clone(Cloner &_cloner) const {
        StructTypePtr pCopy = NEW_CLONE(this, _cloner, StructType());
        pCopy->getNamesOrd().appendClones(getNamesOrd(), _cloner);
        pCopy->getTypesOrd().appendClones(getTypesOrd(), _cloner);
        pCopy->getNamesSet().appendClones(getNamesSet(), _cloner);
        return pCopy;
    }

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

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, EnumValue(getName(), getOrdinal(), _cloner.get(getType())));
    }

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

    virtual NodePtr clone(Cloner &_cloner) const {
        EnumTypePtr pCopy = NEW_CLONE(this, _cloner, EnumType());
        pCopy->getValues().appendClones(getValues(), _cloner);
        return pCopy;
    }

private:
    Collection<EnumValue> m_values;
};

class UnionType;

class UnionConstructorDeclaration : public Node {
public:
    UnionConstructorDeclaration(const std::wstring &_strName, size_t _ord = 0, const UnionTypePtr &_pUnion = NULL) :
        m_strName(_strName), m_pUnion(_pUnion), m_ord(_ord) {}

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

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        UnionConstructorDeclarationPtr pCopy = NEW_CLONE(this, _cloner, UnionConstructorDeclaration(getName(), getOrdinal(),
                _cloner.get(getUnion())));
        pCopy->getFields().appendClones(getFields(), _cloner);
        return pCopy;
    }

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
    virtual bool rewrite(const TypePtr &_pOld, const TypePtr &_pNew, bool _bRewriteFlags = true);
    virtual TypePtr getMeet(Type &_other);
    virtual TypePtr getJoin(Type &_other);
    virtual bool less(const Type &_other) const;
    virtual int getMonotonicity(const Type &_var) const;

    virtual bool contains(const TypePtr &_pType) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        UnionTypePtr pCopy = NEW_CLONE(this, _cloner, UnionType());
        pCopy->getConstructors().appendClones(getConstructors(), _cloner);
        return pCopy;
    }

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

    virtual bool contains(const TypePtr &_pType) const {
        return *m_pBaseType == *_pType || m_pBaseType->contains(_pType);
    }

private:
    TypePtr m_pBaseType;
};

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

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, OptionalType(_cloner.get(getBaseType())));
    }
};

/// Sequence type.
class SeqType : public DerivedType {
public:
    /// Initialize with base type.
    /// \param _pType Base type.
    SeqType(const TypePtr &_pType = NULL) : DerivedType(_pType) {}

    /// Get type kind.
    /// \returns #Seq.
    virtual int getKind() const { return SEQ; }

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, SeqType(_cloner.get(getBaseType())));
    }
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

    RangePtr asRange() const;

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, Subtype(_cloner.get(getParam()), _cloner.get(getExpression())));
    }

    virtual bool less(const Type &_other) const;
    virtual TypePtr getMeet(Type &_other);
    virtual TypePtr getJoin(Type &_other);
    virtual bool rewrite(const TypePtr &_pOld, const TypePtr &_pNew, bool _bRewriteFlags = true);
    virtual int compare(const Type &_other) const;
    virtual int getMonotonicity(const Type &_var) const;

    virtual bool hasFresh() const {
        return m_pParam && m_pParam->getType()->hasFresh();
    }

    virtual bool contains(const TypePtr &_pType) const {
        return m_pParam && ((*m_pParam->getType() == *_pType) || m_pParam->getType()->contains(_pType));
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

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, Range(_cloner.get(getMin()), _cloner.get(getMax())));
    }

private:
    ExpressionPtr m_pMin, m_pMax;
};

/// Array type.
class ArrayType : public DerivedType {
public:
    /// Initialize with base type.
    /// \param _pType Base type.
    ArrayType(const TypePtr &_pType = NULL, const TypePtr &_pDimensionType = NULL) :
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
            ? getBaseType().as<ArrayType>()->getRootType()
            : getBaseType();
    }

    /// Get array dimensions as list of types.
    /// \return List of ranges.
    void getDimensions(std::list<TypePtr> &_dimensions) const;

    size_t getDimensionsCount() const {
        return getBaseType()->getKind() == ARRAY
            ? getBaseType().as<ArrayType>()->getDimensionsCount() + 1
            : 1;
    }

    virtual bool less(const Type &_other) const;
    virtual TypePtr getMeet(Type &_other);
    virtual TypePtr getJoin(Type &_other);
    virtual bool rewrite(const TypePtr &_pOld, const TypePtr &_pNew, bool _bRewriteFlags = true);
    virtual int compare(const Type &_other) const;
    virtual int getMonotonicity(const Type &_var) const;

    virtual bool hasFresh() const {
        return getBaseType()->hasFresh();
    }

    virtual bool contains(const TypePtr &_pType) const {
        return *getBaseType() == *_pType
            || getBaseType()->contains(_pType)
            || *getDimensionType() == *_pType
            || getDimensionType()->contains(_pType);
    }

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, ArrayType(_cloner.get(getBaseType()), _cloner.get(getDimensionType())));
    }

private:
    TypePtr m_pDimensionType;
};

/// Set type.
class SetType : public DerivedType {
public:
    /// Initialize with base type.
    /// \param _pType Base type.
    SetType(const TypePtr &_pType = NULL) : DerivedType(_pType) {}

    /// Get type kind.
    /// \returns #Set.
    virtual int getKind() const { return SET; }

    virtual TypePtr getMeet(Type &_other);
    virtual TypePtr getJoin(Type &_other);

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, SetType(_cloner.get(getBaseType())));
    }
};

/// Map type.
/// Base type (defined in DerivedType) is the type of values contained in a map.
class MapType : public DerivedType {
public:
    /// Initialize with base type.
    /// \param _pIndexType Index type.
    /// \param _pBaseType Base type.
    MapType(const TypePtr &_pIndexType = NULL, const TypePtr &_pBaseType = NULL)
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
    virtual bool rewrite(const TypePtr &_pOld, const TypePtr &_pNew, bool _bRewriteFlags = true);
    virtual int compare(const Type &_other) const;
    virtual bool less(const Type &_other) const;
    virtual TypePtr getJoin(Type &_other);
    virtual TypePtr getMeet(Type &_other);
    virtual int getMonotonicity(const Type &_var) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, MapType(_cloner.get(getIndexType()), _cloner.get(getBaseType())));
    }

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

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, ListType(_cloner.get(getBaseType())));
    }
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

    virtual bool contains(const TypePtr &_pType) const;

    virtual int compare(const Type &_other) const;
    virtual bool less(const Type &_other) const;
    virtual TypePtr getJoin(Type &_other);
    virtual TypePtr getMeet(Type &_other);
    virtual int getMonotonicity(const Type &_var) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        PredicateTypePtr pCopy = NEW_CLONE(this, _cloner, PredicateType(_cloner.get(getPreCondition()), _cloner.get(getPostCondition())));
        pCopy->getInParams().appendClones(getInParams(), _cloner);
        pCopy->getOutParams().appendClones(getOutParams(), _cloner);
        return pCopy;
    }

private:
    Params m_paramsIn;
    Branches m_paramsOut;
    FormulaPtr m_pPreCond, m_pPostCond;
};

} // namespace ir

#endif /* TYPES_H_ */
