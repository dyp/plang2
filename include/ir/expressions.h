/// \file expressions.h
/// Internal structures representing expressions.
///

#ifndef EXPRESSIONS_H_
#define EXPRESSIONS_H_

#include "numbers.h"
#include "base.h"

namespace ir {

class Type;

/// Overflow handling description.
class Overflow {
public:
    enum {
        SATURATE,
        STRICT,
        RETURN,
        WRAP,
    };

    Overflow() : m_overflow(WRAP), m_pLabel(NULL) {}

    /// Get overflow handling strategy.
    /// \return Overflow handling strategy.
    int get() const { return m_overflow; }

    /// Set overflow handling strategy.
    /// \param _overflow Overflow handling strategy.
    /// \param _pLabel return label (optional).
    void set(int _overflow, const Label * _pLabel = NULL) { m_overflow = _overflow; m_pLabel = _pLabel; }

    /// Set overflow handling strategy.
    /// \param _overflow Overflow handling strategy and (optionally) return label.
    void set(const Overflow & _other) { set(_other.get(), _other.getLabel()); }

    /// \return Pointer to Label object (assuming type is IntOverflow or
    ///     RealOverflow and overflow handling strategy is Label.)
    const Label * getLabel() const { return m_pLabel; }

private:
    int m_overflow;
    const Label * m_pLabel;
};

/// Virtual ancestor of all expressions.
class Expression : public Node {
public:
    /// Kind of the expression.
    enum {
        /// Numeric, character, string or unit literal.
        /// The object can be cast to Literal.
        LITERAL,
        /// Variable or parameter.
        /// The object can be cast to VariableReference.
        VAR,
        /// Predicate reference.
        /// The object can be cast to PredicateReference.
        PREDICATE,
        /// Unary expression.
        /// The object can be cast to Unary.
        UNARY,
        /// Binary expression.
        /// The object can be cast to Binary.
        BINARY,
        /// Ternary expression.
        /// The object can be cast to Ternary.
        TERNARY,
        /// Type expression.
        /// The object can be cast to TypeExpr.
        TYPE,
        /// Struct field, or subscript of array, sequence, list or map.
        /// The object can be cast to Component.
        COMPONENT,
        /// Function call.
        /// The object can be cast to FunctionCall.
        FUNCTION_CALL,
        /// Formula call.
        /// The object can be cast to FunctionCall.
        FORMULA_CALL,
        /// Anonymous function.
        /// The object can be cast to Lambda.
        LAMBDA,
        /// Partially applied predicate.
        /// The object can be cast to Binder.
        BINDER,
        /// Precondition or postcondition formula.
        /// The object can be cast to Formula.
        FORMULA,
        /// Struct, array, sequence, set, list or map constructor.
        /// The object can be cast to Constructor.
        CONSTRUCTOR,
        /// Cast expression.
        /// The object can be cast to CastExpr.
        CAST,
    };

    /// Default constructor.
    Expression() : m_pType(NULL) {}

    /// Destructor.
    virtual ~Expression() { _delete(m_pType); }

    virtual int getNodeKind() const { return Node::EXPRESSION; }

    /// Get expression kind.
    /// \returns Expression kind.
    virtual int getKind() const = 0;

    /// Get type of the expression.
    /// \returns Type associated with expression.
    virtual Type * getType() const { return m_pType; }

    /// Set type of the expression.
    /// \param _pType Type associated with expression.
    /// \param _bReparent If specified (default) also sets parent of _pType to this node.
    void setType(Type * _pType, bool _bReparent = true) {
        _assign(m_pType, _pType, _bReparent);
    }

private:
    Type * m_pType;
};

/// Representation of nil and numeric, character and string literals.
class Literal : public Expression {
public:
    /// Kind of the literal.
    enum {
        /// Unit literal (\c nil or empty constructor).
        UNIT,
        /// Numeric literal. Use getNumber() and setNumber() accessors.
        NUMBER,
        /// Boolean literal. Use getBool() and setBool() accessors.
        BOOL,
        /// Character literal. Use getChar() and setChar() accessors.
        CHAR,
        /// String literal. Use getString() and setString() accessors.
        STRING
    };

    /// Default constructor.
    Literal () : m_literalKind(UNIT) {}

    /// Initialize the literal with numeric value (sets kind to #Number).
    /// \param _number Value.
    Literal (const Number & _number) : m_literalKind(NUMBER), m_number(_number) {}

    /// Initialize the literal with boolean value (sets kind to #Bool).
    /// \param _number Value.
    Literal (bool _b) : m_literalKind(BOOL), m_bool(_b) {}

    /// Initialize the literal with character value (sets kind to #Char).
    /// \param _c Value.
    Literal (wchar_t _c) : m_literalKind(CHAR), m_char(_c) {}

    /// Initialize the literal with string value (sets kind to #String).
    /// \param _str Value.
    Literal (const std::wstring & _str) : m_literalKind(STRING), m_string(_str) {}

    /// Get expression kind.
    /// \return #Literal.
    virtual int getKind() const { return LITERAL; }

    /// Get literal kind.
    /// \return Literal kind (#Unit, #Number, #Char or #String).
    int getLiteralKind() const { return m_literalKind; }

    /// Set kind to #Unit.
    void setUnit() { m_literalKind = UNIT; }

    /// Get numeric value. Only valid if literal kind is #Number.
    /// \return Numeric value.
    const Number & getNumber() const { return m_number; }

    /// Set numeric value. Also changes kind to #Number.
    /// \param _number Value.
    void setNumber(const Number & _number) {
        m_literalKind = NUMBER;
        m_number = _number;
    }

    /// Get boolean value. Only valid if literal kind is #Bool.
    /// \return Value.
    bool getBool() const { return m_bool; }

    /// Set boolean value. Also changes kind to #Bool.
    /// \param _b Value.
    void setBool(bool _b) {
        m_literalKind = BOOL;
        m_bool = _b;
    }

    /// Get character value. Only valid if literal kind is #Char.
    /// \return Value.
    wchar_t getChar() const { return m_char; }

    /// Set character value. Also changes kind to #Char.
    /// \param _c Value.
    void setChar(wchar_t _c) {
        m_literalKind = CHAR;
        m_char = _c;
    }

    /// Get string value. Only valid if literal kind is #String.
    /// \return Value.
    const std::wstring & getString() const { return m_string; }

    /// Set string value. Also changes kind to #String.
    /// \param _str Value.
    void setString(const std::wstring & _str) {
        m_literalKind = STRING;
        m_string = _str;
    }

private:
    int m_literalKind;
    std::wstring m_string;
    wchar_t m_char;
    Number m_number;
    bool m_bool;
};

/// Expression containing reference to a variable or a parameter.
class VariableReference : public Expression {
public:
    /// Default constructor.
    VariableReference() : m_pTarget(NULL) {}

    /// Initialize using name.
    /// \param _strName Identifier.
    VariableReference(const std::wstring & _strName) : m_pTarget(NULL), m_strName(_strName) {}

    /// Initialize using referenced variable. Name is set accordingly.
    /// \param _pTarget Referenced variable.
    VariableReference(const NamedValue * _pTarget) {
        setTarget(_pTarget);
    }

    /// Get expression kind.
    /// \return #Var.
    virtual int getKind() const { return VAR; }

    /// Get name of the variable.
    /// \returns Identifier.
    const std::wstring & getName() const { return m_strName; }

    /// Set name of the variable.
    /// \param _strName Identifier.
    void setName(const std::wstring & _strName) { m_strName = _strName; }

    /// Get referenced variable.
    /// \return Referenced variable.
    const NamedValue * getTarget() const { return m_pTarget; }

    /// Set referenced variable.
    /// \param _pTarget Referenced variable.
    void setTarget(const NamedValue * _pTarget) {
        m_pTarget = _pTarget;
        if (_pTarget) {
            m_strName = _pTarget->getName();
            setType(_pTarget->getType(), false);
        }
    }

private:
    const NamedValue * m_pTarget;
    std::wstring m_strName;
};

class Predicate;

/// Expression containing reference to a predicate.
class PredicateReference : public Expression {
public:
    /// Default constructor.
    PredicateReference() : m_pTarget(NULL) {}

    /// Initialize using name.
    /// \param _strName Identifier.
    PredicateReference(const std::wstring & _strName) : m_pTarget(NULL), m_strName(_strName) {}

    /// Initialize using referenced predicate.
    /// \param _pTarget Referenced variable.
    PredicateReference(const Predicate * _pTarget) : m_pTarget(_pTarget) {}

    /// Get expression kind.
    /// \return #Predicate.
    virtual int getKind() const { return PREDICATE; }

    /// Get name of the predicate.
    /// \returns Identifier.
    const std::wstring & getName() const;

    /// Set name of the predicate.
    /// \param _strName Identifier.
    void setName(const std::wstring & _strName) { m_strName = _strName; }

    /// Get referenced predicate.
    /// \return Referenced predicate.
    const Predicate * getTarget() const { return m_pTarget; }

    /// Set referenced predicate.
    /// \param _pTarget Referenced predicate.
    void setTarget(const Predicate * _pTarget) { m_pTarget = _pTarget; }

private:
    const Predicate * m_pTarget;
    std::wstring m_strName;
};

/// Unary expression.
class Unary : public Expression {
public:
    /// Unary operator.
    enum {
        /// Unary plus.
        PLUS,
        /// Unary minus.
        MINUS,
        /// Logical negation ('!').
        BOOL_NEGATE,
        /// Bitwise negation ('~').
        BITWISE_NEGATE
    };

    /// Default constructor.
    Unary() : m_operator(0), m_pExpression(NULL) {}

    /// Initialize with operator.
    /// \param _operator Operator (one of #Minus, #BoolNegate and #BitwiseNegate).
    /// \param _pExpression Subexpression.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    Unary(int _operator, Expression * _pExpression = NULL, bool _bReparent = true)
        : m_operator(_operator), m_pExpression(NULL)
    {
        _assign(m_pExpression, _pExpression, _bReparent);
    }

    /// Destructor.
    virtual ~Unary() { _delete(m_pExpression); }

    /// Get expression kind.
    /// \return #Unary.
    virtual int getKind() const { return UNARY; }

    /// Get operator.
    /// \return Operator (one of #Minus, #BoolNegate and #BitwiseNegate).
    int getOperator() const { return m_operator; }

    /// Set operator.
    /// \param _operator Operator (one of #Minus, #BoolNegate and #BitwiseNegate).
    void setOperator(int _operator) { m_operator = _operator; }

    /// Get subexpression to which the operator is applied.
    /// \return Subexpression.
    Expression * getExpression() const { return m_pExpression; }

    /// Set subexpression to which the operator is applied.
    /// \param _pExpression Subexpression.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setExpression(Expression * _pExpression, bool _bReparent = true) {
        _assign(m_pExpression, _pExpression, _bReparent);
    }

    /// Get or set overflow strategy.
    /// \return Reference to overflow handling descriptor.
    Overflow & getOverflow() { return m_overflow; }

private:
    int m_operator;
    Expression * m_pExpression;
    Overflow m_overflow;
};

/// Binary expression.
class Binary : public Expression {
public:
    /// Binary operator.
    enum {
        /// Binary plus.
        /// Addition (when applied to numeric values), set or array union, or
        /// string concatenation.
        ADD,
        /// Binary minus.
        /// Subtraction (when applied to numeric values) or set difference.
        SUBTRACT,
        /// Multiplication.
        /// Applicable only to numeric values.
        MULTIPLY,
        /// Division.
        /// Applicable only to numeric values.
        DIVIDE,
        /// Remainder.
        /// Applicable only to integral values.
        REMAINDER,
        /// Power.
        /// Applicable only to numeric values.
        POWER,
        /// Bitwise shift left.
        /// Equivalent to multiplication by corresponding power of 2.
        /// Applicable only to integral values.
        SHIFT_LEFT,
        /// Bitwise shift right.
        /// Equivalent to integer division by corresponding power of 2.
        /// Applicable only to integral values.
        SHIFT_RIGHT,
        /// Containment test.
        /// Applicable to sets, ranges, sequences, arrays and maps.
        IN,
        /// Less than comparison.
        /// Applicable only to numeric values.
        LESS,
        /// Less than or equals comparison.
        /// Applicable only to numeric values.
        LESS_OR_EQUALS,
        /// Greater than comparison.
        /// Applicable only to numeric values.
        GREATER,
        /// Greater than or equals comparison.
        /// Applicable only to numeric values.
        GREATER_OR_EQUALS,
        /// Equality comparison.
        EQUALS,
        /// Inequality comparison.
        NOT_EQUALS,
        /// Logical AND.
        /// Applicable only to boolean values.
        BOOL_AND,
        /// Logical OR.
        /// Applicable only to boolean values.
        BOOL_OR,
        /// Logical XOR.
        /// Applicable only to boolean values.
        BOOL_XOR,
        /// Bitwise AND.
        /// Applicable only to integral values.
        BITWISE_AND,
        /// Bitwise OR.
        /// Applicable only to integral values.
        BITWISE_OR,
        /// Bitwise XOR.
        /// Applicable only to integral values.
        BITWISE_XOR,
        /// Implication.
        /// Applicable only to boolean values.
        IMPLIES,
        /// Equivalence.
        /// Applicable only to boolean values.
        IFF
    };

    /// Default constructor.
    Binary() : m_operator(0), m_pLeft(NULL), m_pRight(NULL) {}

    /// Initialize with operator.
    /// \param _operator Operator (#Add, #Subtract, etc.)
    /// \param _pLeft Left subexpression.
    /// \param _pRight Right subexpression.
    /// \param _bReparent If specified (default) also sets parent of _pLeft and _pRight to this node.
    Binary(int _operator, Expression * _pLeft = NULL, Expression * _pRight = NULL, bool _bReparent = true)
        : m_operator(_operator), m_pLeft(NULL), m_pRight(NULL)
    {
        _assign(m_pLeft, _pLeft, _bReparent);
        _assign(m_pRight, _pRight, _bReparent);
    }

    /// Destructor.
    virtual ~Binary() {
        _delete(m_pLeft);
        _delete(m_pRight);
    }

    /// Get expression kind.
    /// \return #Binary.
    virtual int getKind() const { return BINARY; }

    /// Get operator.
    /// \return Operator (#Add, #Subtract, etc.)
    int getOperator() const { return m_operator; }

    /// Set operator.
    /// \param _operator  Operator (#Add, #Subtract, etc.)
    void setOperator(int _operator) { m_operator = _operator; }

    /// Get left operand.
    /// \return Subexpression.
    Expression * getLeftSide() const { return m_pLeft; }

    /// Set left operand.
    /// \param _pExpression Subexpression.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setLeftSide(Expression * _pExpression, bool _bReparent = true) {
        _assign(m_pLeft, _pExpression, _bReparent);
    }

    /// Get right operand.
    /// \return Subexpression.
    Expression * getRightSide() const { return m_pRight; }

    /// Set right operand.
    /// \param _pExpression Subexpression.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setRightSide(Expression * _pExpression, bool _bReparent = true) {
        _assign(m_pRight, _pExpression, _bReparent);
    }

    /// Get or set overflow strategy.
    /// \return Reference to overflow handling descriptor.
    Overflow & getOverflow() { return m_overflow; }

private:
    int m_operator;
    Expression * m_pLeft, * m_pRight;
    Overflow m_overflow;
};

/// Ternary ("condition ? then part : else part") expression.
class Ternary : public Expression {
public:
    /// Default constructor.
    Ternary() :  m_pIf(NULL), m_pThen(NULL), m_pElse(NULL) {}

    /// Initialize with operands.
    /// \param _pIf If-subexpression.
    /// \param _pThen Then-subexpression.
    /// \param _pElse Else-subexpression.
    /// \param _bReparent If specified (default) also sets parent of subexpressions to this node.
    Ternary(Expression * _pIf = NULL, Expression * _pThen = NULL, Expression * _pElse = NULL, bool _bReparent = true)
        : m_pIf(NULL), m_pThen(NULL), m_pElse(NULL)
    {
        _assign(m_pIf, _pIf, _bReparent);
        _assign(m_pThen, _pThen, _bReparent);
        _assign(m_pElse, _pElse, _bReparent);
    }

    /// Destructor.
    virtual ~Ternary() {
        _delete(m_pIf);
        _delete(m_pThen);
        _delete(m_pElse);
    }

    /// Get expression kind.
    /// \return #Ternary.
    virtual int getKind() const { return TERNARY; }

    /// Get logical condition.
    /// \return Condition.
    Expression * getIf() const { return m_pIf; }

    /// Set logical condition.
    /// \param _pExpression Condition.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setIf(Expression * _pExpression, bool _bReparent = true) {
        _assign(m_pIf, _pExpression, _bReparent);
    }

    /// Get 'then' part.
    /// \return Expression that should be evaluated if condition is true.
    Expression * getThen() const { return m_pThen; }

    /// Set 'then' part.
    /// \param _pExpression Expression that should be evaluated if condition is true.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setThen(Expression * _pExpression, bool _bReparent = true) {
        _assign(m_pThen, _pExpression, _bReparent);
    }

    /// Get 'else' part.
    /// \return Expression that should be evaluated if condition is false.
    Expression * getElse() const { return m_pElse; }

    /// Set 'else' part.
    /// \param _pExpression Expression that should be evaluated if condition is false.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setElse(Expression * _pExpression, bool _bReparent = true) {
        _assign(m_pElse, _pExpression, _bReparent);
    }

private:
    Expression * m_pIf, * m_pThen, * m_pElse;
};

/// Type as a part of expression.
class TypeExpr : public Expression {
public:
    /// Default constructor.
    TypeExpr() : m_pContents(NULL) {}

    /// Initialize with contained type.
    /// \param _pContents Contained type.
    /// \param _bReparent If specified (default) also sets parent of _pContents to this node.
    TypeExpr(Type * _pContents, bool _bReparent = true) : m_pContents(NULL) {
        _assign(m_pContents, _pContents, _bReparent);
    }

    /// Destructor.
    virtual ~TypeExpr() { _delete(m_pContents); }

    /// Get expression kind.
    /// \return #Type.
    virtual int getKind() const { return TYPE; }

    /// Get contained type.
    /// \return Contained type.
    Type * getContents() const { return m_pContents; }

    /// Set contained type.
    /// \param _pContents Contained type.
    /// \param _bReparent If specified (default) also sets parent of _pContents to this node.
    void setContents(Type * _pContents, bool _bReparent = true) {
        _assign(m_pContents, _pContents, _bReparent);
    }

private:
    Type * m_pContents;
};

/// Type as a part of expression.
class CastExpr : public Expression {
public:
    /// Default constructor.
    CastExpr() : m_pExpression(NULL), m_pToType(NULL) {}

    /// Initialize.
    /// \param _pExpr Expression being casted.
    /// \param _pToType Destination type..
    /// \param _bReparent If specified (default) also sets parent to this node.
    CastExpr(Expression * _pExpr, TypeExpr * _pToType, bool _bReparent = true) : m_pExpression(NULL), m_pToType(NULL) {
        _assign(m_pExpression, _pExpr, _bReparent);
        _assign(m_pToType, _pToType, _bReparent);
    }

    /// Destructor.
    virtual ~CastExpr() { _delete(m_pExpression); _delete(m_pToType); }

    /// Get expression kind.
    /// \return #Cast.
    virtual int getKind() const { return CAST; }

    /// Get expression being casted.
    /// \return Expression being casted.
    Expression * getExpression() const { return m_pExpression; }

    /// Get destination type.
    /// \return Destination type.
    TypeExpr * getToType() const { return m_pToType; }

    /// Set expression to cast.
    /// \param _pExpression Expression.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setExpression(Expression * _pExpression, bool _bReparent = true) {
        _assign(m_pExpression, _pExpression, _bReparent);
    }

    /// Set destination type.
    /// \param _pType Destination type.
    /// \param _bReparent If specified (default) also sets parent of _pType to this node.
    void setToType(TypeExpr * _pType, bool _bReparent = true) {
        _assign(m_pToType, _pType, _bReparent);
    }

private:
    Expression * m_pExpression;
    TypeExpr * m_pToType;
};

/// Possibly quantified logical formula.
class Formula : public Expression {
public:
    /// Quantifier.
    enum {
        /// No quantifier.
        NONE,
        /// Universal quantifier.
        UNIVERSAL,
        /// Existential quantifier.
        EXISTENTIAL
    };

    /// Default constructor.
    Formula() : m_quantifier(NONE), m_pSubformula(NULL) {}

    /// Initialize with quantifier and subformula.
    /// \param _quantifier Quantifier (one of #None, #Universal, #Existential).
    /// \param _pSubformula Formula.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    Formula(int _quantifier, Expression * _pSubformula = NULL, bool _bReparent = true) : m_quantifier(_quantifier), m_pSubformula(NULL) {
        _assign(m_pSubformula, _pSubformula, _bReparent);
    }

    /// Destructor.
    virtual ~Formula() { _delete(m_pSubformula); }

    /// Get expression kind.
    /// \return #Formula.
    virtual int getKind() const { return FORMULA; }

    /// Get quantifier.
    /// \return Quantifier (one of #None, #Universal, #Existential).
    int getQuantifier() const { return m_quantifier; }

    /// Set quantifier.
    /// \param _quantifier Quantifier (one of #None, #Universal, #Existential).
    void setQuantifier(int _quantifier) { m_quantifier = _quantifier; }

    /// Get list of bound variables.
    /// \return Refernce to bound variables list.
    NamedValues & getBoundVariables() { return m_boundVariables; }

    /// Get subformula.
    /// \return Formula. E.g. given quantified formula "! X . Y" this
    ///   method returns "Y".
    Expression * getSubformula() const { return m_pSubformula; }

    /// Set subformula.
    /// \param _pExpression Formula.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setSubformula(Expression * _pExpression, bool _bReparent = true) {
        _assign(m_pSubformula, _pExpression, _bReparent);
    }

private:
    int m_quantifier;
    Expression * m_pSubformula;
    NamedValues m_boundVariables;
};

/// Component of a compound value (e.g. array element or field of structure).
class Component : public Expression {
public:
    /// Component kind.
    enum {
        /// Part of an array (itself is another array).
        ARRAY_PART,
        /// Field of a structure.
        STRUCT_FIELD,
        /// Union alternative name. Only used in switch/case construct.
        /// The object can be cast to UnionAlternativeReference.
        UNION_ALTERNATIVE,
        /// Map element.
        MAP_ELEMENT,
        /// List element.
        LIST_ELEMENT,
        /// Replacement of a part of compound value.
        REPLACEMENT,
    };

    /// Default constructor.
    Component() : m_pObject(NULL) {}

    /// Destructor.
    virtual ~Component() { _delete(m_pObject); }

    /// Get expression kind.
    /// \return #Formula.
    virtual int getKind() const { return COMPONENT; }

    /// Get component kind (implemented in descendants).
    /// \return Component kind.
    virtual int getComponentKind() const = 0;

    /// Get expression to which the subscript is applied.
    /// \return Expression of compound type.
    Expression * getObject() const { return m_pObject; }

    /// Set expression to which the subscript is applied.
    /// \param _pExpression Expression of compound type.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setObject(Expression * _pExpression, bool _bReparent = true) {
        _assign(m_pObject, _pExpression, _bReparent);
    }

private:
    Expression * m_pObject;
};

/// Array or sequence element or array part.
class ArrayPartExpr : public Component {
public:
    /// Default constructor.
    ArrayPartExpr() {}

    /// Get component kind.
    /// \return #ArrayPart.
    virtual int getComponentKind() const { return ARRAY_PART; }

    /// Get list of subscripts.
    /// \return List of indices.
    Collection<Expression> & getIndices() { return m_indices; }

private:
    Collection<Expression> m_indices;
};

// Found in types.h
class StructType;

/// Structure field.
class StructFieldExpr : public Component {
public:
    StructFieldExpr(const std::wstring & _strField = L"") : m_strField(_strField) {}

    /// Get component kind.
    /// \return #StructField.
    virtual int getComponentKind() const { return STRUCT_FIELD; }

    const std::wstring & getFieldName() const { return m_strField; }

private:
    std::wstring m_strField;
};

/*

/// Structure field.
class StructFieldExpr : public Component {
public:
    /// Default constructor.
    StructFieldExpr() : m_pStructType(NULL), m_cFieldIdx(-1) {}

    /// Initialize with type and field.
    /// \param _pStructType Structure type.
    /// \param _cFieldIdx Structure field index.
    /// \param _bReparent If specified (default) also sets parent of _pStructType to this node.
    StructFieldExpr(Type * _pStructType, size_t _cFieldIdx, bool _bReparent = true) : m_pStructType(NULL), m_cFieldIdx(_cFieldIdx) {
        _assign(m_pStructType, (Type *) _pStructType, _bReparent);
    }

    /// Destructor.
    virtual ~StructFieldExpr() { _delete(m_pStructType); }

    /// Get component kind.
    /// \return #StructField.
    virtual int getComponentKind() const { return StructField; }

    /// Get corresponding structure type.
    /// \return Structure type.
    StructType * getStructType() const { return (StructType *) m_pStructType; }

    /// Set corresponding structure type.
    /// \param _pStructType Structure type.
    /// \param _bReparent If specified (default) also sets parent of _pStructType to this node.
    void setStructType(StructType * _pStructType, bool _bReparent = true) {
        _assign(m_pStructType, (Type *) _pStructType, _bReparent);
    }

    /// Get corresponding structure field.
    /// \return Structure field.
    const NamedValue * getField() const;

    /// Get corresponding structure field index.
    /// \return Structure field index.
    size_t getFieldIdx() const { return m_cFieldIdx; }

    /// Set corresponding structure field index.
    /// \param _pField Structure field.
    void setField(size_t _cFieldIdx) { m_cFieldIdx = _cFieldIdx; }

private:
    Type * m_pStructType;
    size_t m_cFieldIdx;
};

 */

class UnionType;
class UnionConstructorDeclaration;
typedef std::pair<size_t, size_t> UnionFieldIdx;

/// Union alternative name. Only used inside switch/case construct.
class UnionAlternativeExpr : public Component {
public:
    UnionAlternativeExpr(const std::wstring & _strName) :
        m_strName(_strName), m_pType(NULL), m_idx(-1, -1)
    {}

    UnionAlternativeExpr(const UnionType * _pType, const UnionFieldIdx & _idx);

    /// Get component kind.
    /// \return #UnionAlternative.
    virtual int getComponentKind() const { return UNION_ALTERNATIVE; }

    /// Get name of the alternative.
    /// \returns Identifier.
    const std::wstring & getName() const { return m_strName; }

    /// Set name of the alternative.
    /// \param _strName Identifier.
    void setName(const std::wstring & _strName) { m_strName = _strName; }

    /// Get corresponding union type.
    /// \return Union type.
    UnionType * getUnionType() const { return (UnionType *) m_pType; }

    /// Set corresponding union type.
    /// \param _pUnionType Union type.
    void setUnionType(const UnionType * _pUnionType) { m_pType = (const Type *) _pUnionType; }

    UnionFieldIdx getIdx() const { return m_idx; }

    void setIdx(size_t _cCons, size_t _cField) { m_idx = UnionFieldIdx(_cCons, _cField); }

    const NamedValue * getField() const;
    const UnionConstructorDeclaration * getConstructor() const;

private:
    std::wstring m_strName;
    const Type * m_pType;
    UnionFieldIdx m_idx;
};

/// Map element.
class MapElementExpr : public Component {
public:
    /// Default constructor.
    MapElementExpr() : m_pIndex(NULL) {}

    /// Destructor.
    virtual ~MapElementExpr() { _delete(m_pIndex); }

    /// Get component kind.
    /// \return #MapElement.
    virtual int getComponentKind() const { return MAP_ELEMENT; }

    /// Get index expression.
    /// \return Element index.
    Expression * getIndex() const { return m_pIndex; }

    /// Set index expression.
    /// \param _pExpression Element index.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setIndex(Expression * _pExpression, bool _bReparent = true) {
        _assign(m_pIndex, _pExpression, _bReparent);
    }

private:
    Expression * m_pIndex;
};

/// List element.
class ListElementExpr : public Component {
public:
    /// Default constructor.
    ListElementExpr() : m_pIndex(NULL) {}

    /// Destructor.
    virtual ~ListElementExpr() { _delete(m_pIndex); }

    /// Get component kind.
    /// \return #ListElement.
    virtual int getComponentKind() const { return LIST_ELEMENT; }

    /// Get index expression.
    /// \return Element index.
    Expression * getIndex() const { return m_pIndex; }

    /// Set index expression.
    /// \param _pExpression Element index.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setIndex(Expression * _pExpression, bool _bReparent = true) {
        _assign(m_pIndex, _pExpression, _bReparent);
    }

private:
    Expression * m_pIndex;
};

// Declared below.
class Constructor;

/// Replacement expression. Result is a new expression of compound type with
/// some elements replaced.
class Replacement : public Component {
public:
    /// Default constructor.
    Replacement() : m_pConstructor(NULL) {}

    /// Desctructor.
    virtual ~Replacement() { _delete(m_pConstructor); }

    /// Get component kind.
    /// \return #Replacement.
    virtual int getComponentKind() const { return REPLACEMENT; }

    /// Get expression of compound type containing new values.
    /// \return Expression containing new values.
    Constructor * getNewValues() const { return (Constructor *) m_pConstructor; }

    /// Set expression of compound type containing new values.
    /// \param _pConstructor Expression containing new values.
    /// \param _bReparent If specified (default) also sets parent of _pConstructor to this node.
    void setNewValues(Constructor * _pConstructor, bool _bReparent = true) {
        _assign(m_pConstructor, (Expression *) _pConstructor, _bReparent);
    }

private:
    Expression * m_pConstructor;
};

/// Function call as a part of an expression.
class FunctionCall : public Expression {
public:
    /// Default constructor.
    FunctionCall() : m_pPredicate(NULL) {}

    /// Destructor.
    virtual ~FunctionCall() { _delete(m_pPredicate); }

    virtual int getKind() const { return FUNCTION_CALL; }

    /// Get predicate expression which is called.
    /// \return Expression of predicate type.
    Expression * getPredicate() const { return m_pPredicate; }

    /// Set predicate expression which is called.
    /// \param _pExpression Expression of predicate type.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setPredicate(Expression * _pExpression, bool _bReparent = true) {
        _assign(m_pPredicate, _pExpression, _bReparent);
    }

    /// Get list of actual parameters.
    /// \return List of expressions.
    Collection<Expression> & getArgs() { return m_args; }
    const Collection<Expression> & getArgs() const { return m_args; }

    /// Get type of the expression.
    /// \returns Type associated with expression.
    virtual Type * getType() const;

private:
    Expression * m_pPredicate;
    Collection<Expression> m_args;
};
/*
class ParamBinding : public Node {
public:
    ParamBinding() : m_pValue(NULL), m_pParam(NULL) {}

    ParamBinding(Expression * _pValue, const Param * _pParam = NULL, bool _bReparent = true)
        : m_pValue(NULL), m_pParam(_pParam)
    {
        _assign(m_pValue, _pValue, _bReparent);
    }

    virtual ~ParamBinding() { _delete(m_pValue); }

    /// Get predicate expression which is called.
    /// \return Expression of predicate type.
    Expression * getValue() const { return m_pPredicate; }

    /// Set predicate expression which is called.
    /// \param _pExpression Expression of predicate type.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setValue(Expression * _pValue, bool _bReparent = true) {
        _assign(m_pValue, _pValue, _bReparent);
    }

    /// Get predicate expression which is called.
    /// \return Expression of predicate type.
    Expression * getParam() const { return m_pPredicate; }

    /// Set predicate expression which is called.
    /// \param _pExpression Expression of predicate type.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setParam(Param * _pValue, bool _bReparent = true) {
        _assign(m_pValue, _pValue, _bReparent);
    }

private:
    Expression * m_pValue;
    const Param * m_pParam;
};
*/

/// Partially applied predicate.
class Binder : public Expression {
public:
    /// Default constructor.
    Binder() : m_pPredicate(NULL) {}

    /// Destructor.
    virtual ~Binder() { _delete(m_pPredicate); }

    virtual int getKind() const { return BINDER; }

    /// Get predicate expression whose parameters are bound.
    /// \return Expression of predicate type.
    Expression * getPredicate() const { return m_pPredicate; }

    /// Set predicate expression.
    /// \param _pExpression Expression of predicate type.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setPredicate(Expression * _pExpression, bool _bReparent = true) {
        _assign(m_pPredicate, _pExpression, _bReparent);
    }

    /// Get list of bound parameters. Can contain NULL values.
    /// \return List of expressions.
    Collection<Expression> & getArgs() { return m_args; }

private:
    Expression * m_pPredicate;
    Collection<Expression> m_args;
};

class FormulaDeclaration;

/// Function call as a part of an expression.
class FormulaCall : public Expression {
public:
    /// Default constructor.
    FormulaCall() : m_pTarget(NULL) {}

    virtual int getKind() const { return FORMULA_CALL; }

    /// Get formula which is called.
    /// \return Formula declaration.
    const FormulaDeclaration * getTarget() const { return m_pTarget; }

    /// Set formula declaration which is called.
    /// \param _pFormula Formula.
    void setTarget(const FormulaDeclaration * _pFormula) { m_pTarget = _pFormula; }

    /// Get list of actual parameters.
    /// \return List of expressions.
    Collection<Expression> & getArgs() { return m_args; }

    std::wstring getName() const;

private:
    const FormulaDeclaration * m_pTarget;
    Collection<Expression> m_args;
};

/// Output branch of a predicate.
class Branch : public Params {
public:
    /// Default constructor.
    Branch() : m_pLabel(NULL), m_pPreCondition(NULL), m_pPostCondition(NULL) {}

    /// Destructor.
    virtual ~Branch() {
        _delete(m_pLabel);
        _delete(m_pPreCondition);
        _delete(m_pPostCondition);
    }

    /// Get label associated with the branch.
    /// Possibly NULL if it is the only branch.
    /// \return Label.
    Label * getLabel() const { return m_pLabel; }

    /// Set label associated with the branch.
    /// \param _pLabel Label of the output branch.
    /// \param _bReparent If specified (default) also sets parent of _pLabel to this node.
    void setLabel(Label * _pLabel, bool _bReparent = true) {
        _assign(m_pLabel, _pLabel, _bReparent);
    }

    /// Get branch precondition.
    /// \return Precondition.
    Formula * getPreCondition() const { return m_pPreCondition; }

    /// Set branch precondition.
    /// \param _pCondition Precondition.
    /// \param _bReparent If specified (default) also sets parent of _pCondition to this node.
    void setPreCondition(Formula * _pCondition, bool _bReparent = true) {
        _assign(m_pPreCondition, _pCondition, _bReparent);
    }

    /// Get branch precondition.
    /// \return Postcondition.
    Formula * getPostCondition() const { return m_pPostCondition; }

    /// Set branch postcondition.
    /// \param _pCondition Postcondition.
    /// \param _bReparent If specified (default) also sets parent of _pCondition to this node.
    void setPostCondition(Formula * _pCondition, bool _bReparent = true) {
        _assign(m_pPostCondition, _pCondition, _bReparent);
    }

private:
    Label * m_pLabel;
    Formula * m_pPreCondition, * m_pPostCondition;
};

/// Collection of output branches.
/// Used in predicate, process and lambda declarations.
/// \extends Node
class Branches : public Collection<Branch> {
};

class PredicateType;

/// Predicate declaration base (also used by Lambda).
class AnonymousPredicate : public Statement {
public:
    /// Default constructor.
    AnonymousPredicate() : m_pPreCond(NULL), m_pPostCond(NULL), m_pBlock(NULL), m_pType(NULL) {}

    /// Destructor.
    virtual ~AnonymousPredicate();

    /// Get list of formal input parameters.
    /// \return List of parameters.
    Params & getInParams() { return m_paramsIn; }
    const Params & getInParams() const { return m_paramsIn; }

    /// Get list of output branches. Each branch can contain a list of parameters,
    /// a precondition and a postcondition.
    /// \return List of branches.
    Branches & getOutParams() { return m_paramsOut; }
    const Branches & getOutParams() const { return m_paramsOut; }

    /// Set predicate body.
    /// \param _pBlock Predicate body.
    /// \param _bReparent If specified (default) also sets parent of _pBlock to this node.
    void setBlock(Block * _pBlock, bool _bReparent = true) {
        _assign(m_pBlock, _pBlock, _bReparent);
    }

    /// Get predicate body.
    /// \return Predicate body.
    Block * getBlock() const { return m_pBlock; }

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

    // Check if the statement ends like a block (i.e. separating semicolon is not needed).
    // \return True.
    virtual bool isBlockLike() const { return true; }

    PredicateType * getType() const {
        if (! m_pType) updateType();
        return m_pType;
    }

    void updateType() const;

private:
    Params m_paramsIn;
    Branches m_paramsOut;
    Formula * m_pPreCond, * m_pPostCond;
    Block * m_pBlock;
    mutable PredicateType * m_pType;
};

/// Anonymous predicate.
class Lambda : public Expression {
public:
    /// Default constructor.
    Lambda() {}

    /// Get expression kind.
    /// \return #Lambda.
    virtual int getKind() const { return LAMBDA; }

    AnonymousPredicate & getPredicate() { return m_pred; }

private:
    AnonymousPredicate m_pred;
};

/// Indexed element.
class ElementDefinition : public Node {
public:
    /// Default constructor.
    ElementDefinition() : m_pIndex(NULL), m_pValue(NULL) {}

    virtual ~ElementDefinition() {
        _delete(m_pIndex);
        _delete(m_pValue);
    }

    virtual int getNodeKind() const { return Node::ELEMENT_DEFINITION; }

    /// Get index expression.
    /// \return Element index.
    Expression * getIndex() const { return m_pIndex; }

    /// Set index expression.
    /// \param _pExpression Element index.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setIndex(Expression * _pExpression, bool _bReparent = true) {
        _assign(m_pIndex, _pExpression, _bReparent);
    }

    /// Get value expression.
    /// \return Element value.
    Expression * getValue() const { return m_pValue; }

    /// Set value expression.
    /// \param _pExpression Element value.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setValue(Expression * _pExpression, bool _bReparent = true) {
        _assign(m_pValue, _pExpression, _bReparent);
    }

private:
    Expression * m_pIndex;
    Expression * m_pValue;
};

/// Literal of \c struct type.
class StructFieldDefinition : public Node {
public:
    /// Default constructor.
    StructFieldDefinition() : m_pValue(NULL) {}

    /// Initialize with struct and field references.
    /// \param _pStructType Type of structure.
    /// \param _pField Field.
    StructFieldDefinition(const StructType * _pStructType, const NamedValue * _pField)
        : m_pValue(NULL), m_pStructType(_pStructType), m_pField(_pField) {}

    /// Destructor.
    virtual ~StructFieldDefinition() { _delete(m_pValue); }

    virtual int getNodeKind() const { return Node::STRUCT_FIELD_DEFINITION; }

    /// Get type of structure.
    /// \return Type of structure.
    const StructType * getStructType() const { return m_pStructType; }

    /// Set type of structure.
    /// \param _pStructType Type of structure.
    void setStructType(const StructType * _pStructType) { m_pStructType = _pStructType; }

    /// Get field reference.
    /// \return Field reference.
    const NamedValue * getField() const { return m_pField; }

    /// Set field reference.
    /// \param _pField Field reference.
    void setField(const NamedValue * _pField) { m_pField = _pField; }

    /// Get value expression.
    /// \return Field value.
    Expression * getValue() const { return m_pValue; }

    /// Set value expression.
    /// \param _pExpression Field value.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setValue(Expression * _pExpression, bool _bReparent = true) {
        _assign(m_pValue, _pExpression, _bReparent);
    }

    /// Get name of the field.
    /// \returns Identifier.
    const std::wstring & getName() const { return m_strName; }

    /// Set name of the field.
    /// \param _strName Identifier.
    void setName(const std::wstring & _strName) { m_strName = _strName; }

private:
    Expression * m_pValue;
    const StructType * m_pStructType;
    const NamedValue * m_pField;
    std::wstring m_strName;
};

/// Constructor of compound value (array, set, struct, etc.)
class Constructor : public Expression {
public:
    /// Kind of constructor.
    enum {
        /// Struct initializer.
        STRUCT_FIELDS,
        /// Array initializer.
        ARRAY_ELEMENTS,
        /// Set initializer.
        SET_ELEMENTS,
        /// Map initializer.
        MAP_ELEMENTS,
        /// List initializer.
        LIST_ELEMENTS,
        /// Array generator.
        ARRAY_ITERATION,
        /// Union constructor.
        UNION_CONSTRUCTOR,
    };

    /// Default constructor.
    Constructor() {}

    /// Get expression kind.
    /// \return #Constructor.
    virtual int getKind() const { return CONSTRUCTOR; }

    /// Get constructor kind (implemented in descendants).
    /// \return Constructor kind.
    virtual int getConstructorKind() const = 0;
};

/// Structure value. \extends Constructor
/// This object is a collection of StructFieldDefinition instances, use methods
/// of Collection to access field definitions.
class StructConstructor : public Collection<StructFieldDefinition, Constructor> {
public:
    /// Default constructor.
    StructConstructor() {}

    /// Get constructor kind.
    /// \return #StructFields.
    virtual int getConstructorKind() const { return STRUCT_FIELDS; }
};

class VariableDeclaration;
class UnionConstructorDeclaration;

/// Union value. \extends Constructor
/// The class extends struct constructor with union constructor name.
/// May contain not-fully defined fields (used in switch construct).
class UnionConstructor : public StructConstructor {
public:
    UnionConstructor() : m_pProto(NULL) {}

    /// Get constructor kind.
    /// \return #UnionConstructor.
    virtual int getConstructorKind() const { return UNION_CONSTRUCTOR; }

    /// Get list of variables declared as part of the constructor.
    /// \return List of variables.
    Collection<VariableDeclaration> & getDeclarations() { return m_decls; }
    const Collection<VariableDeclaration> & getDeclarations() const { return m_decls; }

    bool isComplete() const;

    UnionConstructorDeclaration * getPrototype() const { return m_pProto; }
    void setPrototype(UnionConstructorDeclaration * _pProto) { m_pProto = _pProto; }

private:
    Collection<VariableDeclaration> m_decls;
    UnionConstructorDeclaration * m_pProto;
};

/// Array value. \extends Constructor
/// This object is a collection of ElementDefinition instances, use methods
/// of Collection to access element definitions.
class ArrayConstructor : public Collection<ElementDefinition, Constructor> {
public:
    /// Default constructor.
    ArrayConstructor() {}

    /// Get constructor kind.
    /// \return #ArrayElements.
    virtual int getConstructorKind() const { return ARRAY_ELEMENTS; }
};

/// Map value. \extends Constructor
/// This object is a collection of ElementDefinition instances, use methods
/// of Collection to access element definitions.
class MapConstructor : public Collection<ElementDefinition, Constructor> {
public:
    /// Default constructor.
    MapConstructor() {}

    /// Get constructor kind.
    /// \return #MapElements.
    virtual int getConstructorKind() const { return MAP_ELEMENTS; }
};

/// Set value. \extends Constructor
/// This object is a collection of Expression instances, use methods
/// of Collection to access element definitions.
class SetConstructor : public Collection<Expression, Constructor> {
public:
    /// Default constructor.
    SetConstructor() {}

    /// Get constructor kind.
    /// \return #SetElements.
    virtual int getConstructorKind() const { return SET_ELEMENTS; }
};

/// List value. \extends Constructor
/// This object is a collection of Expression instances, use methods
/// of Collection to access element definitions.
class ListConstructor : public Collection<Expression, Constructor> {
public:
    /// Default constructor.
    ListConstructor() {}

    /// Get constructor kind.
    /// \return #ListElements.
    virtual int getConstructorKind() const { return LIST_ELEMENTS; }
};

/// Definition of array part.Consider array generator:
/// \code
/// a = for (0..10 i) {
///   case 0..4  : 'a'
///   case 5..10 : 'b'
/// }
/// \endcode
/// In the above example "case 0..4  : 'a'" and "case 5..10  : 'b'" are array parts.
class ArrayPartDefinition : public Node {
public:
    /// Default constructor.
    ArrayPartDefinition() : m_pExpression(NULL) {}

    /// Destructor.
    virtual ~ArrayPartDefinition() { _delete(m_pExpression); }

    virtual int getNodeKind() const { return Node::ARRAY_PART_DEFINITION; }

    /// Get \c case conditions.
    /// \return List of expressions (can contain ranges).
    Collection<Expression> & getConditions() { return m_conditions; }

    /// Get expression.
    /// \return Expression.
    Expression * getExpression() const { return m_pExpression; }

    /// Set expression.
    /// \param _pExpression Expression.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setExpression(Expression * _pExpression, bool _bReparent = true) {
        _assign(m_pExpression, _pExpression, _bReparent);
    }

private:
    Expression * m_pExpression;
    Collection<Expression> m_conditions;
};

/// Array constructor by means of iteration. \extends Constructor
/// Example:
/// \code
/// for (0..15 i) {
///   case 2..7   : 'a'
///   case 10..12 : 'b'
///   default     : 'c'
/// }
/// \endcode
class ArrayIteration : public Collection<ArrayPartDefinition, Constructor> {
public:
    /// Default constructor.
    ArrayIteration() : m_pDefault(NULL) {}

    /// Destructor.
    virtual ~ArrayIteration() { _delete(m_pDefault); }

    /// Get constructor kind.
    /// \return #ArrayIteration.
    virtual int getConstructorKind() const { return ARRAY_ITERATION; }

    /// Get list of iterator variables.
    /// \return Iterator variables.
    NamedValues & getIterators() { return m_iterators; }

    /// Get expression for default alternative.
    /// \return Expression.
    Expression * getDefault() const { return m_pDefault; }

    /// Set expression for default alternative.
    /// \param _pExpression Expression.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setDefault(Expression * _pExpression, bool _bReparent = true) {
        _assign(m_pDefault, _pExpression, _bReparent);
    }

private:
    Expression * m_pDefault;
    NamedValues m_iterators;
};

} // namespace ir

#endif /* EXPRESSIONS_H_ */
