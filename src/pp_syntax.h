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

class Context : public Counted {
public:
    Context() {}

    NameGenerator& nameGenerator() { return m_names; }

    void collectPaths(ir::Node &_node);

    void getPath(const ir::NodePtr& _pNode, std::list<ir::ModulePtr>& _container);
    void sortModule(ir::Module &_module, ir::Nodes& _sorted);
    void clear();

private:
    std::multimap<ir::NodePtr, ir::NodePtr> m_decls, m_deps;
    std::map<ir::NodePtr, std::list<ir::ModulePtr>> m_paths;
    NameGenerator m_names;

    void _buildDependencies(ir::NodePtr _pRoot);
    void _topologicalSort(const ir::NodePtr& _pDecl, ir::Nodes& _sorted);
};
typedef Auto<Context> ContextPtr;

class PrettyPrinterSyntax: public PrettyPrinterBase {
public:
    template<typename _Stream>
    PrettyPrinterSyntax(ir::Node &_node, _Stream &_os, ContextPtr _pContext) :
        PrettyPrinterBase(_os), m_pNode(&_node), m_nFlags(0), m_bCompact(false), m_bSingleLine(false),
        m_pContext(!_pContext ? new Context() : _pContext)
    {}

    template<typename _Stream>
    PrettyPrinterSyntax(ir::Node &_node, _Stream &_os, size_t nDepth = 0) :
        PrettyPrinterBase(_os), m_pNode(&_node), m_nFlags(0), m_bCompact(false), m_bSingleLine(false),
        m_pContext(new Context())
    {}

    template<typename _Stream>
    PrettyPrinterSyntax(_Stream &_os, bool _bCompact = false, int _nFlags = 0, ContextPtr _pContext = NULL) :
        PrettyPrinterBase(_os), m_pNode(NULL), m_nFlags(_nFlags), m_bCompact(_bCompact), m_bSingleLine(false),
        m_pContext(!_pContext ? new Context() : _pContext)
    {}

    void run();
    void print(ir::Node &_node);
    void print(const ir::NodePtr &_pNode);

protected:
    // NODE / MODULE
    virtual bool traverseModule(ir::Module &_module);
    void printPath(const ir::NodePtr& _pNode);

    // NODE / LABEL
    virtual bool visitLabel(ir::Label &_label);

    // NODE / TYPE
    virtual bool visitType(ir::Type &_type);
    virtual bool visitTypeType(ir::TypeType &_type);
    virtual bool traverseNamedReferenceType(ir::NamedReferenceType &_type);
    virtual bool visitSetType(ir::SetType &_type);
    virtual bool visitListType(ir::ListType &_type);
    virtual bool visitRefType(ir::RefType &_type);
    virtual bool traverseMapType(ir::MapType &_type);
    virtual bool traverseSubtype(ir::Subtype &_type);
    virtual bool traverseRange(ir::Range &_type);
    virtual bool visitArrayType(ir::ArrayType &_type);
    virtual bool traverseEnumType(ir::EnumType &_type);
    bool needsIndent();
    virtual bool traverseStructType(ir::StructType &_type);
    virtual bool traverseUnionConstructorDeclaration(ir::UnionConstructorDeclaration &_cons);
    virtual bool traverseUnionType(ir::UnionType &_type);
    virtual bool traversePredicateType(ir::PredicateType &_type);
    virtual bool traverseParameterizedType(ir::ParameterizedType &_type);
    virtual bool visitSeqType(ir::SeqType& _type);
    virtual bool visitOptionalType(ir::OptionalType& _type);

    // NODE / STATEMENT
    virtual bool traverseJump(ir::Jump &_stmt);
    virtual bool traverseBlock(ir::Block &_stmt);
    virtual bool traverseParallelBlock(ir::ParallelBlock &_stmt);
    virtual bool _traverseAnonymousPredicate(ir::AnonymousPredicate &_decl);
    virtual bool traversePredicate(ir::Predicate &_stmt);
    virtual bool traverseAssignment(ir::Assignment &_stmt);
    virtual bool traverseMultiassignment(ir::Multiassignment &_stmt);
    virtual bool traverseCall(ir::Call &_stmt);
    virtual bool traverseIf(ir::If &_stmt);
    virtual bool traverseSwitch(ir::Switch &_stmt);
    virtual bool traverseSwitchCase(ir::SwitchCase &_case);
    virtual bool traverseFor(ir::For &_stmt);
    virtual bool traverseWhile(ir::While &_stmt);
    virtual bool traverseBreak(ir::Break &_stmt);
    virtual bool traverseWith(ir::With &_stmt);
    virtual bool traverseTypeDeclaration(ir::TypeDeclaration &_stmt);
    virtual bool traverseVariableDeclaration(ir::VariableDeclaration &_stmt);
    virtual bool traverseVariableDeclarationGroup(ir::VariableDeclarationGroup &_stmt);
    virtual bool traverseFormulaDeclaration(ir::FormulaDeclaration &_node);
    virtual bool traverseLemmaDeclaration(ir::LemmaDeclaration &_stmt);

    // NODE / NAMED_VALUE
    virtual bool traverseNamedValue(ir::NamedValue &_val);
    virtual bool traverseEnumValue(ir::EnumValue &_val);
    virtual bool traverseParam(ir::Param &_val);
    virtual bool traverseVariable(ir::Variable &_val);

    // NODE / EXPRESSION
    void printLiteralKind(ir::Literal &_node);
    void printUnaryOperator(ir::Unary &_node);
    void printBinaryOperator(ir::Binary &_node);
    void printQuantifier(int _quantifier);
    bool needsParen();
    virtual bool traverseExpression(ir::Expression &_node);
    virtual bool visitLiteral(ir::Literal &_node);
    virtual bool visitVariableReference(ir::VariableReference &_node);
    virtual bool visitPredicateReference(ir::PredicateReference &_node);
    virtual bool visitLambda(ir::Lambda &_node);
    virtual bool traverseBinder(ir::Binder &_expr);
    virtual bool visitUnary(ir::Unary &_node);
    virtual bool traverseBinary(ir::Binary &_node);
    virtual bool traverseTernary(ir::Ternary &_node);
    virtual bool traverseFormula(ir::Formula &_node);
    virtual bool traverseReplacement(ir::Replacement &_expr);
    virtual bool traverseRecognizerExpr(ir::RecognizerExpr &_expr);
    virtual bool traverseAccessorExpr(ir::AccessorExpr &_expr);
    virtual bool traverseFunctionCall(ir::FunctionCall &_expr);
    virtual bool traverseFormulaCall(ir::FormulaCall &_node);
    virtual bool traverseStructFieldDefinition(ir::StructFieldDefinition &_cons);
    virtual bool traverseStructConstructor(ir::StructConstructor &_expr);
    virtual bool traverseUnionConstructor(ir::UnionConstructor &_expr);
    virtual bool traverseElementDefinition(ir::ElementDefinition &_cons);
    virtual bool traverseArrayConstructor(ir::ArrayConstructor &_expr);
    virtual bool traverseMapConstructor(ir::MapConstructor &_expr);
    virtual bool traverseSetConstructor(ir::SetConstructor &_expr);
    virtual bool traverseListConstructor(ir::ListConstructor &_expr);
    virtual bool traverseArrayPartDefinition(ir::ArrayPartDefinition &_cons);
    virtual bool traverseArrayIteration(ir::ArrayIteration &_expr);
    virtual bool traverseArrayPartExpr(ir::ArrayPartExpr &_expr);
    virtual bool traverseFieldExpr(ir::FieldExpr &_expr);
    virtual bool traverseMapElementExpr(ir::MapElementExpr &_expr);
    virtual bool traverseListElementExpr(ir::ListElementExpr &_expr);
    virtual bool traverseCastExpr(ir::CastExpr &_expr);
    virtual bool visitConstructor(ir::Constructor& _expr);

private:
    ir::NodePtr m_pNode;
    ir::ModulePtr m_pCurrentModule;
    int m_nFlags;
    bool m_bCompact, m_bSingleLine;
    ContextPtr m_pContext;

    bool _traverseStructType(ir::StructType &_type);
};

void prettyPrintSyntax(ir::Node &_node, std::wostream & _os = std::wcout, const ContextPtr& _pContext = NULL,  bool _bNewLine = false);
void prettyPrintSyntax(ir::Node &_node, size_t nDepth, std::wostream & _os = std::wcout);

void prettyPrintCompact(ir::Node &_node, std::wostream &_os, int _nFlags, const ContextPtr& _pContext = NULL);

std::wstring fmtRule(size_t _cRuleInd = 0);
void prettyPrint(const vf::ConjunctPtr& _pConjunct, std::wostream &_os = std::wcout, const ContextPtr& _pContext = NULL);
void prettyPrint(const vf::ConjunctionPtr& _pConj, std::wostream &_os = std::wcout, const ContextPtr& _pContext = NULL);
void prettyPrint(const vf::Condition& _cond, std::wostream &_os = std::wcout, const ContextPtr& _pContext = NULL);
void prettyPrint(const vf::Context& _context, std::wostream &_os = std::wcout, const ContextPtr& _pContext = NULL);

}

template<typename _Stream>
void prettyPrintCompact(ir::Node &_node, _Stream &_os, int _nFlags = 0) {
    pp::PrettyPrinterSyntax(_os, true, _nFlags).print(_node);
}

#endif /* PP_SYNTAX_H_ */
