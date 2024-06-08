/// \file pp_syntax.h
///

#ifndef PP_SYNTAX_H_
#define PP_SYNTAX_H_

#include <iostream>
#include "ir/base.h"
#include "prettyprinter.h"
#include "verification.h"
#include "generate_name.h"

namespace pp {

class Context;
using ContextPtr = std::shared_ptr<Context>;

class Context {
public:
    Context() {}

    NameGenerator& nameGenerator() { return m_names; }

    void collectPaths(const ir::NodePtr &_node);

    void getPath(const ir::NodePtr& _pNode, std::list<ir::ModulePtr>& _container);
    void clear();

private:
    std::map<ir::NodePtr, std::list<ir::ModulePtr>> m_paths;
    NameGenerator m_names;
};

class PrettyPrinterSyntax: public PrettyPrinterBase {
public:
    template<typename _Stream>
    PrettyPrinterSyntax(const ir::NodePtr &_node, _Stream &_os, ContextPtr _pContext) :
        PrettyPrinterBase(_os), m_pNode(_node),
        m_pContext(!_pContext ? std::make_shared<Context>() : _pContext)
    {}

    template<typename _Stream>
    PrettyPrinterSyntax(const ir::NodePtr &_node, _Stream &_os, size_t nDepth = 0) :
        PrettyPrinterBase(_os), m_pNode(_node),
        m_pContext(std::make_shared<Context>())
    {}

    template<typename _Stream>
    PrettyPrinterSyntax(_Stream &_os, bool _bCompact = false, int _nFlags = 0, ContextPtr _pContext = ContextPtr()) :
        PrettyPrinterBase(_os), m_nFlags(_nFlags), m_bCompact(_bCompact),
        m_pContext(!_pContext ? std::make_shared<Context>() : _pContext)
    {
        if (m_bCompact)
            m_os.setInline(true);
    }

    ~PrettyPrinterSyntax() {
        if (m_bCompact)
            m_os.setInline(false);
    }

    void run();
    void print(const ir::NodePtr &_pNode);

protected:
    // NODE / MODULE
    bool traverseModule(const ir::ModulePtr &_module) override;
    void printPath(const ir::NodePtr& _pNode);

    // NODE / LABEL
    bool visitLabel(const ir::LabelPtr &_label) override;

    // NODE / TYPE
    bool visitType(const ir::TypePtr &_type) override;
    bool visitTypeType(const ir::TypeTypePtr &_type) override;
    bool traverseNamedReferenceType(const ir::NamedReferenceTypePtr &_type) override;
    bool visitSetType(const ir::SetTypePtr &_type) override;
    bool visitListType(const ir::ListTypePtr &_type) override;
    bool visitRefType(const ir::RefTypePtr &_type) override;
    bool traverseMapType(const ir::MapTypePtr &_type) override;
    bool traverseSubtype(const ir::SubtypePtr &_type) override;
    bool traverseRange(const ir::RangePtr &_type) override;
    bool visitArrayType(const ir::ArrayTypePtr &_type) override;
    bool traverseEnumType(const ir::EnumTypePtr &_type) override;
    bool needsIndent();
    bool traverseStructType(const ir::StructTypePtr &_type) override;
    bool traverseUnionConstructorDeclaration(const ir::UnionConstructorDeclarationPtr &_cons) override;
    bool traverseUnionType(const ir::UnionTypePtr &_type) override;
    bool traversePredicateType(const ir::PredicateTypePtr &_type) override;
    bool traverseParameterizedType(const ir::ParameterizedTypePtr &_type) override;
    bool visitSeqType(const ir::SeqTypePtr& _type) override;
    bool visitOptionalType(const ir::OptionalTypePtr& _type) override;

    // NODE / STATEMENT
    bool traverseJump(const ir::JumpPtr &_stmt) override;
    bool traverseBlock(const ir::BlockPtr &_stmt) override;
    bool traverseParallelBlock(const ir::ParallelBlockPtr &_stmt) override;
    bool _traverseAnonymousPredicate(const ir::AnonymousPredicatePtr &_decl) override;
    bool traversePredicate(const ir::PredicatePtr &_stmt) override;
    bool traverseAssignment(const ir::AssignmentPtr &_stmt) override;
    bool traverseMultiassignment(const ir::MultiassignmentPtr &_stmt) override;
    bool traverseCall(const ir::CallPtr &_stmt) override;
    bool traverseIf(const ir::IfPtr &_stmt) override;
    bool traverseSwitch(const ir::SwitchPtr &_stmt) override;
    bool traverseSwitchCase(const ir::SwitchCasePtr &_case) override;
    bool traverseFor(const ir::ForPtr &_stmt) override;
    bool traverseWhile(const ir::WhilePtr &_stmt) override;
    bool traverseBreak(const ir::BreakPtr &_stmt) override;
    bool traverseWith(const ir::WithPtr &_stmt) override;
    bool traverseTypeDeclaration(const ir::TypeDeclarationPtr &_stmt) override;
    bool traverseVariableDeclaration(const ir::VariableDeclarationPtr &_stmt) override;
    bool traverseVariableDeclarationGroup(const ir::VariableDeclarationGroupPtr &_stmt) override;
    bool traverseFormulaDeclaration(const ir::FormulaDeclarationPtr &_node) override;
    bool traverseLemmaDeclaration(const ir::LemmaDeclarationPtr &_stmt) override;

    // NODE / NAMED_VALUE
    bool traverseNamedValue(const ir::NamedValuePtr &_val) override;
    bool traverseEnumValue(const ir::EnumValuePtr &_val) override;
    bool traverseParam(const ir::ParamPtr &_val) override;
    bool traverseVariable(const ir::VariablePtr &_val) override;

    // NODE / EXPRESSION
    void printLiteralKind(const ir::LiteralPtr &_node);
    void printUnaryOperator(const ir::UnaryPtr &_node);
    void printBinaryOperator(const ir::BinaryPtr &_node);
    void printQuantifier(int _quantifier);
    bool needsParen();
    bool traverseExpression(const ir::ExpressionPtr &_node) override;
    bool visitLiteral(const ir::LiteralPtr &_node) override;
    bool visitVariableReference(const ir::VariableReferencePtr &_node) override;
    bool visitPredicateReference(const ir::PredicateReferencePtr &_node) override;
    bool visitLambda(const ir::LambdaPtr &_node) override;
    bool traverseBinder(const ir::BinderPtr &_expr) override;
    bool visitUnary(const ir::UnaryPtr &_node) override;
    bool traverseBinary(const ir::BinaryPtr &_node) override;
    bool traverseTernary(const ir::TernaryPtr &_node) override;
    bool traverseFormula(const ir::FormulaPtr &_node) override;
    bool traverseReplacement(const ir::ReplacementPtr &_expr) override;
    bool traverseRecognizerExpr(const ir::RecognizerExprPtr &_expr) override;
    bool traverseAccessorExpr(const ir::AccessorExprPtr &_expr) override;
    bool traverseFunctionCall(const ir::FunctionCallPtr &_expr) override;
    bool traverseFormulaCall(const ir::FormulaCallPtr &_node) override;
    bool traverseStructFieldDefinition(const ir::StructFieldDefinitionPtr &_cons) override;
    bool traverseStructConstructor(const ir::StructConstructorPtr &_expr) override;
    bool traverseUnionConstructor(const ir::UnionConstructorPtr &_expr) override;
    bool traverseElementDefinition(const ir::ElementDefinitionPtr &_cons) override;
    bool traverseArrayConstructor(const ir::ArrayConstructorPtr &_expr) override;
    bool traverseMapConstructor(const ir::MapConstructorPtr &_expr) override;
    bool traverseSetConstructor(const ir::SetConstructorPtr &_expr) override;
    bool traverseListConstructor(const ir::ListConstructorPtr &_expr) override;
    bool traverseArrayPartDefinition(const ir::ArrayPartDefinitionPtr &_cons) override;
    bool traverseArrayIteration(const ir::ArrayIterationPtr &_expr) override;
    bool traverseArrayPartExpr(const ir::ArrayPartExprPtr &_expr) override;
    bool traverseFieldExpr(const ir::FieldExprPtr &_expr) override;
    bool traverseMapElementExpr(const ir::MapElementExprPtr &_expr) override;
    bool traverseListElementExpr(const ir::ListElementExprPtr &_expr) override;
    bool traverseCastExpr(const ir::CastExprPtr &_expr) override;
    bool visitConstructor(const ir::ConstructorPtr& _expr) override;

private:
    ir::NodePtr m_pNode;
    ir::ModulePtr m_pCurrentModule;
    int m_nFlags = 0;
    bool m_bCompact = false;
    bool m_bSingleLine = false;
    ContextPtr m_pContext;

    bool _traverseStructType(const ir::StructTypePtr &_type);
};

void prettyPrintSyntax(const ir::NodePtr &_node, std::wostream & _os = std::wcout, const ContextPtr& _pContext = NULL,  bool _bNewLine = false);
void prettyPrintSyntax(const ir::NodePtr &_node, size_t nDepth, std::wostream & _os = std::wcout);

std::wstring fmtRule(size_t _cRuleInd = 0);
void prettyPrint(const vf::ConjunctPtr& _pConjunct, std::wostream &_os = std::wcout, const ContextPtr& _pContext = NULL);
void prettyPrint(const vf::ConjunctionPtr& _pConj, std::wostream &_os = std::wcout, const ContextPtr& _pContext = NULL);
void prettyPrint(const vf::Condition& _cond, std::wostream &_os = std::wcout, const ContextPtr& _pContext = NULL);
void prettyPrint(const vf::Context& _context, std::wostream &_os = std::wcout, const ContextPtr& _pContext = NULL);

}

template<typename _Stream>
void prettyPrintCompact(const ir::NodePtr &_node, _Stream &_os, int _nFlags = 0, const pp::ContextPtr & _pContext = nullptr) {
    pp::PrettyPrinterSyntax(_os, true, _nFlags, _pContext).print(_node);
}

#endif /* PP_SYNTAX_H_ */
