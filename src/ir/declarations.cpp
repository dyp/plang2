/// \file declarations.cpp
///

#include "ir/declarations.h"

using namespace ir;

bool Predicate::less(const Node& _other) const {
    if (!AnonymousPredicate::equals(_other))
        return AnonymousPredicate::less(_other);
    const Predicate& other = (const Predicate&)_other;
    if (isBuiltin() != other.isBuiltin())
        return !isBuiltin() && other.isBuiltin();
    return getName() < other.getName();
}

bool Predicate::equals(const Node& _other) const {
    if (!AnonymousPredicate::equals(_other))
        return false;
    const Predicate& other = (const Predicate&)_other;
    return isBuiltin() == other.isBuiltin() && getName() == other.getName();
}

bool VariableDeclaration::less(const Node& _other) const {
    if (!Statement::equals(_other))
        return Statement::less(_other);
    const VariableDeclaration& other = (const VariableDeclaration&)_other;
    if (!_equals(getVariable(), other.getVariable()))
        return _less(getVariable(), other.getVariable());
    return _less(getValue(), other.getValue());
}

bool VariableDeclaration::equals(const Node& _other) const {
    if (!Statement::equals(_other))
        return false;
    const VariableDeclaration& other = (const VariableDeclaration&)_other;
    return _equals(getVariable(), other.getVariable()) && _equals(getValue(), other.getValue());
}

bool FormulaDeclaration::less(const Node& _other) const {
    if (!Statement::equals(_other))
        return Statement::less(_other);
    const FormulaDeclaration& other = (const FormulaDeclaration&)_other;
    if (getName() != other.getName())
        return getName() < other.getName();
    if (!_equals(getResultType(), other.getResultType()))
        return _less(getResultType(), other.getResultType());
    if (!_equals(getFormula(), other.getFormula()))
        return _less(getFormula(), other.getFormula());
    return getParams() < other.getParams();
}

bool FormulaDeclaration::equals(const Node& _other) const {
    if (!Statement::equals(_other))
        return false;
    const FormulaDeclaration& other = (const FormulaDeclaration&)_other;
    return getName() == other.getName()
        && _equals(getResultType(), other.getResultType())
        && getParams() == other.getParams();
}

bool LemmaDeclaration::less(const Node& _other) const {
    if (!Statement::equals(_other))
        return Statement::less(_other);
    const LemmaDeclaration& other = (const LemmaDeclaration&)_other;
    if (getStatus() != other.getStatus())
        return getStatus() < other.getStatus();
    return _less(getProposition(), other.getProposition());
}

bool LemmaDeclaration::equals(const Node& _other) const {
    if (!Statement::equals(_other))
        return false;
    const LemmaDeclaration& other = (const LemmaDeclaration&)_other;
    return getStatus() == other.getStatus()
        && _equals(getProposition(), other.getProposition());
}

bool DeclarationGroup::less(const Node& _other) const {
    if (!Node::equals(_other))
        return Node::less(_other);
    const DeclarationGroup& other = (const DeclarationGroup&)_other;
    if (getPredicates() != other.getPredicates())
        return getPredicates() < other.getPredicates();
    if (getTypes() != other.getTypes())
        return getTypes() < other.getTypes();
    if (getVariables() != other.getVariables())
        return getVariables() < other.getVariables();
    if (getMessages() != other.getMessages())
        return getMessages() < other.getMessages();
    if (getProcesses() != other.getProcesses())
        return getProcesses() < other.getProcesses();
    if (getFormulas() != other.getFormulas())
        return getFormulas() < other.getFormulas();
    return getLemmas() < other.getLemmas();
}

bool DeclarationGroup::equals(const Node& _other) const {
    if (!Node::equals(_other))
        return false;
    const DeclarationGroup& other = (const DeclarationGroup&)_other;
    return getPredicates() == other.getPredicates() && getTypes() == other.getTypes()
        && getVariables() == other.getVariables() && getMessages() == other.getMessages()
        && getProcesses() == other.getProcesses() && getFormulas() == other.getFormulas()
        && getLemmas() == other.getLemmas();
}

bool Module::less(const Node& _other) const {
    if (!DeclarationGroup::equals(_other))
        return DeclarationGroup::less(_other);
    const Module& other = (const Module&)_other;
    if (getName() != other.getName())
        return getName() < other.getName();
    if (getParams() != other.getParams())
        return getParams() < other.getParams();
    if (getImports() != other.getImports())
        return getImports() < other.getImports();
    if (getClasses() != other.getClasses())
        return getClasses() < other.getClasses();
    return getModules() < other.getModules();
}

bool Module::equals(const Node& _other) const {
    if (!DeclarationGroup::equals(_other))
        return false;
    const Module& other = (const Module&)_other;
    return getName() == other.getName() && getParams() == other.getParams()
        && getImports() == other.getImports() && getClasses() == other.getClasses()
        && getModules() == other.getModules();
}
