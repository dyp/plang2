/// \file pp_syntax.h
///

#ifndef PP_SYNTAX_H_
#define PP_SYNTAX_H_

#include <iostream>
#include "ir/base.h"
#include "prettyprinter.h"

class PrettyPrinterSyntax: public PrettyPrinterBase {
public:
    PrettyPrinterSyntax(ir::Node &_node, std::wostream &_os) :
        PrettyPrinterBase(_os), m_pNode(&_node), m_szDepth(0), m_bCompact(false), m_nFlags(0), m_bMergeLines(false), m_bSingleLine(false),
        m_nLastFoundIdentifier(1)
    {}
    PrettyPrinterSyntax(std::wostream &_os, bool _bCompact = false, int _nFlags = 0) :
        PrettyPrinterBase(_os), m_pNode(NULL), m_szDepth(0), m_bCompact(_bCompact), m_nFlags(_nFlags), m_bMergeLines(false), m_bSingleLine(false),
        m_nLastFoundIdentifier(1)
    {}

    void run();
    void print(ir::Node &_node);

protected:

    // NODE / MODULE
    virtual bool _traverseDeclarationGroup(ir::DeclarationGroup &_decl);
    void printDeclarationGroup(ir::Module &_module);
    virtual bool traverseModule(ir::Module &_module);

    // NODE / LABEL
    virtual bool visitLabel(ir::Label &_label);

    // NODE / TYPE
    virtual bool traverseType(ir::Type &_type);
    virtual bool visitType(ir::Type &_type);
    virtual bool visitTypeType(ir::TypeType &_type);
    virtual bool visitNamedReferenceType(ir::NamedReferenceType &_type);
    virtual bool visitSetType(ir::SetType &_type);
    virtual bool visitListType(ir::ListType &_type);
    virtual bool traverseMapType(ir::MapType &_type);
    virtual bool traverseSubtype(ir::Subtype &_type);
    virtual bool traverseRange(ir::Range &_type);
    virtual bool visitArrayType(ir::ArrayType &_type);
    virtual bool traverseEnumType(ir::EnumType &_type);
    bool needsIndent();
    void printStructNamedValues(const ir::NamedValues& _nvs, std::set<std::wstring>& _usedNames, bool& _bIsFirst, bool _bNeedsIndent);
    virtual bool traverseStructType(ir::StructType &_type);
    virtual bool traverseUnionConstructorDeclaration(ir::UnionConstructorDeclaration &_cons);
    virtual bool traverseUnionType(ir::UnionType &_type);
    virtual bool traversePredicateType(ir::PredicateType &_type);
    virtual bool traverseParameterizedType(ir::ParameterizedType &_type);
    virtual bool visitSeqType(ir::SeqType& _type);
    virtual bool visitOptionalType(ir::OptionalType& _type);

    // NODE / STATEMENT
    virtual bool traverseStatement(ir::Statement &_stmt);
    virtual bool traverseJump(ir::Jump &_stmt);
    virtual bool traverseBlock(ir::Block &_stmt);
    virtual bool traverseParallelBlock(ir::ParallelBlock &_stmt);
    virtual bool _traverseAnonymousPredicate(ir::AnonymousPredicate &_decl);
    virtual bool traversePredicate(ir::Predicate &_stmt);
    virtual bool traverseAssignment(ir::Assignment &_stmt);
    virtual bool traverseMultiassignment(ir::Multiassignment &_stmt);
    virtual bool traverseCall(ir::Call &_stmt);
    void printMergedStatement(const ir::StatementPtr _pStmt);
    void feedLine(const ir::Statement& _stmt);
    virtual bool traverseIf(ir::If &_stmt);
    virtual bool traverseSwitch(ir::Switch &_stmt);
    virtual bool traverseFor(ir::For &_stmt);
    virtual bool traverseWhile(ir::While &_stmt);
    virtual bool traverseBreak(ir::Break &_stmt);
    virtual bool traverseWith(ir::With &_stmt);
    virtual bool traverseTypeDeclaration(ir::TypeDeclaration &_stmt);
    virtual bool traverseVariable(ir::Variable &_val);
    virtual bool traverseVariableDeclaration(ir::VariableDeclaration &_stmt);
    virtual bool traverseVariableDeclarationGroup(ir::VariableDeclarationGroup &_stmt);
    virtual bool traverseFormulaDeclaration(ir::FormulaDeclaration &_node);
    virtual bool traverseLemmaDeclaration(ir::LemmaDeclaration &_stmt);

    // NODE / NAMED_VALUE
    std::wstring getNamedValueName(ir::NamedValue &_val);
    virtual bool visitNamedValue(ir::NamedValue &_val);

    // NODE / EXPRESSION
    void printLiteralKind(ir::Literal &_node);
    void printUnaryOperator(ir::Unary &_node);
    void printBinaryOperator(ir::Binary &_node);
    void printQuantifier(int _quantifier);
    void printComma();
    ir::ExpressionPtr getChild();
    ir::ExpressionPtr getParent();
    int getParentKind();
    int getChildKind();
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
    virtual bool visitConstructor(ir::Constructor& _expr);

    size_t getDepth() const;
    std::wstring fmtIndent(const std::wstring &_s = L"");
    void incTab();
    void decTab();
    void mergeLines();
    void separateLines();
    std::wstring getNewLabelName(const std::wstring& _name = L"");

private:
    ir::NodePtr m_pNode;
    size_t m_szDepth;
    int m_nFlags;
    bool m_bMergeLines, m_bCompact, m_bSingleLine;
    std::set<std::wstring> m_usedLabels;
    std::map<ir::NamedValuePtr, std::wstring> m_identifiers;
    std::set<std::wstring> m_usedIdentifiers;
    int m_nLastFoundIdentifier;
    std::multimap<ir::NodePtr, ir::NodePtr> m_decls, m_deps;

    void _buildDependencies(ir::NodePtr _pRoot);
    void _topologicalSort(const ir::NodePtr& _pDecl, std::list<ir::NodePtr>& _sorted);

};

void prettyPrintSyntax(ir::Node &_node, std::wostream & _os = std::wcout);
void prettyPrintSyntaxCompact(ir::Node &_node, size_t _depth = 3, std::wostream & _os = std::wcout);

#endif /* PP_SYNTAX_H_ */
