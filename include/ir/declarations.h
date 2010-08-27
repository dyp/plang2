/// \file declarations.h
/// Internal structures representing top level, class and block level declarations.
///


#ifndef DECLARATIONS_H_
#define DECLARATIONS_H_

#include "base.h"
#include "types.h"

namespace ir {

/// Predicate declaration.
class CPredicate : public CAnonymousPredicate {
public:
    /// Initialize with predicate name.
    /// \param _strName Predicate name.
    CPredicate(const std::wstring & _strName, bool _bBuiltin = false) : m_strName(_strName), m_bBuiltin(_bBuiltin) {}

    /// Get statement kind.
    /// \returns #PredicateDeclaration.
    virtual int getKind() const { return PredicateDeclaration; }

    /// Get predicate name.
    /// \return Identifier.
    const std::wstring & getName() const { return m_strName; }

    /// Set predicate name.
    /// \param _strName Identifier.
    void setName(const std::wstring & _strName) { m_strName = _strName; }

    bool isBuiltin() const { return m_bBuiltin; }

private:
    std::wstring m_strName;
    bool m_bBuiltin;
};

typedef CCollection<CPredicate> Predicates;

/// Message declaration.
class CMessage : public CNode {
public:
    /// Message processing type.
    enum {
        /// Message is sent synchronously.
        /// Send statement won't exit until message is received.
        Message,
        /// Message is placed placed on receiver's queue.
        /// Send exits immediately.
        Queue
    };

    /// Default constructor.
    CMessage() : m_processingType(Message) {}

    /// Initialize with message processing type.
    /// \param _processingType Processing type (one of #Message and #Queue).
    CMessage(int _processingType) : m_processingType(_processingType) {}

    /// Get message name.
    /// \return Identifier.
    const std::wstring & getName() const { return m_strName; }

    /// Set message name.
    /// \param _strName Identifier.
    void setName(const std::wstring & _strName) { m_strName = _strName; }

    /// Get processing type.
    /// \return Processing type (one of #Message and #Queue).
    int getProcessingType() const { return m_processingType; }

    /// Set processing type.
    /// \param _type Processing type (one of #Message and #Queue).
    void setProcessingType(int _type) { m_processingType = _type; }

    /// Get list of message parameters.
    /// \return List of parameters.
    CParams & getParams() { return m_params; }

private:
    int m_processingType;
    std::wstring m_strName;
    CParams m_params;
};

/// Process declaration.
class CProcess : public CNode {
public:
    /// Default constructor.
    CProcess() : m_pBlock(NULL) {}

    /// Initialize with process name.
    /// \param _strName Process name.
    CProcess(const std::wstring & _strName) : m_strName(_strName), m_pBlock(NULL) {}

    /// Destructor.
    virtual ~CProcess() {
        _delete(m_pBlock);
    }

    /// Get process name.
    /// \return Identifier.
    const std::wstring & getName() const { return m_strName; }

    /// Set process name.
    /// \param _strName Identifier.
    void setName(const std::wstring & _strName) { m_strName = _strName; }

    /// Get list of formal input parameters.
    /// \return List of parameters.
    CParams & getInParams() { return m_paramsIn; }

    /// Get list of output branches. Each branch can contain a list of parameters,
    /// a precondition and a postcondition.
    /// \return List of branches.
    CBranches & getOutParams() { return m_paramsOut; }

    /// Set predicate body.
    /// \param _pBlock Predicate body.
    /// \param _bReparent If specified (default) also sets parent of _pBlock to this node.
    void setBlock(CBlock * _pBlock, bool _bReparent = true) {
        _assign(m_pBlock, _pBlock, _bReparent);
    }

    /// Get predicate body.
    /// \return Predicate body.
    CBlock * getBlock() const { return m_pBlock; }

private:
    CBranches m_paramsOut;
    CParams m_paramsIn;
    std::wstring m_strName;
    CBlock * m_pBlock;
};

class CVariableDeclaration;

/// Variable declaration.
class CVariable : public CNamedValue {
public:
    /// Initialize with variable name.
    /// \param _bLocal Specifies if it is a local variable.
    /// \param _strName Variable name.
    CVariable(bool _bLocal, const std::wstring & _strName = L"")
        : CNamedValue(_strName), m_bMutable(false), m_kind(_bLocal ? Local : Global), m_pDeclaration(NULL) {}

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
    const CVariableDeclaration * getDeclaration() const { return m_pDeclaration; }

    /// Set referenced variable.
    /// \param _pTarget Referenced variable.
    void setDeclaration(const CVariableDeclaration * _pDeclaration) { m_pDeclaration = _pDeclaration; }

private:
    bool m_bMutable;
    const int m_kind;
    const CVariableDeclaration * m_pDeclaration;
};

/// Statement that wraps variable declaration.
class CVariableDeclaration : public CStatement {
public:
    /// Default constructor.
    CVariableDeclaration() : m_var(true), m_pValue(NULL) { m_var.setDeclaration(this); }

    /// Initialize with variable name.
    /// \param _bLocal Specifies if it is a local variable.
    /// \param _strName Variable name.
    CVariableDeclaration(bool _bLocal, const std::wstring & _strName) : m_var(_bLocal, _strName), m_pValue(NULL) {
        m_var.setDeclaration(this);
    }

    /// Destructor.
    virtual ~CVariableDeclaration() { _delete(m_pValue); }

    /// Get statement kind.
    /// \returns #VariableDeclaration.
    virtual int getKind() const { return VariableDeclaration; }

    /// Get underlying variable.
    /// \return Variable reference.
    CVariable & getVariable() { return m_var; }
    const CVariable & getVariable() const { return m_var; }

    /// Get value expression. Possibly NULL if variable is not initialized.
    /// \return Value.
    CExpression * getValue() const { return m_pValue; }

    /// Set expression. Use NULL if variable is not initialized.
    /// \param _pExpression Value.
    /// \param _bReparent If specified (default) also sets parent of _pExpression to this node.
    void setValue(CExpression * _pExpression, bool _bReparent = true) {
        _assign(m_pValue, _pExpression, _bReparent);
    }

private:
    CVariable m_var;
    CExpression * m_pValue;
};

/// Statement that wraps type declaration.
class CTypeDeclaration : public CStatement {
public:
    /// Default constructor.
    CTypeDeclaration() : m_pType(NULL) {}

    /// Initialize with type name.
    /// \param _strName Declared type name.
    CTypeDeclaration(const std::wstring & _strName) : m_strName(_strName), m_pType(NULL) {}

    /// Get statement kind.
    /// \returns #TypeDeclaration.
    virtual int getKind() const { return TypeDeclaration; }

    /// Get underlying type.
    /// \return Type reference.
    CType * getType() { return m_pType; }
    const CType * getType() const { return m_pType; }

    /// Set underlying type.
    /// \param _pType Underlying type.
    /// \param _bReparent If specified (default) also sets parent of _pType to this node.
    void setType(CType * _pType, bool _bReparent = true) {
        _assign(m_pType, _pType, _bReparent);
    }

    /// Get type identifier.
    /// \return Identifier.
    const std::wstring & getName() const { return m_strName; }

    /// Set type identifier.
    /// \param _strName Identifier.
    void setName(const std::wstring & _strName) { m_strName = _strName; }

private:
    std::wstring m_strName;
    CType * m_pType;
};

/// Named formula declaration.
class CFormulaDeclaration : public CStatement {
public:
    /// Default constructor.
    CFormulaDeclaration() : m_pFormula(NULL) {}

    /// Initialize with formula name.
    /// \param _strName Declared type name.
    CFormulaDeclaration(const std::wstring & _strName) : m_strName(_strName), m_pFormula(NULL) {}

    /// Destructor.
    virtual ~CFormulaDeclaration() {
        _delete(m_pFormula);
    }
    /// Get statement kind.
    /// \returns #FormulaDeclaration.
    virtual int getKind() const { return FormulaDeclaration; }

    /// Get list of formal parameters.
    /// \return List of parameters.
    CNamedValues & getParams() { return m_params; }

    /// Get formula name.
    /// \return Identifier.
    const std::wstring & getName() const { return m_strName; }

    /// Set formula name.
    /// \param _strName Identifier.
    void setName(const std::wstring & _strName) { m_strName = _strName; }

    /// Get declared formula.
    /// \return Postcondition.
    CExpression * getFormula() const { return m_pFormula; }

    /// Set declared formula postcondition.
    /// \param _pFormula Formula.
    /// \param _bReparent If specified (default) also sets parent of _pFormula to this node.
    void setFormula(CExpression * _pFormula, bool _bReparent = true) {
        _assign(m_pFormula, _pFormula, _bReparent);
    }

private:
    std::wstring m_strName;
    CNamedValues m_params;
    CExpression * m_pFormula;
};

/// Base class for objects containing common declarations.
class CDeclarationGroup : public CNode {
public:
    /// Default constructor.
    CDeclarationGroup() {}

    /// Get list of predicates.
    /// \return List of predicates.
    CCollection<CPredicate> & getPredicates() { return m_predicates; }
    const CCollection<CPredicate> & getPredicates() const { return m_predicates; }

    /// Get list of declared types.
    /// \return List of declared types.
    CCollection<CTypeDeclaration> & getTypes() { return m_types; }

    /// Get list of declared variables.
    /// \return List of declared variables.
    CCollection<CVariableDeclaration> & getVariables() { return m_variables; }

    /// Get list of declared messages.
    /// \return List of declared messages.
    CCollection<CMessage> & getMessages() { return m_messages; }

    /// Get list of processes.
    /// \return List of processes.
    CCollection<CProcess> & getProcesses() { return m_processes; }

    /// Get list of formulas.
    /// \return List of formulas.
    CCollection<CFormulaDeclaration> & getFormulas() { return m_formulas; }

private:
    CCollection<CPredicate> m_predicates;
    CCollection<CTypeDeclaration> m_types;
    CCollection<CVariableDeclaration> m_variables;
    CCollection<CFormulaDeclaration> m_formulas;
    CCollection<CMessage> m_messages;
    CCollection<CProcess> m_processes;
};

/// Class declaration.
class CClass : public CDeclarationGroup {
public:
    /// Default constructor.
    CClass() : m_pAncestor(NULL) {}

    /// Initialize with class name.
    /// \param _strName Class name.
    CClass(const std::wstring & _strName) : m_pAncestor(NULL), m_strName(_strName) {}

    /// Get class name.
    /// \return Identifier.
    const std::wstring & getName() const { return m_strName; }

    /// Set class name.
    /// \param _strName Identifier.
    void setName(const std::wstring & _strName) { m_strName = _strName; }

    /// Get ancestor class.
    /// \return Pointer to ancestor class declaration.
    const CClass * getAncestor() const { return m_pAncestor; }

    /// Set ancestor class.
    /// \param _pClass Pointer to ancestor class declaration.
    void setAncestor(const CClass * _pClass) { m_pAncestor = _pClass; }

private:
    const CClass * m_pAncestor;
    std::wstring m_strName;
};

/// Module declaration.
/// If module is not declared explicitly implicit module declaration is assumed anyway.
class CModule : public CDeclarationGroup {
public:
    /// Default constructor.
    CModule() {}

    /// Get module name.
    /// \return Identifier.
    const std::wstring & getName() const { return m_strName; }

    /// Set module name.
    /// \param _strName Identifier.
    void setName(const std::wstring & _strName) { m_strName = _strName; }

    /// Get list of imported module names.
    /// \return List of imported module names.
    std::vector<std::wstring> & getImports() { return m_imports; }

    /// Get list of declared classes.
    /// \return List of declared classes.
    CCollection<CClass> & getClasses() { return m_classes; }

    /// Get list of combinations of pragmas.
    /// \return List of combinations of pragmas.
//    CCollection<CPragmaGroup> & getPragmas() { return m_pragmas; }

private:
    std::vector<std::wstring> m_imports;
    CCollection<CClass> m_classes;
//    CCollection<CPragmaGroup> m_pragmas;
    std::wstring m_strName;
};

} // namespace ir

#endif /* DECLARATIONS_H_ */
