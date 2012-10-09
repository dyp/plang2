
#include "prettyprinter.h"
#include "utils.h"
#include "options.h"

using namespace ir;

class PrettyPrinterSyntax: public PrettyPrinterBase {
public:
    PrettyPrinterSyntax(ir::Node &_node, std::wostream &_os) :
        PrettyPrinterBase(_os), m_szDepth(0), m_bMergeLines(false)
    {
        m_pNode = &_node;
    }

    template <class _Base, class _Node>
    void printGroup(const Collection<_Base, _Node>& _group, const std::wstring& _strSeparator) {
        for (size_t i = 0; i < _group.size(); ++i) {
            if (!_group.get(i))
                continue;
            m_os << fmtIndent();

            mergeLines();
            traverseNode(*_group.get(i));
            separateLines();

            m_os << _strSeparator;
        }
    }

    // NODE / MODULE
    bool _traverseDeclarationGroup(DeclarationGroup &_decl) {
        printGroup(_decl.getPredicates(), L"\n");
        printGroup(_decl.getTypes(), L";\n");
        printGroup(_decl.getVariables(), L";\n");
        printGroup(_decl.getFormulas(), L";\n");
        printGroup(_decl.getLemmas(), L";\n");

        VISITOR_TRAVERSE_COL(Message, MessageDecl, _decl.getMessages());
        VISITOR_TRAVERSE_COL(Process, ProcessDecl, _decl.getProcesses());
        return true;
    }

    bool traverseModule(Module &_module) {
        VISITOR_ENTER(Module, _module);

        const bool bNeedsIndent = !_module.getName().empty();

        if (bNeedsIndent) {
            m_os << fmtIndent(L"module ") << _module.getName();
            if (!_module.getParams().empty()) {
                m_os << L"(";
                VISITOR_TRAVERSE_COL(Param, ModuleParam, _module.getParams());
                m_os << L")";
            }
            m_os << L" {\n";
            incTab();
        }

        VISITOR_TRAVERSE_COL(Module, ModuleDecl, _module.getModules());

        if (!_traverseDeclarationGroup(_module))
            return false;

        VISITOR_TRAVERSE_COL(Class, ClassDecl, _module.getClasses());

        if (bNeedsIndent) {
            decTab();
            m_os << fmtIndent(L"}\n");
        }

        VISITOR_EXIT();
    }

    // NODE / LABEL
    bool visitLabel(ir::Label &_label) {
        m_usedLabels.insert(_label.getName());
        m_os << _label.getName() << ": ";
        return true;
    }

    // NODE / STATEMENT
    bool traverseStatement(Statement &_stmt) {
        if (!m_bMergeLines
            && _stmt.getKind() != Statement::PARALLEL_BLOCK
            && _stmt.getKind() != Statement::VARIABLE_DECLARATION_GROUP)
            m_os << fmtIndent();
        separateLines();
        return Visitor::traverseStatement(_stmt);
    }

    bool traverseJump(Jump &_stmt) {
        VISITOR_ENTER(Jump, _stmt);
        VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);
        m_os << L"#" << _stmt.getDestination()->getName();
        VISITOR_EXIT();
    }

    bool traverseBlock(Block &_stmt) {
        VISITOR_ENTER(Block, _stmt);
        VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);

        m_os << L"{\n";
        incTab();

        for (size_t i=0; i<_stmt.size(); ++i) {
            const bool bIsLast = i != _stmt.size() - 1;
            traverseStatement(*_stmt.get(i));
            m_os << (bIsLast && !_stmt.get(i)->isBlockLike() ? L";\n" : L"\n");
        }

        decTab();
        m_os << fmtIndent(L"}");

        VISITOR_EXIT();
    }

    bool traverseParallelBlock(ParallelBlock &_stmt) {
        VISITOR_ENTER(ParallelBlock, _stmt);
        VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);

        for (size_t i = 0; i < _stmt.size(); ++i) {
            if (i > 0) {
                m_os << fmtIndent(L"|| ");
                mergeLines();
            }
            traverseStatement(*_stmt.get(i));
            if (i != _stmt.size() - 1)
                m_os << L"\n";
            if (i == 0 && _stmt.size() != 1)
                incTab();
        }

        if (_stmt.size() != 1)
            decTab();

        VISITOR_EXIT();
    }

    bool _traverseAnonymousPredicate(AnonymousPredicate &_decl) {
        m_os << L"(";
        VISITOR_TRAVERSE_COL(Param, PredicateInParam, _decl.getInParams());
        m_os << L" : ";

        bool bHavePreConditions = false, bHavePostConditions = false;

        for (size_t i = 0; i < _decl.getOutParams().size(); ++i) {
            Branch &br = *_decl.getOutParams().get(i);
            bHavePreConditions = bHavePreConditions || br.getPreCondition();
            bHavePostConditions = bHavePostConditions || br.getPostCondition();
            VISITOR_TRAVERSE_COL(Param, PredicateOutParam, br);
            if (br.getLabel() && !br.getLabel()->getName().empty())
                m_os << L" #" << br.getLabel()->getName();
            if (i < _decl.getOutParams().size() - 1)
                m_os << L" : ";
        }

        bHavePreConditions = bHavePreConditions || _decl.getPreCondition();
        bHavePostConditions = bHavePostConditions || _decl.getPostCondition();

        m_os << L") ";

        if (bHavePreConditions) {
            m_os << L"\n";
            if (_decl.getPreCondition()) {
                m_os << fmtIndent(L"pre ");
                VISITOR_TRAVERSE(Formula, PredicatePreCondition, _decl.getPreCondition(), _decl, AnonymousPredicate, setPreCondition);
                m_os << L"\n";
            }
            for (size_t i = 0; i < _decl.getOutParams().size(); ++i) {
                Branch &br = *_decl.getOutParams().get(i);
                if (br.getPreCondition()) {
                    m_os << fmtIndent(L"pre ") << br.getLabel()->getName() << L": ";
                    VISITOR_TRAVERSE(Formula, PredicateBranchPreCondition, br.getPreCondition(), br, Branch, setPreCondition);
                    m_os << L"\n";
                }
            }
        }
        else
            mergeLines();

        traverseStatement(*_decl.getBlock());

        if (bHavePostConditions) {
            if (_decl.getPostCondition()) {
                m_os << L"\n" << fmtIndent(L"post ");
                VISITOR_TRAVERSE(Formula, PredicatePostCondition, _decl.getPostCondition(), _decl, AnonymousPredicate, setPostCondition);
            }
            for (size_t i = 0; i < _decl.getOutParams().size(); ++i) {
                Branch &br = *_decl.getOutParams().get(i);
                if (br.getPostCondition()) {
                    m_os << L"\n" << fmtIndent(L"post ") << br.getLabel()->getName() << L": ";
                    VISITOR_TRAVERSE(Formula, PredicateBranchPostCondition, br.getPostCondition(), br, Branch, setPostCondition);
                }
            }
        }

        if (_decl.getMeasure()) {
            m_os << L"\n" << fmtIndent(L"measure ");
            VISITOR_TRAVERSE(Expression, PredicateMeasure, _decl.getMeasure(), _decl, AnonymousPredicate, setMeasure);
        }

        if (_decl.getMeasure() || bHavePostConditions)
            m_os << L";";

        return true;
    }

    bool traversePredicate(Predicate &_stmt) {
        VISITOR_ENTER(Predicate, _stmt);
        VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);

        m_os << _stmt.getName();

        if (!_traverseAnonymousPredicate(_stmt))
            return false;

        VISITOR_EXIT();
    }

    bool traverseAssignment(ir::Assignment &_stmt) {
        VISITOR_ENTER(Assignment, _stmt);

        VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);
        VISITOR_TRAVERSE(Expression, LValue, _stmt.getLValue(), _stmt, Assignment, setLValue);
        m_os << " = ";
        VISITOR_TRAVERSE(Expression, RValue, _stmt.getExpression(), _stmt, Assignment, setExpression);

        VISITOR_EXIT();
    }

    bool traverseMultiassignment(Multiassignment &_stmt) {
        VISITOR_ENTER(Multiassignment, _stmt);
        VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);

        VISITOR_TRAVERSE_COL(Expression, LValue, _stmt.getLValues());
        m_os << " = ";
        VISITOR_TRAVERSE_COL(Expression, RValue, _stmt.getExpressions());

        VISITOR_EXIT();
    }

    VariableDeclarationPtr getVarDecl(const Call& _call, const ExpressionPtr& _pArg) {
        if (!_pArg || _pArg->getKind() != Expression::VAR)
            return NULL;
        for (size_t i = 0; i < _call.getDeclarations().size(); ++i) {
            VariableDeclarationPtr pVar = _call.getDeclarations().get(i);
            if (pVar->getVariable() == _pArg.as<VariableReference>()->getTarget())
                return pVar;
        }
        return NULL;
    }

    bool traverseCall(Call &_stmt) {
        VISITOR_ENTER(Call, _stmt);
        VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);
        VISITOR_TRAVERSE(Expression, PredicateCallee, _stmt.getPredicate(), _stmt, Call, setPredicate);

        m_os << L"(";
        VISITOR_TRAVERSE_COL(Expression, PredicateCallArgs, _stmt.getArgs());

        if (_stmt.getBranches().size() == 0) {
            m_os << L")";
            VISITOR_EXIT();
        }

        m_os << L": ";

        if (_stmt.getBranches().size() == 1) {
            traverseCollection(*_stmt.getBranches().get(0));
            m_os << L")";
            VISITOR_EXIT();
        }

        const std::wstring strGeneralName = _stmt.getPredicate()->getKind() == Expression::PREDICATE
            ? _stmt.getPredicate().as<PredicateReference>()->getName()
            : L"";

        std::vector<std::wstring> names;
        for (size_t i = 0; i < _stmt.getBranches().size(); ++i)
            names.push_back(getNewLabelName(strGeneralName));

        for (size_t i = 0; i < _stmt.getBranches().size(); ++i) {
            CallBranch &br = *_stmt.getBranches().get(i);

            bool bIsFirst = true;
            for (size_t j = 0; j < br.size(); ++j) {
                ExpressionPtr pArg = br.get(j);
                if (!pArg)
                    continue;

                if (!bIsFirst)
                    m_os << L", ";
                bIsFirst = false;

                if (VariableDeclarationPtr pVar = getVarDecl(_stmt, pArg))
                    traverseVariableDeclaration(*pVar);
                else
                    traverseExpression(*pArg);
            }

            if (_stmt.getBranches().size() > 1)
                m_os << L" #" << names[i];
            if (i != _stmt.getBranches().size() - 1)
                m_os << L" : ";
        }

        m_os << L")";

        bool bHasHandlers = false;
        for (size_t i = 0; i < _stmt.getBranches().size(); ++i)
            if (_stmt.getBranches().get(i)->getHandler()) {
                bHasHandlers = true;
                break;
            }


        for (size_t i = 0; i < _stmt.getBranches().size(); ++i) {
            CallBranch &br = *_stmt.getBranches().get(i);
            if (!br.getHandler())
                continue;

            incTab();

            m_os << "\n" << fmtIndent() << L"case " << names[i] << ": ";
            mergeLines();
            traverseStatement(*br.getHandler());
            decTab();
        }

        VISITOR_EXIT();
    }

    void printMergedStatement(const StatementPtr _pStmt) {
        if (_pStmt->getKind() == Statement::BLOCK) {
            mergeLines();
            traverseStatement(*_pStmt);
        } else {
            incTab();
            m_os << L"\n";
            traverseStatement(*_pStmt);
            decTab();
        }
    }

    void feedLine(const Statement& _stmt) {
        if (_stmt.getLabel())
            m_os << L"\n" << fmtIndent();
    }

    bool traverseIf(If &_stmt) {
        VISITOR_ENTER(If, _stmt);
        VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);

        feedLine(_stmt);

        m_os << L"if (";
        VISITOR_TRAVERSE(Expression, IfArg, _stmt.getArg(), _stmt, If, setArg);
        m_os << L") ";

        if (_stmt.getBody())
            printMergedStatement(_stmt.getBody());

        if (_stmt.getElse()) {
            m_os << L"\n" << fmtIndent(L"else ");
            printMergedStatement(_stmt.getElse());
        }

        VISITOR_EXIT();
    }

    bool traverseSwitch(Switch &_stmt) {
        VISITOR_ENTER(Switch, _stmt);
        VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);

        feedLine(_stmt);

        m_os << L"switch (";
        if (_stmt.getParamDecl()) {
            mergeLines();
            VISITOR_TRAVERSE(VariableDeclaration, SwitchParamDecl, _stmt.getParamDecl(), _stmt, Switch, setParamDecl);
        }
        else
            VISITOR_TRAVERSE(Expression, SwitchArg, _stmt.getArg(), _stmt, Switch, setArg);
        m_os << L") {\n";
        incTab();

        for (size_t i=0; i < _stmt.size(); ++i) {
            if (!_stmt.get(i))
                continue;

            m_os << fmtIndent(L"case ");
            traverseCollection(_stmt.get(i)->getExpressions());
            m_os << L" : ";
            mergeLines();

            if (_stmt.get(i)->getBody())
                traverseStatement(*_stmt.get(i)->getBody());

            m_os << "\n";
        }

        if (_stmt.getDefault()) {
            m_os << fmtIndent(L"default: ");
            mergeLines();
            VISITOR_TRAVERSE(Statement, SwitchDefault, _stmt.getDefault(), _stmt, Switch, setDefault);
            m_os << L"\n";
        }

        decTab();
        m_os << fmtIndent(L"}");
        VISITOR_EXIT();
    }

    bool traverseFor(For &_stmt) {
        VISITOR_ENTER(For, _stmt);
        VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);

        feedLine(_stmt);

        m_os << L"for (";
        VISITOR_TRAVERSE(VariableDeclaration, ForIterator, _stmt.getIterator(), _stmt, For, setIterator);
        m_os << L"; ";
        VISITOR_TRAVERSE(Expression, ForInvariant, _stmt.getInvariant(), _stmt, For, setInvariant);
        m_os << L"; ";
        mergeLines();
        VISITOR_TRAVERSE(Statement, ForIncrement, _stmt.getIncrement(), _stmt, For, setIncrement);
        m_os << L") ";

        if (_stmt.getBody())
            printMergedStatement(_stmt.getBody());

        VISITOR_EXIT();
    }

    bool traverseWhile(While &_stmt) {
        VISITOR_ENTER(While, _stmt);
        VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);

        feedLine(_stmt);

        m_os << L"while (";
        VISITOR_TRAVERSE(Expression, WhileInvariant, _stmt.getInvariant(), _stmt, While, setInvariant);
        m_os << L") ";

        if (_stmt.getBody())
            printMergedStatement(_stmt.getBody());

        VISITOR_EXIT();
    }

    bool traverseBreak(Break &_stmt) {
        VISITOR_ENTER(Break, _stmt);
        VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);
        m_os << L"break";
        VISITOR_EXIT();
    }

    bool traverseWith(With &_stmt) {
        VISITOR_ENTER(With, _stmt);
        VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);

        feedLine(_stmt);

        m_os << L"with (";
        VISITOR_TRAVERSE_COL(Expression, WithArg, _stmt.getArgs());
        m_os << L") ";

        if (_stmt.getBody())
            printMergedStatement(_stmt.getBody());

        VISITOR_EXIT();
    }

    bool traverseTypeDeclaration(TypeDeclaration &_stmt) {
        VISITOR_ENTER(TypeDeclaration, _stmt);

        VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);
        m_os << L"type " << _stmt.getName();

        if (_stmt.getType()) {
            m_os << L" = ";
            VISITOR_TRAVERSE(Type, TypeDeclBody, _stmt.getType(), _stmt, TypeDeclaration, setType);
        }

        VISITOR_EXIT();
    }

    bool traverseVariable(Variable &_val) {
        VISITOR_ENTER(Variable, _val);
        if (_val.isMutable())
            m_os << L"mutable ";
        VISITOR_TRAVERSE(Type, VariableType, _val.getType(), _val, NamedValue, setType);
        m_os << L" " << _val.getName();
        VISITOR_EXIT();
    }

    bool traverseVariableDeclaration(VariableDeclaration &_stmt) {
        VISITOR_ENTER(VariableDeclaration, _stmt);
        VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);
        VISITOR_TRAVERSE(Variable, VarDeclVar, _stmt.getVariable(), _stmt, VariableDeclaration, setVariable);

        if (_stmt.getValue()) {
            m_os << L" = ";
            VISITOR_TRAVERSE(Expression, VarDeclInit, _stmt.getValue(), _stmt, VariableDeclaration, setValue);
        }

        VISITOR_EXIT();
    }

    bool traverseVariableDeclarationGroup(VariableDeclarationGroup &_stmt) {
        VISITOR_ENTER(Block, _stmt);

        if (_stmt.empty())
            VISITOR_EXIT();

        m_os << fmtIndent();
        traverseType(*_stmt.get(0)->getVariable()->getType());
        m_os << L" ";

        for (size_t i = 0; i < _stmt.size(); ++i) {
            const bool bIsLast = (_stmt.size() - 1 == i);
            VariableDeclaration var = *_stmt.get(i);

            m_os << var.getName();
            if (var.getValue()) {
                m_os << L" = ";
                traverseExpression(*var.getValue());
            }
            if (!bIsLast)
                m_os << L", ";
        }

        VISITOR_EXIT();
    }

    // NODE / NAMED_VALUE

    bool traverseNamedValue(NamedValue &_val) {
        printComma();

        switch (_val.getKind()) {
            case NamedValue::ENUM_VALUE:
                return traverseEnumValue((EnumValue &)_val);
            case NamedValue::PREDICATE_PARAMETER:
                return traverseParam((Param &)_val);
            case NamedValue::LOCAL:
            case NamedValue::GLOBAL:
                return traverseVariable((Variable &)_val);
        }

        VISITOR_ENTER(NamedValue, _val);
        VISITOR_TRAVERSE(Type, NamedValueType, _val.getType(), _val, NamedValue, setType);
        m_os << L" " << _val.getName();
        VISITOR_EXIT();
    }

    bool traverseParam(Param &_val) {
        VISITOR_ENTER(Param, _val);
        VISITOR_TRAVERSE(Type, ParamType, _val.getType(), _val, NamedValue, setType);
        m_os << L" " << _val.getName();
        VISITOR_EXIT();
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
            case ir::Binary::IFF:
                m_os << L"<=>";
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

    virtual bool traverseFunctionCall(ir::FunctionCall &_expr) {
        VISITOR_ENTER(FunctionCall, _expr);
        VISITOR_TRAVERSE(Expression, FunctionCallee, _expr.getPredicate(), _expr, FunctionCall, setPredicate);
        m_os << "(";
        VISITOR_TRAVERSE_COL(Expression, FunctionCallArgs, _expr.getArgs());
        m_os << ")";
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
        VISITOR_EXIT();
    }

    virtual bool traverseLemmaDeclaration(ir::LemmaDeclaration &_stmt) {
        VISITOR_ENTER(LemmaDeclaration, _stmt);

        VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);
        m_os << "lemma ";
        VISITOR_TRAVERSE(Expression, LemmaDeclBody, _stmt.getProposition(), _stmt, LemmaDeclaration, setProposition);

        VISITOR_EXIT();
    }

    void run() {
        traverseNode( *m_pNode );
    }

    size_t getDepth() const {
        return m_szDepth;
    }

    std::wstring fmtIndent(const std::wstring &_s = L"") {
        std::wstring res;

        for (size_t i = 0; i < getDepth(); ++i)
            res += L" ";

        return res + _s;
    }

    void incTab() {
        m_szDepth += 4;
    }

    void decTab() {
        if (m_szDepth > 4)
            m_szDepth -= 4;
        else
            m_szDepth = 0;
    }

    void mergeLines() {
        m_bMergeLines = true;
    }
    void separateLines() {
        m_bMergeLines = false;
    }
    std::wstring getNewLabelName(const std::wstring& _name = L"") {
        for (size_t i = 1;; ++i) {
            const std::wstring strName = _name + fmtInt(i, L"%d");
            if (m_usedLabels.insert(strName).second)
                return strName;
        }
    }

protected:
    inline void callPrettyPrintCompact(ir::Node &_node) {
       prettyPrintCompact(_node, m_os, PPC_NO_INCOMPLETE_TYPES);
    }

    NodePtr m_pNode;
    size_t m_szDepth;
    bool m_bMergeLines;
    std::set<std::wstring> m_usedLabels;

};

class PrettyPrinterSyntaxCompact : public PrettyPrinterSyntax {
public:
    PrettyPrinterSyntaxCompact(ir::Node &_node, size_t _depth, std::wostream &_os) :
        PrettyPrinterSyntax(_node, _os), m_szDepth(_depth)
    {}

    virtual bool traverseStatement(Statement &_stmt) {
        bool result = true;
        if (m_szDepth != 0) {
            --m_szDepth;
            result = Visitor::traverseStatement(_stmt);
            ++m_szDepth;
        }
        else
            m_os << "...";
        return result;
    }

    virtual bool traverseAssignment(Assignment &_stmt) {
        VISITOR_ENTER(Assignment, _stmt);
        VISITOR_TRAVERSE(Expression, LValue, _stmt.getLValue(), _stmt, Assignment, setLValue);
        m_os << " := ";
        VISITOR_TRAVERSE(Expression, RValue, _stmt.getExpression(), _stmt, Assignment, setExpression);
        VISITOR_EXIT();
    }

    virtual bool traverseMultiassignment(Multiassignment &_stmt) {
        VISITOR_ENTER(Multiassignment, _stmt);
        m_os << "| ";
        VISITOR_TRAVERSE_COL(Expression, LValue, _stmt.getLValues());
        m_os << " | := | ";
        VISITOR_TRAVERSE_COL(Expression, RValue, _stmt.getExpressions());
        m_os << " |";
        VISITOR_EXIT();
    }

    virtual bool traverseBlock(Block &_stmt) {
        VISITOR_ENTER(Block, _stmt);
        m_os << "{ ";

        size_t i = 0;
        for (i = 0; i<_stmt.size() && i<m_szDepth; ++i) {
            traverseStatement(*_stmt.get(i));
            if (i + 1 != _stmt.size())
                m_os << "; ";
        }

        if (i<_stmt.size() && i == m_szDepth)
            m_os << " ... ";

        m_os << " }";
        VISITOR_EXIT();
    }

    virtual bool traverseParallelBlock(ParallelBlock &_stmt) {
        VISITOR_ENTER(ParallelBlock, _stmt);
        m_os << "{ ";

        size_t i = 0;
        for (i = 0; i<_stmt.size() && i<m_szDepth; ++i) {
            traverseStatement(*_stmt.get(i));
            if (i + 1 != _stmt.size())
                m_os << " || ";
        }

        if (i<_stmt.size() && i == m_szDepth)
            m_os << " ... ";

        m_os << " }";
        VISITOR_EXIT();
    }

    virtual bool traverseIf(If &_stmt) {
        VISITOR_ENTER(If, _stmt);
        m_os << "if (";
        VISITOR_TRAVERSE(Expression, IfArg, _stmt.getArg(), _stmt, If, setArg);
        m_os << ") ";
        VISITOR_TRAVERSE(Statement, IfBody, _stmt.getBody(), _stmt, If, setBody);
        if (_stmt.getElse()) {
            m_os << " else ";
            VISITOR_TRAVERSE(Statement, IfElse, _stmt.getElse(), _stmt, If, setElse);
        }
        VISITOR_EXIT();
    }

    virtual bool traverseSwitch(Switch &_stmt) {
        VISITOR_ENTER(Switch, _stmt);
        m_os << "switch (";
        VISITOR_TRAVERSE(Expression, SwitchArg, _stmt.getArg(), _stmt, Switch, setArg);
        m_os << ") ...";
        VISITOR_EXIT();
    }

    virtual bool traverseCall(Call &_stmt) {
        VISITOR_ENTER(Call, _stmt);
        if (_stmt.getPredicate()->getKind() == Expression::PREDICATE)
            m_os << _stmt.getPredicate().as<PredicateReference>()->getName();
        else
            m_os << "AnonymousPredicate";
        m_os << "(";
        VISITOR_TRAVERSE_COL(Expression, PredicateCallArgs, _stmt.getArgs());
        m_os << ": ";

        for (size_t i = 0; i < _stmt.getBranches().size(); ++i) {
            CallBranch &br = *_stmt.getBranches().get(i);
            if (_stmt.getBranches().size() > 1)
                m_os << "#" << i + 1 << " ";
            VISITOR_TRAVERSE_COL(Expression, PredicateCallBranchResults, br);
        }

        m_os << ")";
        VISITOR_EXIT();
    }

    virtual bool traversePredicate(Predicate &_stmt) {
        m_os << _stmt.getName();
        m_os << "(";
        VISITOR_TRAVERSE_COL(Param, PredicateInParam, _stmt.getInParams());
        m_os << ": ";
        for (size_t i = 0; i < _stmt.getOutParams().size(); ++i) {
            Branch &br = *_stmt.getOutParams().get(i);
            if (_stmt.getOutParams().size() > 1) {
                m_os << "#";
                if (br.getLabel())
                    m_os << br.getLabel()->getName() << " ";
                else
                    m_os << i + 1 << " ";
            }
            VISITOR_TRAVERSE_COL(Param, PredicateOutParam, br);
        }
        m_os << ")";
        return true;
    }

private:
    size_t m_szDepth;
};

void prettyPrintSyntax(ir::Node &_node, std::wostream & _os) {
    PrettyPrinterSyntax(_node, _os).run();
}

void prettyPrintSyntaxCompact(ir::Node &_node, size_t _depth, std::wostream & _os) {
    PrettyPrinterSyntaxCompact(_node, _depth, _os).run();
}
