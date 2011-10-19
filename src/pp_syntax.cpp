
#include "prettyprinter.h"
#include "utils.h"
#include "options.h"

using namespace ir;

class PrettyPrinterSyntax: public PrettyPrinterBase {
public:
    PrettyPrinterSyntax(ir::Node &_node, std::wostream &_os) : PrettyPrinterBase(_os) {
        m_pNode = &_node;
    }

    void printLiteralKind(ir::Literal &_node) {
        switch (_node.getLiteralKind()) {
            case ir::Literal::UNIT:
                m_os << L"0";
                break;
            case ir::Literal::NUMBER:
                m_os << _node.getNumber().toString();
                break;
            case ir::Literal::BOOL:
                m_os << (_node.getBool() ? L"true" : L"false");
                break;
            case ir::Literal::CHAR:
                m_os <<  L"\'" << _node.getChar() << L"\'";
                break;
            case ir::Literal::STRING:
                m_os <<  L"\"" << _node.getString().c_str() << L"\"";
                break;
        }
    }

    void printUnaryOperator(ir::Unary &_node) {
        switch (_node.getOperator()) {
            case ir::Unary::PLUS:
                m_os << L"+";
                break;
            case ir::Unary::MINUS:
                m_os << L"-";
                break;
            case ir::Unary::BOOL_NEGATE:
                m_os << L"!";
                break;
            case ir::Unary::BITWISE_NEGATE:
                m_os << L"~";
                break;
        }
    }

    void printBinaryOperator(ir::Binary &_node) {
        switch (_node.getOperator()) {
            case ir::Binary::ADD:
                m_os << L"+";
                break;
            case ir::Binary::SUBTRACT:
                m_os << L"-";
                break;
            case ir::Binary::MULTIPLY:
                m_os << L"*";
                break;
            case ir::Binary::DIVIDE:
                m_os << L"/";
                break;
            case ir::Binary::REMAINDER:
                m_os << L"%";
                break;
            case ir::Binary::SHIFT_LEFT:
                m_os << L"<<";
                break;
            case ir::Binary::SHIFT_RIGHT:
                m_os << L">>";
                break;
            case ir::Binary::POWER:
                m_os << L"^";
                break;
            case ir::Binary::BOOL_AND:
                m_os << L"&";
                break;
            case ir::Binary::BOOL_OR:
                m_os << L"or";
                break;
            case ir::Binary::BOOL_XOR:
                m_os << L"^";
                break;
            case ir::Binary::EQUALS:
                m_os << L"=";
                break;
            case ir::Binary::NOT_EQUALS:
                m_os << L"!=";
                break;
            case ir::Binary::LESS:
                m_os << L"<";
                break;
            case ir::Binary::LESS_OR_EQUALS:
                m_os << L"<=";
                break;
            case ir::Binary::GREATER:
                m_os << L">";
                break;
            case ir::Binary::GREATER_OR_EQUALS:
                m_os << L">=";
                break;
            case ir::Binary::IMPLIES:
                m_os << L"=>";
                break;
        }
    }

    inline void printComma() {
        if (getLoc().bPartOfCollection && getLoc().cPosInCollection > 0)
            m_os << ", ";
    }

    void printQuantifier(int _quantifier) {
        switch (_quantifier) {
            case ir::Formula::NONE:
                m_os << "none";
                break;
            case ir::Formula::UNIVERSAL:
                m_os << "forall";
                break;
            case ir::Formula::EXISTENTIAL:
                m_os << "exists";
                break;
        }
    }

    //FIXME
    bool needsParen() {

        std::list<Visitor::Loc>::iterator i = prev(m_path.end());

        //if child NODE is not expresssion return false
        if (i->pNode->getNodeKind() != ir::Node::EXPRESSION)
            return false;
        const int nChildKind = ((ir::Expression*)i->pNode)->getKind();

        switch (nChildKind) {
            case ir::Expression::FORMULA_CALL:
            case ir::Expression::FUNCTION_CALL:
            case ir::Expression::LITERAL:
            case ir::Expression::VAR:
                return false;
            default:
                break;
        }

        if (i == m_path.begin())
            return false;
        else
            --i;

        //if parent NODE is not expression return false
        if (i->pNode->getNodeKind() != ir::Node::EXPRESSION)
            return false;
        const int nParentKind = ((ir::Expression*)i->pNode)->getKind();

        //if child expression have a same kind, that parent expression have, return true
        if (nParentKind == nChildKind)
            return true;

        switch (nParentKind) {
            case ir::Expression::UNARY:
            case ir::Expression::BINARY:
            case ir::Expression::TERNARY:
                break;

            default:
                return false;
        }

        //if parent expression have UNARY, BINARY or TERNARY kind, return true
        return true;

    }

    virtual bool visitLiteral(ir::Literal &_node) {
        printLiteralKind(_node);
        return true;
    }

    virtual bool visitVariableReference(ir::VariableReference &_node) {
        m_os << _node.getName();
        return true;
    }

    virtual bool visitPredicateReference(ir::PredicateReference &_node) {
        m_os << _node.getName();
        return true;
    }

    virtual bool traverseExpression(ir::Expression &_node) {

        printComma();

        const bool bParen = needsParen();
        if (bParen)
            m_os << "(";
        const bool result = Visitor::traverseExpression(_node);
        if (bParen)
            m_os << ")";

        return result;

    }

    virtual bool visitUnary(ir::Unary &_node) {
        printUnaryOperator(_node);
        return true;
    }

    virtual bool traverseBinary(ir::Binary &_node) {

        VISITOR_ENTER(Binary, _node);
        VISITOR_TRAVERSE(Expression, BinarySubexpression, _node.getLeftSide(), _node, Binary, setLeftSide);

        m_os << " ";
        printBinaryOperator(_node);
        m_os << " ";

        VISITOR_TRAVERSE(Expression, BinarySubexpression, _node.getRightSide(), _node, Binary, setRightSide);
        VISITOR_EXIT();

    }

    virtual bool traverseTernary(ir::Ternary &_node) {

        VISITOR_ENTER(Ternary, _node);

        VISITOR_TRAVERSE(Expression, TernarySubexpression, _node.getIf(), _node, Ternary, setIf);
        m_os << " ? ";
        VISITOR_TRAVERSE(Expression, TernarySubexpression, _node.getThen(), _node, Ternary, setThen);
        m_os << " : ";
        VISITOR_TRAVERSE(Expression, TernarySubexpression, _node.getElse(), _node, Ternary, setElse);

        VISITOR_EXIT();

    }

    virtual bool traverseFormula(ir::Formula &_node) {

        VISITOR_ENTER(Formula, _node);

        const int nQuantifier = _node.getQuantifier();

        if (nQuantifier != ir::Formula::NONE) {
            printQuantifier(nQuantifier);
            m_os << " ";
            VISITOR_TRAVERSE_COL(NamedValue, FormulaBoundVariable, _node.getBoundVariables());
            m_os << ". ";
        }

        VISITOR_TRAVERSE(Expression, Subformula, _node.getSubformula(), _node, Formula, setSubformula);
        VISITOR_EXIT();

    }

    virtual bool traverseFormulaCall(ir::FormulaCall &_node) {

        m_os << _node.getName();

        m_os << "(";
        const bool bResult = Visitor::traverseFormulaCall(_node);
        m_os << ")";

        return bResult;

    }

    virtual bool visitNamedValue(ir::NamedValue &_node) {
        printComma();
        callPrettyPrintCompact(_node);
        return true;
    }

    bool printParams(ir::FormulaDeclaration &_node) {

        if (_formulaParamExists(_node))
            VISITOR_TRAVERSE_COL(NamedValue, FormulaDeclParams, _node.getParams());

        if (_formulaResultTypeExists(_node)) {
            m_os << " : ";
            callPrettyPrintCompact(*(_node.getResultType()));
        }

    }

    virtual bool traverseFormulaDeclaration(ir::FormulaDeclaration &_node) {

        VISITOR_ENTER(FormulaDeclaration, _node);

        m_os << "formula " << _node.getName();

        if (_formulaParamExists(_node) || _formulaResultTypeExists(_node)) {
            m_os << " ( ";
            printParams(_node);
            m_os << " )";
        }

        m_os << " = ";
        VISITOR_TRAVERSE(Expression, FormulaDeclBody, _node.getFormula(), _node, FormulaDeclaration, setFormula);
        m_os << " ;\n";

        VISITOR_EXIT();

    }

    virtual bool traverseLemmaDeclaration(ir::LemmaDeclaration &_stmt) {
        VISITOR_ENTER(LemmaDeclaration, _stmt);
        VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);

        m_os << "lemma ";
        VISITOR_TRAVERSE(Expression, LemmaDeclBody, _stmt.getProposition(), _stmt, LemmaDeclaration, setProposition);
        m_os << " ;\n";

        VISITOR_EXIT();
    }

    virtual bool traverseArrayPartExpr(ir::ArrayPartExpr &_node) {
        VISITOR_ENTER(ArrayPartExpr, _node);

        VISITOR_TRAVERSE(Expression, ArrayPartObject, _node.getObject(), _node, Component, setObject);
        m_os << "[";
        VISITOR_TRAVERSE_COL(Expression, ArrayPartIndex, _node.getIndices());
        m_os << "]";

        VISITOR_EXIT();
    }

    virtual bool traverseFieldExpr(ir::FieldExpr &_node) {
        VISITOR_ENTER(FieldExpr, _node);

        m_os << _node.getFieldName() << ".";
        VISITOR_TRAVERSE(Expression, FieldObject, _node.getObject(), _node, Component, setObject);

        VISITOR_EXIT();
    }

    virtual bool traverseUnionAlternativeExpr(ir::UnionAlternativeExpr &_node) {
        VISITOR_ENTER(UnionAlternativeExpr, _node);

        m_os << _node.getName() << ".";
        VISITOR_TRAVERSE(Expression, UnionAlternativeObject, _node.getObject(), _node, Component, setObject);

        VISITOR_EXIT();
    }

    virtual bool traverseListElementExpr(ir::ListElementExpr &_node) {
        VISITOR_ENTER(ListElementExpr, _node);

        VISITOR_TRAVERSE(Expression, ListElementObject, _node.getObject(), _node, Component, setObject);
        m_os << "[";
        VISITOR_TRAVERSE(Expression, ListElementIndex, _node.getIndex(), _node, ListElementExpr, setIndex);
        m_os << "]";

        VISITOR_EXIT();
    }

    virtual bool traverseFunctionCall(ir::FunctionCall &_node) {
        VISITOR_ENTER(FunctionCall, _node);

        VISITOR_TRAVERSE(Expression, FunctionCallee, _node.getPredicate(), _node, FunctionCall, setPredicate);
        m_os << "(";
        VISITOR_TRAVERSE_COL(Expression, FunctionCallArgs, _node.getArgs());
        m_os << ")";

        VISITOR_EXIT();
    }

    virtual bool traverseBinder(ir::Binder &_node) {
        VISITOR_ENTER(Binder, _node);

        VISITOR_TRAVERSE(Expression, BinderCallee, _node.getPredicate(), _node, Binder, setPredicate);
        m_os << "(";
        VISITOR_TRAVERSE_COL(Expression, BinderArgs, _node.getArgs());
        m_os << ")";

        VISITOR_EXIT();
    }

    virtual bool traverseCastExpr(ir::CastExpr &_node) {
        VISITOR_ENTER(CastExpr, _node);

        m_os << "(";
        VISITOR_TRAVERSE(TypeExpr, CastToType, _node.getToType(), _node, CastExpr, setToType);
        m_os << ")";
        VISITOR_TRAVERSE(Expression, CastParam, _node.getExpression(), _node, CastExpr, setExpression);

        VISITOR_EXIT();
    }

    //TODO: MAP_ELEMENT, REPLACEMENT, LAMBDA, CONSTRUCTOR, TYPE

    void run() {
        traverseNode( *m_pNode );
    }

private:
    inline void callPrettyPrintCompact(ir::Node &_node) {
       prettyPrintCompact(_node, m_os, PPC_NO_INCOMPLETE_TYPES);
    }

    static bool _formulaParamExists(ir::FormulaDeclaration &_formula) {
        return !(_formula.getParams().empty());
    }

    static bool _formulaResultTypeExists(ir::FormulaDeclaration &_formula) {
        return (bool)_formula.getResultType();
    }

    ir::NodePtr m_pNode;

};

void prettyPrintSyntax(ir::Node &_node, std::wostream & _os) {
    PrettyPrinterSyntax pp(_node, _os);
    pp.run();
}
