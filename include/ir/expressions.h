/// \file expressions.h
/// Internal structures representing expressions.
///

#ifndef EXPRESSIONS_H_
#define EXPRESSIONS_H_

#include "numbers.h"
#include "base.h"

namespace ir {

class CType;

/// Overflow handling description.
class COverflow {
public:
    enum {
        Saturate,
        Strict,
        Return,
        Wrap,
    };

    COverflow() : m_overflow(Wrap), m_pLabel(NULL) {}

    /// Get overflow handling strategy.
    /// \return Overflow handling strategy.
    int get() const { return m_overflow; }

    /// Set overflow handling strategy.
    /// \param _overflow Overflow handling strategy.
    /// \param _pLabel return label (optional).
    void set(int _overflow, const CLabel * _pLabel = NULL) { m_overflow = _overflow; m_pLabel = _pLabel; }

    /// Set overflow handling strategy.
    /// \param _overflow Overflow handling strategy and (optionally) return label.
    void set(const COverflow & _other) { set(_other.get(), _other.getLabel()); }

    /// \return Pointer to CLabel object (assuming type is IntOverflow or
    ///     RealOverflow and overflow handling strategy is Label.)
    const CLabel * getLabel() const { return m_pLabel; }

private:
    int m_overflow;
    const CLabel * m_pLabel;
};

/// Virtual ancestor of all expressions.
class CExpression : public CNode {
public:
    /// Kind of the expression.
    enum {
        /// Numeric, character, string or unit literal.
        /// The object can be cast to CLiteral.
        Literal,
        /// Variable or parameter.
        /// The object can be cast to CVariableReference.
        Var,
        /// Predicate reference.
        /// The object can be cast to CPredicateReference.
        Predicate,
        /// Unary expression.
        /// The object can be cast to CUnary.
        Unary,
        /// Binary expression.
        /// The object can be cast to CBinary.
        Binary,
        /// Ternary expression.
        /// The object can be cast to CTernary.
        Ternary,
        /// Type expression.
        /// The object can be cast to CTypeExpr.
        Type,
        /// Struct field, or subscript of array, sequence, list or map.
        /// The object can be cast to CComponent.
        Component,
        /// Function call.
        /// The object can be cast to CFunctionCall.
        FunctionCall,
        /// Formula call.
        /// The object can be cast to CFunctionCall.
        FormulaCall,
        /// Anonymous function.
        /// The object can be cast to CLambda.
        Lambda,
        /// Partially applied predicate.
        /// The object can be cast to CBinder.
        Binder,
        /// Precondition or postcondition formula.
        /// The object can be cast to CFormula.
        Formula,
        /// Struct, array, sequence, set, list or map constructor.
        /// The object can be cast to CConstructor.
        Constructor,
        /// Cast expression.
        /// The object can be cast to CCastExpr.
        Cast,
    };

    /// Default constructor.
    CExpression () : m_pType(NULL) {}

    /// Destructor.
    virtual ~CExpression() { _delete(m_pType); }

    /// Get expression kind.
    /// \returns Expression kind.
    virtual int getKind() const = 0;

    /// Get type of the expression.
    /// \returns Type associated with expression.
    virtual CType * getType() const { return m_pType; }

    /// Set type of the expression.
    /// \param _pType Type associated with expression.
    /// \param _bReparent If specified (default) also sets parent of _pType to this node.
    void setType(CType * _pType, bool _bReparent = true) {
        _assign(m_pType, _pType, _bReparent);
    }

private:
    CType * m_pType;
};

/// Representation of nil and numeric, character and string literals.
class CLiteral : public CExpression {
public:
    /// Kind of the literal.
    enum {
        /// Unit literal (\c nil or empty constructor).
        Unit,
        /// Numeric literal. Use getNumber() and setNumber() accessors.
        Number,
        /// Boolean literal. Use getBool() and setBool() accessors.
        Bool,
        /// Character literal. Use getChar() and setChar() accessors.
        Char,
        /// String literal. Use getString() and setString() accessors.
        String
    };

    /// Default constructor.
    CLiteral () : m_literalKind(Unit) {}

    /// Initialize the literal with numeric value (sets kind to #Number).
    /// \param _number Value.
    CLiteral (const CNumber & _number) : m_literalKind(Number), m_number(_number) {}

    /// Initialize the literal with boolean value (sets kind to #Bool).
    /// \param _number Value.
    CLiteral (bool _b) : m_literalKind(Bool), m_bool(_b) {}

    /// Initialize the literal with character value (sets kind to #Char).
    /// \param _c Value.
    CLiteral (wchar_t _c) : m_literalKind(Char), m_char(_c) {}

    /// Initialize the literal with string value (sets kind to #String).
    /// \param _str Value.
    CLiteral (const std::wstring & _str) : m_literalKind(String), m_string(_str) {}

    /// Get expression kind.
    /// \return #Literal.
    virtual int getKind() const { return Literal; }

    /// Get literal kind.
    /// \return Literal kind (#Unit, #Number, #Char or #String).
    int getLiteralKind() const { return m_literalKind; }

    /// Set kind to #Unit.
    void setUnit() { m_literalKind = Unit; }

    /// Get numeric value. Only valid if literal kind is #Number.
    /// \return Numeric value.
    const CNumber & getNumber() const { return m_number; }

    /// Set numeric value. Also changes kind to #Number.
    /// \param _number Value.
    void setNumber(const CNumber & _number) {
        m_literalKind = Number;
        m_number = _number;
    }

    /// Get boolean value. Only valid if literal kind is #Bool.
    /// \return Value.
    bool getBool() const { return m_bool; }

    /// Set boolean value. Also changes kind to #Bool.
    /// \param _b Value.
    void setBool(bool _b) {
        m_literalKind = Bool;
        m_bool = _b;
    }

    /// Get character value. Only valid if literal kind is #Char.
    /// \return Value.
    wchar_t getChar() const { return m_char; }

    /// Set character value. Also changes kind to #Char.
    /// \param _c Value.
    void setChar(wchar_t _c) {
        m_literalKind = Char;
        m_char = _c;
    }

    /// Get string value. Only valid if literal kind is #String.
    /// \return Value.
    const std::wstring & getString() const { return m_string; }

    /// Set string value. Also changes kind to #String.
    /// \param _str Value.
    void setString(const std::wstring & _str) {
        m_literalKind = String;
        m_string = _str;
    }

private:
    int m_literalKind;
    std::wstring m_string;
    wchar_t m_char;
    CNumber m_number;
    bool m_bool;
};

/// Expression containing reference to a variable or a parameter.
class CVariableReference : public CExpression {
public:
    /// Default constructor.
    CVariableReference() : m_pTarget(NULL) {}

    /// Initialize using name.
    /// \param _strName Identifier.
    CVariableReference(const std::wstring & _strName) : m_pTarget(NULL), m_strName(_strName) {}

    /// Initialize using referenced variable. Name is set accordingly.
    /// \param _pTarget Referenced variable.
    CVariableReference(const CNamedValue * _pTarget) {
        setTarget(_pTarget);
    }

    /// Get expression kind.
    /// \return #Var.
    virtual int getKind() const { return Var; }

    /// Get name of the variable.
    /// \returns Identifier.
    const std::wstring & getName() const { return m_strName; }

    /// Set name of the variable.
    /// \param _strName Identifier.
    void setName(const std::wstring & _strName) { m_strName = _strName; }

    /// Get referenced variable.
    /// \return Referenced variable.
    const CNamedValue * getTarget() const { return m_pTarget; }

    /// Set referenced variable.
    /// \param _pTarget Referenced variable.
    void setTarget(const CNamedValue * _pTarget) {
        m_pTarget = _pTarget;
        if (_pTarget) {
            m_strName = _pTarget->getName();
            setType(_pTarget->getType(), false);
        }
    }

private:
    const CNamedValue * m_pTarget;
    std::wstring m_strName;
};

class CPredicate;

/// Expression containing reference to a predicate.
class CPredicateReference : public CExpression {
public:
    /// Default constructor.
    CPredicateReference() : m_pTarget(NULL) {}

    /// Initialize using name.
    /// \param _strName Identifier.
    CPredicateReference(const std::wstring & _strName) : m_pTarget(NULL), m_strName(_strName) {}

    /// Initialize using referenced predicate.
    /// \param _pTarget Referenced variable.
    CPredicateReference(const CPredicate * _pTarget) : m_pTarget(_pTarget) {}

    /// Get expression kind.
    /// \return #Predicate.
    virtual int getKind() const { return Predicate; }

    /// Get name of the predicate.
    /// \returns Identifier.
    const std::wstring & getName() const { return m_strName; }

    /// Set name of the predicate.
    /// \param _strName Identifier.
    void setName(const std::wstring & _strName) { m_strName = _strName; }

    /// Get referenced predicate.
    /// \return Referenced predicate.
    const CPredicate * getTarget() const { return m_pTarget; }

    /// Set referenced predicate.
    /// \param _pTarget Referenced predicate.
    void setTarget(const CPredicate * _pTarget) { m_pTarget = _pTarget; }

private:
    const CPredicate * m_pTarget;
    std::wstring m_strName;
};

/// Unary expression.
class CUnary : public CExpression {
public:
    /// Unary operator.
    enum {
        /// Unary plus.
        Plus,
        /// Unary minus.
        Minus,
        /// Logical negation ('!').
        BoolNegate,
        /// Bitwise negation ('~').
        BitwiseNegate
    };

    /// Default constructor.
    CUnary() : m_operator(0), m_pExpression(NULL) {}

    /// Initialize with operator.
    /// \param _operator Operator (one of #Minus, #BoolNegate and #BitwiseNegate).
    /// \param _pExpression Subexpression.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    CUnary(int _operator, CExpression * _pExpression = NULL, bool _bReparent = true)
        : m_operator(_operator), m_pExpression(NULL)
    {
        _assign(m_pExpression, _pExpression, _bReparent);
    }

    /// Destructor.
    virtual ~CUnary() { _delete(m_pExpression); }

    /// Get expression kind.
    /// \return #Unary.
    virtual int getKind() const { return Unary; }

    /// Get operator.
    /// \return Operator (one of #Minus, #BoolNegate and #BitwiseNegate).
    int getOperator() const { return m_operator; }

    /// Set operator.
    /// \param _operator Operator (one of #Minus, #BoolNegate and #BitwiseNegate).
    void setOperator(int _operator) { m_operator = _operator; }

    /// Get subexpression to which the operator is applied.
    /// \return Subexpression.
    CExpression * getExpression() const { return m_pExpression; }

    /// Set subexpression to which the operator is applied.
    /// \param _pExpression Subexpression.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setExpression(CExpression * _pExpression, bool _bReparent = true) {
        _assign(m_pExpression, _pExpression, _bReparent);
    }

    /// Get or set overflow strategy.
    /// \return Reference to overflow handling descriptor.
    COverflow & overflow() { return m_overflow; }

private:
    int m_operator;
    CExpression * m_pExpression;
    COverflow m_overflow;
};

/// Binary expression.
class CBinary : public CExpression {
public:
    /// Binary operator.
    enum {
        /// Binary plus.
        /// Addition (when applied to numeric values), set or array union, or
        /// string concatenation.
        Add,
        /// Binary minus.
        /// Subtraction (when applied to numeric values) or set difference.
        Subtract,
        /// Multiplication.
        /// Applicable only to numeric values.
        Multiply,
        /// Division.
        /// Applicable only to numeric values.
        Divide,
        /// Remainder.
        /// Applicable only to integral values.
        Remainder,
        /// Power.
        /// Applicable only to numeric values.
        Power,
        /// Bitwise shift left.
        /// Equivalent to multiplication by corresponding power of 2.
        /// Applicable only to integral values.
        ShiftLeft,
        /// Bitwise shift right.
        /// Equivalent to integer division by corresponding power of 2.
        /// Applicable only to integral values.
        ShiftRight,
        /// Containment test.
        /// Applicable to sets, ranges, sequences, arrays and maps.
        In,
        /// Less than comparison.
        /// Applicable only to numeric values.
        Less,
        /// Less than or equals comparison.
        /// Applicable only to numeric values.
        LessOrEquals,
        /// Greater than comparison.
        /// Applicable only to numeric values.
        Greater,
        /// Greater than or equals comparison.
        /// Applicable only to numeric values.
        GreaterOrEquals,
        /// Equality comparison.
        Equals,
        /// Inequality comparison.
        NotEquals,
        /// Logical AND.
        /// Applicable only to boolean values.
        BoolAnd,
        /// Logical OR.
        /// Applicable only to boolean values.
        BoolOr,
        /// Logical XOR.
        /// Applicable only to boolean values.
        BoolXor,
        /// Bitwise AND.
        /// Applicable only to integral values.
        BitwiseAnd,
        /// Bitwise OR.
        /// Applicable only to integral values.
        BitwiseOr,
        /// Bitwise XOR.
        /// Applicable only to integral values.
        BitwiseXor,
        /// Implication.
        /// Applicable only to boolean values.
        Implies,
        /// Equivalence.
        /// Applicable only to boolean values.
        Iff
    };

    /// Default constructor.
    CBinary() : m_operator(0), m_pLeft(NULL), m_pRight(NULL) {}

    /// Initialize with operator.
    /// \param _operator Operator (#Add, #Subtract, etc.)
    /// \param _pLeft Left subexpression.
    /// \param _pRight Right subexpression.
    /// \param _bReparent If specified (default) also sets parent of _pLeft and _pRight to this node.
    CBinary(int _operator, CExpression * _pLeft = NULL, CExpression * _pRight = NULL, bool _bReparent = true)
        : m_operator(_operator), m_pLeft(NULL), m_pRight(NULL)
    {
        _assign(m_pLeft, _pLeft, _bReparent);
        _assign(m_pRight, _pRight, _bReparent);
    }

    /// Destructor.
    virtual ~CBinary() {
        _delete(m_pLeft);
        _delete(m_pRight);
    }

    /// Get expression kind.
    /// \return #Binary.
    virtual int getKind() const { return Binary; }

    /// Get operator.
    /// \return Operator (#Add, #Subtract, etc.)
    int getOperator() const { return m_operator; }

    /// Set operator.
    /// \param _operator  Operator (#Add, #Subtract, etc.)
    void setOperator(int _operator) { m_operator = _operator; }

    /// Get left operand.
    /// \return Subexpression.
    CExpression * getLeftSide() const { return m_pLeft; }

    /// Set left operand.
    /// \param _pExpression Subexpression.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setLeftSide(CExpression * _pExpression, bool _bReparent = true) {
        _assign(m_pLeft, _pExpression, _bReparent);
    }

    /// Get right operand.
    /// \return Subexpression.
    CExpression * getRightSide() const { return m_pRight; }

    /// Set right operand.
    /// \param _pExpression Subexpression.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setRightSide(CExpression * _pExpression, bool _bReparent = true) {
        _assign(m_pRight, _pExpression, _bReparent);
    }

    /// Get or set overflow strategy.
    /// \return Reference to overflow handling descriptor.
    COverflow & overflow() { return m_overflow; }

private:
    int m_operator;
    CExpression * m_pLeft, * m_pRight;
    COverflow m_overflow;
};

/// Ternary ("condition ? then part : else part") expression.
class CTernary : public CExpression {
public:
    /// Default constructor.
    CTernary() :  m_pIf(NULL), m_pThen(NULL), m_pElse(NULL) {}

    /// Initialize with operands.
    /// \param _pIf If-subexpression.
    /// \param _pThen Then-subexpression.
    /// \param _pElse Else-subexpression.
    /// \param _bReparent If specified (default) also sets parent of subexpressions to this node.
    CTernary(CExpression * _pIf = NULL, CExpression * _pThen = NULL, CExpression * _pElse = NULL, bool _bReparent = true)
        : m_pIf(NULL), m_pThen(NULL), m_pElse(NULL)
    {
        _assign(m_pIf, _pIf, _bReparent);
        _assign(m_pThen, _pThen, _bReparent);
        _assign(m_pElse, _pElse, _bReparent);
    }

    /// Destructor.
    virtual ~CTernary() {
        _delete(m_pIf);
        _delete(m_pThen);
        _delete(m_pElse);
    }

    /// Get expression kind.
    /// \return #Ternary.
    virtual int getKind() const { return Ternary; }

    /// Get logical condition.
    /// \return Condition.
    CExpression * getIf() const { return m_pIf; }

    /// Set logical condition.
    /// \param _pExpression Condition.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setIf(CExpression * _pExpression, bool _bReparent = true) {
        _assign(m_pIf, _pExpression, _bReparent);
    }

    /// Get 'then' part.
    /// \return Expression that should be evaluated if condition is true.
    CExpression * getThen() const { return m_pThen; }

    /// Set 'then' part.
    /// \param _pExpression Expression that should be evaluated if condition is true.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setThen(CExpression * _pExpression, bool _bReparent = true) {
        _assign(m_pThen, _pExpression, _bReparent);
    }

    /// Get 'else' part.
    /// \return Expression that should be evaluated if condition is false.
    CExpression * getElse() const { return m_pElse; }

    /// Set 'else' part.
    /// \param _pExpression Expression that should be evaluated if condition is false.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setElse(CExpression * _pExpression, bool _bReparent = true) {
        _assign(m_pElse, _pExpression, _bReparent);
    }

private:
    CExpression * m_pIf, * m_pThen, * m_pElse;
};

/// Type as a part of expression.
class CTypeExpr : public CExpression {
public:
    /// Default constructor.
    CTypeExpr() : m_pContents(NULL) {}

    /// Initialize with contained type.
    /// \param _pContents Contained type.
    /// \param _bReparent If specified (default) also sets parent of _pContents to this node.
    CTypeExpr(CType * _pContents, bool _bReparent = true) : m_pContents(NULL) {
        _assign(m_pContents, _pContents, _bReparent);
    }

    /// Destructor.
    virtual ~CTypeExpr() { _delete(m_pContents); }

    /// Get expression kind.
    /// \return #Type.
    virtual int getKind() const { return Type; }

    /// Get contained type.
    /// \return Contained type.
    CType * getContents() const { return m_pContents; }

    /// Set contained type.
    /// \param _pContents Contained type.
    /// \param _bReparent If specified (default) also sets parent of _pContents to this node.
    void setContents(CType * _pContents, bool _bReparent = true) {
        _assign(m_pContents, _pContents, _bReparent);
    }

private:
    CType * m_pContents;
};

/// Type as a part of expression.
class CCastExpr : public CExpression {
public:
    /// Default constructor.
    CCastExpr() : m_pExpression(NULL), m_pToType(NULL) {}

    /// Initialize.
    /// \param _pExpr Expression being casted.
    /// \param _pToType Destination type..
    /// \param _bReparent If specified (default) also sets parent to this node.
    CCastExpr(CExpression * _pExpr, CTypeExpr * _pToType, bool _bReparent = true) : m_pExpression(NULL), m_pToType(NULL) {
        _assign(m_pExpression, _pExpr, _bReparent);
        _assign(m_pToType, _pToType, _bReparent);
    }

    /// Destructor.
    virtual ~CCastExpr() { _delete(m_pExpression); _delete(m_pToType); }

    /// Get expression kind.
    /// \return #Cast.
    virtual int getKind() const { return Cast; }

    /// Get expression being casted.
    /// \return Expression being casted.
    CExpression * getExpression() const { return m_pExpression; }

    /// Get destination type.
    /// \return Destination type.
    CTypeExpr * getToType() const { return m_pToType; }

    /// Set expression to cast.
    /// \param _pExpression Expression.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setExpression(CExpression * _pExpression, bool _bReparent = true) {
        _assign(m_pExpression, _pExpression, _bReparent);
    }

    /// Set destination type.
    /// \param _pType Destination type.
    /// \param _bReparent If specified (default) also sets parent of _pType to this node.
    void setToType(CTypeExpr * _pType, bool _bReparent = true) {
        _assign(m_pToType, _pType, _bReparent);
    }

private:
    CExpression * m_pExpression;
    CTypeExpr * m_pToType;
};

/// Possibly quantified logical formula.
class CFormula : public CExpression {
public:
    /// Quantifier.
    enum {
        /// No quantifier.
        None,
        /// Universal quantifier.
        Universal,
        /// Existential quantifier.
        Existential
    };

    /// Default constructor.
    CFormula() : m_quantifier(None), m_pSubformula(NULL) {}

    /// Initialize with quantifier and subformula.
    /// \param _quantifier Quantifier (one of #None, #Universal, #Existential).
    /// \param _pSubformula Formula.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    CFormula(int _quantifier, CExpression * _pSubformula = NULL, bool _bReparent = true) : m_quantifier(_quantifier), m_pSubformula(NULL) {
        _assign(m_pSubformula, _pSubformula, _bReparent);
    }

    /// Destructor.
    virtual ~CFormula() { _delete(m_pSubformula); }

    /// Get expression kind.
    /// \return #Formula.
    virtual int getKind() const { return Formula; }

    /// Get quantifier.
    /// \return Quantifier (one of #None, #Universal, #Existential).
    int getQuantifier() const { return m_quantifier; }

    /// Set quantifier.
    /// \param _quantifier Quantifier (one of #None, #Universal, #Existential).
    void setQuantifier(int _quantifier) { m_quantifier = _quantifier; }

    /// Get list of bound variables.
    /// \return Refernce to bound variables list.
    CNamedValues & getBoundVariables() { return m_boundVariables; }

    /// Get subformula.
    /// \return Formula. E.g. given quantified formula "! X . Y" this
    ///   method returns "Y".
    CExpression * getSubformula() const { return m_pSubformula; }

    /// Set subformula.
    /// \param _pExpression Formula.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setSubformula(CExpression * _pExpression, bool _bReparent = true) {
        _assign(m_pSubformula, _pExpression, _bReparent);
    }

private:
    int m_quantifier;
    CExpression * m_pSubformula;
    CNamedValues m_boundVariables;
};

/// Component of a compound value (e.g. array element or field of structure).
class CComponent : public CExpression {
public:
    /// Component kind.
    enum {
        /// Part of an array (itself is another array).
        ArrayPart,
        /// Field of a structure.
        StructField,
        /// Union alternative name. Only used in switch/case construct.
        /// The object can be cast to CUnionAlternativeReference.
        UnionAlternative,
        /// Map element.
        MapElement,
        /// List element.
        ListElement,
        /// Replacement of a part of compound value.
        Replacement,
    };

    /// Default constructor.
    CComponent() : m_pObject(NULL) {}

    /// Destructor.
    virtual ~CComponent() { _delete(m_pObject); }

    /// Get expression kind.
    /// \return #Formula.
    virtual int getKind() const { return Component; }

    /// Get component kind (implemented in descendants).
    /// \return Component kind.
    virtual int getComponentKind() const = 0;

    /// Get expression to which the subscript is applied.
    /// \return Expression of compound type.
    CExpression * getObject() const { return m_pObject; }

    /// Set expression to which the subscript is applied.
    /// \param _pExpression Expression of compound type.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setObject(CExpression * _pExpression, bool _bReparent = true) {
        _assign(m_pObject, _pExpression, _bReparent);
    }

private:
    CExpression * m_pObject;
};

/// Array or sequence element or array part.
class CArrayPartExpr : public CComponent {
public:
    /// Default constructor.
    CArrayPartExpr() {}

    /// Get component kind.
    /// \return #ArrayPart.
    virtual int getComponentKind() const { return ArrayPart; }

    /// Get list of subscripts.
    /// \return List of indices.
    CCollection<CExpression> & getIndices() { return m_indices; }

private:
    CCollection<CExpression> m_indices;
};

// Found in types.h
class CStructType;

/// Structure field.
class CStructFieldExpr : public CComponent {
public:
    CStructFieldExpr(const std::wstring & _strField = L"") : m_strField(_strField) {}

    /// Get component kind.
    /// \return #StructField.
    virtual int getComponentKind() const { return StructField; }

    const std::wstring & getFieldName() const { return m_strField; }

private:
    std::wstring m_strField;
};

/*

/// Structure field.
class CStructFieldExpr : public CComponent {
public:
    /// Default constructor.
    CStructFieldExpr() : m_pStructType(NULL), m_cFieldIdx(-1) {}

    /// Initialize with type and field.
    /// \param _pStructType Structure type.
    /// \param _cFieldIdx Structure field index.
    /// \param _bReparent If specified (default) also sets parent of _pStructType to this node.
    CStructFieldExpr(CType * _pStructType, size_t _cFieldIdx, bool _bReparent = true) : m_pStructType(NULL), m_cFieldIdx(_cFieldIdx) {
        _assign(m_pStructType, (CType *) _pStructType, _bReparent);
    }

    /// Destructor.
    virtual ~CStructFieldExpr() { _delete(m_pStructType); }

    /// Get component kind.
    /// \return #StructField.
    virtual int getComponentKind() const { return StructField; }

    /// Get corresponding structure type.
    /// \return Structure type.
    CStructType * getStructType() const { return (CStructType *) m_pStructType; }

    /// Set corresponding structure type.
    /// \param _pStructType Structure type.
    /// \param _bReparent If specified (default) also sets parent of _pStructType to this node.
    void setStructType(CStructType * _pStructType, bool _bReparent = true) {
        _assign(m_pStructType, (CType *) _pStructType, _bReparent);
    }

    /// Get corresponding structure field.
    /// \return Structure field.
    const CNamedValue * getField() const;

    /// Get corresponding structure field index.
    /// \return Structure field index.
    size_t getFieldIdx() const { return m_cFieldIdx; }

    /// Set corresponding structure field index.
    /// \param _pField Structure field.
    void setField(size_t _cFieldIdx) { m_cFieldIdx = _cFieldIdx; }

private:
    CType * m_pStructType;
    size_t m_cFieldIdx;
};

 */

class CUnionType;
class CUnionConstructorDefinition;
typedef std::pair<size_t, size_t> union_field_idx_t;

/// Union alternative name. Only used inside switch/case construct.
class CUnionAlternativeExpr : public CComponent {
public:
    CUnionAlternativeExpr(const std::wstring & _strName) :
        m_strName(_strName), m_pType(NULL), m_idx(-1, -1)
    {}

    CUnionAlternativeExpr(const CUnionType * _pType, const union_field_idx_t & _idx);

    /// Get component kind.
    /// \return #UnionAlternative.
    virtual int getComponentKind() const { return UnionAlternative; }

    /// Get name of the alternative.
    /// \returns Identifier.
    const std::wstring & getName() const { return m_strName; }

    /// Set name of the alternative.
    /// \param _strName Identifier.
    void setName(const std::wstring & _strName) { m_strName = _strName; }

    /// Get corresponding union type.
    /// \return Union type.
    CUnionType * getUnionType() const { return (CUnionType *) m_pType; }

    /// Set corresponding union type.
    /// \param _pUnionType Union type.
    void setUnionType(const CUnionType * _pUnionType) { m_pType = (const CType *) _pUnionType; }

    union_field_idx_t getIdx() const { return m_idx; }

    void setIdx(size_t _cCons, size_t _cField) { m_idx = union_field_idx_t(_cCons, _cField); }

    const CNamedValue * getField() const;
    const CUnionConstructorDefinition * getConstructor() const;

private:
    std::wstring m_strName;
    const CType * m_pType;
    union_field_idx_t m_idx;
};

/// Map element.
class CMapElementExpr : public CComponent {
public:
    /// Default constructor.
    CMapElementExpr() : m_pIndex(NULL) {}

    /// Destructor.
    virtual ~CMapElementExpr() { _delete(m_pIndex); }

    /// Get component kind.
    /// \return #MapElement.
    virtual int getComponentKind() const { return MapElement; }

    /// Get index expression.
    /// \return Element index.
    CExpression * getIndex() const { return m_pIndex; }

    /// Set index expression.
    /// \param _pExpression Element index.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setIndex(CExpression * _pExpression, bool _bReparent = true) {
        _assign(m_pIndex, _pExpression, _bReparent);
    }

private:
    CExpression * m_pIndex;
};

/// List element.
class CListElementExpr : public CComponent {
public:
    /// Default constructor.
    CListElementExpr() : m_pIndex(NULL) {}

    /// Destructor.
    virtual ~CListElementExpr() { _delete(m_pIndex); }

    /// Get component kind.
    /// \return #ListElement.
    virtual int getComponentKind() const { return ListElement; }

    /// Get index expression.
    /// \return Element index.
    CExpression * getIndex() const { return m_pIndex; }

    /// Set index expression.
    /// \param _pExpression Element index.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setIndex(CExpression * _pExpression, bool _bReparent = true) {
        _assign(m_pIndex, _pExpression, _bReparent);
    }

private:
    CExpression * m_pIndex;
};

// Declared below.
class CConstructor;

/// Replacement expression. Result is a new expression of compound type with
/// some elements replaced.
class CReplacement : public CComponent {
public:
    /// Default constructor.
    CReplacement() : m_pConstructor(NULL) {}

    /// Desctructor.
    virtual ~CReplacement() { _delete(m_pConstructor); }

    /// Get component kind.
    /// \return #Replacement.
    virtual int getComponentKind() const { return Replacement; }

    /// Get expression of compound type containing new values.
    /// \return Expression containing new values.
    CConstructor * getNewValues() const { return (CConstructor *) m_pConstructor; }

    /// Set expression of compound type containing new values.
    /// \param _pConstructor Expression containing new values.
    /// \param _bReparent If specified (default) also sets parent of _pConstructor to this node.
    void setNewValues(CConstructor * _pConstructor, bool _bReparent = true) {
        _assign(m_pConstructor, (CExpression *) _pConstructor, _bReparent);
    }

private:
    CExpression * m_pConstructor;
};

/// Function call as a part of an expression.
class CFunctionCall : public CExpression {
public:
    /// Default constructor.
    CFunctionCall() : m_pPredicate(NULL) {}

    /// Destructor.
    virtual ~CFunctionCall() { _delete(m_pPredicate); }

    virtual int getKind() const { return FunctionCall; }

    /// Get predicate expression which is called.
    /// \return Expression of predicate type.
    CExpression * getPredicate() const { return m_pPredicate; }

    /// Set predicate expression which is called.
    /// \param _pExpression Expression of predicate type.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setPredicate(CExpression * _pExpression, bool _bReparent = true) {
        _assign(m_pPredicate, _pExpression, _bReparent);
    }

    /// Get list of actual parameters.
    /// \return List of expressions.
    CCollection<CExpression> & getParams() { return m_params; }
    const CCollection<CExpression> & getParams() const { return m_params; }

    /// Get type of the expression.
    /// \returns Type associated with expression.
    virtual CType * getType() const;

private:
    CExpression * m_pPredicate;
    CCollection<CExpression> m_params;
};
/*
class CParamBinding : public CNode {
public:
    CParamBinding() : m_pValue(NULL), m_pParam(NULL) {}

    CParamBinding(CExpression * _pValue, const CParam * _pParam = NULL, bool _bReparent = true)
        : m_pValue(NULL), m_pParam(_pParam)
    {
        _assign(m_pValue, _pValue, _bReparent);
    }

    virtual ~CParamBinding() { _delete(m_pValue); }

    /// Get predicate expression which is called.
    /// \return Expression of predicate type.
    CExpression * getValue() const { return m_pPredicate; }

    /// Set predicate expression which is called.
    /// \param _pExpression Expression of predicate type.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setValue(CExpression * _pValue, bool _bReparent = true) {
        _assign(m_pValue, _pValue, _bReparent);
    }

    /// Get predicate expression which is called.
    /// \return Expression of predicate type.
    CExpression * getParam() const { return m_pPredicate; }

    /// Set predicate expression which is called.
    /// \param _pExpression Expression of predicate type.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setParam(CParam * _pValue, bool _bReparent = true) {
        _assign(m_pValue, _pValue, _bReparent);
    }

private:
    CExpression * m_pValue;
    const CParam * m_pParam;
};
*/

/// Partially applied predicate.
class CBinder : public CExpression {
public:
    /// Default constructor.
    CBinder() : m_pPredicate(NULL) {}

    /// Destructor.
    virtual ~CBinder() { _delete(m_pPredicate); }

    virtual int getKind() const { return Binder; }

    /// Get predicate expression whose parameters are bound.
    /// \return Expression of predicate type.
    CExpression * getPredicate() const { return m_pPredicate; }

    /// Set predicate expression.
    /// \param _pExpression Expression of predicate type.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setPredicate(CExpression * _pExpression, bool _bReparent = true) {
        _assign(m_pPredicate, _pExpression, _bReparent);
    }

    /// Get list of bound parameters. Can contain NULL values.
    /// \return List of expressions.
    CCollection<CExpression> & getParams() { return m_params; }

private:
    CExpression * m_pPredicate;
    CCollection<CExpression> m_params;
};

class CFormulaDeclaration;

/// Function call as a part of an expression.
class CFormulaCall : public CExpression {
public:
    /// Default constructor.
    CFormulaCall() : m_pTarget(NULL) {}

    virtual int getKind() const { return FormulaCall; }

    /// Get formula which is called.
    /// \return Formula declaration.
    const CFormulaDeclaration * getTarget() const { return m_pTarget; }

    /// Set formula declaration which is called.
    /// \param _pFormula Formula.
    void setTarget(const CFormulaDeclaration * _pFormula) { m_pTarget = _pFormula; }

    /// Get list of actual parameters.
    /// \return List of expressions.
    CCollection<CExpression> & getParams() { return m_params; }

private:
    const CFormulaDeclaration * m_pTarget;
    CCollection<CExpression> m_params;
};

/// Output branch of a predicate.
class CBranch : public CParams {
public:
    /// Default constructor.
    CBranch() : m_pLabel(NULL), m_pPreCondition(NULL), m_pPostCondition(NULL) {}

    /// Destructor.
    virtual ~CBranch() {
        _delete(m_pLabel);
        _delete(m_pPreCondition);
        _delete(m_pPostCondition);
    }

    /// Get label associated with the branch.
    /// Possibly NULL if it is the only branch.
    /// \return Label.
    CLabel * getLabel() const { return m_pLabel; }

    /// Set label associated with the branch.
    /// \param _pLabel Label of the output branch.
    /// \param _bReparent If specified (default) also sets parent of _pLabel to this node.
    void setLabel(CLabel * _pLabel, bool _bReparent = true) {
        _assign(m_pLabel, _pLabel, _bReparent);
    }

    /// Get branch precondition.
    /// \return Precondition.
    CFormula * getPreCondition() const { return m_pPreCondition; }

    /// Set branch precondition.
    /// \param _pCondition Precondition.
    /// \param _bReparent If specified (default) also sets parent of _pCondition to this node.
    void setPreCondition(CFormula * _pCondition, bool _bReparent = true) {
        _assign(m_pPreCondition, _pCondition, _bReparent);
    }

    /// Get branch precondition.
    /// \return Postcondition.
    CFormula * getPostCondition() const { return m_pPostCondition; }

    /// Set branch postcondition.
    /// \param _pCondition Postcondition.
    /// \param _bReparent If specified (default) also sets parent of _pCondition to this node.
    void setPostCondition(CFormula * _pCondition, bool _bReparent = true) {
        _assign(m_pPostCondition, _pCondition, _bReparent);
    }

private:
    CLabel * m_pLabel;
    CFormula * m_pPreCondition, * m_pPostCondition;
};

/// Collection of output branches.
/// Used in predicate, process and lambda declarations.
/// \extends CNode
class CBranches : public CCollection<CBranch> {
};

class CPredicateType;

/// Predicate declaration base (also used by CLambda).
class CAnonymousPredicate : public CStatement {
public:
    /// Default constructor.
    CAnonymousPredicate() : m_pPreCond(NULL), m_pPostCond(NULL), m_pBlock(NULL), m_pType(NULL) {}

    /// Destructor.
    virtual ~CAnonymousPredicate();

    /// Get list of formal input parameters.
    /// \return List of parameters.
    CParams & getInParams() { return m_paramsIn; }
    const CParams & getInParams() const { return m_paramsIn; }

    /// Get list of output branches. Each branch can contain a list of parameters,
    /// a precondition and a postcondition.
    /// \return List of branches.
    CBranches & getOutParams() { return m_paramsOut; }
    const CBranches & getOutParams() const { return m_paramsOut; }

    /// Set predicate body.
    /// \param _pBlock Predicate body.
    /// \param _bReparent If specified (default) also sets parent of _pBlock to this node.
    void setBlock(CBlock * _pBlock, bool _bReparent = true) {
        _assign(m_pBlock, _pBlock, _bReparent);
    }

    /// Get predicate body.
    /// \return Predicate body.
    CBlock * getBlock() const { return m_pBlock; }

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

    // Check if the statement ends like a block (i.e. separating semicolon is not needed).
    // \return True.
    virtual bool isBlockLike() const { return true; }

    CPredicateType * getType() const {
        if (! m_pType) updateType();
        return m_pType;
    }

    void updateType() const;

private:
    CParams m_paramsIn;
    CBranches m_paramsOut;
    CFormula * m_pPreCond, * m_pPostCond;
    CBlock * m_pBlock;
    mutable CPredicateType * m_pType;
};

/// Anonymous predicate.
class CLambda : public CExpression {
public:
    /// Default constructor.
    CLambda() {}

    /// Get expression kind.
    /// \return #Lambda.
    virtual int getKind() const { return Lambda; }

    CAnonymousPredicate & getPredicate() { return m_pred; }

private:
    CAnonymousPredicate m_pred;
};

/// Indexed element.
class CElementDefinition : public CNode {
public:
    /// Default constructor.
    CElementDefinition() : m_pIndex(NULL), m_pValue(NULL) {}

    virtual ~CElementDefinition() {
        _delete(m_pIndex);
        _delete(m_pValue);
    }

    /// Get index expression.
    /// \return Element index.
    CExpression * getIndex() const { return m_pIndex; }

    /// Set index expression.
    /// \param _pExpression Element index.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setIndex(CExpression * _pExpression, bool _bReparent = true) {
        _assign(m_pIndex, _pExpression, _bReparent);
    }

    /// Get value expression.
    /// \return Element value.
    CExpression * getValue() const { return m_pValue; }

    /// Set value expression.
    /// \param _pExpression Element value.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setValue(CExpression * _pExpression, bool _bReparent = true) {
        _assign(m_pValue, _pExpression, _bReparent);
    }

private:
    CExpression * m_pIndex;
    CExpression * m_pValue;
};

/// Literal of \c struct type.
class CStructFieldDefinition : public CNode {
public:
    /// Default constructor.
    CStructFieldDefinition() : m_pValue(NULL) {}

    /// Initialize with struct and field references.
    /// \param _pStructType Type of structure.
    /// \param _pField Field.
    CStructFieldDefinition(const CStructType * _pStructType, const CNamedValue * _pField)
        : m_pValue(NULL), m_pStructType(_pStructType), m_pField(_pField) {}

    /// Destructor.
    virtual ~CStructFieldDefinition() { _delete(m_pValue); }

    /// Get type of structure.
    /// \return Type of structure.
    const CStructType * getStructType() const { return m_pStructType; }

    /// Set type of structure.
    /// \param _pStructType Type of structure.
    void setStructType(const CStructType * _pStructType) { m_pStructType = _pStructType; }

    /// Get field reference.
    /// \return Field reference.
    const CNamedValue * getField() const { return m_pField; }

    /// Set field reference.
    /// \param _pField Field reference.
    void setField(const CNamedValue * _pField) { m_pField = _pField; }

    /// Get value expression.
    /// \return Field value.
    CExpression * getValue() const { return m_pValue; }

    /// Set value expression.
    /// \param _pExpression Field value.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setValue(CExpression * _pExpression, bool _bReparent = true) {
        _assign(m_pValue, _pExpression, _bReparent);
    }

    /// Get name of the field.
    /// \returns Identifier.
    const std::wstring & getName() const { return m_strName; }

    /// Set name of the field.
    /// \param _strName Identifier.
    void setName(const std::wstring & _strName) { m_strName = _strName; }

private:
    CExpression * m_pValue;
    const CStructType * m_pStructType;
    const CNamedValue * m_pField;
    std::wstring m_strName;
};

/// Constructor of compound value (array, set, struct, etc.)
class CConstructor : public CExpression {
public:
    /// Kind of constructor.
    enum {
        /// Struct initializer.
        StructFields,
        /// Array initializer.
        ArrayElements,
        /// Set initializer.
        SetElements,
        /// Map initializer.
        MapElements,
        /// List initializer.
        ListElements,
        /// Array generator.
        ArrayIteration,
        /// Union constructor.
        UnionConstructor,
    };

    /// Default constructor.
    CConstructor() {}

    /// Get expression kind.
    /// \return #Constructor.
    virtual int getKind() const { return Constructor; }

    /// Get constructor kind (implemented in descendants).
    /// \return Constructor kind.
    virtual int getConstructorKind() const = 0;
};

/// Structure value. \extends CConstructor
/// This object is a collection of CStructFieldDefinition instances, use methods
/// of CCollection to access field definitions.
class CStructConstructor : public CCollection<CStructFieldDefinition, CConstructor> {
public:
    /// Default constructor.
    CStructConstructor() {}

    /// Get constructor kind.
    /// \return #StructFields.
    virtual int getConstructorKind() const { return StructFields; }
};

class CVariableDeclaration;
class CUnionConstructorDefinition;

/// Union value. \extends CConstructor
/// The class extends struct constructor with union constructor name.
/// May contain not-fully defined fields (used in switch construct).
class CUnionConstructor : public CStructConstructor {
public:
    CUnionConstructor() {}

    /// Get constructor kind.
    /// \return #UnionConstructor.
    virtual int getConstructorKind() const { return UnionConstructor; }

    /// Get list of variables declared as part of the constructor.
    /// \return List of variables.
    CCollection<CVariableDeclaration> & getDeclarations() { return m_decls; }
    const CCollection<CVariableDeclaration> & getDeclarations() const { return m_decls; }

    bool isComplete() const;

    CUnionConstructorDefinition * getDefinition() const { return m_pDef; }
    void setDefinition(CUnionConstructorDefinition * _pDef) { m_pDef = _pDef; }

private:
    CCollection<CVariableDeclaration> m_decls;
    CUnionConstructorDefinition * m_pDef;
};

/// Array value. \extends CConstructor
/// This object is a collection of CElementDefinition instances, use methods
/// of CCollection to access element definitions.
class CArrayConstructor : public CCollection<CElementDefinition, CConstructor> {
public:
    /// Default constructor.
    CArrayConstructor() {}

    /// Get constructor kind.
    /// \return #ArrayElements.
    virtual int getConstructorKind() const { return ArrayElements; }
};

/// Map value. \extends CConstructor
/// This object is a collection of CElementDefinition instances, use methods
/// of CCollection to access element definitions.
class CMapConstructor : public CCollection<CElementDefinition, CConstructor> {
public:
    /// Default constructor.
    CMapConstructor() {}

    /// Get constructor kind.
    /// \return #MapElements.
    virtual int getConstructorKind() const { return MapElements; }
};

/// Set value. \extends CConstructor
/// This object is a collection of CExpression instances, use methods
/// of CCollection to access element definitions.
class CSetConstructor : public CCollection<CExpression, CConstructor> {
public:
    /// Default constructor.
    CSetConstructor() {}

    /// Get constructor kind.
    /// \return #SetElements.
    virtual int getConstructorKind() const { return SetElements; }
};

/// List value. \extends CConstructor
/// This object is a collection of CExpression instances, use methods
/// of CCollection to access element definitions.
class CListConstructor : public CCollection<CExpression, CConstructor> {
public:
    /// Default constructor.
    CListConstructor() {}

    /// Get constructor kind.
    /// \return #ListElements.
    virtual int getConstructorKind() const { return ListElements; }
};

/// Definition of array part.Consider array generator:
/// \code
/// a = for (0..10 i) {
///   case 0..4  : 'a'
///   case 5..10 : 'b'
/// }
/// \endcode
/// In the above example "case 0..4  : 'a'" and "case 5..10  : 'b'" are array parts.
class CArrayPartDefinition : public CNode {
public:
    /// Default constructor.
    CArrayPartDefinition() : m_pExpression(NULL) {}

    /// Destructor.
    virtual ~CArrayPartDefinition() { _delete(m_pExpression); }

    /// Get \c case conditions.
    /// \return List of expressions (can contain ranges).
    CCollection<CExpression> & getConditions() { return m_conditions; }

    /// Get expression.
    /// \return Expression.
    CExpression * getExpression() const { return m_pExpression; }

    /// Set expression.
    /// \param _pExpression Expression.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setExpression(CExpression * _pExpression, bool _bReparent = true) {
        _assign(m_pExpression, _pExpression, _bReparent);
    }

private:
    CExpression * m_pExpression;
    CCollection<CExpression> m_conditions;
};

/// Array constructor by means of iteration. \extends CConstructor
/// Example:
/// \code
/// for (0..15 i) {
///   case 2..7   : 'a'
///   case 10..12 : 'b'
///   default     : 'c'
/// }
/// \endcode
class CArrayIteration : public CCollection<CArrayPartDefinition, CConstructor> {
public:
    /// Default constructor.
    CArrayIteration() : m_pDefault(NULL) {}

    /// Destructor.
    virtual ~CArrayIteration() { _delete(m_pDefault); }

    /// Get constructor kind.
    /// \return #ArrayIteration.
    virtual int getConstructorKind() const { return ArrayIteration; }

    /// Get list of iterator variables.
    /// \return Iterator variables.
    CNamedValues & getIterators() { return m_iterators; }

    /// Get expression for default alternative.
    /// \return Expression.
    CExpression * getDefault() const { return m_pDefault; }

    /// Set expression for default alternative.
    /// \param _pExpression Expression.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setDefault(CExpression * _pExpression, bool _bReparent = true) {
        _assign(m_pDefault, _pExpression, _bReparent);
    }

private:
    CExpression * m_pDefault;
    CNamedValues m_iterators;
};

} // namespace ir

#endif /* EXPRESSIONS_H_ */
