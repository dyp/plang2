/// \file backend_pvs.cpp
///

#include <iostream>

#include "ir/visitor.h"
#include "prettyprinter.h"
#include "pp_syntax.h"
#include "utils.h"

using namespace ir;

class GeneratePvs : public Visitor {
public:
    GeneratePvs(std::wostream &_os) :
        m_os(_os)
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

    bool traverseVariable(Variable &_val) {
        VISITOR_ENTER(Variable, _val);
        m_os << namedValueName(&_val) << L" : VAR ";
        VISITOR_TRAVERSE(Type, VariableType, _val.getType(), _val, NamedValue, setType);
        VISITOR_EXIT();
    }

    void printArrayDeclaration(const NamedValuePtr& _pValue, const ArrayConstructorPtr& _pConstructor) {
        const TypePtr& pArrayType = _pValue->getType()->getKind() == Type::ARRAY
            ? _pValue->getType() : _pConstructor->getType();
        m_os << namedValueName(_pValue);
        printArrayConstructor(*_pConstructor, pArrayType.as<ArrayType>(), true);
    }

    bool traverseVariableDeclaration(VariableDeclaration &_stmt) {
        VISITOR_ENTER(VariableDeclaration, _stmt);
        if (_stmt.getValue() && _stmt.getValue()->getKind() == Expression::CONSTRUCTOR
            && _stmt.getValue().as<Constructor>()->getConstructorKind() == Constructor::ARRAY_ELEMENTS) {
            printArrayDeclaration(_stmt.getVariable(), _stmt.getValue().as<ArrayConstructor>());
            m_os << L"\n";
            VISITOR_EXIT();
        }
        VISITOR_TRAVERSE(Variable, VarDeclVar, _stmt.getVariable(), _stmt, VariableDeclaration, setVariable);
        if (_stmt.getValue()) {
            m_os << L" = ";
            VISITOR_TRAVERSE(Expression, VarDeclInit, _stmt.getValue(), _stmt, VariableDeclaration, setValue);
        }
        m_os << L"\n";
        VISITOR_EXIT();
    }

    bool traverseTypeDeclaration(TypeDeclaration &_stmt) {
        VISITOR_ENTER(TypeDeclaration, _stmt);

        VISITOR_TRAVERSE(Label, StmtLabel, _stmt.getLabel(), _stmt, Statement, setLabel);
        m_os << _stmt.getName();

        if (_stmt.getType()->getKind() == Type::PARAMETERIZED) {
            m_os << L"(";
            traverseCollection(_stmt.getType().as<ParameterizedType>()->getParams());
            m_os << L")";
        }

        m_os << L" : TYPE";

        if (_stmt.getType()) {
            m_os << L" = ";
            VISITOR_TRAVERSE(Type, TypeDeclBody, _stmt.getType(), _stmt, TypeDeclaration, setType);
        }

        m_os << L"\n";
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
        m_os << L" | ";
        VISITOR_TRAVERSE(Expression, SubtypeCond, _type.getExpression(), _type, Subtype, setExpression);
        m_os << "}";
        VISITOR_EXIT();
    }

    bool traverseRange(Range &_type) {
        VISITOR_ENTER(Range, _type);
        m_os << L"subrange(";
        VISITOR_TRAVERSE(Expression, RangeMin, _type.getMin(), _type, Range, setMin);
        m_os << L", ";
        VISITOR_TRAVERSE(Expression, RangeMax, _type.getMax(), _type, Range, setMax);
        m_os << ")";
        VISITOR_EXIT();
    }

    bool traverseNamedReferenceType(NamedReferenceType &_type) {
        VISITOR_ENTER(NamedReferenceType, _type);

        if (!_type.getDeclaration())
            VISITOR_EXIT();

        m_os << _type.getDeclaration()->getName();

        if (!_type.getArgs().empty()) {
            m_os << L"(";
            VISITOR_TRAVERSE_COL(Expression, NamedTypeArg, _type.getArgs());
            m_os << L")";
        }

        VISITOR_EXIT();
    }

    bool visitArrayType(ArrayType& _array) {
        const TypePtr& pBaseType = _array.getRootType();

        std::list<TypePtr> dims;
        _array.getDimensions(dims);

        m_os << L"[";

        for (std::list<TypePtr>::iterator i = dims.begin(); i != dims.end(); ++i) {
            if (i != dims.begin())
                m_os << ", ";
            traverseNode(**i);
        }

        m_os << L" -> ";
        traverseNode(*pBaseType);
        m_os << L"]";

        return false;
    }

    bool traverseEnumValue(EnumValue &_val) {
        printComma();
        m_os << _val.getName();
        return true;
    }

    bool traverseEnumType(EnumType &_type) {
        VISITOR_ENTER(EnumType, _type);
        m_os << L"{";
        VISITOR_TRAVERSE_COL(EnumValue, EnumValueDecl, _type.getValues());
        m_os << L"}";
        VISITOR_EXIT();
    }

    bool traverseListType(ListType &_type) {
        VISITOR_ENTER(ListType, _type);
        m_os << L"list[";
        VISITOR_TRAVERSE(Type, ListBaseType, _type.getBaseType(), _type, DerivedType, setBaseType);
        m_os << L"]";
        VISITOR_EXIT();
    }

    bool traverseSetType(SetType &_type) {
        VISITOR_ENTER(SetType, _type);
        m_os << L"setof[";
        VISITOR_TRAVERSE(Type, SetBaseType, _type.getBaseType(), _type, DerivedType, setBaseType);
        m_os << L"]";
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
        if (!m_path.empty() && getLoc().role == R_VarDeclVar)
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

    bool traverseArrayPartExpr(ArrayPartExpr &_expr) {
        VISITOR_ENTER(ArrayPartExpr, _expr);
        VISITOR_TRAVERSE(Expression, ArrayPartObject, _expr.getObject(), _expr, Component, setObject);
        m_os << L"(";
        VISITOR_TRAVERSE_COL(Expression, ArrayPartIndex, _expr.getIndices());
        m_os << L")";
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
            traverseNode(*_expr.get(i));
            m_os << L", ";
            strParens += L")";
        }
        m_os << L"null" << strParens;
        return false;
    }

    void printArrayConstructor(const ArrayConstructor& _array, const ArrayTypePtr& _pType = NULL, bool bNeedType = false) {
        if (!_array.getType())
            return;

        const ArrayType& array = !_pType ? *_array.getType().as<ArrayType>() : *_pType;

        std::list<TypePtr> dims;
        array.getDimensions(dims);

        StructConstructor index;
        std::vector<NamedValuePtr> indexes;

        m_os << L"(";
        for (std::list<TypePtr>::iterator i = dims.begin(); i != dims.end(); ++i) {
            if (i != dims.begin())
                m_os << L", ";

            indexes.push_back(new NamedValue(L"", *i));
            traverseNode(*indexes.back());

            index.add(new StructFieldDefinition(new VariableReference(indexes.back())));
        }
        m_os << L") : ";

        if (bNeedType) {
            const TypePtr& pRootType = array.getRootType();
            traverseNode(*pRootType);
            m_os << L" = ";
        }

        NodePtr pIndex = &index;
        if (indexes.size() == 1)
            pIndex = new VariableReference(indexes.back());

        m_os << L"COND ";
        for (size_t i = 0; i < _array.size(); ++i) {
            if (i != 0)
                m_os << L", ";

            traverseNode(*pIndex);
            m_os << L" = ";
            traverseNode(*_array.get(i)->getIndex());
            m_os << L" -> ";
            traverseNode(*_array.get(i)->getValue());
        }
        m_os << L" ENDCOND";
    }

    bool visitArrayConstructor(ArrayConstructor& _array) {
        m_os << L"LAMBDA ";
        printArrayConstructor(_array);
        return false;
    }

    bool traverseSetConstructor(SetConstructor &_expr) {
        VISITOR_ENTER(SetConstructor, _expr);
        if (!_expr.getType())
            VISITOR_EXIT();

        NamedValue nv(L"", _expr.getType().as<SetType>()->getBaseType());

        m_os << L"{";
        traverseNode(nv);
        m_os << L" | ";

        for (size_t i = 0; i < _expr.size(); ++i) {
            if (i != 0)
                m_os << L" OR ";
            m_os << m_context.nameGenerator().getNamedValueName(nv) << " = ";
            traverseNode(*_expr.get(i));
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

    void printArrayDimTypes(const ArrayType& _array) {
        std::list<TypePtr> dims;
        _array.getDimensions(dims);

        if (dims.size() > 1)
            m_os << L"[";

        for (std::list<TypePtr>::iterator i = dims.begin(); i != dims.end(); ++i) {
            if (i != dims.begin())
                m_os << ", ";
            traverseNode(**i);
        }

        if (dims.size() > 1)
            m_os << L"]";
    }

    void printArrayPartIndices(const Collection<Expression>& _indices) {
        if (_indices.size() > 1)
            m_os << L"[";

        for (size_t i = 0; i < _indices.size(); ++i) {
            if (i != 0)
                m_os << L", ";
            traverseNode(*_indices.get(i));
        }

        if (_indices.size() > 1)
            m_os << L"]";
    }

    bool visitArrayPartExpr(ArrayPartExpr &_expr) {
        if (!_expr.getType())
            return false;

        const ArrayType& array = *_expr.getObject()->getType().as<ArrayType>();
        m_os << L"restrict[";
        printArrayDimTypes(array);
        m_os << L", ";
        printArrayPartIndices(_expr.getIndices());
        m_os << L", ";
        traverseNode(*array.getRootType());
        m_os << L"]";

        m_os << L"(";
        traverseNode(*_expr.getObject());
        m_os << L")";

        return false;
    }

    bool traverseArrayPartDefinition(ArrayPartDefinition &_cons) {
        VISITOR_ENTER(ArrayPartDefinition, _cons);
        VISITOR_TRAVERSE_COL(Expression, ArrayPartCond, _cons.getConditions());
        m_os << L" -> ";
        VISITOR_TRAVERSE(Expression, ArrayPartValue, _cons.getExpression(), _cons, ArrayPartDefinition, setExpression);
        VISITOR_EXIT();
    }

    void printCondition(const NamedValuePtr& _pValue, const ExpressionPtr& _pExpr) {
        if (!_pValue || !_pExpr)
            return;

        if (_pExpr->getKind() != Expression::TYPE) {
            m_os << _pValue->getName();
            m_os << L" = ";
            traverseNode(*_pExpr);
            return;
        }

        if (_pExpr.as<TypeExpr>()->getContents()->getKind() != Type::SUBTYPE)
            return;

        const Subtype& sub = *_pExpr.as<TypeExpr>()->getContents().as<Subtype>();

        ExpressionPtr pCond = clone(sub.getExpression());
        pCond = Expression::substitute(pCond, new VariableReference(sub.getParam()), new VariableReference(_pValue)).as<Expression>();

        m_os << L"(";
        traverseNode(*pCond);
        m_os << L")";
    }

    bool traverseArrayIteration(ArrayIteration & _expr) {
        VISITOR_ENTER(ArrayIteration, _expr);
        m_os << L"(LAMBDA (";
        VISITOR_TRAVERSE_COL(NamedValue, ArrayIterator, _expr.getIterators());
        m_os << L") : COND ";

        for (size_t i = 0; i < _expr.size(); ++i) {
            if (i != 0)
                m_os << L", ";

            const ArrayPartDefinitionPtr& pDef = _expr.get(i);

            m_os << L"(";

            for (size_t j = 0; j < pDef->getConditions().size(); ++j) {
                if (j != 0)
                    m_os << L" OR ";

                const ExpressionPtr& pCond = pDef->getConditions().get(j);
                if (pCond->getKind() == Expression::CONSTRUCTOR
                    && pCond.as<Constructor>()->getConstructorKind() == Constructor::STRUCT_FIELDS) {
                    const StructConstructor& tuple = *pCond.as<StructConstructor>();

                    m_os << L"(";
                    for (size_t k = 0; k < tuple.size(); ++k) {
                        if (k != 0)
                            m_os << L" AND ";
                        printCondition(_expr.getIterators().get(k), tuple.get(k)->getValue());
                    }
                    m_os << L")";

                    continue;
                }

                printCondition(_expr.getIterators().get(0), pCond);
            }

            m_os << L") -> ";

            traverseNode(*pDef->getExpression());
        }

        if (_expr.getDefault()) {
            m_os << L", ELSE -> ";
            VISITOR_TRAVERSE(Expression, ArrayIterationDefault, _expr.getDefault(), _expr, ArrayIteration, setDefault);
        }

        m_os << L" ENDCOND)";
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

        std::list<NodePtr> sorted;
        m_context.sortModule(_module, sorted);

        for (std::list<NodePtr>::iterator i = sorted.begin(); i != sorted.end(); ++i)
            traverseNode(**i);

        m_os << L"\n";

        traverseNode(_module.getLemmas());

        m_os << L"\nEND " << cyrillicToASCII(_module.getName()) << L"\n";

        return true;
    }

private:
    pp::Context m_context;
    std::wostream &m_os;
};

void generatePvs(Module &_module, std::wostream & _os) {
    GeneratePvs(_os).traverseNode(_module);
}
