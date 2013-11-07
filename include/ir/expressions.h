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
    void set(int _overflow, const LabelPtr &_pLabel = NULL) { m_overflow = _overflow; m_pLabel = _pLabel; }

    /// Set overflow handling strategy.
    /// \param _other Overflow handling strategy and (optionally) return label.
    void set(const Overflow &_other) { set(_other.get(), _other.getLabel()); }

    /// \return Pointer to Label object (assuming type is IntOverflow or
    ///     RealOverflow and overflow handling strategy is Label.)
    const LabelPtr &getLabel() const { return m_pLabel; }

private:
    int m_overflow;
    LabelPtr m_pLabel;
};

class Matches;
typedef Auto<Matches> MatchesPtr;

/// Virtual ancestor of all expressions.
class Expression : public Node {
public:
    /// Kind of the expression.
    enum {
        /// Wild expression
        WILD,
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

    virtual int getNodeKind() const { return Node::EXPRESSION; }

    /// Get expression kind.
    /// \returns Expression kind.
    virtual int getKind() const = 0;

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;

    virtual bool matches(const Expression& _other, MatchesPtr _pMatches = NULL) const;
    static bool matches(const ExpressionPtr& _pLeft, const ExpressionPtr& _pRight, MatchesPtr _pMatches = NULL) {
        return _matches(_pLeft, _pRight, _pMatches);
    }

    static void substitute(ExpressionPtr& _pExpr, Matches& _matches);
    static NodePtr substitute(const ir::NodePtr &_pNode, const ir::ExpressionPtr &_pFrom, const ir::ExpressionPtr &_pTo);

    static bool implies(const ExpressionPtr& _pLeft, const ExpressionPtr& _pRight);
    bool implies(const Expression& _other) const {
        return implies(this, &_other);
    }

    /// Get type of the expression.
    /// \returns Type associated with expression.
    virtual TypePtr getType() const { return m_pType; }

    /// Set type of the expression.
    /// \param _pType Type associated with expression.
    void setType(const TypePtr &_pType) { m_pType = _pType; }

protected:
    static bool _matches(const ExpressionPtr& _pLeft, const ExpressionPtr& _pRight, MatchesPtr _pMatches = NULL);
    static bool matchNamedValues(const NamedValues& _left, const NamedValues& _right);
    static bool matchCollections(const Collection<Expression>& _left, const Collection<Expression>& _right, MatchesPtr _pMatches = NULL);

private:
    TypePtr m_pType;
};

// Wild expression.
class Wild : public Expression {
public:
    Wild(const std::wstring &_name) : m_strName(_name) {}

    virtual int getKind() const { return WILD; }

    const std::wstring &getName() const { return m_strName; }

    void setName(const std::wstring &_strName) { m_strName = _strName; }

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, Wild(m_strName));
    }

private:
    std::wstring m_strName;
};

template<class _Marker>
class MarkedMap : public Node {
public:
    MarkedMap() {}
    void addExpression(const _Marker& _mark, const ExpressionPtr& _pExpr) {
        m_map.insert(std::make_pair(&_mark, _pExpr));
    }
    ExpressionPtr getExpression(const _Marker& _mark) {
        typename std::map<Auto<_Marker>, ExpressionPtr, PtrLess<_Marker> >::iterator it = m_map.find(&_mark);
        if (it != m_map.end())
            return it->second;
        return NULL;
    }
    void swap(MarkedMap& _other) {
        m_map.swap(_other.m_map);
    }
protected:
    std::map<Auto<_Marker>, ExpressionPtr, PtrLess<_Marker> > m_map;
};

class Matches : public MarkedMap<Wild> {
public:
    Matches() {}
    ExpressionPtr getExprByName(const std::wstring& _sName) {
        return getExpression(Wild(_sName));
    }
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
    Literal() {}

    /// Copy constructor.
    Literal(const Literal &_other) :
        m_literalKind(_other.m_literalKind), m_string(_other.m_string),
        m_char(_other.m_char), m_number(_other.m_number), m_bool(_other.m_bool) {}

    /// Initialize the literal with numeric value (sets kind to #Number).
    /// \param _number Value.
    Literal(const Number &_number) : m_literalKind(NUMBER), m_number(_number) {}

    /// Initialize the literal with boolean value (sets kind to #Bool).
    /// \param _number Value.
    Literal(bool _b) : m_literalKind(BOOL), m_bool(_b) {}

    /// Initialize the literal with character value (sets kind to #Char).
    /// \param _c Value.
    Literal(wchar_t _c) : m_literalKind(CHAR), m_char(_c) {}

    /// Initialize the literal with string value (sets kind to #String).
    /// \param _str Value.
    Literal(const std::wstring &_str) : m_literalKind(STRING), m_string(_str) {}

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
    const Number &getNumber() const { return m_number; }

    /// Set numeric value. Also changes kind to #Number.
    /// \param _number Value.
    void setNumber(const Number &_number) {
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
    const std::wstring &getString() const { return m_string; }

    /// Set string value. Also changes kind to #String.
    /// \param _str Value.
    void setString(const std::wstring &_str) {
        m_literalKind = STRING;
        m_string = _str;
    }

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;
    virtual bool matches(const Expression& _other, MatchesPtr _pMatches = NULL) const;

    virtual NodePtr clone(Cloner &_cloner) const { return NEW_CLONE(this, _cloner, Literal(*this)); }

private:
    int m_literalKind = UNIT;
    std::wstring m_string;
    wchar_t m_char = 0;
    Number m_number;
    bool m_bool = false;
};

/// Expression containing reference to a variable or a parameter.
class VariableReference : public Expression {
public:
    /// Default constructor.
    VariableReference() : m_pTarget(NULL) {}

    /// Initialize using name.
    /// \param _strName Identifier.
    /// \param _pTarget Referenced variable.
    VariableReference(const std::wstring &_strName, const NamedValuePtr &_pTarget = NULL) :
        m_pTarget(_pTarget), m_strName(_strName)
    {
        if (_pTarget)
            setType(_pTarget->getType());
    }

    /// Initialize using referenced variable. Name is set accordingly.
    /// \param _pTarget Referenced variable.
    VariableReference(const NamedValuePtr &_pTarget) {
        setTarget(_pTarget);
    }

    /// Get expression kind.
    /// \return #Var.
    virtual int getKind() const { return VAR; }

    /// Get name of the variable.
    /// \returns Identifier.
    const std::wstring &getName() const { return m_strName; }

    /// Set name of the variable.
    /// \param _strName Identifier.
    void setName(const std::wstring &_strName) { m_strName = _strName; }

    /// Get referenced variable.
    /// \return Referenced variable.
    const NamedValuePtr &getTarget() const { return m_pTarget; }

    /// Set referenced variable.
    /// \param _pTarget Referenced variable.
    void setTarget(const NamedValuePtr &_pTarget) {
        m_pTarget = _pTarget;
        if (_pTarget) {
            m_strName = _pTarget->getName();
            setType(_pTarget->getType());
        }
    }

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;
    virtual bool matches(const Expression& _other, MatchesPtr _pMatches = NULL) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, VariableReference(m_strName, _cloner.get(m_pTarget, true)));
    }

private:
    NamedValuePtr m_pTarget;
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
    /// \param _pTarget Referenced variable.
    PredicateReference(const std::wstring &_strName, const PredicatePtr &_pTarget = NULL, const TypePtr &_pType = NULL) :
        m_pTarget(_pTarget), m_strName(_strName)
    {
        setType(_pType);
    }

    /// Initialize using referenced predicate.
    /// \param _pTarget Referenced variable.
    PredicateReference(const PredicatePtr &_pTarget) : m_pTarget(_pTarget) {}

    /// Get expression kind.
    /// \return #Predicate.
    virtual int getKind() const { return PREDICATE; }

    /// Get name of the predicate.
    /// \returns Identifier.
    const std::wstring &getName() const;

    /// Set name of the predicate.
    /// \param _strName Identifier.
    void setName(const std::wstring &_strName) { m_strName = _strName; }

    /// Get referenced predicate.
    /// \return Referenced predicate.
    const PredicatePtr &getTarget() const { return m_pTarget; }

    /// Set referenced predicate.
    /// \param _pTarget Referenced predicate.
    void setTarget(const PredicatePtr &_pTarget) { m_pTarget = _pTarget; }

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;
    virtual bool matches(const Expression& _other, MatchesPtr _pMatches = NULL) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, PredicateReference(m_strName, _cloner.get(m_pTarget, true), _cloner.get(getType())));
    }

private:
    PredicatePtr m_pTarget;
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
    /// \param _overflow Overflow handling strategy.
    Unary(int _operator, const ExpressionPtr &_pExpression = NULL, const Overflow &_overflow = Overflow()) :
        m_operator(_operator), m_pExpression(_pExpression), m_overflow(_overflow) {}

    /// Get expression kind.
    /// \return #Unary.
    virtual int getKind() const { return UNARY; }

    static int getPrecedence(int _operator);

    /// Get operator.
    /// \return Operator (one of #Minus, #BoolNegate and #BitwiseNegate).
    int getOperator() const { return m_operator; }

    /// Set operator.
    /// \param _operator Operator (one of #Minus, #BoolNegate and #BitwiseNegate).
    void setOperator(int _operator) { m_operator = _operator; }

    /// Get subexpression to which the operator is applied.
    /// \return Subexpression.
    const ExpressionPtr &getExpression() const { return m_pExpression; }

    /// Set subexpression to which the operator is applied.
    /// \param _pExpression Subexpression.
    void setExpression(const ExpressionPtr &_pExpression) { m_pExpression = _pExpression; }

    /// Get or set overflow strategy.
    /// \return Reference to overflow handling descriptor.
    Overflow &getOverflow() { return m_overflow; }

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;
    virtual bool matches(const Expression& _other, MatchesPtr _pMatches = NULL) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, Unary(m_operator, _cloner.get(m_pExpression), m_overflow));
    }

private:
    int m_operator;
    ExpressionPtr m_pExpression;
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
    /// \param _overflow Overflow handling strategy.
    Binary(int _operator, const ExpressionPtr &_pLeft = NULL, const ExpressionPtr &_pRight = NULL,
            const Overflow &_overflow = Overflow()) :
        m_operator(_operator), m_pLeft(_pLeft), m_pRight(_pRight), m_overflow(_overflow) { }

    /// Get expression kind.
    /// \return #Binary.
    virtual int getKind() const { return BINARY; }

    static int getPrecedence(int _operator);

    /// Get operator.
    /// \return Operator (#Add, #Subtract, etc.)
    int getOperator() const { return m_operator; }

    /// Set operator.
    /// \param _operator  Operator (#Add, #Subtract, etc.)
    void setOperator(int _operator) { m_operator = _operator; }

    /// Get left operand.
    /// \return Subexpression.
    const ExpressionPtr &getLeftSide() const { return m_pLeft; }

    /// Set left operand.
    /// \param _pExpression Subexpression.
    void setLeftSide(const ExpressionPtr &_pExpression) { m_pLeft = _pExpression; }

    /// Get right operand.
    /// \return Subexpression.
    const ExpressionPtr &getRightSide() const { return m_pRight; }

    /// Set right operand.
    /// \param _pExpression Subexpression.
    void setRightSide(const ExpressionPtr &_pExpression) { m_pRight = _pExpression; }

    /// Get or set overflow strategy.
    /// \return Reference to overflow handling descriptor.
    Overflow &getOverflow() { return m_overflow; }

    bool isSymmetrical() const;
    int getInverseOperator() const;

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;
    virtual bool matches(const Expression& _other, MatchesPtr _pMatches = NULL) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, Binary(m_operator, _cloner.get(m_pLeft), _cloner.get(m_pRight), m_overflow));
    }

private:
    int m_operator;
    ExpressionPtr m_pLeft, m_pRight;
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
    Ternary(const ExpressionPtr &_pIf = NULL, const ExpressionPtr &_pThen = NULL, const ExpressionPtr &_pElse = NULL)
        : m_pIf(_pIf), m_pThen(_pThen), m_pElse(_pElse) { }

    /// Get expression kind.
    /// \return #Ternary.
    virtual int getKind() const { return TERNARY; }

    static int getPrecedence();

    /// Get logical condition.
    /// \return Condition.
    const ExpressionPtr &getIf() const { return m_pIf; }

    /// Set logical condition.
    /// \param _pExpression Condition.
    void setIf(const ExpressionPtr &_pExpression) { m_pIf = _pExpression; }

    /// Get 'then' part.
    /// \return Expression that should be evaluated if condition is true.
    const ExpressionPtr &getThen() const { return m_pThen; }

    /// Set 'then' part.
    /// \param _pExpression Expression that should be evaluated if condition is true.
    void setThen(const ExpressionPtr &_pExpression) { m_pThen = _pExpression; }

    /// Get 'else' part.
    /// \return Expression that should be evaluated if condition is false.
    const ExpressionPtr &getElse() const { return m_pElse; }

    /// Set 'else' part.
    /// \param _pExpression Expression that should be evaluated if condition is false.
    void setElse(const ExpressionPtr &_pExpression) { m_pElse = _pExpression; }

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;
    virtual bool matches(const Expression& _other, MatchesPtr _pMatches = NULL) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, Ternary(_cloner.get(m_pIf), _cloner.get(m_pThen), _cloner.get(m_pElse)));
    }

private:
    ExpressionPtr m_pIf, m_pThen, m_pElse;
};

/// Type as a part of expression.
class TypeExpr : public Expression {
public:
    /// Default constructor.
    TypeExpr() : m_pContents(NULL) {}

    /// Initialize with contained type.
    /// \param _pContents Contained type.
    TypeExpr(const TypePtr &_pContents) : m_pContents(_pContents) {}

    /// Get expression kind.
    /// \return #Type.
    virtual int getKind() const { return TYPE; }

    /// Get contained type.
    /// \return Contained type.
    const TypePtr &getContents() const { return m_pContents; }

    /// Set contained type.
    /// \param _pContents Contained type.
    void setContents(const TypePtr &_pContents) { m_pContents = _pContents; }

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;
    virtual bool matches(const Expression& _other, MatchesPtr _pMatches = NULL) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, TypeExpr(_cloner.get(m_pContents)));
    }

private:
    TypePtr m_pContents;
};

/// Type as a part of expression.
class CastExpr : public Expression {
public:
    /// Default constructor.
    CastExpr() : m_pExpression(NULL), m_pToType(NULL) {}

    /// Initialize.
    /// \param _pExpr Expression being casted.
    /// \param _pToType Destination type..
    CastExpr(const ExpressionPtr &_pExpr, const TypeExprPtr &_pToType) : m_pExpression(_pExpr), m_pToType(_pToType) {}

    /// Get expression kind.
    /// \return #Cast.
    virtual int getKind() const { return CAST; }

    /// Get expression being casted.
    /// \return Expression being casted.
    const ExpressionPtr &getExpression() const { return m_pExpression; }

    /// Get destination type.
    /// \return Destination type.
    const TypeExprPtr &getToType() const { return m_pToType; }

    /// Set expression to cast.
    /// \param _pExpression Expression.
    void setExpression(const ExpressionPtr &_pExpression) { m_pExpression = _pExpression; }

    /// Set destination type.
    /// \param _pType Destination type.
    void setToType(const TypeExprPtr &_pType) { m_pToType = _pType; }

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;
    virtual bool matches(const Expression& _other, MatchesPtr _pMatches = NULL) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, CastExpr(_cloner.get(m_pExpression), _cloner.get(m_pToType)));
    }

private:
    ExpressionPtr m_pExpression;
    TypeExprPtr m_pToType;
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
    Formula(int _quantifier, const ExpressionPtr &_pSubformula = NULL) : m_quantifier(_quantifier), m_pSubformula(_pSubformula) {}

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
    /// \return Reference to bound variables list.
    NamedValues &getBoundVariables() { return m_boundVariables; }
    const NamedValues &getBoundVariables() const { return m_boundVariables; }

    /// Get subformula.
    /// \return Formula. E.g. given quantified formula "! X . Y" this
    ///   method returns "Y".
    const ExpressionPtr &getSubformula() const { return m_pSubformula; }

    /// Set subformula.
    /// \param _pExpression Formula.
    void setSubformula(const ExpressionPtr &_pExpression) { m_pSubformula = _pExpression; }

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;
    virtual bool matches(const Expression& _other, MatchesPtr _pMatches = NULL) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        FormulaPtr pFormula = NEW_CLONE(this, _cloner, Formula(m_quantifier, _cloner.get(m_pSubformula)));
        pFormula->getBoundVariables().appendClones(getBoundVariables(), _cloner);
        return pFormula;
    }

private:
    int m_quantifier;
    ExpressionPtr m_pSubformula;
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
    /// \param _pObject Expression of compound type.
    Component(const ExpressionPtr &_pObject = NULL) : m_pObject(_pObject) {}

    /// Get expression kind.
    /// \return #Formula.
    virtual int getKind() const { return COMPONENT; }

    /// Get component kind (implemented in descendants).
    /// \return Component kind.
    virtual int getComponentKind() const = 0;

    /// Get expression to which the subscript is applied.
    /// \return Expression of compound type.
    const ExpressionPtr &getObject() const { return m_pObject; }

    /// Set expression to which the subscript is applied.
    /// \param _pExpression Expression of compound type.
    void setObject(const ExpressionPtr &_pExpression) { m_pObject = _pExpression; }

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;
    virtual bool matches(const Expression& _other, MatchesPtr _pMatches = NULL) const;

private:
    ExpressionPtr m_pObject;
};

/// Array or sequence element or array part.
class ArrayPartExpr : public Component {
public:
    /// Default constructor.
    /// \param _pObject Expression of compound type.
    ArrayPartExpr(const ExpressionPtr &_pObject = NULL) : Component(_pObject) {}

    /// Get component kind.
    /// \return #ArrayPart.
    virtual int getComponentKind() const { return ARRAY_PART; }

    /// Get list of subscripts.
    /// \return List of indices.
    Collection<Expression> &getIndices() { return m_indices; }
    const Collection<Expression> &getIndices() const { return m_indices; }

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;
    virtual bool matches(const Expression& _other, MatchesPtr _pMatches = NULL) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        ArrayPartExprPtr pExpr = NEW_CLONE(this, _cloner, ArrayPartExpr(_cloner.get(getObject())));
        pExpr->getIndices().appendClones(getIndices(), _cloner);
        return pExpr;
    }

private:
    Collection<Expression> m_indices;
};

// Found in types.h
class StructType;

/// Structure field.
class FieldExpr : public Component {
public:
    /// Default constructor.
    /// \param _strField Field name.
    /// \param _pObject Expression of compound type.
    FieldExpr(const std::wstring &_strField = L"", const ExpressionPtr &_pObject = NULL) :
        Component(_pObject), m_strField(_strField) {}

    /// Get component kind.
    /// \return #StructField.
    virtual int getComponentKind() const { return STRUCT_FIELD; }

    const std::wstring &getFieldName() const { return m_strField; }

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;
    virtual bool matches(const Expression& _other, MatchesPtr _pMatches = NULL) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, FieldExpr(m_strField, _cloner.get(getObject())));
    }

private:
    std::wstring m_strField;
};

class UnionType;
class UnionConstructorDeclaration;
typedef std::pair<size_t, size_t> UnionFieldIdx;

/// Union alternative name. Only used inside switch/case construct.
class UnionAlternativeExpr : public Component {
public:
    UnionAlternativeExpr(const std::wstring & _strName, const ExpressionPtr &_pObject = NULL,
            const UnionTypePtr &_pType = NULL, const UnionFieldIdx &_idx = UnionFieldIdx(-1, -1)) :
        Component(_pObject), m_strName(_strName), m_pType(_pType), m_idx(_idx)
    {}

    UnionAlternativeExpr(const UnionTypePtr &_pType, const UnionFieldIdx &_idx);

    /// Get component kind.
    /// \return #UnionAlternative.
    virtual int getComponentKind() const { return UNION_ALTERNATIVE; }

    /// Get name of the alternative.
    /// \returns Identifier.
    const std::wstring &getName() const { return m_strName; }

    /// Set name of the alternative.
    /// \param _strName Identifier.
    void setName(const std::wstring &_strName) { m_strName = _strName; }

    /// Get corresponding union type.
    /// \return Union type.
    UnionTypePtr getUnionType() const { return m_pType.as<UnionType>(); }

    /// Set corresponding union type.
    /// \param _pUnionType Union type.
    void setUnionType(const UnionTypePtr &_pUnionType) { m_pType = _pUnionType; }

    UnionFieldIdx getIdx() const { return m_idx; }

    void setIdx(size_t _cCons, size_t _cField) { m_idx = UnionFieldIdx(_cCons, _cField); }

    NamedValuePtr getField() const;
    UnionConstructorDeclarationPtr getConstructor() const;

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;
    virtual bool matches(const Expression& _other, MatchesPtr _pMatches = NULL) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, UnionAlternativeExpr(getName(), _cloner.get(getObject()),
                _cloner.get(getUnionType()), m_idx));
    }

private:
    std::wstring m_strName;
    TypePtr m_pType;
    UnionFieldIdx m_idx;
};

/// Map element.
class MapElementExpr : public Component {
public:
    /// Default constructor.
    /// \param _pIndex Element index.
    /// \param _pObject Expression of compound type.
    MapElementExpr(const ExpressionPtr &_pIndex = NULL, const ExpressionPtr &_pObject = NULL) :
        Component(_pObject), m_pIndex(_pIndex) {}

    /// Get component kind.
    /// \return #MapElement.
    virtual int getComponentKind() const { return MAP_ELEMENT; }

    /// Get index expression.
    /// \return Element index.
    const ExpressionPtr &getIndex() const { return m_pIndex; }

    /// Set index expression.
    /// \param _pExpression Element index.
    void setIndex(const ExpressionPtr &_pExpression) { m_pIndex = _pExpression; }

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;
    virtual bool matches(const Expression& _other, MatchesPtr _pMatches = NULL) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, MapElementExpr(_cloner.get(getIndex()), _cloner.get(getObject())));
    }

private:
    ExpressionPtr m_pIndex;
};

/// List element.
class ListElementExpr : public Component {
public:
    /// Default constructor.
    /// \param _pIndex Element index.
    /// \param _pObject Expression of compound type.
    ListElementExpr(const ExpressionPtr &_pIndex = NULL, const ExpressionPtr &_pObject = NULL) :
        Component(_pObject), m_pIndex(_pIndex) {}

    /// Get component kind.
    /// \return #ListElement.
    virtual int getComponentKind() const { return LIST_ELEMENT; }

    /// Get index expression.
    /// \return Element index.
    const ExpressionPtr &getIndex() const { return m_pIndex; }

    /// Set index expression.
    /// \param _pExpression Element index.
    void setIndex(const ExpressionPtr &_pExpression) { m_pIndex = _pExpression; }

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;
    virtual bool matches(const Expression& _other, MatchesPtr _pMatches = NULL) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, ListElementExpr(_cloner.get(getIndex()), _cloner.get(getObject())));
    }

private:
    ExpressionPtr m_pIndex;
};

// Declared below.
class Constructor;

/// Replacement expression. Result is a new expression of compound type with
/// some elements replaced.
class Replacement : public Component {
public:
    /// Default constructor.
    /// \param _pNewValues Expression containing new values.
    /// \param _pObject Expression of compound type.
    Replacement(const ConstructorPtr &_pNewValues = NULL, const ExpressionPtr &_pObject = NULL) :
        Component(_pObject), m_pConstructor(_pNewValues) {}

    /// Get component kind.
    /// \return #Replacement.
    virtual int getComponentKind() const { return REPLACEMENT; }

    /// Get expression of compound type containing new values.
    /// \return Expression containing new values.
    ConstructorPtr getNewValues() const { return m_pConstructor.as<Constructor>(); }

    /// Set expression of compound type containing new values.
    /// \param _pConstructor Expression containing new values.
    void setNewValues(const ConstructorPtr &_pConstructor) { m_pConstructor = _pConstructor; }

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;
    virtual bool matches(const Expression& _other, MatchesPtr _pMatches = NULL) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, Replacement(_cloner.get(getNewValues()), _cloner.get(getObject())));
    }

private:
    ExpressionPtr m_pConstructor;
};

/// Function call as a part of an expression.
class FunctionCall : public Expression {
public:
    /// Default constructor.
    /// \param _pPredicate Expression of predicate type.
    FunctionCall(const ExpressionPtr &_pPredicate = NULL) : m_pPredicate(_pPredicate) {}

    virtual int getKind() const { return FUNCTION_CALL; }

    /// Get predicate expression which is called.
    /// \return Expression of predicate type.
    const ExpressionPtr &getPredicate() const { return m_pPredicate; }

    /// Set predicate expression which is called.
    /// \param _pExpression Expression of predicate type.
    void setPredicate(const ExpressionPtr &_pExpression) { m_pPredicate = _pExpression; }

    /// Get list of actual parameters.
    /// \return List of expressions.
    Collection<Expression> &getArgs() { return m_args; }
    const Collection<Expression> &getArgs() const { return m_args; }

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;
    virtual bool matches(const Expression& _other, MatchesPtr _pMatches = NULL) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        FunctionCallPtr pExpr = NEW_CLONE(this, _cloner, FunctionCall(_cloner.get(getPredicate())));
        pExpr->getArgs().appendClones(getArgs(), _cloner);
        return pExpr;
    }

private:
    ExpressionPtr m_pPredicate;
    Collection<Expression> m_args;
};

/// Partially applied predicate.
class Binder : public Expression {
public:
    /// Default constructor.
    /// \param _pPredicate Expression of predicate type.
    Binder(const ExpressionPtr &_pPredicate = NULL) : m_pPredicate(_pPredicate) {}

    virtual int getKind() const { return BINDER; }

    /// Get predicate expression whose parameters are bound.
    /// \return Expression of predicate type.
    ExpressionPtr getPredicate() const { return m_pPredicate; }

    /// Set predicate expression.
    /// \param _pExpression Expression of predicate type.
    void setPredicate(const ExpressionPtr &_pExpression) { m_pPredicate = _pExpression; }

    /// Get list of bound parameters. Can contain NULL values.
    /// \return List of expressions.
    Collection<Expression> &getArgs() { return m_args; }
    const Collection<Expression> &getArgs() const { return m_args; }

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;
    virtual bool matches(const Expression& _other, MatchesPtr _pMatches = NULL) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        BinderPtr pExpr = NEW_CLONE(this, _cloner, Binder(_cloner.get(getPredicate())));
        pExpr->getArgs().appendClones(getArgs(), _cloner);
        return pExpr;
    }

private:
    ExpressionPtr m_pPredicate;
    Collection<Expression> m_args;
};

class FormulaDeclaration;

/// Function call as a part of an expression.
class FormulaCall : public Expression {
public:
    /// Default constructor.
    /// \param _pTarget Target formula.
    FormulaCall(const FormulaDeclarationPtr &_pTarget = NULL) : m_pTarget(_pTarget) {}

    virtual int getKind() const { return FORMULA_CALL; }

    /// Get formula which is called.
    /// \return Formula declaration.
    const FormulaDeclarationPtr &getTarget() const { return m_pTarget; }

    /// Set formula declaration which is called.
    /// \param _pFormula Formula.
    void setTarget(const FormulaDeclarationPtr &_pFormula) { m_pTarget = _pFormula; }

    /// Get list of actual parameters.
    /// \return List of expressions.
    Collection<Expression> &getArgs() { return m_args; }
    const Collection<Expression> &getArgs() const { return m_args; }

    std::wstring getName() const;

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;
    virtual bool matches(const Expression& _other, MatchesPtr _pMatches = NULL) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        FormulaCallPtr pExpr = NEW_CLONE(this, _cloner, FormulaCall(_cloner.get(getTarget(), true)));
        pExpr->getArgs().appendClones(getArgs(), _cloner);
        return pExpr;
    }

private:
    FormulaDeclarationPtr m_pTarget;
    Collection<Expression> m_args;
};

/// Output branch of a predicate.
class Branch : public Params {
public:
    /// Default constructor.
    /// \param _pLabel Label of the output branch.
    /// \param _pPreCondition Precondition.
    /// \param _pPostCondition Postcondition.
    Branch(const LabelPtr &_pLabel = NULL, const FormulaPtr &_pPreCondition = NULL, const FormulaPtr &_pPostCondition = NULL) :
        m_pLabel(_pLabel), m_pPreCondition(_pPreCondition), m_pPostCondition(_pPostCondition) {}

    /// Get label associated with the branch.
    /// Possibly NULL if it is the only branch.
    /// \return Label.
    const LabelPtr &getLabel() const { return m_pLabel; }

    /// Set label associated with the branch.
    /// \param _pLabel Label of the output branch.
    void setLabel(const LabelPtr &_pLabel) { m_pLabel = _pLabel; }

    /// Get branch precondition.
    /// \return Precondition.
    const FormulaPtr &getPreCondition() const { return m_pPreCondition; }

    /// Set branch precondition.
    /// \param _pCondition Precondition.
    void setPreCondition(const FormulaPtr &_pCondition) { m_pPreCondition = _pCondition; }

    /// Get branch precondition.
    /// \return Postcondition.
    const FormulaPtr &getPostCondition() const { return m_pPostCondition; }

    /// Set branch postcondition.
    /// \param _pCondition Postcondition.
    void setPostCondition(const FormulaPtr &_pCondition) { m_pPostCondition = _pCondition; }

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        BranchPtr pExpr = NEW_CLONE(this, _cloner, Branch(_cloner.get(getLabel()), _cloner.get(getPreCondition()), _cloner.get(getPostCondition())));
        pExpr->appendClones(*this, _cloner);
        return pExpr;
    }

private:
    LabelPtr m_pLabel;
    FormulaPtr m_pPreCondition, m_pPostCondition;
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
    AnonymousPredicate() : m_pPreCond(NULL), m_pPostCond(NULL), m_pBlock(NULL), m_pType(NULL), m_pMeasure(NULL) {}

    /// Get list of formal input parameters.
    /// \return List of parameters.
    Params &getInParams() { return m_paramsIn; }
    const Params &getInParams() const { return m_paramsIn; }

    /// Get list of output branches. Each branch can contain a list of parameters,
    /// a precondition and a postcondition.
    /// \return List of branches.
    Branches &getOutParams() { return m_paramsOut; }
    const Branches &getOutParams() const { return m_paramsOut; }

    /// Set predicate body.
    /// \param _pBlock Predicate body.
    void setBlock(const BlockPtr &_pBlock) { m_pBlock = _pBlock; }

    /// Get predicate body.
    /// \return Predicate body.
    const BlockPtr &getBlock() const { return m_pBlock; }

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

    /// Get measure function as nat-typed expression dependent on parameters.
    /// \return Measure function.
    const ExpressionPtr &getMeasure() const { return m_pMeasure; }

    /// Set measure function.
    /// \param _pMeasure Measure function.
    void setMeasure(const ExpressionPtr &_pMeasure) { m_pMeasure = _pMeasure; }

    /// Check if the predicate is a hyperfunction.
    /// \return True if the predicate has more than one branch or it's branch has
    ///   a handler assigned.
    bool isHyperFunction() const {
        return m_paramsOut.size() > 1 ||
            (m_paramsOut.size() == 1 && m_paramsOut.get(0)->getLabel());
    }

    // Check if the statement ends like a block (i.e. separating semicolon is not needed).
    // \return True.
    virtual bool isBlockLike() const { return true; }

    const PredicateTypePtr &getType() const {
        if (! m_pType) updateType();
        return m_pType;
    }

    void updateType() const;

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;

    void cloneTo(AnonymousPredicate &_pred, Cloner &_cloner) const {
        _pred.setLabel(getLabel());
        _pred.getInParams().appendClones(getInParams(), _cloner);
        _pred.getOutParams().appendClones(getOutParams(), _cloner);
        _pred.setPreCondition(_cloner.get(getPreCondition()));
        _pred.setPostCondition(_cloner.get(getPostCondition()));
        _pred.setBlock(_cloner.get(getBlock()));
        _pred.setMeasure(_cloner.get(getMeasure()));
    }

private:
    Params m_paramsIn;
    Branches m_paramsOut;
    FormulaPtr m_pPreCond, m_pPostCond;
    BlockPtr m_pBlock;
    mutable PredicateTypePtr m_pType;
    ExpressionPtr m_pMeasure;
};

typedef Auto<AnonymousPredicate> AnonymousPredicatePtr;

/// Anonymous predicate.
class Lambda : public Expression {
public:
    /// Default constructor.
    Lambda() {}

    /// Get expression kind.
    /// \return #Lambda.
    virtual int getKind() const { return LAMBDA; }

    AnonymousPredicate &getPredicate() { return m_pred; }
    const AnonymousPredicate &getPredicate() const { return m_pred; }

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;
    virtual bool matches(const Expression& _other, MatchesPtr _pMatches = NULL) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        LambdaPtr pExpr = NEW_CLONE(this, _cloner, Lambda());
        m_pred.cloneTo(pExpr->m_pred, _cloner);
        return pExpr;
    }

private:
    AnonymousPredicate m_pred;
};

/// Indexed element.
class ElementDefinition : public Node {
public:
    /// Default constructor.
    /// \param _pIndex Element index.
    /// \param _pValue Element value.
    ElementDefinition(const ExpressionPtr &_pIndex = NULL, const ExpressionPtr &_pValue = NULL) :
        m_pIndex(_pIndex), m_pValue(_pValue) {}

    virtual int getNodeKind() const { return Node::ELEMENT_DEFINITION; }

    /// Get index expression.
    /// \return Element index.
    const ExpressionPtr &getIndex() const { return m_pIndex; }

    /// Set index expression.
    /// \param _pExpression Element index.
    void setIndex(const ExpressionPtr &_pExpression) { m_pIndex = _pExpression; }

    /// Get value expression.
    /// \return Element value.
    const ExpressionPtr &getValue() const { return m_pValue; }

    /// Set value expression.
    /// \param _pExpression Element value.
    void setValue(const ExpressionPtr &_pExpression) { m_pValue = _pExpression; }

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, ElementDefinition(_cloner.get(getIndex()), _cloner.get(getValue())));
    }

private:
    ExpressionPtr m_pIndex, m_pValue;
};

/// Literal of \c struct type.
class StructFieldDefinition : public Node {
public:
    /// Default constructor.
    /// \param _pValue Field value.
    /// \param _pField Field reference.
    /// \param _strName Identifier.
    StructFieldDefinition(const ExpressionPtr &_pValue = NULL, const NamedValuePtr &_pField = NULL, const std::wstring &_strName = L"") :
        m_pValue(_pValue), m_pField(_pField), m_strName(_strName) {}

    virtual int getNodeKind() const { return Node::STRUCT_FIELD_DEFINITION; }

    /// Get field reference.
    /// \return Field reference.
    const NamedValuePtr &getField() const { return m_pField; }

    /// Set field reference.
    /// \param _pField Field reference.
    void setField(const NamedValuePtr &_pField) { m_pField = _pField; }

    /// Get value expression.
    /// \return Field value.
    const ExpressionPtr &getValue() const { return m_pValue; }

    /// Set value expression.
    /// \param _pExpression Field value.
    void setValue(const ExpressionPtr &_pExpression) { m_pValue = _pExpression; }

    /// Get name of the field.
    /// \returns Identifier.
    const std::wstring &getName() const { return m_strName; }

    /// Set name of the field.
    /// \param _strName Identifier.
    void setName(const std::wstring &_strName) { m_strName = _strName; }

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, StructFieldDefinition(_cloner.get(getValue()), _cloner.get(getField()), getName()));
    }

private:
    ExpressionPtr m_pValue;
    NamedValuePtr m_pField;
    std::wstring m_strName;
};

/// Constructor of compound value (array, set, struct, etc.)
class Constructor : public Expression {
public:
    /// Kind of constructor.
    enum {
        /// Undefined.
        NONE_CONSTRUCTOR,
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
    virtual int getConstructorKind() const { return NONE_CONSTRUCTOR; }

    virtual bool less(const Node& _other) const {
        return Expression::equals(_other)
            ? getConstructorKind() < ((const Constructor&)_other).getConstructorKind()
            : Expression::less(_other);
    }

    virtual bool equals(const Node& _other) const {
        return Expression::equals(_other)
            ? getConstructorKind() == ((const Constructor&)_other).getConstructorKind()
            : false;
    }

    virtual bool matches(const Expression& _other, MatchesPtr _pMatches = NULL) const {
        return Expression::equals(_other)
            ? getConstructorKind() == ((const Constructor&)_other).getConstructorKind()
            : Expression::matches(_other, _pMatches);
    }
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

    virtual NodePtr clone(Cloner &_cloner) const {
        StructConstructorPtr pCopy = NEW_CLONE(this, _cloner, StructConstructor());
        pCopy->appendClones(*this, _cloner);
        return pCopy;
    }
};

class VariableDeclaration;
class UnionConstructorDeclaration;

/// Union value. \extends Constructor
/// The class extends struct constructor with union constructor name.
/// May contain not-fully defined fields (used in switch construct).
class UnionConstructor : public StructConstructor {
public:
    UnionConstructor(const std::wstring &_strName, const UnionConstructorDeclarationPtr &_pProto = NULL) :
        m_strName(_strName), m_pProto(_pProto) {}

    /// Get constructor kind.
    /// \return #UnionConstructor.
    virtual int getConstructorKind() const { return UNION_CONSTRUCTOR; }

    /// Get list of variables declared as part of the constructor.
    /// \return List of variables.
    Collection<VariableDeclaration> & getDeclarations() { return m_decls; }
    const Collection<VariableDeclaration> & getDeclarations() const { return m_decls; }

    bool isComplete() const;

    // Should be set after type checking is done.
    const UnionConstructorDeclarationPtr &getPrototype() const { return m_pProto; }
    void setPrototype(const UnionConstructorDeclarationPtr &_pProto) { m_pProto = _pProto; }

    const std::wstring &getName() const { return m_strName; }
    void setName(const std::wstring &_strName) { m_strName = _strName; }

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        UnionConstructorPtr pCopy = NEW_CLONE(this, _cloner, UnionConstructor(getName(), _cloner.get(getPrototype(), true)));
        pCopy->getDeclarations().appendClones(getDeclarations(), _cloner);
        return pCopy;
    }

private:
    std::wstring m_strName;
    Collection<VariableDeclaration> m_decls;
    UnionConstructorDeclarationPtr m_pProto;
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

    virtual NodePtr clone(Cloner &_cloner) const {
        ArrayConstructorPtr pCopy = NEW_CLONE(this, _cloner, ArrayConstructor());
        pCopy->appendClones(*this, _cloner);
        return pCopy;
    }
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

    virtual NodePtr clone(Cloner &_cloner) const {
        MapConstructorPtr pCopy = NEW_CLONE(this, _cloner, MapConstructor());
        pCopy->appendClones(*this, _cloner);
        return pCopy;
    }
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

    virtual NodePtr clone(Cloner &_cloner) const {
        SetConstructorPtr pCopy = NEW_CLONE(this, _cloner, SetConstructor());
        pCopy->appendClones(*this, _cloner);
        return pCopy;
    }
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

    virtual NodePtr clone(Cloner &_cloner) const {
        ListConstructorPtr pCopy = NEW_CLONE(this, _cloner, ListConstructor());
        pCopy->appendClones(*this, _cloner);
        return pCopy;
    }
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
    /// \param _pExpression Expression.
    ArrayPartDefinition(const ExpressionPtr &_pExpression = NULL) : m_pExpression(_pExpression) {}

    virtual int getNodeKind() const { return Node::ARRAY_PART_DEFINITION; }

    /// Get \c case conditions.
    /// \return List of expressions (can contain ranges).
    Collection<Expression> &getConditions() { return m_conditions; }
    const Collection<Expression> &getConditions() const { return m_conditions; }

    /// Get expression.
    /// \return Expression.
    const ExpressionPtr &getExpression() const { return m_pExpression; }

    /// Set expression.
    /// \param _pExpression Expression.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setExpression(const ExpressionPtr &_pExpression) { m_pExpression = _pExpression; }

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        ArrayPartDefinitionPtr pCopy = NEW_CLONE(this, _cloner, ArrayPartDefinition(_cloner.get(getExpression())));
        pCopy->getConditions().appendClones(getConditions(), _cloner);
        return pCopy;
    }

private:
    ExpressionPtr m_pExpression;
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
    /// \param _pDefault Expression.
    ArrayIteration(const ExpressionPtr &_pDefault = NULL) : m_pDefault(_pDefault) {}

    /// Get constructor kind.
    /// \return #ArrayIteration.
    virtual int getConstructorKind() const { return ARRAY_ITERATION; }

    /// Get list of iterator variables.
    /// \return Iterator variables.
    NamedValues &getIterators() { return m_iterators; }
    const NamedValues &getIterators() const { return m_iterators; }

    /// Get expression for default alternative.
    /// \return Expression.
    const ExpressionPtr &getDefault() const { return m_pDefault; }

    /// Set expression for default alternative.
    /// \param _pExpression Expression.
    void setDefault(const ExpressionPtr &_pExpression) { m_pDefault = _pExpression; }

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        ArrayIterationPtr pCopy = NEW_CLONE(this, _cloner, ArrayIteration(_cloner.get(getDefault())));
        pCopy->appendClones(*this, _cloner);
        pCopy->getIterators().appendClones(getIterators(), _cloner);
        return pCopy;
    }

private:
    ExpressionPtr m_pDefault;
    NamedValues m_iterators;

    typedef Collection<ArrayPartDefinition, Constructor> Base;
};

} // namespace ir

#endif /* EXPRESSIONS_H_ */
