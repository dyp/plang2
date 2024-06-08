/// \file generate_semantics.h
///


#ifndef GENERATE_SEMANTICS_H_
#define GENERATE_SEMANTICS_H_

#include "ir/declarations.h"
#include "ir/visitor.h"
#include "utils.h"
#include "verification.h"

namespace ir{

class CollectPreConditions : public Visitor {
    const ModulePtr m_module;
    class NameGenerator;
    std::shared_ptr<NameGenerator> m_pNameGen;
    PredicatePtr m_pPredicate;
    ProcessPtr m_pProcess;
    ModulePtr m_pNewModule;
    Collection<LemmaDeclaration> m_lemmas;

public:
    CollectPreConditions(const ModulePtr &_module);

    ///collecting preconditions, conditions from If's and Switch's for generating lemmas
    ExpressionPtr collectConditions();

    ExpressionPtr caseNonintersection(const ExpressionPtr& _pExpr1, const ExpressionPtr& _pExpr2);

    ExpressionPtr compatibility(const TypePtr& _pExpr1, const TypePtr& _pExpr2);

    RangePtr arrayRangeWithCurrentParams(const ExpressionPtr& _pArray);

    Collection<Range> arrayRangesWithCurrentParams(const ExpressionPtr& _pArray);

    ExpressionPtr varBelongsSetOneDimension(const VariableReferencePtr& _pVar, const ExpressionPtr& _pExpr);
    ExpressionPtr varsBelongSetSeveralDimensions(const Collection<VariableReference>& _vars, const ExpressionPtr& _pExpr);

    static TypePtr getNotNamedReferenceType(const TypePtr& _pType);

    bool visitPredicateType(const PredicateTypePtr &_node) override;          //creating modules for theory
    bool visitLambda(const LambdaPtr &_node) override;
    int handlePredicateDecl(NodePtr &_node) override;
    int handleProcessDecl(NodePtr &_node) override;

    int handlePredicatePreCondition(NodePtr &_node) override;      //adding preconditions to module
    int handlePredicateBranchPreCondition(NodePtr &_node) override;
    int handlePredicateTypePreCondition(NodePtr &_node) override;
    int handlePredicateTypeBranchPreCondition(NodePtr &_node) override;
    int handleProcessBranchPreCondition(NodePtr &_node) override;

    int handleFunctionCallee(NodePtr &_node) override;      //generating lemmas
    int handlePredicateCallBranchResults(NodePtr &_node) override;
    int handlePredicateCallArgs(NodePtr &_node) override;
    bool visitCall(const CallPtr &_node) override;
    bool visitBinary(const BinaryPtr &_node) override;
    bool visitIf(const IfPtr &_node) override;
    bool visitSwitch(const SwitchPtr &_node) override;
    bool visitArrayPartExpr(const ArrayPartExprPtr &_node) override;
    bool visitReplacement(const ReplacementPtr &_node) override;
    int handleSwitchDefault(NodePtr &_node) override;
    int handleSwitchCase(NodePtr &_node) override;
    int handleUnionConsFields(NodePtr &_node) override;
    bool visitAssignment(const AssignmentPtr &_node) override;
    bool visitVariableDeclaration(const VariableDeclarationPtr &_node) override;
    bool visitNamedValue(const NamedValuePtr &_node) override;
};

class VarSubstitute: public Visitor {
public:
    VarSubstitute() : Visitor(CHILDREN_FIRST) {}

    VarSubstitute(NamedValuePtr _pNamedValue, NodePtr _pSubstitution) : Visitor(CHILDREN_FIRST), m_pSubstitution(_pSubstitution), m_pNamedValue(_pNamedValue) {}

    bool visitVariableReference(const VariableReferencePtr &_var) override {
        if(m_pNamedValue->getName() == _var->getName())
            callSetter(m_pSubstitution);
        return true;
    }

    bool visitTypeDeclaration(const TypeDeclarationPtr &_typeDecl) override {
        if(m_pNamedValue->getName() == _typeDecl->getName()) {
            callSetter(m_pSubstitution);
//            _typeDecl.setType(m_pSubstitution);
        }
        return true;
    }
private:
    NodePtr m_pSubstitution;
    NamedValuePtr m_pNamedValue;
};

ModulePtr processPreConditions(const ModulePtr &_module);

void getRanges(const ArrayType &_array, Collection<Range> &_ranges);

};

vf::ConjunctionPtr getPreConditionForExpression(const ir::ExpressionPtr& _pExpr);
vf::ConjunctionPtr getPreConditionForStatement(const ir::StatementPtr& _pStmt, const ir::PredicatePtr& _pPred = NULL, const vf::ContextPtr& _pContext = NULL);
vf::ConjunctionPtr getPostConditionForStatement(const ir::StatementPtr& _pStmt, const vf::ContextPtr& _pContext = NULL);

#endif /* GENERATE_SEMANTICS_H_ */
