/// \file verification.cpp
///

#include "ir/visitor.h"
#include "utils.h"
#include <iostream>

using namespace ir;

class GeneratePvsBase : public ir::Visitor {
public:
    GeneratePvsBase(const ir::Node &_node, std::wostream &_os) :
        m_pNode(&_node), m_os(_os)
    {}

    // TODO Types translation.
    bool visitType(ir::Type &_type) {
        switch (_type.getKind()) {
            case ir::Type::BOOL:
                m_os << "bool";
                break;
            case ir::Type::NAT:
                m_os << "nat";
                break;
        }
        return false;
    }

protected:
    ir::NodePtr m_pNode;
    std::wostream &m_os;

};

void printPvsType(const ir::NodePtr &_pNode, std::wostream & _os) {
    if (!_pNode)
        return;
    GeneratePvsBase(*_pNode, _os).traverseNode(*_pNode);
}

class VariableNamesMangling : public ir::Visitor {
public:
    VariableNamesMangling(ir::Node &_node) :
        m_pNode(&_node)
    {}

    std::wstring getMangling(const ir::TypePtr _pType) {
        if (!_pType)
            return L"";
        switch (_pType->getKind()) {
            case ir::Type::BOOL:
                return L"b";
            case ir::Type::NAT:
                return L"n";
        }
    }

    bool visitVariableReference(ir::VariableReference &_var) {
        _var.setName(_var.getName() + L"_" + getMangling(_var.getType()));
        return false;
    }

    bool visitNamedValue(ir::NamedValue &_val) {
        _val.setName(_val.getName() + getMangling(_val.getType()));
        return false;
    }

    void run() {
        traverseNode(*m_pNode);
    }

private:
    ir::NodePtr m_pNode;
};

void mangleVariableNames(ir::Node &_node) {
    VariableNamesMangling(_node).run();
}

class GeneratePvsDeclarations : public ir::Visitor {
public:
    GeneratePvsDeclarations(const ir::Module &_module, std::wostream &_os) :
        m_pModule(&_module), m_os(_os)
    {}

    void printLines() {
        if (m_vars.size() == 0)
            return;
        for (size_t i=0; i<m_vars.size(); ++i) {
            for (size_t j=0; j<m_vars.get(i)->m_pVals->size(); ++j) {
                if (j > 0)
                    m_os << ", ";
                m_os << cyrillicToASCII(m_vars.get(i)->m_pVals->get(j)->getName());
            }
            m_os << ": VAR ";
            printPvsType(m_vars.get(i)->m_pType, m_os);
            m_os << "\n";
        }
    }

    void addNewLine(ir::NamedValuePtr _pVal) {
        DeclLinePtr pDeclLine = new DeclLine(*_pVal);
        m_vars.add(pDeclLine);
    }

    void addVariable(ir::NamedValuePtr _pVal) {
        if (m_vars.size() != 0)
        {
            for (size_t i=0; i<m_vars.size(); ++i)
                if (*m_vars.get(i)->m_pType == *_pVal->getType()) {
                    if (m_vars.get(i)->m_pVals->findByNameIdx(_pVal->getName()) == -1)
                        m_vars.get(i)->m_pVals->add(_pVal);
                    return;
                }
        }

        addNewLine(_pVal);
    }

    bool visitNamedValue(ir::NamedValue &_val) {
        addVariable(&_val);
        return false;
    }

    bool visitVariableReference(ir::VariableReference &_var) {
        addVariable(new ir::NamedValue(_var.getName(), _var.getType()));
        return false;
    }

    void run() {
        traverseNode(m_pModule->getFormulas());
        traverseNode(m_pModule->getLemmas());
        if (!m_vars.empty()) {
            m_os << "% Declarations.\n";
            printLines();
            m_os << "\n";
        }
    }

private:

    class DeclLine : public Node {
    public:
        DeclLine(ir::NamedValue &_val) :
            m_pVals(new ir::NamedValues), m_pType(_val.getType())
        {
            m_pVals->add(&_val);
        }

        ir::NamedValuesPtr m_pVals;
        ir::TypePtr m_pType;
    };
    typedef Auto<DeclLine> DeclLinePtr;

    ir::ModulePtr m_pModule;
    Collection<DeclLine> m_vars;
    std::wostream &m_os;
};

class GeneratePvs : public GeneratePvsBase {
public:
    GeneratePvs(const ir::Node &_node, std::wostream &_os) :
        GeneratePvsBase(_node, _os), m_bForall(true)
    {}

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
                if (pChild->getOperator() == pParent->getOperator())
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

    void printComma() {
        if (m_path.empty())
            return;
        if (getLoc().bPartOfCollection && getLoc().cPosInCollection > 0)
            m_os << ", ";
    }

    // TODO Operation translation.
    void printUnaryOperator(ir::Unary &_node) {
        switch (_node.getOperator()) {
            case ir::Unary::PLUS:
                m_os << L"+";
                break;
            case ir::Unary::MINUS:
                m_os << L"-";
                break;
            case ir::Unary::BOOL_NEGATE:
                m_os << L"NOT";
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
            case ir::Binary::POWER:
                m_os << L"^";
                break;
            case ir::Binary::BOOL_AND:
                m_os << L"AND";
                break;
            case ir::Binary::BOOL_OR:
                m_os << L"OR";
                break;
            case ir::Binary::BOOL_XOR:
                m_os << L"XOR";
                break;
            case ir::Binary::EQUALS:
                m_os << L"=";
                break;
            case ir::Binary::NOT_EQUALS:
                m_os << L"/=";
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
                m_os << L"IMPLIES";
                break;
        }
    }

    bool visitNamedValue(ir::NamedValue &_value) {
        printComma();
        m_os << cyrillicToASCII(_value.getName());
        return false;
    }

    virtual bool visitLiteral(ir::Literal &_node) {
        switch (_node.getLiteralKind()) {
            case ir::Literal::UNIT:
                m_os << L"0";
                break;
            case ir::Literal::NUMBER:
                m_os << _node.getNumber().toString();
                break;
            case ir::Literal::BOOL:
                m_os << (_node.getBool() ? L"TRUE" : L"FALSE");
                break;
        }
        return true;
    }

    bool visitVariableReference(ir::VariableReference &_node) {
        m_os << _node.getName();
        return false;
    }

    bool visitLabel(ir::Label &_label) {
        m_os << cyrillicToASCII(_label.getName()) << ": ";
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
        m_os << "IF ";
        VISITOR_TRAVERSE(Expression, TernarySubexpression, _node.getIf(), _node, Ternary, setIf);
        m_os << " THEN ";
        VISITOR_TRAVERSE(Expression, TernarySubexpression, _node.getThen(), _node, Ternary, setThen);
        m_os << " ELSE ";
        VISITOR_TRAVERSE(Expression, TernarySubexpression, _node.getElse(), _node, Ternary, setElse);
        VISITOR_EXIT();
    }

    virtual bool traverseFormula(ir::Formula &_node) {
        VISITOR_ENTER(Formula, _node);

        const int nQuantifier = _node.getQuantifier();

        if (nQuantifier == ir::Formula::EXISTENTIAL) {
            m_os << "EXISTS ";
            VISITOR_TRAVERSE_COL(NamedValue, FormulaBoundVariable, _node.getBoundVariables());
            m_os << ": ";
        }

        if (nQuantifier == ir::Formula::UNIVERSAL && m_bForall) {
            m_os << "FORALL ";
            VISITOR_TRAVERSE_COL(NamedValue, FormulaBoundVariable, _node.getBoundVariables());
            m_os << ": ";
        }

        VISITOR_TRAVERSE(Expression, Subformula, _node.getSubformula(), _node, Formula, setSubformula);
        VISITOR_EXIT();
    }

    virtual bool traverseFormulaCall(ir::FormulaCall &_node) {
        m_os << cyrillicToASCII(_node.getName());

        m_os << "(";
        const bool bResult = Visitor::traverseFormulaCall(_node);
        m_os << ")";

        return bResult;
    }

    virtual bool traverseLemmaDeclaration(ir::LemmaDeclaration &_stmt) {
        VISITOR_ENTER(LemmaDeclaration, _stmt);

        VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);
        m_os << "LEMMA\n    ";
        VISITOR_TRAVERSE(Expression, LemmaDeclBody, _stmt.getProposition(), _stmt, LemmaDeclaration, setProposition);
        m_os << "\n";

        VISITOR_EXIT();
    }

    virtual bool traverseFormulaDeclaration(ir::FormulaDeclaration &_node) {
        VISITOR_ENTER(FormulaDeclaration, _node);

        m_os << cyrillicToASCII(_node.getName());

        m_os << " (";
        VISITOR_TRAVERSE_COL(NamedValue, FormulaDeclParams, _node.getParams());
        m_os << ")";

        if (_node.getResultType()) {
            m_os << " : ";
            traverseNode(*_node.getResultType());
        }

        m_os << " = ";
        VISITOR_TRAVERSE(Expression, FormulaDeclBody, _node.getFormula(), _node, FormulaDeclaration, setFormula);
        m_os << "\n";

        VISITOR_EXIT();
    }

    bool visitPredicate(ir::Predicate &_pred) {
        return false;
    }

    bool traverseModule(ir::Module &_module) {
        if (_module.getName().empty())
            return Visitor::traverseModule(_module);

        m_os << cyrillicToASCII(_module.getName()) << " : THEORY\n" << "BEGIN\n\n";

        // Mangling.
        mangleVariableNames(_module.getFormulas());
        mangleVariableNames(_module.getLemmas());

        GeneratePvsDeclarations(_module, m_os).run();

        if (!_module.getFormulas().empty()) {
            m_os << "% Formulas.\n";
            traverseNode(_module.getFormulas());
            m_os << "\n";
        }

        m_bForall = false;

        if (!_module.getLemmas().empty()) {
            m_os << "% Lemmas.\n";
            traverseNode(_module.getLemmas());
            m_os << "\n";
        }

        m_os << "END " << cyrillicToASCII(_module.getName()) << "\n";

        return true;
    }

    void run() {
        traverseNode(*m_pNode);
    }

private:
    bool m_bForall;
};

void generatePvs(const ir::Module &_module, std::wostream & _os) {
    GeneratePvs(_module, _os).run();
}