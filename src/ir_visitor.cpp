#include "ir/visitor.h"

#include <iostream>

using namespace ir;

#define ENTER(_TYPE, _PARAM)                                \
    do {                                                    \
        if (isStopped())                                    \
            return false;                                   \
        if (m_order == PARENTS_FIRST) {                      \
            callRoleHandler();                              \
            if (!walkUpFrom##_TYPE(_PARAM))                 \
                return !isStopped();                        \
        } else                                              \
            getLoc().walkUp = &Visitor::walkUpFrom##_TYPE;  \
    } while (0)

#define TRAVERSE(_TYPE, _ROLE, _PARAM, _PARENT, _PTYPE, _SETTER)                            \
    do {                                                                                    \
        if (isStopped())                                                                    \
            return false;                                                                   \
        if ((_PARAM) != NULL) {                                                             \
            NodeSetterImpl< _PTYPE, _TYPE, &_PTYPE::_SETTER > setter(_PARENT);     \
            Ctx ctx(this, _PARAM, N_##_TYPE, R_##_ROLE, &Visitor::handle##_ROLE, &setter);  \
            if (!traverse##_TYPE(*(_PARAM)))                                                \
                return false;                                                               \
            if (m_order == CHILDREN_FIRST) {                                                 \
                callRoleHandler();                                                          \
                if (!callWalkUp())                                                          \
                    return !isStopped();                                                    \
            }                                                                               \
        }                                                                                   \
    } while (0)

#define TRAVERSE_COL(_TYPE, _ROLE, _PARAM)                                              \
    do {                                                                                \
        if (isStopped())                                                                \
            return false;                                                               \
        if ((_PARAM) != NULL) {                                                         \
            Ctx ctx(this, _PARAM, N_##_TYPE, R_##_ROLE, &Visitor::handle##_ROLE, NULL); \
            if (!traverseCollection(*(_PARAM)))                                         \
                return false;                                                           \
        }                                                                               \
    } while (0)

bool Visitor::visitNode(Node &_node) {
    return true;
}

bool Visitor::walkUpFromNode(Node &_node) {
    return visitNode(_node);
}

bool Visitor::traverseNode(Node &_node) {
    switch (_node.getNodeKind()) {
        case Node::COLLECTION:
            return traverseCollection((Collection<Node> &)_node);
        case Node::TYPE:
            return traverseType((Type &)_node);
        case Node::NAMED_VALUE:
            return traverseNamedValue((NamedValue &)_node);
        case Node::STATEMENT:
            return traverseStatement((Statement &)_node);
        case Node::EXPRESSION:
            return traverseExpression((Expression &)_node);
        case Node::MODULE:
            return traverseModule((Module &)_node);
        case Node::CLASS:
            return traverseClass((Class &)_node);
        case Node::LABEL:
            return traverseLabel((Label &)_node);
        case Node::MESSAGE:
            return traverseMessage((Message &)_node);
        case Node::PROCESS:
            return traverseProcess((Process &)_node);
        case Node::UNION_CONSTRUCTOR_DECLARATION:
            return traverseUnionConstructorDeclaration((UnionConstructorDeclaration &)_node);
        case Node::ELEMENT_DEFINITION:
            return traverseElementDefinition((ElementDefinition &)_node);
        case Node::STRUCT_FIELD_DEFINITION:
            return traverseStructFieldDefinition((StructFieldDefinition &)_node);
        case Node::ARRAY_PART_DEFINITION:
            return traverseArrayPartDefinition((ArrayPartDefinition &)_node);
        case Node::SWITCH_CASE:
            return traverseSwitchCase((SwitchCase &)_node);
        case Node::MESSAGE_HANDLER:
            return traverseMessageHandler((MessageHandler &)_node);
    }

    return true;
}

bool Visitor::traverseType(Type &_type) {
    switch (_type.getKind()) {
        case Type::TYPE:
            return traverseTypeType((TypeType &)_type);
        case Type::ENUM:
            return traverseEnumType((EnumType &)_type);
        case Type::STRUCT:
            return traverseStructType((StructType &)_type);
        case Type::UNION:
            return traverseUnionType((UnionType &)_type);
        case Type::ARRAY:
            return traverseArrayType((ArrayType &)_type);
        case Type::SET:
            return traverseSetType((SetType &)_type);
        case Type::MAP:
            return traverseMapType((MapType &)_type);
        case Type::LIST:
            return traverseListType((ListType &)_type);
        case Type::SUBTYPE:
            return traverseSubtype((Subtype &)_type);
        case Type::RANGE:
            return traverseRange((Range &)_type);
        case Type::PREDICATE:
            return traversePredicateType((PredicateType &)_type);
        case Type::PARAMETERIZED:
            return traverseParameterizedType((ParameterizedType &)_type);
        case Type::NAMED_REFERENCE:
            return traverseNamedReferenceType((NamedReferenceType &)_type);
    }

    ENTER(Type, _type);
    return true;
}

bool Visitor::traverseTypeType(TypeType &_type) {
    ENTER(TypeType, _type);
    TRAVERSE(TypeDeclaration, TypeTypeDecl, _type.getDeclaration(), &_type, TypeType, setDeclaration);
    return true;
}

bool Visitor::traverseEnumType(EnumType &_type) {
    ENTER(EnumType, _type);
    TRAVERSE_COL(EnumValue, EnumValueDecl, &_type.getValues());
    return true;
}

bool Visitor::traverseStructType(StructType &_type) {
    ENTER(StructType, _type);
    TRAVERSE_COL(NamedValue, StructFieldDecl, &_type.getFields());
    return true;
}

bool Visitor::traverseUnionType(UnionType &_type) {
    ENTER(UnionType, _type);
    TRAVERSE_COL(UnionConstructorDeclaration, UnionConstructorDecl, &_type.getConstructors());
    return true;
}

bool Visitor::traverseArrayType(ArrayType &_type) {
    ENTER(ArrayType, _type);
    TRAVERSE_COL(Range, ArrayDimDecl, &_type.getDimensions());
    TRAVERSE(Type, ArrayBaseType, _type.getBaseType(), &_type, DerivedType, setBaseType);
    return true;
}

bool Visitor::traverseSetType(SetType &_type) {
    ENTER(SetType, _type);
    TRAVERSE(Type, SetBaseType, _type.getBaseType(), &_type, DerivedType, setBaseType);
    return true;
}

bool Visitor::traverseMapType(MapType &_type) {
    ENTER(MapType, _type);
    TRAVERSE(Type, MapIndexType, _type.getIndexType(), &_type, MapType, setIndexType);
    TRAVERSE(Type, MapBaseType, _type.getBaseType(), &_type, DerivedType, setBaseType);
    return true;
}

bool Visitor::traverseListType(ListType &_type) {
    ENTER(ListType, _type);
    TRAVERSE(Type, ListBaseType, _type.getBaseType(), &_type, DerivedType, setBaseType);
    return true;
}

bool Visitor::traverseSubtype(Subtype &_type) {
    ENTER(Subtype, _type);
    TRAVERSE(NamedValue, SubtypeParam, _type.getParam(), &_type, Subtype, setParam);
    TRAVERSE(Expression, SubtypeCond, _type.getExpression(), &_type, Subtype, setExpression);
    return true;
}

bool Visitor::traverseRange(Range &_type) {
    ENTER(Range, _type);
    TRAVERSE(Expression, RangeMin, _type.getMin(), &_type, Range, setMin);
    TRAVERSE(Expression, RangeMax, _type.getMax(), &_type, Range, setMax);
    return true;
}

bool Visitor::traversePredicateType(PredicateType &_type) {
    ENTER(PredicateType, _type);
    TRAVERSE_COL(Param, PredicateTypeInParam, &_type.getInParams());

    for (size_t i = 0; i < _type.getOutParams().size(); ++i) {
        Branch &br = *_type.getOutParams().get(i);

        TRAVERSE(Label, PredicateTypeBranchLabel, br.getLabel(), &br, Branch, setLabel);
        TRAVERSE(Formula, PredicateTypeBranchPreCondition, br.getPreCondition(), &br, Branch, setPreCondition);
        TRAVERSE(Formula, PredicateTypeBranchPostCondition, br.getPostCondition(), &br, Branch, setPostCondition);
        TRAVERSE_COL(Param, PredicateTypeOutParam, &br);
    }

    TRAVERSE(Formula, PredicateTypePreCondition, _type.getPreCondition(), &_type, PredicateType, setPreCondition);
    TRAVERSE(Formula, PredicateTypePostCondition, _type.getPostCondition(), &_type, PredicateType, setPreCondition);
    return true;
}

bool Visitor::traverseParameterizedType(ParameterizedType &_type) {
    ENTER(ParameterizedType, _type);
    TRAVERSE_COL(NamedValue, ParameterizedTypeParam, &_type.getParams());
    TRAVERSE(Type, ParameterizedTypeBase, _type.getActualType(), &_type, ParameterizedType, setActualType);
    return true;
}

bool Visitor::traverseNamedReferenceType(NamedReferenceType &_type) {
    ENTER(NamedReferenceType, _type);
    TRAVERSE_COL(Expression, NamedTypeArg, &_type.getArgs());
    return true;
}

bool Visitor::traverseDerivedType(DerivedType &_type) {
    return true;
}

// Named.

bool Visitor::traverseNamedValue(NamedValue &_val) {
    switch (_val.getKind()) {
        case NamedValue::ENUM_VALUE:
            return traverseEnumValue((EnumValue &)_val);
        case NamedValue::PREDICATE_PARAMETER:
            return traverseParam((Param &)_val);
        case NamedValue::LOCAL:
        case NamedValue::GLOBAL:
            return traverseVariable((Variable &)_val);
    }

    ENTER(NamedValue, _val);
    TRAVERSE(Type, NamedValueType, _val.getType(), &_val, NamedValue, setType);
    return true;
}

bool Visitor::traverseEnumValue(EnumValue &_val) {
    ENTER(EnumValue, _val);
    if (getRole() != R_EnumValueDecl)
        TRAVERSE(Type, EnumValueType, _val.getType(), &_val, NamedValue, setType);
    return true;
}

bool Visitor::traverseParam(Param &_val) {
    ENTER(Param, _val);
    TRAVERSE(Type, ParamType, _val.getType(), &_val, NamedValue, setType);
    return true;
}

bool Visitor::traverseVariable(Variable &_val) {
    ENTER(Variable, _val);
    TRAVERSE(Type, VariableType, _val.getType(), &_val, NamedValue, setType);
    return true;
}

// Expressions.

bool Visitor::traverseExpression(Expression &_expr) {
    switch (_expr.getKind()) {
        case Expression::LITERAL:
            return traverseLiteral((Literal &)_expr);
        case Expression::VAR:
            return traverseVariableReference((VariableReference &)_expr);
        case Expression::PREDICATE:
            return traversePredicateReference((PredicateReference &)_expr);
        case Expression::UNARY:
            return traverseUnary((Unary &)_expr);
        case Expression::BINARY:
            return traverseBinary((Binary &)_expr);
        case Expression::TERNARY:
            return traverseTernary((Ternary &)_expr);
        case Expression::TYPE:
            return traverseTypeExpr((TypeExpr &)_expr);
        case Expression::COMPONENT:
            return traverseComponent((Component &)_expr);
        case Expression::FUNCTION_CALL:
            return traverseFunctionCall((FunctionCall &)_expr);
        case Expression::FORMULA_CALL:
            return traverseFormulaCall((FormulaCall &)_expr);
        case Expression::LAMBDA:
            return traverseLambda((Lambda &)_expr);
        case Expression::BINDER:
            return traverseBinder((Binder &)_expr);
        case Expression::FORMULA:
            return traverseFormula((Formula &)_expr);
        case Expression::CONSTRUCTOR:
            return traverseConstructor((Constructor &)_expr);
        case Expression::CAST:
            return traverseCastExpr((CastExpr &)_expr);
    }

    return true;
}

bool Visitor::traverseLiteral(Literal &_expr) {
    ENTER(Literal, _expr);
    return true;
}

bool Visitor::traverseVariableReference(VariableReference &_expr) {
    ENTER(VariableReference, _expr);
    return true;
}

bool Visitor::traversePredicateReference(PredicateReference &_expr) {
    ENTER(PredicateReference, _expr);
    return true;
}

bool Visitor::traverseUnary(Unary &_expr) {
    ENTER(Unary, _expr);
    TRAVERSE(Expression, UnarySubexpression, _expr.getExpression(), &_expr, Unary, setExpression);
    return true;
}

bool Visitor::traverseBinary(Binary &_expr) {
    ENTER(Binary, _expr);
    TRAVERSE(Expression, BinarySubexpression, _expr.getLeftSide(), &_expr, Binary, setLeftSide);
    TRAVERSE(Expression, BinarySubexpression, _expr.getRightSide(), &_expr, Binary, setRightSide);
    return true;
}

bool Visitor::traverseTernary(Ternary &_expr) {
    ENTER(Ternary, _expr);
    TRAVERSE(Expression, TernarySubexpression, _expr.getIf(), &_expr, Ternary, setIf);
    TRAVERSE(Expression, TernarySubexpression, _expr.getThen(), &_expr, Ternary, setThen);
    TRAVERSE(Expression, TernarySubexpression, _expr.getElse(), &_expr, Ternary, setElse);
    return true;
}

bool Visitor::traverseTypeExpr(TypeExpr &_expr) {
    ENTER(TypeExpr, _expr);
    TRAVERSE(Type, TypeExprValue, _expr.getContents(), &_expr, TypeExpr, setContents);
    return true;
}

bool Visitor::traverseComponent(Component &_expr) {
    switch (_expr.getComponentKind()) {
        case Component::ARRAY_PART:
            return traverseArrayPartExpr((ArrayPartExpr &)_expr);
        case Component::STRUCT_FIELD:
            return traverseStructFieldExpr((StructFieldExpr &)_expr);
        case Component::UNION_ALTERNATIVE:
            return traverseUnionAlternativeExpr((UnionAlternativeExpr &)_expr);
        case Component::MAP_ELEMENT:
            return traverseMapElementExpr((MapElementExpr &)_expr);
        case Component::LIST_ELEMENT:
            return traverseListElementExpr((ListElementExpr &)_expr);
        case Component::REPLACEMENT:
            return traverseReplacement((Replacement &)_expr);
    }

    return true;
}

bool Visitor::traverseArrayPartExpr(ArrayPartExpr &_expr) {
    ENTER(ArrayPartExpr, _expr);
    TRAVERSE_COL(Expression, ArrayPartIndex, &_expr.getIndices());
    return true;
}

bool Visitor::traverseStructFieldExpr(StructFieldExpr &_expr) {
    ENTER(StructFieldExpr, _expr);
    return true;
}

bool Visitor::traverseUnionAlternativeExpr(UnionAlternativeExpr &_expr) {
    ENTER(UnionAlternativeExpr, _expr);
    return true;
}

bool Visitor::traverseMapElementExpr(MapElementExpr &_expr) {
    ENTER(MapElementExpr, _expr);
    TRAVERSE(Expression, MapElementIndex, _expr.getIndex(), &_expr, MapElementExpr, setIndex);
    return true;
}

bool Visitor::traverseListElementExpr(ListElementExpr &_expr) {
    ENTER(ListElementExpr, _expr);
    TRAVERSE(Expression, ListElementIndex, _expr.getIndex(), &_expr, ListElementExpr, setIndex);
    return true;
}

bool Visitor::traverseReplacement(Replacement &_expr) {
    ENTER(Replacement, _expr);
    TRAVERSE(Constructor, ReplacementValue, _expr.getNewValues(), &_expr, Replacement, setNewValues);
    return true;
}

bool Visitor::traverseFunctionCall(FunctionCall &_expr) {
    ENTER(FunctionCall, _expr);
    TRAVERSE(Expression, FunctionCallee, _expr.getPredicate(), &_expr, FunctionCall, setPredicate);
    TRAVERSE_COL(Expression, FunctionCallParams, &_expr.getParams());
    return true;
}

bool Visitor::traverseFormulaCall(FormulaCall &_expr) {
    ENTER(FormulaCall, _expr);
    TRAVERSE_COL(Expression, FormulaCallParams, &_expr.getParams());
    return true;
}

bool Visitor::traverseLambda(Lambda &_expr) {
    ENTER(Lambda, _expr);
    if (!_traverseAnonymousPredicate(_expr.getPredicate()))
        return false;
    return true;
}

bool Visitor::traverseBinder(Binder &_expr) {
    ENTER(Binder, _expr);
    TRAVERSE(Expression, BinderCallee, _expr.getPredicate(), &_expr, Binder, setPredicate);
    TRAVERSE_COL(Expression, BinderParams, &_expr.getParams());
    return true;
}

bool Visitor::traverseFormula(Formula &_expr) {
    ENTER(Formula, _expr);
    TRAVERSE_COL(NamedValue, FormulaBoundVariable, &_expr.getBoundVariables());
    TRAVERSE(Expression, Subformula, _expr.getSubformula(), &_expr, Formula, setSubformula);
    return true;
}

bool Visitor::traverseConstructor(Constructor &_expr) {
    switch (_expr.getConstructorKind()) {
        case Constructor::STRUCT_FIELDS:
            return traverseStructConstructor((StructConstructor &)_expr);
        case Constructor::ARRAY_ELEMENTS:
            return traverseArrayConstructor((ArrayConstructor &)_expr);
        case Constructor::SET_ELEMENTS:
            return traverseSetConstructor((SetConstructor &)_expr);
        case Constructor::MAP_ELEMENTS:
            return traverseMapConstructor((MapConstructor &)_expr);
        case Constructor::LIST_ELEMENTS:
            return traverseListConstructor((ListConstructor &)_expr);
        case Constructor::ARRAY_ITERATION:
            return traverseArrayIteration((ArrayIteration &)_expr);
        case Constructor::UNION_CONSTRUCTOR:
            return traverseUnionConstructor((UnionConstructor &)_expr);
    }

    return true;
}

bool Visitor::traverseStructConstructor(StructConstructor &_expr) {
    ENTER(StructConstructor, _expr);
    TRAVERSE_COL(StructFieldDefinition, StructFieldDef, &_expr);
    return true;
}

bool Visitor::traverseArrayConstructor(ArrayConstructor &_expr) {
    ENTER(ArrayConstructor, _expr);
    TRAVERSE_COL(ElementDefinition, ArrayElementDef, &_expr);
    return true;
}

bool Visitor::traverseSetConstructor(SetConstructor &_expr) {
    ENTER(SetConstructor, _expr);
    TRAVERSE_COL(Expression, SetElementDef, &_expr);
    return true;
}

bool Visitor::traverseMapConstructor(MapConstructor &_expr) {
    ENTER(MapConstructor, _expr);
    TRAVERSE_COL(ElementDefinition, MapElementDef, &_expr);
    return true;
}

bool Visitor::traverseListConstructor(ListConstructor &_expr) {
    ENTER(ListConstructor, _expr);
    TRAVERSE_COL(Expression, ListElementDef, &_expr);
    return true;
}

bool Visitor::traverseArrayIteration(ArrayIteration &_expr) {
    ENTER(ArrayIteration, _expr);
    TRAVERSE_COL(NamedValue, ArrayIterator, &_expr.getIterators());
    TRAVERSE(Expression, ArrayIterationDefault, _expr.getDefault(), &_expr, ArrayIteration, setDefault);
    TRAVERSE_COL(ArrayPartDefinition, ArrayIterationPart, &_expr);
    return true;
}

bool Visitor::traverseUnionConstructor(UnionConstructor &_expr) {
    ENTER(UnionConstructor, _expr);
    TRAVERSE_COL(VariableDeclaration, UnionCostructorVarDecl, &_expr.getDeclarations());
    TRAVERSE_COL(StructFieldDefinition, UnionCostructorParam, &_expr);
    return true;
}

bool Visitor::traverseCastExpr(CastExpr &_expr) {
    ENTER(CastExpr, _expr);
    TRAVERSE(TypeExpr, CastToType, _expr.getToType(), &_expr, CastExpr, setToType);
    TRAVERSE(Expression, CastParam, _expr.getExpression(), &_expr, CastExpr, setExpression);
    return true;
}

// Statements.

bool Visitor::traverseStatement(Statement &_stmt) {
    switch (_stmt.getKind()) {
        case Statement::BLOCK:
            return traverseBlock((Block &)_stmt);
        case Statement::PARALLEL_BLOCK:
            return traverseParallelBlock((ParallelBlock &)_stmt);
        case Statement::JUMP:
            return traverseJump((Jump &)_stmt);
        case Statement::ASSIGNMENT:
            return traverseAssignment((Assignment &)_stmt);
        case Statement::MULTIASSIGNMENT:
            return traverseMultiassignment((Multiassignment &)_stmt);
        case Statement::CALL:
            return traverseCall((Call &)_stmt);
        case Statement::SWITCH:
            return traverseSwitch((Switch &)_stmt);
        case Statement::IF:
            return traverseIf((If &)_stmt);
        case Statement::FOR:
            return traverseFor((For &)_stmt);
        case Statement::WHILE:
            return traverseWhile((While &)_stmt);
        case Statement::BREAK:
            return traverseBreak((Break &)_stmt);
        case Statement::WITH:
            return traverseWith((With &)_stmt);
        case Statement::RECEIVE:
            return traverseReceive((Receive &)_stmt);
        case Statement::SEND:
            return traverseSend((Send &)_stmt);
        case Statement::TYPE_DECLARATION:
            return traverseTypeDeclaration((TypeDeclaration &)_stmt);
        case Statement::VARIABLE_DECLARATION:
            return traverseVariableDeclaration((VariableDeclaration &)_stmt);
        case Statement::FORMULA_DECLARATION:
            return traverseFormulaDeclaration((FormulaDeclaration &)_stmt);
        case Statement::PREDICATE_DECLARATION:
            return traversePredicate((Predicate &)_stmt);
    }

    ENTER(Statement, _stmt);
    TRAVERSE(Label, StmtLabel, _stmt.getLabel(), &_stmt, Statement, setLabel);
    return true;
}

bool Visitor::traverseBlock(Block &_stmt) {
    ENTER(Block, _stmt);
    TRAVERSE(Label, StmtLabel, _stmt.getLabel(), &_stmt, Statement, setLabel);
    TRAVERSE_COL(Statement, Stmt, &_stmt);
    return true;
}

bool Visitor::traverseParallelBlock(ParallelBlock &_stmt) {
    ENTER(ParallelBlock, _stmt);
    TRAVERSE(Label, StmtLabel, _stmt.getLabel(), &_stmt, Statement, setLabel);
    TRAVERSE_COL(Statement, Stmt, &_stmt);
    return true;
}

bool Visitor::traverseJump(Jump &_stmt) {
    ENTER(Jump, _stmt);
    TRAVERSE(Label, StmtLabel, _stmt.getLabel(), &_stmt, Statement, setLabel);
    return true;
}

bool Visitor::traverseAssignment(Assignment &_stmt) {
    ENTER(Assignment, _stmt);
    TRAVERSE(Label, StmtLabel, _stmt.getLabel(), &_stmt, Statement, setLabel);
    TRAVERSE(Expression, LValue, _stmt.getLValue(), &_stmt, Assignment, setLValue);
    TRAVERSE(Expression, RValue, _stmt.getExpression(), &_stmt, Assignment, setExpression);
    return true;
}

bool Visitor::traverseMultiassignment(Multiassignment &_stmt) {
    ENTER(Multiassignment, _stmt);
    TRAVERSE(Label, StmtLabel, _stmt.getLabel(), &_stmt, Statement, setLabel);
    TRAVERSE_COL(Expression, LValue, &_stmt.getLValues());
    TRAVERSE_COL(Expression, RValue, &_stmt.getExpressions());
    return true;
}

bool Visitor::traverseCall(Call &_stmt) {
    ENTER(Call, _stmt);
    TRAVERSE(Label, StmtLabel, _stmt.getLabel(), &_stmt, Statement, setLabel);
    TRAVERSE(Expression, PredicateCallee, _stmt.getPredicate(), &_stmt, Call, setPredicate);
    TRAVERSE_COL(VariableDeclaration, PredicateVarDecl, &_stmt.getDeclarations());
    TRAVERSE_COL(Expression, PredicateCallParams, &_stmt.getParams());

    for (size_t i = 0; i < _stmt.getBranches().size(); ++i) {
        CallBranch &br = *_stmt.getBranches().get(i);
        TRAVERSE(Statement, PredicateCallBranchHandler, br.getHandler(), &br, CallBranch, setHandler);
        TRAVERSE_COL(Expression, PredicateCallBranchResults, &br);
    }

    return true;
}

bool Visitor::traverseSwitch(Switch &_stmt) {
    ENTER(Switch, _stmt);
    TRAVERSE(Label, StmtLabel, _stmt.getLabel(), &_stmt, Statement, setLabel);
    TRAVERSE(VariableDeclaration, SwitchParamDecl, _stmt.getParamDecl(), &_stmt, Switch, setParamDecl);
    TRAVERSE(Expression, SwitchParam, _stmt.getParam(), &_stmt, Switch, setParam);
    TRAVERSE(Statement, SwitchDefault, _stmt.getDefault(), &_stmt, Switch, setDefault);
    TRAVERSE_COL(SwitchCase, SwitchCase, &_stmt);
    return true;
}

bool Visitor::traverseIf(If &_stmt) {
    ENTER(If, _stmt);
    TRAVERSE(Label, StmtLabel, _stmt.getLabel(), &_stmt, Statement, setLabel);
    TRAVERSE(Expression, IfParam, _stmt.getParam(), &_stmt, If, setParam);
    TRAVERSE(Statement, IfBody, _stmt.getBody(), &_stmt, If, setBody);
    TRAVERSE(Statement, IfElse, _stmt.getElse(), &_stmt, If, setElse);
    return true;
}

bool Visitor::traverseFor(For &_stmt) {
    ENTER(For, _stmt);
    TRAVERSE(Label, StmtLabel, _stmt.getLabel(), &_stmt, Statement, setLabel);
    TRAVERSE(VariableDeclaration, ForIterator, _stmt.getIterator(), &_stmt, For, setIterator);
    TRAVERSE(Expression, ForInvariant, _stmt.getInvariant(), &_stmt, For, setInvariant);
    TRAVERSE(Statement, ForIncrement, _stmt.getIncrement(), &_stmt, For, setIncrement);
    TRAVERSE(Statement, ForBody, _stmt.getBody(), &_stmt, For, setBody);
    return true;
}

bool Visitor::traverseWhile(While &_stmt) {
    ENTER(While, _stmt);
    TRAVERSE(Label, StmtLabel, _stmt.getLabel(), &_stmt, Statement, setLabel);
    TRAVERSE(Expression, WhileInvariant, _stmt.getInvariant(), &_stmt, While, setInvariant);
    TRAVERSE(Statement, WhileBody, _stmt.getBody(), &_stmt, While, setBody);
    return true;
}

bool Visitor::traverseBreak(Break &_stmt) {
    ENTER(Break, _stmt);
    TRAVERSE(Label, StmtLabel, _stmt.getLabel(), &_stmt, Statement, setLabel);
    return true;
}

bool Visitor::traverseWith(With &_stmt) {
    ENTER(With, _stmt);
    TRAVERSE(Label, StmtLabel, _stmt.getLabel(), &_stmt, Statement, setLabel);
    TRAVERSE_COL(Expression, WithParam, &_stmt.getParams());
    TRAVERSE(Statement, WithBody, _stmt.getBody(), &_stmt, With, setBody);
    return true;
}

bool Visitor::traverseReceive(Receive &_stmt) {
    ENTER(Receive, _stmt);
    TRAVERSE(Label, StmtLabel, _stmt.getLabel(), &_stmt, Statement, setLabel);
    TRAVERSE(Expression, ReceiveTimeout, _stmt.getTimeout(), &_stmt, Receive, setTimeout);
    TRAVERSE(Statement, ReceiveTimeoutHandler, _stmt.getTimeoutHandler(), &_stmt, Receive, setTimeoutHandler);
    TRAVERSE_COL(MessageHandler, ReceiveHandler, &_stmt);
    return true;
}

bool Visitor::traverseSend(Send &_stmt) {
    ENTER(Send, _stmt);
    TRAVERSE(Label, StmtLabel, _stmt.getLabel(), &_stmt, Statement, setLabel);
    TRAVERSE_COL(Expression, SendParams, &_stmt.getParams());
    return true;
}

bool Visitor::traverseTypeDeclaration(TypeDeclaration &_stmt) {
    ENTER(TypeDeclaration, _stmt);
    TRAVERSE(Label, StmtLabel, _stmt.getLabel(), &_stmt, Statement, setLabel);
    TRAVERSE(Type, TypeDeclBody, _stmt.getType(), &_stmt, TypeDeclaration, setType);
    return true;
}

bool Visitor::traverseVariableDeclaration(VariableDeclaration &_stmt) {
    ENTER(VariableDeclaration, _stmt);
    TRAVERSE(Label, StmtLabel, _stmt.getLabel(), &_stmt, Statement, setLabel);
    TRAVERSE(Variable, VarDeclVar, _stmt.getVariable(), &_stmt, VariableDeclaration, setVariable);
    TRAVERSE(Expression, VarDeclInit, _stmt.getValue(), &_stmt, VariableDeclaration, setValue);
    return true;
}

bool Visitor::traverseFormulaDeclaration(FormulaDeclaration &_stmt) {
    ENTER(FormulaDeclaration, _stmt);
    TRAVERSE(Label, StmtLabel, _stmt.getLabel(), &_stmt, Statement, setLabel);
    TRAVERSE_COL(NamedValue, FormulaDeclParams, &_stmt.getParams());
    TRAVERSE(Expression, FormulaDeclBody, _stmt.getFormula(), &_stmt, FormulaDeclaration, setFormula);
    return true;
}

bool Visitor::traversePredicate(Predicate &_stmt) {
    ENTER(Predicate, _stmt);
    TRAVERSE(Label, StmtLabel, _stmt.getLabel(), &_stmt, Statement, setLabel);

    if (!_traverseAnonymousPredicate(_stmt))
        return false;

    return true;
}

// Misc.

bool Visitor::traverseUnionConstructorDeclaration(UnionConstructorDeclaration &_cons) {
    ENTER(UnionConstructorDeclaration, _cons);
    TRAVERSE_COL(NamedValue, UnionConsField, &_cons.getStruct().getFields());
    return true;
}

bool Visitor::traverseStructFieldDefinition(StructFieldDefinition &_cons) {
    ENTER(StructFieldDefinition, _cons);
    TRAVERSE(Expression, StructFieldValue, _cons.getValue(), &_cons, StructFieldDefinition, setValue);
    return true;
}

bool Visitor::traverseElementDefinition(ElementDefinition &_cons) {
    ENTER(ElementDefinition, _cons);
    TRAVERSE(Expression, ElementIndex, _cons.getIndex(), &_cons, ElementDefinition, setIndex);
    TRAVERSE(Expression, ElementValue, _cons.getValue(), &_cons, ElementDefinition, setValue);
    return true;
}

bool Visitor::traverseArrayPartDefinition(ArrayPartDefinition &_cons) {
    ENTER(ArrayPartDefinition, _cons);
    TRAVERSE_COL(Expression, ArrayPartCond, &_cons.getConditions());
    TRAVERSE(Expression, ArrayPartValue, _cons.getExpression(), &_cons, ArrayPartDefinition, setExpression);
    return true;
}

bool Visitor::traverseLabel(Label &_label) {
    ENTER(Label, _label);
    return true;
}

bool Visitor::traverseSwitchCase(SwitchCase &_case) {
    ENTER(SwitchCase, _case);
    TRAVERSE_COL(Expression, SwitchCaseValue, &_case.getExpressions());
    TRAVERSE(Statement, SwitchCaseBody, _case.getBody(), &_case, SwitchCase, setBody);
    return true;
}

bool Visitor::_traverseAnonymousPredicate(AnonymousPredicate &_decl) {
    TRAVERSE_COL(Param, PredicateInParam, &_decl.getInParams());

    for (size_t i = 0; i < _decl.getOutParams().size(); ++i) {
        Branch &br = *_decl.getOutParams().get(i);

        TRAVERSE(Label, PredicateBranchLabel, br.getLabel(), &br, Branch, setLabel);
        TRAVERSE(Formula, PredicateBranchPreCondition, br.getPreCondition(), &br, Branch, setPreCondition);
        TRAVERSE(Formula, PredicateBranchPostCondition, br.getPostCondition(), &br, Branch, setPostCondition);
        TRAVERSE_COL(Param, PredicateOutParam, &br);
    }

    TRAVERSE(Formula, PredicatePreCondition, _decl.getPreCondition(), &_decl, AnonymousPredicate, setPreCondition);
    TRAVERSE(Formula, PredicatePostCondition, _decl.getPostCondition(), &_decl, AnonymousPredicate, setPostCondition);
    TRAVERSE(Block, PredicateBody, _decl.getBlock(), &_decl, AnonymousPredicate, setBlock);

    return true;
}

bool Visitor::_traverseDeclarationGroup(DeclarationGroup &_decl) {
    TRAVERSE_COL(Predicate, PredicateDecl, &_decl.getPredicates());
    TRAVERSE_COL(TypeDeclaration, TypeDecl, &_decl.getTypes());
    TRAVERSE_COL(VariableDeclaration, VarDecl, &_decl.getVariables());
    TRAVERSE_COL(FormulaDeclaration, FormulaDecl, &_decl.getFormulas());
    TRAVERSE_COL(Message, MessageDecl, &_decl.getMessages());
    TRAVERSE_COL(Process, ProcessDecl, &_decl.getProcesses());
    return true;
}

bool Visitor::traverseDeclarationGroup(DeclarationGroup &_type) {
    return true;
}

bool Visitor::traverseModule(Module &_module) {
    ENTER(Module, _module);

    if (!_traverseDeclarationGroup(_module))
        return false;

    TRAVERSE_COL(Class, ClassDecl, &_module.getClasses());

    return true;
}

bool Visitor::traverseClass(Class &_class) {
    ENTER(Class, _class);

    if (!_traverseDeclarationGroup(_class))
        return false;

    return true;
}

bool Visitor::traverseMessage(Message &_message) {
    ENTER(Message, _message);
    TRAVERSE_COL(Param, MessageParam, &_message.getParams());
    return true;
}

bool Visitor::traverseProcess(Process &_process) {
    ENTER(Process, _process);
    TRAVERSE_COL(Param, ProcessInParam, &_process.getInParams());

    for (size_t i = 0; i < _process.getOutParams().size(); ++i) {
        Branch &br = *_process.getOutParams().get(i);

        TRAVERSE(Label, ProcessBranchLabel, br.getLabel(), &br, Branch, setLabel);
        TRAVERSE(Formula, ProcessBranchPreCondition, br.getPreCondition(), &br, Branch, setPreCondition);
        TRAVERSE(Formula, ProcessBranchPostCondition, br.getPostCondition(), &br, Branch, setPostCondition);
        TRAVERSE_COL(Param, ProcessOutParam, &br);
    }

    TRAVERSE(Block, ProcessBody, _process.getBlock(), &_process, Process, setBlock);
    return true;
}

bool Visitor::traverseMessageHandler(MessageHandler &_messageHandler) {
    ENTER(MessageHandler, _messageHandler);
    TRAVERSE(Statement, MessageHandlerBody, _messageHandler.getBody(), &_messageHandler, MessageHandler, setBody);
    return true;
}
