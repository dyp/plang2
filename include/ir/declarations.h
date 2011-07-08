/// \file declarations.h
/// Internal structures representing top level, class and block level declarations.
///


#ifndef DECLARATIONS_H_
#define DECLARATIONS_H_

#include "base.h"
#include "types.h"

namespace ir {

/// Predicate declaration.
class Predicate : public AnonymousPredicate {
public:
    /// Initialize with predicate name.
    /// \param _strName Predicate name.
    Predicate(const std::wstring &_strName, bool _bBuiltin = false) : m_strName(_strName), m_bBuiltin(_bBuiltin) {}

    /// Get statement kind.
    /// \returns #PredicateDeclaration.
    virtual int getKind() const { return PREDICATE_DECLARATION; }

    /// Get predicate name.
    /// \return Identifier.
    const std::wstring &getName() const { return m_strName; }

    /// Set predicate name.
    /// \param _strName Identifier.
    void setName(const std::wstring &_strName) { m_strName = _strName; }

    bool isBuiltin() const { return m_bBuiltin; }

private:
    std::wstring m_strName;
    bool m_bBuiltin;
};

typedef Collection<Predicate> Predicates;

/// Message declaration.
class Message : public Node {
public:
    /// Message processing type.
    enum {
        /// Message is sent synchronously.
        /// Send statement won't exit until message is received.
        MESSAGE,
        /// Message is placed placed on receiver's queue.
        /// Send exits immediately.
        QUEUE
    };

    /// Default constructor.
    Message() : m_processingType(MESSAGE) {}

    /// Initialize with message processing type.
    /// \param _processingType Processing type (one of #Message and #Queue).
    Message(int _processingType) : m_processingType(_processingType) {}

    virtual int getNodeKind() const { return Node::MESSAGE; }

    /// Get message name.
    /// \return Identifier.
    const std::wstring &getName() const { return m_strName; }

    /// Set message name.
    /// \param _strName Identifier.
    void setName(const std::wstring &_strName) { m_strName = _strName; }

    /// Get processing type.
    /// \return Processing type (one of #Message and #Queue).
    int getProcessingType() const { return m_processingType; }

    /// Set processing type.
    /// \param _type Processing type (one of #Message and #Queue).
    void setProcessingType(int _type) { m_processingType = _type; }

    /// Get list of message parameters.
    /// \return List of parameters.
    Params &getParams() { return m_params; }

private:
    int m_processingType;
    std::wstring m_strName;
    Params m_params;
};

/// Process declaration.
class Process : public Node {
public:
    /// Default constructor.
    Process() : m_pBlock(NULL) {}

    /// Initialize with process name.
    /// \param _strName Process name.
    Process(const std::wstring &_strName) : m_strName(_strName), m_pBlock(NULL) {}

    virtual int getNodeKind() const { return Node::PROCESS; }

    /// Get process name.
    /// \return Identifier.
    const std::wstring &getName() const { return m_strName; }

    /// Set process name.
    /// \param _strName Identifier.
    void setName(const std::wstring &_strName) { m_strName = _strName; }

    /// Get list of formal input parameters.
    /// \return List of parameters.
    Params &getInParams() { return m_paramsIn; }

    /// Get list of output branches. Each branch can contain a list of parameters,
    /// a precondition and a postcondition.
    /// \return List of branches.
    Branches &getOutParams() { return m_paramsOut; }

    /// Set predicate body.
    /// \param _pBlock Predicate body.
    void setBlock(const BlockPtr &_pBlock) { m_pBlock = _pBlock; }

    /// Get predicate body.
    /// \return Predicate body.
    const BlockPtr &getBlock() const { return m_pBlock; }

private:
    Branches m_paramsOut;
    Params m_paramsIn;
    std::wstring m_strName;
    BlockPtr m_pBlock;
};

class VariableDeclaration;

/// Variable declaration.
class Variable : public NamedValue {
public:
    /// Initialize with variable name.
    /// \param _bLocal Specifies if it is a local variable.
    /// \param _strName Variable name.
    Variable(bool _bLocal, const std::wstring &_strName = L"")
        : NamedValue(_strName), m_bMutable(false), m_kind(_bLocal ? LOCAL : GLOBAL), m_pDeclaration(NULL) {}

    /// Get value kind.
    /// \returns #PredicateParameter.
    virtual int getKind() const { return m_kind; }

    /// Check if variable is declared mutable.
    /// \return True if the variable is mutable, false otherwise.
    bool isMutable() const { return m_bMutable; }

    /// Set mutable modifier.
    /// \param _bMutable If specified the variable is considered mutable.
    void setMutable(bool _bMutable) { m_bMutable = _bMutable; }

    /// Get referenced variable.
    /// \return Referenced variable.
    const VariableDeclarationPtr &getDeclaration() const { return m_pDeclaration; }

    /// Set referenced variable.
    /// \param _pTarget Referenced variable.
    void setDeclaration(const VariableDeclarationPtr &_pDeclaration) { m_pDeclaration = _pDeclaration; }

private:
    bool m_bMutable;
    const int m_kind;
    VariableDeclarationPtr m_pDeclaration;
};

/// Statement that wraps variable declaration.
class VariableDeclaration : public Statement {
public:
    /// Default constructor.
    VariableDeclaration() : m_pVar(NULL), m_pValue(NULL) { }

    /// Initialize with variable name.
    /// \param _bLocal Specifies if it is a local variable.
    /// \param _strName Variable name.
    VariableDeclaration(bool _bLocal, const std::wstring &_strName) : m_pVar(NULL), m_pValue(NULL) {
        setVariable(new Variable(_bLocal, _strName));
    }

    /// Get statement kind.
    /// \returns #VariableDeclaration.
    virtual int getKind() const { return VARIABLE_DECLARATION; }

    /// Get underlying variable.
    /// \return Variable.
    const VariablePtr &getVariable() const { return m_pVar; }

    void setVariable(const VariablePtr &_pVar) {
        m_pVar = _pVar;
        m_pVar->setDeclaration(this);
    }

    /// Get value expression. Possibly NULL if variable is not initialized.
    /// \return Value.
    const ExpressionPtr &getValue() const { return m_pValue; }

    /// Set expression. Use NULL if variable is not initialized.
    /// \param _pExpression Value.
    void setValue(const ExpressionPtr &_pExpression) { m_pValue = _pExpression; }

    void setType(const TypePtr &_pType) { m_pVar->setType(_pType); }

    std::wstring getName() const;

private:
    VariablePtr m_pVar;
    ExpressionPtr m_pValue;
};

/// Statement that wraps type declaration.
class TypeDeclaration : public Statement {
public:
    /// Default constructor.
    TypeDeclaration() : m_pType(NULL) {}

    /// Initialize with type name.
    /// \param _strName Declared type name.
    TypeDeclaration(const std::wstring &_strName) : m_strName(_strName), m_pType(NULL) {}

    /// Get statement kind.
    /// \returns #TypeDeclaration.
    virtual int getKind() const { return TYPE_DECLARATION; }

    /// Get underlying type.
    /// \return Type reference.
    const TypePtr &getType() const { return m_pType; }

    /// Set underlying type.
    /// \param _pType Underlying type.
    void setType(const TypePtr &_pType) { m_pType = _pType; }

    /// Get type identifier.
    /// \return Identifier.
    const std::wstring &getName() const { return m_strName; }

    /// Set type identifier.
    /// \param _strName Identifier.
    void setName(const std::wstring &_strName) { m_strName = _strName; }

private:
    std::wstring m_strName;
    TypePtr m_pType;
};

/// Named formula declaration.
class FormulaDeclaration : public Statement {
public:
    /// Default constructor.
    FormulaDeclaration() : m_pFormula(NULL) {}

    /// Initialize with formula name.
    /// \param _strName Declared type name.
    /// \param _pType Result type.
    FormulaDeclaration(const std::wstring &_strName, const TypePtr &_pType = NULL) : m_strName(_strName), m_pFormula(NULL), m_pType(_pType) {
        if (!_pType)
            m_pType = new Type(Type::BOOL);
    }

    /// Get statement kind.
    /// \returns #FormulaDeclaration.
    virtual int getKind() const { return FORMULA_DECLARATION; }

    /// Get list of formal parameters.
    /// \return List of parameters.
    NamedValues &getParams() { return m_params; }

    /// Get formula name.
    /// \return Identifier.
    const std::wstring &getName() const { return m_strName; }

    /// Set formula name.
    /// \param _strName Identifier.
    void setName(const std::wstring &_strName) { m_strName = _strName; }

    /// Get result type.
    /// \return Type reference.
    const TypePtr &getResultType() { return m_pType; }

    /// Set result type.
    /// \param _pType Result type.
    void setResultType(const TypePtr &_pType) { m_pType = _pType; }

    /// Get declared formula.
    /// \return Formula.
    const ExpressionPtr &getFormula() const { return m_pFormula; }

    /// Set declared formula postcondition.
    /// \param _pFormula Formula.
    void setFormula(const ExpressionPtr &_pFormula) { m_pFormula = _pFormula; }

private:
    std::wstring m_strName;
    NamedValues m_params;
    ExpressionPtr m_pFormula;
    TypePtr m_pType;
};

/// Lemma declaration.
class LemmaDeclaration : public Statement {
public:
    /// Default constructor.
    LemmaDeclaration() : m_pProposition(NULL) {}

    /// Get statement kind.
    /// \returns #LemmaDeclaration.
    virtual int getKind() const { return LEMMA_DECLARATION; }

    /// Get proposition (boolean-typed expression).
    /// \return Proposition.
    const ExpressionPtr &getProposition() const { return m_pProposition; }

    /// Set proposition.
    /// \param _pProposition Proposition.
    void setProposition(const ExpressionPtr &_pProposition) { m_pProposition = _pProposition; }

private:
    ExpressionPtr m_pProposition;
};

/// Base class for objects containing common declarations.
class DeclarationGroup : public Node {
public:
    /// Default constructor.
    DeclarationGroup() {}

    /// Get list of predicates.
    /// \return List of predicates.
    Collection<Predicate> &getPredicates() { return m_predicates; }
    const Collection<Predicate> &getPredicates() const { return m_predicates; }

    /// Get list of declared types.
    /// \return List of declared types.
    Collection<TypeDeclaration> &getTypes() { return m_types; }

    /// Get list of declared variables.
    /// \return List of declared variables.
    Collection<VariableDeclaration> &getVariables() { return m_variables; }

    /// Get list of declared messages.
    /// \return List of declared messages.
    Collection<Message> &getMessages() { return m_messages; }

    /// Get list of processes.
    /// \return List of processes.
    Collection<Process> &getProcesses() { return m_processes; }

    /// Get list of formulas.
    /// \return List of formulas.
    Collection<FormulaDeclaration> &getFormulas() { return m_formulas; }

    /// Get list of lemmas.
    /// \return List of lemmas.
    Collection<LemmaDeclaration> &getLemmas() { return m_lemmas; }

private:
    Collection<Predicate> m_predicates;
    Collection<TypeDeclaration> m_types;
    Collection<VariableDeclaration> m_variables;
    Collection<FormulaDeclaration> m_formulas;
    Collection<LemmaDeclaration> m_lemmas;
    Collection<Message> m_messages;
    Collection<Process> m_processes;
};

/// Class declaration.
class Class : public DeclarationGroup {
public:
    /// Default constructor.
    Class() : m_pAncestor(NULL) {}

    /// Initialize with class name.
    /// \param _strName Class name.
    Class(const std::wstring &_strName) : m_pAncestor(NULL), m_strName(_strName) {}

    virtual int getNodeKind() const { return Node::CLASS; }

    /// Get class name.
    /// \return Identifier.
    const std::wstring &getName() const { return m_strName; }

    /// Set class name.
    /// \param _strName Identifier.
    void setName(const std::wstring &_strName) { m_strName = _strName; }

    /// Get ancestor class.
    /// \return Pointer to ancestor class declaration.
    const ClassPtr &getAncestor() const { return m_pAncestor; }

    /// Set ancestor class.
    /// \param _pClass Pointer to ancestor class declaration.
    void setAncestor(const ClassPtr &_pClass) { m_pAncestor = _pClass; }

private:
    ClassPtr m_pAncestor;
    std::wstring m_strName;
};

/// Module declaration.
/// If module is not declared explicitly implicit module declaration is assumed anyway.
class Module : public DeclarationGroup {
public:
    /// Default constructor.
    Module() {}

    virtual int getNodeKind() const { return Node::MODULE; }

    /// Get module name.
    /// \return Identifier.
    const std::wstring &getName() const { return m_strName; }

    /// Set module name.
    /// \param _strName Identifier.
    void setName(const std::wstring &_strName) { m_strName = _strName; }

    /// Get list of imported module names.
    /// \return List of imported module names.
    std::vector<std::wstring> &getImports() { return m_imports; }

    /// Get list of declared classes.
    /// \return List of declared classes.
    Collection<Class> &getClasses() { return m_classes; }

private:
    std::vector<std::wstring> m_imports;
    Collection<Class> m_classes;
    std::wstring m_strName;
};

} // namespace ir

#endif /* DECLARATIONS_H_ */
