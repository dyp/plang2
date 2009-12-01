/// \file types.h
/// Internal structures representing types.
///


#ifndef TYPES_H_
#define TYPES_H_

#include "base.h"
#include "expressions.h"

namespace ir {

/// Type with parameters.
/// Anonymous, can be referenced by CNamedReferenceType or CTypeDeclaration.
class CParameterizedType : public CType {
public:
    /// Default constructor.
    CParameterizedType() : m_pActualType(NULL) {}

    /// Destructor.
    virtual ~CParameterizedType() {
        _delete(m_pActualType);
    }

    /// Get type kind.
    /// \returns #Parameterized.
    virtual int getKind() const { return Parameterized; }

    /// Get list of formal type parameters.
    /// \return List of variables.
    CNamedValues & getParams() { return m_params; }

    /// Get actual type.
    /// \return Referenced type.
    CType * getActualType() const { return m_pActualType; }

    /// Set actual type.
    /// \param _pType Referenced type.
    /// \param _bReparent If specified (default) also sets parent of _pType to this node.
    void setActualType(CType * _pType, bool _bReparent = true) {
        _assign(m_pActualType, _pType, _bReparent);
    }

private:
    CNamedValues m_params;
    CType * m_pActualType;
};

class CTypeDeclaration;

/// User-defined type referenced by name.
class CNamedReferenceType : public CType {
public:
    /// Default constructor.
    CNamedReferenceType() : m_pDecl(NULL), m_pVar(NULL) {}

    /// Initialize with type declaration.
    /// \param _pDecl Target type declaration.
    CNamedReferenceType(const CTypeDeclaration * _pDecl) : m_pDecl(_pDecl), m_pVar(NULL) {}

    /// Initialize with variable or type declaration.
    /// \param _pVar Target variable or parameter declaration. Must be of \c type type.
    CNamedReferenceType(const CNamedValue * _pVar) : m_pDecl(NULL), m_pVar(_pVar) {}

    /// Get type kind.
    /// \returns #NamedReference.
    virtual int getKind() const { return NamedReference; }

    /// Get pointer to target type declaration.
    /// \return Target type declaration or NULL if it wasn't declared as a type.
    const CTypeDeclaration * getDeclaration() const { return m_pDecl; }

    /// Set pointer to target type declaration.
    /// \param _pDecl Target type declaration (NULL if it wasn't declared as a type.)
    void setDeclaration(const CTypeDeclaration * _pDecl) { m_pDecl = _pDecl; }

    /// Get pointer to variable or parameter declaration.
    /// \return Target variable or parameter declaration or NULL if it wasn't declared as a variable or a parameter.)
    const CNamedValue * getVariable() const { return m_pVar; }

    /// Set pointer to variable or parameter declaration. Must be of \c type type.
    /// \param _pVar Target variable or parameter declaration (NULL if it wasn't declared as a variable or a parameter.)
    void setVariable(const CNamedValue * _pVar) { m_pVar = _pVar; }

    /// Get list of actual parameters.
    /// \return List of expressions.
    CCollection<CExpression> & getParams() { return m_params; }

private:
    const CTypeDeclaration * m_pDecl;
    const CNamedValue * m_pVar;
    CCollection<CExpression> m_params;
};

/// Struct type.
/// Contains list of fields.
class CStructType : public CType {
public:
    /// Default constructor.
    CStructType() {}

    /// Get type kind.
    /// \returns #Struct.
    virtual int getKind() const { return Struct; }

    /// Get list of struct fields.
    /// \return List of fields.
    CNamedValues & getFields() { return m_fields; }
    const CNamedValues & getFields() const { return m_fields; }

private:
    CNamedValues m_fields;
};

/// Identifier belonging to an enumeration.
class CEnumValue : public CNamedValue {
public:
    /// Default constructor.
    CEnumValue() : m_nOrdinal(-1) {}

    /// Constructor for initializing using name.
    /// \param _strName Identifier.
    /// \param _pType Associated enumeration.
    /// \param _nOrdinal Ordinal corresponding to the value.
    CEnumValue(const std::wstring & _strName, int _nOrdinal = -1, CType * _pEnumType = NULL)
        : CNamedValue(_strName, _pEnumType, false), m_nOrdinal(_nOrdinal) {}

    /// Destructor.
    virtual ~CEnumValue() {}

    /// Get value kind.
    /// \returns #EnumValue.
    virtual int getKind() const { return EnumValue; }

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
class CEnumType : public CType {
public:
    /// Default constructor.
    CEnumType() {}

    /// Get type kind.
    /// \returns #Enum.
    virtual int getKind() const { return Enum; }

    /// Get list of values.
    /// \return List of values.
    CCollection<CEnumValue> & getValues() { return m_values; }

private:
    CCollection<CEnumValue> m_values;
};

/// Union type.
/// Contains list of alternatives.
class CUnionType : public CType {
public:
    /// Default constructor.
    CUnionType() {}

    /// Get type kind.
    /// \returns #Union.
    virtual int getKind() const { return Union; }

    /// Get list of union alternatives.
    /// \return List of alternatives.
    CNamedValues & getAlternatives() { return m_alternatives; }
    const CNamedValues & getAlternatives() const { return m_alternatives; }

private:
    CNamedValues m_alternatives;
};

/// Common base type for arrays, sets, etc.
class CDerivedType : public CType {
public:
    /// Default constructor.
    CDerivedType() : m_pBaseType(NULL) {}

    /// Initialize with base type.
    /// \param _pType Base type.
    /// \param _bReparent If specified (default) also sets parent of _pType to this node.
    CDerivedType(CType * _pType, bool _bReparent = true) : m_pBaseType(NULL) {
        _assign(m_pBaseType, _pType, _bReparent);
    }

    /// Get base type.
    /// \return Base type.
    virtual CType * getBaseType() const { return m_pBaseType; }

    /// Set base type.
    /// \param _pType Base type.
    /// \param _bReparent If specified (default) also sets parent of _pType to this node.
    virtual void setBaseType(CType * _pType, bool _bReparent = true) {
        _assign(m_pBaseType, _pType, _bReparent);
    }

private:
    CType * m_pBaseType;
};

/// Optional type.
/// Values of optional type are either of it's base nype or \c nil.
class COptionalType : public CDerivedType {
public:
    /// Default constructor.
    COptionalType() {}

    /// Initialize with base type.
    /// \param _pType Base type.
    /// \param _bReparent If specified (default) also sets parent of _pType to this node.
    COptionalType(CType * _pType, bool _bReparent = true) : CDerivedType(_pType, _bReparent) {}

    /// Get type kind.
    /// \returns #Optional.
    virtual int getKind() const { return Optional; }
};

/// Sequence type.
class CSeqType : public CDerivedType {
public:
    /// Default constructor.
    CSeqType() {}

    /// Initialize with base type.
    /// \param _pType Base type.
    /// \param _bReparent If specified (default) also sets parent of _pType to this node.
    CSeqType(CType * _pType, bool _bReparent = true) : CDerivedType(_pType, _bReparent) {}

    /// Get type kind.
    /// \returns #Seq.
    virtual int getKind() const { return Seq; }
};

/// Subtype.
class CSubtype : public CType {
public:
    /// Default constructor.
    CSubtype() : m_pParam(NULL), m_pExpression(NULL) {}

    /// Initialize with parameter and expression.
    /// \param _pParam Parameter.
    /// \param _pExpression Boolean expression.
    /// \param _bReparent If specified (default) also sets parent of _pParam and _pExpression to this node.
    CSubtype(CNamedValue * _pParam, CExpression * _pExpression, bool _bReparent = true) : m_pParam(NULL), m_pExpression(NULL) {
        _assign(m_pParam, _pParam, _bReparent);
        _assign(m_pExpression, _pExpression, _bReparent);
    }

    /// Destructor.
    virtual ~CSubtype() {
        _delete(m_pParam);
        _delete(m_pExpression);
    }

    /// Get type kind.
    /// \returns #Subtype.
    virtual int getKind() const { return Subtype; }

    /// Get parameter declaration.
    /// \return Subtype parameter variable.
    CNamedValue * getParam() const { return m_pParam; }

    /// Set parameter declaration.
    /// \param _pParam Subtype parameter variable.
    /// \param _bReparent If specified (default) also sets parent of _pParam to this node.
    void setParam(CNamedValue * _pParam, bool _bReparent = true) {
        _assign(m_pParam, _pParam, _bReparent);
    }

    /// Get logical expression which should evaluate to \c true for all values of type.
    /// \return Boolean expression.
    CExpression * getExpression() const {
        return m_pExpression;
    }

    /// Set logical expression which should evaluate to \c true for all values of type.
    /// \param _pExpression Boolean expression.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setExpression(CExpression * _pExpression, bool _bReparent = true) {
        _assign(m_pExpression, _pExpression, _bReparent);
    }

private:
    CNamedValue * m_pParam;
    CExpression * m_pExpression;
};

/// Range type.
/// A subtype defined as a MIN..MAX continuous range. Should be roughly equivalent
/// to types defined like:
/// \code subtype (T t : t >= tMin & t <= tMax) \endcode
class CRange : public CType {
public:
    /// Default constructor.
    CRange() : m_pMin(NULL), m_pMax(NULL) {}

    /// Initialize with bounds.
    /// \param _pMin Expression corresponding to the lower bound of the type.
    /// \param _pMax Expression corresponding to the upper bound of the type.
    /// \param _bReparent If specified (default) also sets parent of _pMin and _pMax to this node.
    CRange(CExpression * _pMin = NULL, CExpression * _pMax = NULL, bool _bReparent = true)
        : m_pMin(NULL), m_pMax(NULL)
    {
        _assign(m_pMin, _pMin, _bReparent);
        _assign(m_pMax, _pMax, _bReparent);
    }

    /// Destructor.
    virtual ~CRange() {
        _delete(m_pMin);
        _delete(m_pMax);
    }

    /// Get type kind.
    /// \returns #Range.
    virtual int getKind() const { return Range; }

    /// Get minimal value contained in the type.
    /// \return Expression corresponding to the lower bound of the type.
    CExpression * getMin() const { return m_pMin; }

    /// Set minimal value contained in the type.
    /// \param _pExpression Expression corresponding to the lower bound of the type.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setMin(CExpression * _pExpression, bool _bReparent = true) {
        _assign(m_pMin, _pExpression, _bReparent);
    }

    /// Get maximal value contained in the type.
    /// \return Expression corresponding to the upper bound of the type.
    CExpression * getMax() const { return m_pMax; }

    /// Set maximal value contained in the type.
    /// \param _pExpression Expression corresponding to the upper bound of the type.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setMax(CExpression * _pExpression, bool _bReparent = true) {
        _assign(m_pMax, _pExpression, _bReparent);
    }

private:
    CExpression * m_pMin, * m_pMax;
};

/// Array type.
class CArrayType : public CDerivedType {
public:
    /// Default constructor.
    CArrayType() {}

    /// Initialize with base type.
    /// \param _pType Base type.
    /// \param _bReparent If specified (default) also sets parent of _pType to this node.
    CArrayType(CType * _pType, bool _bReparent = true) : CDerivedType(_pType, _bReparent) {}

    /// Get type kind.
    /// \returns #Array.
    virtual int getKind() const { return Array; }

    /// Get array dimensions as list of ranges.
    /// \return List of ranges.
    CCollection<CRange> & getDimensions() { return m_dimensions; }

private:
    CCollection<CRange> m_dimensions;
};

/// Set type.
class CSetType : public CDerivedType {
public:
    /// Default constructor.
    CSetType() {}

    /// Initialize with base type.
    /// \param _pType Base type.
    /// \param _bReparent If specified (default) also sets parent of _pType to this node.
    CSetType(CType * _pType, bool _bReparent = true) : CDerivedType(_pType, _bReparent) {}

    /// Get type kind.
    /// \returns #Set.
    virtual int getKind() const { return Set; }
};

/// Map type.
/// Base type (defined in CDerivedType) is the type of values contained in a map.
class CMapType : public CDerivedType {
public:
    /// Default constructor.
    CMapType() : m_pIndexType(NULL) {}

    /// Initialize with base type.
    /// \param _pIndexType Index type.
    /// \param _pBaseType Base type.
    /// \param _bReparent If specified (default) also sets parent of _pIndexType and _pBaseType to this node.
    CMapType(CType * _pIndexType, CType * _pBaseType, bool _bReparent = true)
        : CDerivedType(_pBaseType, _bReparent), m_pIndexType(NULL)
    {
        _assign(m_pIndexType, _pIndexType, _bReparent);
    }

    /// Get type kind.
    /// \returns #Map.
    virtual int getKind() const { return Map; }

    /// Get index type.
    /// \return Index type.
    CType * getIndexType() const { return m_pIndexType; }

    /// Set index type.
    /// \param _pType Index type.
    /// \param _bReparent If specified (default) also sets parent of _pType to this node.
    void setIndexType(CType * _pType, bool _bReparent = true) {
        _assign(m_pIndexType, _pType, _bReparent);
    }

private:
    CType * m_pIndexType;
};

/// List type.
class CListType : public CDerivedType {
public:
    /// Default constructor.
    CListType() {}

    /// Initialize with base type.
    /// \param _pType Base type.
    /// \param _bReparent If specified (default) also sets parent of _pType to this node.
    CListType(CType * _pType, bool _bReparent = true) : CDerivedType(_pType, _bReparent) {}

    /// Get type kind.
    /// \returns #List.
    virtual int getKind() const { return List; }
};

/// Predicate type.
class CPredicateType : public CType {
public:
    /// Default constructor.
    CPredicateType() : m_pPreCond(NULL), m_pPostCond(NULL) {}

    /// Destructor.
    virtual ~CPredicateType() {
        _delete(m_pPreCond);
        _delete(m_pPostCond);
    }

    /// Get type kind.
    /// \returns #Predicate.
    virtual int getKind() const { return Predicate; }

    /// Get list of formal input parameters.
    /// \return List of parameters.
    CParams & getInParams() { return m_paramsIn; }
    const CParams & getInParams() const { return m_paramsIn; }

    /// Get list of output branches. Each branch can contain a list of parameters,
    /// a precondition and a postcondition.
    /// \return List of branches.
    CBranches & getOutParams() { return m_paramsOut; }
    const CBranches & getOutParams() const { return m_paramsOut; }

    /// Get precondition common for all branches.
    /// \return Precondition.
    CFormula * getPreCondition() const { return m_pPreCond; }

    /// Set precondition common for all branches.
    /// \param _pCondition Precondition.
    /// \param _bReparent If specified (default) also sets parent of _pCondition to this node.
    void setPreCondition(CFormula * _pCondition, bool _bReparent = true) {
        _assign(m_pPreCond, _pCondition, _bReparent);
    }

    /// Get postcondition common for all branches.
    /// \return Postcondition.
    CFormula * getPostCondition() const { return m_pPostCond; }

    /// Set postcondition common for all branches.
    /// \param _pCondition Postcondition.
    /// \param _bReparent If specified (default) also sets parent of _pCondition to this node.
    void setPostCondition(CFormula * _pCondition, bool _bReparent = true) {
        _assign(m_pPostCond, _pCondition, _bReparent);
    }

    /// Check if the predicate is a hyperfunction.
    /// \return True if the predicate has more than one branch or it's branch has
    ///   a handler assigned.
    bool isHyperFunction() const {
        return m_paramsOut.size() > 1 ||
            (m_paramsOut.size() == 1 && m_paramsOut.get(0)->getLabel() != NULL);
    }

private:
    CParams m_paramsIn;
    CBranches m_paramsOut;
    CFormula * m_pPreCond, * m_pPostCond;
};

} // namespace ir

#endif /* TYPES_H_ */
