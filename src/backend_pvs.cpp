/// \file backend_pvs.cpp
///

#include <iostream>

#include "ir/visitor.h"
#include "prettyprinter.h"
#include "pp_syntax.h"
#include "utils.h"
#include "node_analysis.h"

#include <list>

using namespace ir;

class GeneratePvs : public Visitor {
public:
    GeneratePvs(std::wostream &_os) :
        m_os(_os), m_nLemmaIndex(0)
    {}

    std::wstring namedValueName(const NamedValuePtr& pValue) {
        return removeRedundantSymbols(cyrillicToASCII(m_context.nameGenerator().getNamedValueName(*pValue)), L"'", L"?");
    }

    std::wstring fmtType(int _kind) {
        switch (_kind) {
            case Type::INT:     return L"int";
            case Type::NAT:     return L"nat";
            case Type::REAL:    return L"real";
            case Type::BOOL:    return L"bool";
            case Type::CHAR:    return L"char";
            case Type::STRING:  return L"list[char]";
            default:            return L"";
        }
    }

    bool traverseStatement(Statement& _stmt) {
        const bool bResult = Visitor::traverseStatement(_stmt);

        if (_stmt.getKind() >= Statement::TYPE_DECLARATION)
            m_os << L"\n\n";

        return bResult;
    }

    bool traverseVariable(Variable &_val) {
        VISITOR_ENTER(Variable, _val);
        m_os << namedValueName(&_val) << L" : " << setInline(true);
        VISITOR_TRAVERSE(Type, VariableType, _val.getType(), _val, NamedValue, setType);
        m_os << setInline(false);
        VISITOR_EXIT();
    }

    bool printCond(const std::list<std::pair<ExpressionPtr, ExpressionPtr>>& _cases, const ExpressionPtr& _pElse) {
        m_os << L"COND\n" << indent;

        for (auto i: _cases) {
            VISITOR_TRAVERSE_NS(Expression, Expression, i.first);
            m_os << L" -> " << indent;
            VISITOR_TRAVERSE_NS(Expression, Expression, i.second);
            m_os << unindent;

            if (i != *::prev(_cases.end()) || _pElse)
                m_os << L",\n";
        }

        if (_pElse) {
            m_os << "ELSE -> " << indent;
            VISITOR_TRAVERSE_NS(Expression, Expression, _pElse);
            m_os << unindent;
        }

        m_os << unindent << L"\nENDCOND";
        return true;
    }

    bool printTupleTypes(Collection<Type>& _types) {
        if (_types.empty())
            return true;
        if (_types.size() == 1) {
            VISITOR_TRAVERSE_NS(Type, Type, _types.get(0));
            return true;
        }

        m_os << L"[" << setInline(true);

        for (size_t c = 0; c < _types.size(); ++c) {
            if (c > 0)
                m_os << L", ";
            VISITOR_TRAVERSE_ITEM_NS(Type, Type, _types, c);
        }

        m_os << L"]" << setInline(false);

        return true;
    }

    bool printRestrict(Collection<Type>& _orig, Collection<Type>& _restricted,
        const TypePtr& _pResult, const ExpressionPtr& _pArg)
    {
        m_os << L"restrict[" << setInline(true);
        printTupleTypes(_orig);
        m_os << L", ";
        printTupleTypes(_restricted);
        m_os << L", ";
        VISITOR_TRAVERSE_NS(Type, Type, _pResult);
        m_os << L"] (";
        VISITOR_TRAVERSE_NS(Expression, Expression, _pArg);
        m_os << L")" << setInline(false);
        return true;
    }

    bool printLambda(NamedValues& _params, const TypePtr& _pResult, const ExpressionPtr& _pBody) {
        m_os << L"LAMBDA (" << setInline(true);
        VISITOR_TRAVERSE_COL(NamedValue, PredicateInParam, _params);
        m_os << L") : ";
        VISITOR_TRAVERSE_NS(Type, Type, _pResult);
        m_os << setInline(false) << " =\n" << indent;
        VISITOR_TRAVERSE_NS(Expression, Expression, _pBody);
        m_os << unindent;
        return true;
    }

    bool traverseVariableDeclaration(VariableDeclaration &_stmt) {
        VISITOR_ENTER(VariableDeclaration, _stmt);
        VISITOR_TRAVERSE(Variable, VarDeclVar, _stmt.getVariable(), _stmt, VariableDeclaration, setVariable);
        if (_stmt.getValue()) {
            m_os << L" =\n" << indent;
            VISITOR_TRAVERSE(Expression, VarDeclInit, _stmt.getValue(), _stmt, VariableDeclaration, setValue);
            m_os << unindent;
        }
        VISITOR_EXIT();
    }

    bool traverseTypeDeclaration(TypeDeclaration &_stmt) {
        VISITOR_ENTER(TypeDeclaration, _stmt);

        VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);
        m_os << m_context.nameGenerator().getTypeName(_stmt);

        if (_stmt.getType()->getKind() == Type::PARAMETERIZED) {
            m_os << L"(";
            VISITOR_TRAVERSE_COL(Type, Type, _stmt.getType().as<ParameterizedType>()->getParams());
            m_os << L")";
        }

        m_os << L" : TYPE";

        if (_stmt.getType()) {
            m_os << L" =\n" << indent;
            VISITOR_TRAVERSE(Type, TypeDeclBody, _stmt.getType(), _stmt, TypeDeclaration, setType);
            m_os << unindent;
        }

        VISITOR_EXIT();
    }

    bool visitType(ir::Type &_type) {
        if (_type.getKind() <= Type::GENERIC)
            m_os << fmtType(_type.getKind());
        return true;
    }

    bool traverseSubtype(Subtype &_type) {
        VISITOR_ENTER(Subtype, _type);
        m_os << L"{";
        VISITOR_TRAVERSE(NamedValue, SubtypeParam, _type.getParam(), _type, Subtype, setParam);
        m_os << L" | " << setInline(true);
        VISITOR_TRAVERSE(Expression, SubtypeCond, _type.getExpression(), _type, Subtype, setExpression);
        m_os << "}" << setInline(false);
        VISITOR_EXIT();
    }

    bool traverseRange(Range &_type) {
        VISITOR_ENTER(Range, _type);
        m_os << L"subrange(" << setInline(true);
        VISITOR_TRAVERSE(Expression, RangeMin, _type.getMin(), _type, Range, setMin);
        m_os << L", ";
        VISITOR_TRAVERSE(Expression, RangeMax, _type.getMax(), _type, Range, setMax);
        m_os << ")" << setInline(false);
        VISITOR_EXIT();
    }

    bool traverseNamedReferenceType(NamedReferenceType &_type) {
        VISITOR_ENTER(NamedReferenceType, _type);

        if (!_type.getDeclaration())
            VISITOR_EXIT();

        m_os << m_context.nameGenerator().getTypeName(_type);

        if (!_type.getArgs().empty()) {
            m_os << L"(" << setInline(true);
            VISITOR_TRAVERSE_COL(Expression, NamedTypeArg, _type.getArgs());
            m_os << L")" << setInline(false);
        }

        VISITOR_EXIT();
    }

    bool visitArrayType(ArrayType& _array) {
        const TypePtr& pBaseType = _array.getRootType();

        Collection<Type> dims;
        _array.getDimensions(dims);

        m_os << L"[";

        if (!printTupleTypes(dims))
            return false;

        m_os << L" -> " << setInline(true);
        VISITOR_TRAVERSE_NS(Type, ArrayBaseType, pBaseType);
        m_os << L"]" << setInline(false);

        return false;
    }

    bool traverseEnumValue(EnumValue &_val) {
        printComma();
        m_os << L"\n" << _val.getName();
        return true;
    }

    bool traverseEnumType(EnumType &_type) {
        VISITOR_ENTER(EnumType, _type);
        m_os << L"{" << indent;
        VISITOR_TRAVERSE_COL(EnumValue, EnumValueDecl, _type.getValues());
        m_os << unindent << L"\n}";
        VISITOR_EXIT();
    }

    bool traverseListType(ListType &_type) {
        VISITOR_ENTER(ListType, _type);
        m_os << L"list[" << setInline(true);
        VISITOR_TRAVERSE(Type, ListBaseType, _type.getBaseType(), _type, DerivedType, setBaseType);
        m_os << L"]" << setInline(false);
        VISITOR_EXIT();
    }

    bool traverseSetType(SetType &_type) {
        VISITOR_ENTER(SetType, _type);
        m_os << L"setof[" << setInline(true);
        VISITOR_TRAVERSE(Type, SetBaseType, _type.getBaseType(), _type, DerivedType, setBaseType);
        m_os << L"]" << setInline(false);
        VISITOR_EXIT();
    }

    bool visitTypeType(TypeType) {
        m_os << L"TYPE";
        return false;
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
            case ir::Expression::COMPONENT:
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
        if (!m_path.empty() && getRole() == R_VarDeclVar)
            return true;
        printComma();
        m_os << cyrillicToASCII(m_context.nameGenerator().getNamedValueName(_value)) << ": ";
        return true;
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
        m_os << namedValueName(_node.getTarget());
        return false;
    }

    bool visitLabel(ir::Label &_label) {
        m_os << cyrillicToASCII(_label.getName().empty()
            ? m_context.nameGenerator().getNewLabelName(L"L") : _label.getName()) << ": ";
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

    bool traverseFieldExpr(FieldExpr &_expr) {
        VISITOR_ENTER(FieldExpr, _expr);
        VISITOR_TRAVERSE(Expression, FieldObject, _expr.getObject(), _expr, Component, setObject);
        m_os << L"'" << _expr.getFieldName();
        VISITOR_EXIT();
    }

    bool traverseStructFieldDefinition(StructFieldDefinition &_cons) {
        VISITOR_ENTER(StructFieldDefinition, _cons);
        printComma();
        if (!_cons.getName().empty())
            m_os << _cons.getName() << L": ";
        VISITOR_TRAVERSE(Expression, StructFieldValue, _cons.getValue(), _cons, StructFieldDefinition, setValue);
        VISITOR_EXIT();
    }

    bool traverseStructConstructor(StructConstructor &_expr) {
        VISITOR_ENTER(StructConstructor, _expr);
        m_os << L"(# ";
        VISITOR_TRAVERSE_COL(StructFieldDefinition, StructFieldDef, _expr);
        m_os << L" #)";
        VISITOR_EXIT();
    }

    bool visitListConstructor(ListConstructor &_expr) {
        std::wstring strParens;
        for (size_t i = 0; i < _expr.size(); ++i) {
            m_os << L"cons(";
            VISITOR_TRAVERSE_NS(Expression, Expression, _expr.get(i));
            m_os << L", ";
            strParens += L")";
        }
        m_os << L"null" << strParens;
        return false;
    }

    bool traverseArrayConstructor(ArrayConstructor& _array) {
        VISITOR_ENTER(ArrayConstructor, _array);

        const NamedValuePtr pIndex = new NamedValue(L"", _array.getType().as<ArrayType>()->getDimensionType());

        m_os << L"LAMBDA (";
        VISITOR_TRAVERSE_NS(NamedValue, ArrayIterator, pIndex);
        m_os << L") : ";
        VISITOR_TRAVERSE_NS(Type, Type, _array.getType().as<ArrayType>()->getRootType());
        m_os << L" =\n" << indent;

        std::list<std::pair<ExpressionPtr, ExpressionPtr>> cases;
        for (auto i: _array)
            cases.push_back({new Binary(Binary::EQUALS, new VariableReference(L"", pIndex), i->getIndex()),
                i->getValue()});

        printCond(cases, nullptr);

        m_os << unindent;

        VISITOR_EXIT();
    }

    bool traverseSetConstructor(SetConstructor &_expr) {
        VISITOR_ENTER(SetConstructor, _expr);
        if (!_expr.getType())
            VISITOR_EXIT();

        NamedValue nv(L"", _expr.getType().as<SetType>()->getBaseType());

        m_os << L"{";
        VISITOR_TRAVERSE_NS(NamedValue, Variable, NamedValuePtr(&nv));
        m_os << L" | ";

        for (size_t i = 0; i < _expr.size(); ++i) {
            if (i != 0)
                m_os << L" OR ";
            m_os << m_context.nameGenerator().getNamedValueName(nv) << " = ";
            VISITOR_TRAVERSE_NS(Expression, SetElementDef, _expr.get(i));
        }

        m_os << L"}";
        VISITOR_EXIT();
    }

    bool traverseUnary(Unary &_expr) {
        VISITOR_ENTER(Unary, _expr);

        if (_expr.getOperator() == Unary::BITWISE_NEGATE &&
            _expr.getType() && _expr.getType()->getKind() == Type::SET) {
            m_os << L"complement(";
            VISITOR_TRAVERSE(Expression, UnarySubexpression, _expr.getExpression(), _expr, Unary, setExpression);
            m_os << L")";
            VISITOR_EXIT();
        } else if (_expr.getOperator() == Unary::BITWISE_NEGATE) {
            m_os << L"bv2nat(NOT nat2bv(";
            VISITOR_TRAVERSE(Expression, UnarySubexpression, _expr.getExpression(), _expr, Unary, setExpression);
            m_os << L"))";
            VISITOR_EXIT();
        }

        printUnaryOperator(_expr);
        m_os << L"(";
        VISITOR_TRAVERSE(Expression, UnarySubexpression, _expr.getExpression(), _expr, Unary, setExpression);
        m_os << L")";
        VISITOR_EXIT();
    }

    virtual bool traverseBinary(ir::Binary &_node) {
        VISITOR_ENTER(Binary, _node);

        switch (_node.getOperator()) {
            case Binary::REMAINDER: {
                m_os << L"rem(";
                VISITOR_TRAVERSE(Expression, BinarySubexpression, _node.getLeftSide(), _node, Binary, setLeftSide);
                m_os << L")(";
                VISITOR_TRAVERSE(Expression, BinarySubexpression, _node.getRightSide(), _node, Binary, setRightSide);
                m_os << L")";
                VISITOR_EXIT();
            }

            case Binary::IN: {
                m_os << L"member(";
                VISITOR_TRAVERSE(Expression, BinarySubexpression, _node.getLeftSide(), _node, Binary, setLeftSide);
                m_os << L", ";
                VISITOR_TRAVERSE(Expression, BinarySubexpression, _node.getRightSide(), _node, Binary, setRightSide);
                m_os << L")";
                VISITOR_EXIT();
            }

            case Binary::BOOL_AND: {
                if (!_node.getLeftSide()->getType())
                    break;
                switch (_node.getLeftSide()->getType()->getKind()) {
                    case Type::SET: {
                        m_os << L"intersection(";
                        VISITOR_TRAVERSE(Expression, BinarySubexpression, _node.getLeftSide(), _node, Binary, setLeftSide);
                        m_os << L", ";
                        VISITOR_TRAVERSE(Expression, BinarySubexpression, _node.getRightSide(), _node, Binary, setRightSide);
                        m_os << L")";
                        VISITOR_EXIT();
                    }
                }
                break;
            }

            case Binary::ADD:
            case Binary::BOOL_OR: {
                if (!_node.getLeftSide()->getType())
                    break;
                switch (_node.getLeftSide()->getType()->getKind()) {
                    case Type::SET: {
                        m_os << L"union(";
                        VISITOR_TRAVERSE(Expression, BinarySubexpression, _node.getLeftSide(), _node, Binary, setLeftSide);
                        m_os << L", ";
                        VISITOR_TRAVERSE(Expression, BinarySubexpression, _node.getRightSide(), _node, Binary, setRightSide);
                        m_os << L")";
                        VISITOR_EXIT();
                    }
                }
                break;
            }

            case Binary::SUBTRACT: {
                if (!_node.getLeftSide()->getType())
                    break;
                switch (_node.getLeftSide()->getType()->getKind()) {
                    case Type::SET: {
                        m_os << L"difference(";
                        VISITOR_TRAVERSE(Expression, BinarySubexpression, _node.getLeftSide(), _node, Binary, setLeftSide);
                        m_os << L", ";
                        VISITOR_TRAVERSE(Expression, BinarySubexpression, _node.getRightSide(), _node, Binary, setRightSide);
                        m_os << L")";
                        VISITOR_EXIT();
                    }
                }
                break;
            }

            case Binary::BOOL_XOR: {
                if (!_node.getLeftSide()->getType())
                    break;
                switch (_node.getLeftSide()->getType()->getKind()) {
                    case Type::SET: {
                        m_os << L"symmetric_difference(";
                        VISITOR_TRAVERSE(Expression, BinarySubexpression, _node.getLeftSide(), _node, Binary, setLeftSide);
                        m_os << L", ";
                        VISITOR_TRAVERSE(Expression, BinarySubexpression, _node.getRightSide(), _node, Binary, setRightSide);
                        m_os << L")";
                        VISITOR_EXIT();
                    }
                }
                break;
            }
        }

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
        m_os << " ENDIF";
        VISITOR_EXIT();
    }

    virtual bool traverseFormula(ir::Formula &_node) {
        VISITOR_ENTER(Formula, _node);

        m_os << (_node.getQuantifier() == ir::Formula::EXISTENTIAL
            ? L"EXISTS (" : L"FORALL (");

        VISITOR_TRAVERSE_COL(NamedValue, FormulaBoundVariable, _node.getBoundVariables());
        m_os << L"): ";

        VISITOR_TRAVERSE(Expression, Subformula, _node.getSubformula(), _node, Formula, setSubformula);
        VISITOR_EXIT();
    }

    virtual bool traverseFormulaCall(ir::FormulaCall &_node) {
        m_os << m_context.nameGenerator().getFormulaName(_node);

        if (_node.getArgs().empty())
            return true;

        m_os << L"(";
        const bool bResult = Visitor::traverseFormulaCall(_node);
        m_os << L")";

        return bResult;
    }

    virtual bool traverseLemmaDeclaration(ir::LemmaDeclaration &_stmt) {
        VISITOR_ENTER(LemmaDeclaration, _stmt);

        switch (_stmt.getStatus()) {
            case ir::LemmaDeclaration::VALID:
                m_os << L"%VALID\n";
                break;
            case ir::LemmaDeclaration::INVALID:
                m_os << L"%INVALID\n";
                break;
        }
        VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);
        m_os << L"L" << ++m_nLemmaIndex << L": LEMMA\n" << indent;
        VISITOR_TRAVERSE(Expression, LemmaDeclBody, _stmt.getProposition(), _stmt, LemmaDeclaration, setProposition);
        m_os << unindent;

        VISITOR_EXIT();
    }

    virtual bool traverseFormulaDeclaration(ir::FormulaDeclaration &_node) {
        VISITOR_ENTER(FormulaDeclaration, _node);

        m_os << m_context.nameGenerator().getFormulaName(_node);

        if (!_node.getParams().empty()) {
            m_os << L" (";
            VISITOR_TRAVERSE_COL(NamedValue, FormulaDeclParams, _node.getParams());
            m_os << L")";
        }
        m_os << L" : ";

        if (_node.getMeasure())
            m_os << L"RECURSIVE ";

        if (_node.getResultType())
            VISITOR_TRAVERSE_NS(Type, FormulaDeclResultType, _node.getResultType());
        else
            m_os << L"bool";

        m_os << L" =\n" << indent;
        VISITOR_TRAVERSE(Expression, FormulaDeclBody, _node.getFormula(), _node, FormulaDeclaration, setFormula);

        if (_node.getMeasure()) {
            m_os << L"\nMEASURE ";
            VISITOR_TRAVERSE_NS(Expression, FormulaDeclBody, _node.getMeasure());
        }

        m_os << unindent;

        VISITOR_EXIT();
    }

    bool traverseArrayPartExpr(ArrayPartExpr &_expr) {
        if (!_expr.getObject() || !_expr.getObject()->getType())
            return true;

        if (_expr.isRestrict()) {
            Collection<Type> orig, restricted;
            _expr.getObject()->getType().as<ArrayType>()->getDimensions(orig);
            for (auto i: _expr.getIndices())
                restricted.add(i.as<TypeExpr>()->getContents());
            return printRestrict(orig, restricted, _expr.getObject()->
                getType().as<ArrayType>()->getRootType(), _expr.getObject());
        }

        for (auto i: _expr.getIndices())
            if (i->getKind() == Expression::TYPE)
                return true;

        VISITOR_ENTER(ArrayPartExpr, _expr);
        m_os << setInline(true);
        VISITOR_TRAVERSE(Expression, ArrayPartObject, _expr.getObject(), _expr, Component, setObject);
        m_os << L"(";
        VISITOR_TRAVERSE_COL(Expression, ArrayPartIndex, _expr.getIndices());
        m_os << L")" << setInline(false);
        VISITOR_EXIT();
    }

    bool traverseArrayPartDefinition(ArrayPartDefinition &_cons) {
        VISITOR_ENTER(ArrayPartDefinition, _cons);
        VISITOR_TRAVERSE_COL(Expression, ArrayPartCond, _cons.getConditions());
        m_os << L" -> ";
        VISITOR_TRAVERSE(Expression, ArrayPartValue, _cons.getExpression(), _cons, ArrayPartDefinition, setExpression);
        VISITOR_EXIT();
    }

    bool traverseArrayIteration(ArrayIteration & _expr) {
        VISITOR_ENTER(ArrayIteration, _expr);

        if (!_expr.getType())
            return true;

        m_os << L"LAMBDA (";
        VISITOR_TRAVERSE_COL(NamedValue, ArrayIterator, _expr.getIterators());
        m_os << L") : ";
        VISITOR_TRAVERSE_NS(Type, Type, _expr.getType().as<ArrayType>()->getRootType());
        m_os << L" =\n" << indent;

        std::list<std::pair<ExpressionPtr, ExpressionPtr>> cases;
        for (size_t i = 0; i < _expr.size(); ++i)
            cases.push_back({
                na::resolveCase(_expr.getIterators(), _expr.get(i)->getConditions()),
                _expr.get(i)->getExpression()});

        if (!printCond(cases, _expr.getDefault()))
            return false;

        m_os << unindent;

        VISITOR_EXIT();
    }

    bool traverseReplacement(Replacement& _expr) {
        VISITOR_ENTER(Replacement, _expr);

        if (_expr.getNewValues()->getConstructorKind() != Constructor::ARRAY_ITERATION || !_expr.getType())
            VISITOR_EXIT();

        Expression& array = *_expr.getObject();
        ArrayIteration& iteration = *_expr.getNewValues().as<ArrayIteration>();

        m_os << L"LAMBDA (";
        VISITOR_TRAVERSE_COL(NamedValue, ArrayIterator, iteration.getIterators());
        m_os << L") : ";
        VISITOR_TRAVERSE_NS(Type, Type, array.
            getType().as<ArrayType>()->getRootType());
        m_os << L" =\n" << indent;

        std::list<std::pair<ExpressionPtr, ExpressionPtr>> cases;
        for (size_t i = 0; i < iteration.size(); ++i)
            cases.push_back({
                na::resolveCase(iteration.getIterators(), iteration.get(i)->getConditions()),
                iteration.get(i)->getExpression()});

        ExpressionPtr pDefault = iteration.getDefault();

        if (!pDefault) {
            pDefault = new ArrayPartExpr(&array);
            for (size_t i = 0; i < iteration.getIterators().size(); ++i)
                pDefault.as<ArrayPartExpr>()->getIndices().
                    add(new VariableReference(L"", iteration.getIterators().get(i)));
        }

        if (!printCond(cases, pDefault))
            return false;

        m_os << unindent;

        VISITOR_EXIT();
    }

    bool visitPredicate(ir::Predicate &_pred) {
        return false;
    }

    bool traverseModule(ir::Module &_module) {
        m_context.clear();
        m_context.nameGenerator().collect(_module);

        if (_module.getName().empty())
            return Visitor::traverseModule(_module);

        m_os << cyrillicToASCII(_module.getName()) << L" : THEORY\nBEGIN\n\n";

        Nodes sorted;
        m_context.sortModule(_module, sorted);
        VISITOR_TRAVERSE_COL(Node, Decl, sorted);
        VISITOR_TRAVERSE_COL(LemmaDeclaration, LemmaDecl, _module.getLemmas());

        m_os << L"END " << cyrillicToASCII(_module.getName()) << L"\n";

        return true;
    }

private:
    pp::Context m_context;
    IndentingStream<wchar_t> m_os;
    size_t m_nLemmaIndex;
};

void generatePvs(Module &_module, std::wostream & _os) {
    GeneratePvs(_os).traverseNode(_module);
}
