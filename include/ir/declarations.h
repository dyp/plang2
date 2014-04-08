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

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        const PredicatePtr pCopy = NEW_CLONE(this, _cloner, Predicate(getName(), isBuiltin()));
        cloneTo(*pCopy, _cloner);
        pCopy->setLoc(this->getLoc());
        return pCopy;
    }

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

    /// Initialize with message processing type.
    /// \param _processingType Processing type (one of #Message and #Queue).
    /// \param _strName Identifier.
    Message(int _processingType = MESSAGE, const std::wstring &_strName = L"") :
        m_processingType(_processingType), m_strName(_strName) {}

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
    const Params &getParams() const { return m_params; }

    virtual NodePtr clone(Cloner &_cloner) const {
        MessagePtr pCopy = NEW_CLONE(this, _cloner, Message(getProcessingType(), getName()));
        pCopy->getParams().appendClones(getParams(), _cloner);
        return pCopy;
    }

private:
    int m_processingType;
    std::wstring m_strName;
    Params m_params;
};

/// Process declaration.
class Process : public Node {
public:
    /// Initialize with process name.
    /// \param _strName Process name.
    /// \param _pBlock Predicate body.
    Process(const std::wstring &_strName = L"", const BlockPtr &_pBlock = NULL) : m_strName(_strName), m_pBlock(_pBlock) {}

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

    virtual NodePtr clone(Cloner &_cloner) const {
        const ProcessPtr pCopy = NEW_CLONE(this, _cloner, Process(getName(), _cloner.get(getBlock())));
        pCopy->getInParams().appendClones(getInParams(), _cloner);
        pCopy->getOutParams().appendClones(getOutParams(), _cloner);
        pCopy->setLoc(this->getLoc());
        return pCopy;
    }

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
    /// \param _pType Type associated with variable.
    /// \param _bMutable If specified the variable is considered mutable.
    /// \param _pTarget Referenced variable.
    Variable(bool _bLocal, const std::wstring &_strName = L"", const TypePtr &_pType = NULL,
            bool _bMutable = false, const VariableDeclarationPtr &_pDeclaration = NULL) :
        NamedValue(_strName, _pType), m_bMutable(_bMutable), m_kind(_bLocal ? LOCAL : GLOBAL), m_pDeclaration(_pDeclaration) {}

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

    virtual bool less(const Node& _other) const {
        return NamedValue::equals(_other)
            ? !isMutable() && ((const Variable&)_other).isMutable()
            : NamedValue::less(_other);
    }

    virtual bool equals(const Node& _other) const {
        return NamedValue::equals(_other)
            ? getKind() == ((const Variable&)_other).getKind()
            : false;
    }

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, Variable(m_kind == LOCAL, getName(), _cloner.get(getType()), isMutable(), _cloner.get(getDeclaration(), true)));
    }

private:
    bool m_bMutable;
    const int m_kind;
    VariableDeclarationPtr m_pDeclaration;
};

/// Statement that wraps variable declaration.
class VariableDeclaration : public Statement {
public:
    /// Initialize with variable reference.
    /// \param _pVar Variable.
    /// \param _pValue Value.
    /// \param _pLabel Statement label.
    VariableDeclaration(const VariablePtr &_pVar = NULL, const ExpressionPtr &_pValue = NULL, const LabelPtr &_pLabel = NULL) :
        Statement(_pLabel), m_pVar(_pVar), m_pValue(_pValue) {}

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

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        const VariableDeclarationPtr pCopy = NEW_CLONE(this, _cloner, VariableDeclaration(_cloner.get(getVariable()), _cloner.get(getValue()), _cloner.get(getLabel())));
        pCopy->setLoc(this->getLoc());
        return pCopy;
    }

private:
    VariablePtr m_pVar;
    ExpressionPtr m_pValue;
};

class VariableDeclarationGroup : public Collection<VariableDeclaration, Statement> {
public:
    VariableDeclarationGroup() {}
    virtual int getKind() const { return VARIABLE_DECLARATION_GROUP; }

    virtual NodePtr clone(Cloner &_cloner) const {
        Auto<VariableDeclarationGroup > pCopy = NEW_CLONE(this, _cloner, VariableDeclarationGroup());
        pCopy->appendClones(*this, _cloner);
        return pCopy;
    }
};

/// Statement that wraps type declaration.
class TypeDeclaration : public Statement {
public:
    /// Initialize with type name.
    /// \param _strName Declared type name.
    /// \param _pType Underlying type.
    /// \param _pLabel Statement label.
    TypeDeclaration(const std::wstring &_strName = L"", const TypePtr &_pType = NULL, const LabelPtr &_pLabel = NULL) :
        Statement(_pLabel), m_strName(_strName), m_pType(_pType) {}

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

    virtual bool less(const Node &_other) const;
    virtual bool equals(const Node &_other) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        const TypeDeclarationPtr pCopy = NEW_CLONE(this, _cloner, TypeDeclaration(getName(),
                _cloner.get(getType()), _cloner.get(getLabel())));
        pCopy->setLoc(this->getLoc());
        return pCopy;
    }

private:
    std::wstring m_strName;
    TypePtr m_pType;
};

/// Named formula declaration.
class FormulaDeclaration : public Statement {
public:
    /// Initialize with formula name.
    /// \param _strName Declared type name.
    /// \param _pType Result type.
    /// \param _pFormula Formula.
    /// \param _pLabel Statement label.
    FormulaDeclaration(const std::wstring &_strName = L"", const TypePtr &_pType = NULL, const ExpressionPtr &_pFormula = NULL,
        const ExpressionPtr &_pMeasure = NULL, const LabelPtr &_pLabel = NULL) :
        m_strName(_strName), m_pFormula(_pFormula), m_pMeasure(_pMeasure), m_pType(_pType)
    {
        if (!_pType)
            m_pType = new Type(Type::BOOL);
    }

    /// Get statement kind.
    /// \returns #FormulaDeclaration.
    virtual int getKind() const { return FORMULA_DECLARATION; }

    /// Get list of formal parameters.
    /// \return List of parameters.
    NamedValues &getParams() { return m_params; }
    const NamedValues &getParams() const { return m_params; }

    /// Get formula name.
    /// \return Identifier.
    const std::wstring &getName() const { return m_strName; }

    /// Set formula name.
    /// \param _strName Identifier.
    void setName(const std::wstring &_strName) { m_strName = _strName; }

    /// Get result type.
    /// \return Type reference.
    const TypePtr &getResultType() const { return m_pType; }

    /// Set result type.
    /// \param _pType Result type.
    void setResultType(const TypePtr &_pType) { m_pType = _pType; }

    /// Get declared formula.
    /// \return Formula.
    const ExpressionPtr &getFormula() const { return m_pFormula; }

    /// Set declared formula postcondition.
    /// \param _pFormula Formula.
    void setFormula(const ExpressionPtr &_pFormula) { m_pFormula = _pFormula; }

    const ExpressionPtr& getMeasure() const { return m_pMeasure; }
    void setMeasure(const ExpressionPtr& _pMeasure) { m_pMeasure = _pMeasure; }

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        const FormulaDeclarationPtr pCopy = NEW_CLONE(this, _cloner, FormulaDeclaration(getName(), _cloner.get(getResultType()),
            _cloner.get(getFormula()), _cloner.get(getMeasure()), _cloner.get(getLabel())));
        pCopy->getParams().appendClones(getParams(), _cloner);
        pCopy->setLoc(this->getLoc());
        return pCopy;
    }

private:
    std::wstring m_strName;
    NamedValues m_params;
    ExpressionPtr m_pFormula, m_pMeasure;
    TypePtr m_pType;
};

/// Lemma declaration.
class LemmaDeclaration : public Statement {
public:
    /// Lemma status.
    enum {
        VALID,
        INVALID,
        UNKNOWN,
    };

    /// Default constructor.
    /// \param _pProposition Proposition.
    /// \param _pLabel Statement label.
    LemmaDeclaration(const ExpressionPtr &_pProposition = NULL, const LabelPtr &_pLabel = NULL) :
        Statement(_pLabel), m_pProposition(_pProposition), m_cStatus(UNKNOWN) {}

    /// Get statement kind.
    /// \returns #LemmaDeclaration.
    virtual int getKind() const { return LEMMA_DECLARATION; }

    /// Get proposition (boolean-typed expression).
    /// \return Proposition.
    const ExpressionPtr &getProposition() const { return m_pProposition; }

    /// Set proposition.
    /// \param _pProposition Proposition.
    void setProposition(const ExpressionPtr &_pProposition) { m_pProposition = _pProposition; }

    /// Get lemma status.
    int getStatus() const { return m_cStatus; }

    /// Set lemma status.
    /// \param _newStatus New lemma status.
    void setStatus(int _newStatus) { m_cStatus = _newStatus; }

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        return NEW_CLONE(this, _cloner, LemmaDeclaration(_cloner.get(getProposition()), _cloner.get(getLabel())));
    }

private:
    ExpressionPtr m_pProposition;
    int m_cStatus;
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
    const Collection<TypeDeclaration> &getTypes() const { return m_types; }

    /// Get list of declared variables.
    /// \return List of declared variables.
    Collection<VariableDeclaration> &getVariables() { return m_variables; }
    const Collection<VariableDeclaration> &getVariables() const { return m_variables; }

    /// Get list of declared messages.
    /// \return List of declared messages.
    Collection<Message> &getMessages() { return m_messages; }
    const Collection<Message> &getMessages() const { return m_messages; }

    /// Get list of processes.
    /// \return List of processes.
    Collection<Process> &getProcesses() { return m_processes; }
    const Collection<Process> &getProcesses() const { return m_processes; }

    /// Get list of formulas.
    /// \return List of formulas.
    Collection<FormulaDeclaration> &getFormulas() { return m_formulas; }
    const Collection<FormulaDeclaration> &getFormulas() const { return m_formulas; }

    /// Get list of lemmas.
    /// \return List of lemmas.
    Collection<LemmaDeclaration> &getLemmas() { return m_lemmas; }
    const Collection<LemmaDeclaration> &getLemmas() const { return m_lemmas; }

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;

    void cloneTo(DeclarationGroup &_dg, Cloner &_cloner) const {
        _dg.getPredicates().appendClones(getPredicates(), _cloner);
        _dg.getTypes().appendClones(getTypes(), _cloner);
        _dg.getVariables().appendClones(getVariables(), _cloner);
        _dg.getMessages().appendClones(getMessages(), _cloner);
        _dg.getProcesses().appendClones(getProcesses(), _cloner);
        _dg.getFormulas().appendClones(getFormulas(), _cloner);
        _dg.getLemmas().appendClones(getLemmas(), _cloner);
    }

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
    /// Initialize with class name.
    /// \param _strName Class name.
    /// \param _pClass Pointer to ancestor class declaration.
    Class(const std::wstring &_strName = L"", const ClassPtr &_pAncestor = NULL) : m_pAncestor(_pAncestor), m_strName(_strName) {}

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

    virtual NodePtr clone(Cloner &_cloner) const {
        ClassPtr pCopy = NEW_CLONE(this, _cloner, Class(getName(), _cloner.get(getAncestor(), true)));
        cloneTo(*pCopy, _cloner);
        return pCopy;
    }

private:
    ClassPtr m_pAncestor;
    std::wstring m_strName;
};

/// Module declaration.
/// If module is not declared explicitly implicit module declaration is assumed anyway.
class Module : public DeclarationGroup {
public:
    /// Default constructor.
    /// \param _strName Identifier.
    Module(const std::wstring &_strName = L"") : m_strName(_strName) {}

    virtual int getNodeKind() const { return Node::MODULE; }

    /// Get module name.
    /// \return Identifier.
    const std::wstring &getName() const { return m_strName; }

    /// Set module name.
    /// \param _strName Identifier.
    void setName(const std::wstring &_strName) { m_strName = _strName; }

    /// Get list of formal parameters.
    /// \return List of parameters.
    NamedValues &getParams() { return m_params; }
    const NamedValues &getParams() const { return m_params; }

    /// Get list of imported module names.
    /// \return List of imported module names.
    std::vector<std::wstring> &getImports() { return m_imports; }
    const std::vector<std::wstring> &getImports() const { return m_imports; }

    /// Get list of declared classes.
    /// \return List of declared classes.
    Collection<Class> &getClasses() { return m_classes; }
    const Collection<Class> &getClasses() const { return m_classes; }

    /// Get list of declared submodules.
    /// \return List of declared submodules.
    Collection<Module> &getModules() { return m_modules; }
    const Collection<Module> &getModules() const { return m_modules; }

    virtual bool less(const Node& _other) const;
    virtual bool equals(const Node& _other) const;

    virtual NodePtr clone(Cloner &_cloner) const {
        const ModulePtr pCopy = NEW_CLONE(this, _cloner, Module(getName()));
        pCopy->getParams().appendClones(getParams(), _cloner);
        cloneTo(*pCopy, _cloner);
        pCopy->getImports() = getImports();
        pCopy->getClasses().appendClones(getClasses(), _cloner);
        pCopy->getModules().appendClones(getModules(), _cloner);
        pCopy->setLoc(this->getLoc());
        return pCopy;
    }

    bool isTrivial() const {
        return getPredicates().empty() && getTypes().empty() && getVariables().empty()
            && getMessages().empty() && getProcesses().empty() && getFormulas().empty()
            && getLemmas().empty() && m_imports.empty() && m_classes.empty() && m_strName.empty()
            && m_modules.size() == 1;
    }

private:
    NamedValues m_params;
    std::vector<std::wstring> m_imports;
    Collection<Class> m_classes;
    Collection<Module> m_modules;
    std::wstring m_strName;
};

} // namespace ir

#endif /* DECLARATIONS_H_ */
