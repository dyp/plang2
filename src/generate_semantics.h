/// \file generate_semantics.h
///


#ifndef GENERATE_SEMANTICS_H_
#define GENERATE_SEMANTICS_H_

#include "ir/declarations.h"
#include "ir/visitor.h"
#include "utils.h"
#include "generate_name.h"
#include "verification.h"

namespace ir{

class CollectPreConditions : public Visitor {
    Module &m_module;
    NameGenerator m_nameGen;
    Predicate *m_pPredicate = nullptr;
    Process *m_pProcess = nullptr;
    Auto<Module> m_pNewModule;
    Collection<LemmaDeclaration> m_lemmas;

public:
    CollectPreConditions(Module &_module) : m_module(_module) {}

    ///collecting preconditions, conditions from If's and Switch's for generating lemmas
    ExpressionPtr collectConditions();

    ExpressionPtr caseNonintersection(ExpressionPtr _pExpr1, ExpressionPtr _pExpr2);

    ExpressionPtr compatibility(TypePtr _pExpr1, TypePtr _pExpr2);

    RangePtr arrayRangeWithCurrentParams(ExpressionPtr _pArray);

    Collection<Range> arrayRangesWithCurrentParams(ExpressionPtr _pArray);

    ExpressionPtr varBelongsSetOneDimension(VariableReferencePtr _pVar, ExpressionPtr _pExpr);
    ExpressionPtr varsBelongSetSeveralDimensions(Collection<VariableReference> _vars, ExpressionPtr _pExpr);

    static TypePtr getNotNamedReferenceType(TypePtr _pType);

    virtual bool visitPredicateType(PredicateType &_node);          //creating modules for theory
    virtual bool visitLambda(Lambda &_node);
    virtual int handlePredicateDecl(Node &_node);
    virtual int handleProcessDecl(Node &_node);

    virtual int handlePredicatePreCondition(Node &_node);      //adding preconditions to module
    virtual int handlePredicateBranchPreCondition(Node &_node);
    virtual int handlePredicateTypePreCondition(Node &_node);
    virtual int handlePredicateTypeBranchPreCondition(Node &_node);
    virtual int handleProcessBranchPreCondition(Node &_node);

    virtual int handleFunctionCallee(Node &_node);      //generating lemmas
    virtual int handlePredicateCallBranchResults(Node &_node);
    virtual int handlePredicateCallArgs(Node &_node);
    virtual bool visitCall(Call &_node);
    virtual bool visitBinary(Binary &_node);
    virtual bool visitIf(If &_node);
    virtual bool visitSwitch(Switch &_node);
    virtual bool visitArrayPartExpr(ArrayPartExpr &_node);
    virtual bool visitReplacement(Replacement &_node);
    virtual int handleSwitchDefault(Node &_node);
    virtual int handleSwitchCase(Node &_node);
    virtual int handleUnionConsField(Node &_node);
    virtual bool visitAssignment(Assignment &_node);
    virtual bool visitVariableDeclaration(VariableDeclaration &_node);
    virtual bool visitNamedValue(NamedValue &_node);
};

class VarSubstitute: public Visitor {
public:
    VarSubstitute() : Visitor(CHILDREN_FIRST) {}

    VarSubstitute(NamedValuePtr _pNamedValue, NodePtr _pSubstitution) : Visitor(CHILDREN_FIRST), m_pSubstitution(_pSubstitution), m_pNamedValue(_pNamedValue) {}

    bool visitVariableReference(VariableReference &_var) {
        if(m_pNamedValue->getName() == _var.getName())
            callSetter(m_pSubstitution);
        return true;
    }

    bool visitTypeDeclaration(TypeDeclaration &_typeDecl) {
        if(m_pNamedValue->getName() == _typeDecl.getName()) {
            callSetter(m_pSubstitution);
//            _typeDecl.setType(m_pSubstitution);
        }
        return true;
    }
private:
    NodePtr m_pSubstitution;
    NamedValuePtr m_pNamedValue;
};

Auto<Module> processPreConditions(Module &_module);

void getRanges(const ArrayType &_array, Collection<Range> &_ranges);

};

vf::ConjunctionPtr getPreConditionForExpression(const ir::ExpressionPtr& _pExpr);
vf::ConjunctionPtr getPreConditionForStatement(const ir::StatementPtr& _pStmt, const ir::PredicatePtr& _pPred = NULL, const vf::ContextPtr& _pContext = NULL);
vf::ConjunctionPtr getPostConditionForStatement(const ir::StatementPtr& _pStmt, const vf::ContextPtr& _pContext = NULL);

#endif /* GENERATE_SEMANTICS_H_ */
