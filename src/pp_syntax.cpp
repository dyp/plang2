
#include "prettyprinter.h"
#include "utils.h"
#include "options.h"

using namespace ir;

class PrettyPrinterSyntax: public PrettyPrinterBase {
public:
    PrettyPrinterSyntax(ir::Node &_node, std::wostream &_os) : PrettyPrinterBase(_os) {
        m_pNode = &_node;
    }

    // NODE / MODULE
    bool visitModule(ir::Module &_module) {
        if (_module.getName().length() != 0)
            m_os << L"module " << _module.getName() << L";\n";
        return true;
    }

    // NODE / LABEL
    bool visitLabel(ir::Label &_label) {
        m_os << _label.getName() << ": ";
        return true;
    }

    // NODE / STATEMENT
    bool visitPredicate(ir::Predicate &_node) {
        // TODO Predicate
        return false;
    }

    bool traverseAssignment(ir::Assignment &_stmt) {
        VISITOR_ENTER(Assignment, _stmt);

        VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);
        VISITOR_TRAVERSE(Expression, LValue, _stmt.getLValue(), _stmt, Assignment, setLValue);
        m_os << " = ";
        VISITOR_TRAVERSE(Expression, RValue, _stmt.getExpression(), _stmt, Assignment, setExpression);
        m_os << ";\n";

        VISITOR_EXIT();
    }

    // NODE / NAMED_VALUE
    virtual bool visitNamedValue(ir::NamedValue &_node) {
        printComma();
        VISITOR_TRAVERSE(Type, NamedValueType, _node.getType(), _node, NamedValue, setType);
        m_os << L" " << _node.getName();
        return false;
    }

    // NODE / EXPRESSION
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

    void printComma() {
        if (m_path.empty())
            return;
        if (getLoc().bPartOfCollection && getLoc().cPosInCollection > 0)
            m_os << ", ";
    }

    ir::ExpressionPtr getChild() {
        std::list<Visitor::Loc>::iterator i = ::prev(m_path.end());
        if (i->pNode->getNodeKind() != ir::Node::EXPRESSION)
            return NULL;
        return i->pNode;
    }

    ir::ExpressionPtr getParent() {
        std::list<Visitor::Loc>::iterator i = ::prev(m_path.end());
        if (i->pNode->getNodeKind() != ir::Node::EXPRESSION)
            return NULL;
        if (i == m_path.begin())
            return NULL;
        else
            --i;
        if (i->pNode->getNodeKind() != ir::Node::EXPRESSION)
            return NULL;
        return i->pNode;
    }

    int getParentKind() {
        const ir::ExpressionPtr pParent = getParent();
        if (pParent)
            return pParent->getKind();
        else
            return -1;
    }

    int getChildKind() {
        const ir::ExpressionPtr pChild = getChild();
        if (pChild)
            return pChild->getKind();
        else
            return -1;
    }

    bool needsParen() {

        const int nChildKind = getChildKind();
        if (nChildKind == -1)
            return false;

        switch (nChildKind) {
            case ir::Expression::FORMULA_CALL:
            case ir::Expression::FUNCTION_CALL:
            case ir::Expression::LITERAL:
            case ir::Expression::VAR:
                return false;
            default:
                break;
        }

        const int nParentKind = getParentKind();
        if (nParentKind == -1)
            return false;

        if (nParentKind == nChildKind) {
            if (nParentKind == ir::Expression::BINARY) {
                const ir::BinaryPtr pChild = getChild().as<Binary>();
                const ir::BinaryPtr pParent = getParent().as<Binary>();
                if (pChild->getOperator() >= pParent->getOperator())
                    return false;
            }
            return true;
        }

        switch (nParentKind) {
            case ir::Expression::UNARY:
            case ir::Expression::BINARY:
            case ir::Expression::TERNARY:
                break;

            default:
                return false;
        }

        return true;

    }

    virtual bool traverseExpression(ir::Expression &_node) {

        printComma();

        bool bParen;
        if (m_path.empty())
            bParen = false;
        else
            bParen = needsParen();

        if (bParen)
            m_os << "(";
        const bool result = Visitor::traverseExpression(_node);
        if (bParen)
            m_os << ")";

        return result;

    }

    virtual bool visitLiteral(ir::Literal &_node) {
        printLiteralKind(_node);
        return true;
    }

    virtual bool visitVariableReference(ir::VariableReference &_node) {
        m_os << _node.getName();
        return false;
    }

    virtual bool visitPredicateReference(ir::PredicateReference &_node) {
        m_os << _node.getName();
        return true;
    }

    virtual bool visitType(ir::Type &_type) {
        callPrettyPrintCompact(_type);
        return false;
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

    // NODE / STATEMENT
    virtual bool traverseFormulaDeclaration(ir::FormulaDeclaration &_node) {

        VISITOR_ENTER(FormulaDeclaration, _node);

        m_os << "formula " << _node.getName();

        if (!_node.getParams().empty() || _node.getResultType()) {
            m_os << " ( ";

            if (!_node.getParams().empty())
                VISITOR_TRAVERSE_COL(NamedValue, FormulaDeclParams, _node.getParams());

            if (_node.getResultType()) {
                m_os << " : ";
                callPrettyPrintCompact(*(_node.getResultType()));
            }

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
        m_os << ";\n";

        VISITOR_EXIT();
    }

    void run() {
        traverseNode( *m_pNode );
    }

private:
    inline void callPrettyPrintCompact(ir::Node &_node) {
       prettyPrintCompact(_node, m_os, PPC_NO_INCOMPLETE_TYPES);
    }

    ir::NodePtr m_pNode;

};

void prettyPrintSyntax(ir::Node &_node, std::wostream & _os) {
    PrettyPrinterSyntax(_node, _os).run();
}
