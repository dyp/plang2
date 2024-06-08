#include "ir/visitor.h"

#include <iostream>

using namespace ir;

#define ENTER VISITOR_ENTER
#define EXIT VISITOR_EXIT
#define TRAVERSE VISITOR_TRAVERSE
#define TRAVERSE_COL VISITOR_TRAVERSE_COL

NodePtr Visitor::getParent() {
    if (m_path.empty())
        return nullptr;
    auto i = std::prev(m_path.end());
    if (i == m_path.begin())
        return nullptr;
    return (--i)->pNode;
}

bool Visitor::visitNode(const ir::NodePtr &_pNode) {
    return true;
}

bool Visitor::walkUpFromNode(const ir::NodePtr &_pNode) {
    return visitNode(_pNode);
}

bool Visitor::traverseNode(const NodePtr &_pNode) {
    switch (_pNode->getNodeKind()) {
        case Node::COLLECTION: {
            TRAVERSE_COL(Node, TopLevel, (Collection<Node> &) _pNode);
            return true;
        }
        case Node::TYPE:
            return traverseType(_pNode->as<Type>());
        case Node::NAMED_VALUE:
            return traverseNamedValue(_pNode->as<NamedValue>());
        case Node::STATEMENT:
            return traverseStatement(_pNode->as<Statement>());
        case Node::EXPRESSION:
            return traverseExpression(_pNode->as<Expression>());
        case Node::MODULE:
            return traverseModule(_pNode->as<Module>());
        case Node::CLASS:
            return traverseClass(_pNode->as<Class>());
        case Node::LABEL:
            return traverseLabel(_pNode->as<Label>());
        case Node::MESSAGE:
            return traverseMessage(_pNode->as<Message>());
        case Node::PROCESS:
            return traverseProcess(_pNode->as<Process>());
        case Node::UNION_CONSTRUCTOR_DECLARATION:
            return traverseUnionConstructorDeclaration(_pNode->as<UnionConstructorDeclaration>());
        case Node::ELEMENT_DEFINITION:
            return traverseElementDefinition(_pNode->as<ElementDefinition>());
        case Node::STRUCT_FIELD_DEFINITION:
            return traverseStructFieldDefinition(_pNode->as<StructFieldDefinition>());
        case Node::ARRAY_PART_DEFINITION:
            return traverseArrayPartDefinition(_pNode->as<ArrayPartDefinition>());
        case Node::SWITCH_CASE:
            return traverseSwitchCase(_pNode->as<SwitchCase>());
        case Node::MESSAGE_HANDLER:
            return traverseMessageHandler(_pNode->as<MessageHandler>());
    }

    return true;
}

bool Visitor::traverseType(const std::shared_ptr<Type> &_pType) {
    switch (_pType->getKind()) {
    case Type::TYPE:
        return traverseTypeType(_pType->as<TypeType>());
    case Type::ENUM:
        return traverseEnumType(_pType->as<EnumType>());
    case Type::STRUCT:
        return traverseStructType(_pType->as<StructType>());
    case Type::UNION:
        return traverseUnionType(_pType->as<UnionType>());
    case Type::ARRAY:
        return traverseArrayType(_pType->as<ArrayType>());
    case Type::SET:
        return traverseSetType(_pType->as<SetType>());
    case Type::MAP:
        return traverseMapType(_pType->as<MapType>());
    case Type::LIST:
        return traverseListType(_pType->as<ListType>());
    case Type::SUBTYPE:
        return traverseSubtype(_pType->as<Subtype>());
    case Type::RANGE:
        return traverseRange(_pType->as<Range>());
    case Type::PREDICATE:
        return traversePredicateType(_pType->as<PredicateType>());
    case Type::PARAMETERIZED:
        return traverseParameterizedType(_pType->as<ParameterizedType>());
    case Type::NAMED_REFERENCE:
        return traverseNamedReferenceType(_pType->as<NamedReferenceType>());
    case Type::REFERENCE:
        return traverseRefType(_pType->as<RefType>());
    case Type::SEQ:
        return traverseSeqType(_pType->as<SeqType>());
    case Type::OPTIONAL:
        return traverseOptionalType(_pType->as<OptionalType>());
    }

    ENTER(Type, _pType);
    EXIT();
}

bool Visitor::traverseTypeType(const std::shared_ptr<TypeType> &_pType) {
    ENTER(TypeType, _pType);
    TRAVERSE(TypeDeclaration, TypeTypeDecl, _pType->getDeclaration(), _pType, TypeType, setDeclaration);
    EXIT();
}

bool Visitor::traverseOptionalType(const std::shared_ptr<OptionalType> &_pType) {
    traverseDerivedType(_pType);
    ENTER(OptionalType, _pType);
    TRAVERSE(Type, OptionalBaseType, _pType->getBaseType(), _pType, DerivedType, setBaseType);
    EXIT();
}

bool Visitor::traverseSeqType(const std::shared_ptr<SeqType> &_pType) {
    ENTER(SeqType, _pType);
    TRAVERSE(Type, SeqBaseType, _pType->getBaseType(), _pType, DerivedType, setBaseType);
    EXIT();
}

bool Visitor::traverseEnumType(const std::shared_ptr<EnumType> &_pType) {
    ENTER(EnumType, _pType);
    TRAVERSE_COL(EnumValue, EnumValueDecl, _pType->getValues());
    EXIT();
}

bool Visitor::traverseStructType(const std::shared_ptr<StructType> &_pType) {
    ENTER(StructType, _pType);
    TRAVERSE_COL(NamedValue, StructFieldDeclNameOrd, *_pType->getNamesOrd());
    TRAVERSE_COL(NamedValue, StructFieldDeclTypeOrd, *_pType->getTypesOrd());
    TRAVERSE_COL(NamedValue, StructFieldDeclNameSet, *_pType->getNamesSet());
    EXIT();
}

bool Visitor::traverseUnionType(const std::shared_ptr<UnionType> &_pType) {
    ENTER(UnionType, _pType);
    TRAVERSE_COL(UnionConstructorDeclaration, UnionConstructorDecl, _pType->getConstructors());
    EXIT();
}

bool Visitor::traverseArrayType(const std::shared_ptr<ArrayType> &_pType) {
    ENTER(ArrayType, _pType);
    TRAVERSE(Type, ArrayDimType, _pType->getDimensionType(), _pType, ArrayType, setDimensionType);
    TRAVERSE(Type, ArrayBaseType, _pType->getBaseType(), _pType, DerivedType, setBaseType);
    EXIT();
}

bool Visitor::traverseSetType(const std::shared_ptr<SetType> &_pType) {
    ENTER(SetType, _pType);
    TRAVERSE(Type, SetBaseType, _pType->getBaseType(), _pType, DerivedType, setBaseType);
    EXIT();
}

bool Visitor::traverseMapType(const std::shared_ptr<MapType> &_pType) {
    ENTER(MapType, _pType);
    TRAVERSE(Type, MapIndexType, _pType->getIndexType(), _pType, MapType, setIndexType);
    TRAVERSE(Type, MapBaseType, _pType->getBaseType(), _pType, DerivedType, setBaseType);
    EXIT();
}

bool Visitor::traverseListType(const std::shared_ptr<ListType> &_pType) {
    ENTER(ListType, _pType);
    TRAVERSE(Type, ListBaseType, _pType->getBaseType(), _pType, DerivedType, setBaseType);
    EXIT();
}

bool Visitor::traverseRefType(const std::shared_ptr<RefType> &_pType) {
    ENTER(RefType, _pType);
    TRAVERSE(Type, RefBaseType, _pType->getBaseType(), _pType, DerivedType, setBaseType);
    EXIT();
}

bool Visitor::traverseSubtype(const std::shared_ptr<Subtype> &_pType) {
    ENTER(Subtype, _pType);
    TRAVERSE(NamedValue, SubtypeParam, _pType->getParam(), _pType, Subtype, setParam);
    TRAVERSE(Expression, SubtypeCond, _pType->getExpression(), _pType, Subtype, setExpression);
    EXIT();
}

bool Visitor::traverseRange(const std::shared_ptr<Range> &_pType) {
    ENTER(Range, _pType);
    TRAVERSE(Expression, RangeMin, _pType->getMin(), _pType, Range, setMin);
    TRAVERSE(Expression, RangeMax, _pType->getMax(), _pType, Range, setMax);
    EXIT();
}

bool Visitor::traversePredicateType(const std::shared_ptr<PredicateType> &_pType) {
    ENTER(PredicateType, _pType);
    TRAVERSE_COL(Param, PredicateTypeInParam, _pType->getInParams());

    for (size_t i = 0; i < _pType->getOutParams().size(); ++i) {
        auto br = std::make_shared<Branch>(*_pType->getOutParams().get(i));

        TRAVERSE(Label, PredicateTypeBranchLabel, br->getLabel(), br, Branch, setLabel);
        TRAVERSE(Formula, PredicateTypeBranchPreCondition, br->getPreCondition(), br, Branch, setPreCondition);
        TRAVERSE(Formula, PredicateTypeBranchPostCondition, br->getPostCondition(), br, Branch, setPostCondition);
        TRAVERSE_COL(Param, PredicateTypeOutParam, *br);
    }

    TRAVERSE(Formula, PredicateTypePreCondition, _pType->getPreCondition(), _pType, PredicateType, setPreCondition);
    TRAVERSE(Formula, PredicateTypePostCondition, _pType->getPostCondition(), _pType, PredicateType, setPreCondition);
    EXIT();
}

bool Visitor::traverseParameterizedType(const std::shared_ptr<ParameterizedType> &_pType) {
    ENTER(ParameterizedType, _pType);
    TRAVERSE_COL(NamedValue, ParameterizedTypeParam, _pType->getParams());
    TRAVERSE(Type, ParameterizedTypeBase, _pType->getActualType(), _pType, ParameterizedType, setActualType);
    EXIT();
}

bool Visitor::traverseNamedReferenceType(const std::shared_ptr<NamedReferenceType> &_pType) {
    ENTER(NamedReferenceType, _pType);
    TRAVERSE_COL(Expression, NamedTypeArg, _pType->getArgs());
    EXIT();
}

bool Visitor::traverseDerivedType(const std::shared_ptr<DerivedType> &_pType) {
    return true;
}

// Named.

bool Visitor::traverseNamedValue(const std::shared_ptr<NamedValue> & _pVal) {
    switch (_pVal->getKind()) {
        case NamedValue::ENUM_VALUE:
            return traverseEnumValue(_pVal->as<EnumValue>());
        case NamedValue::PREDICATE_PARAMETER:
            return traverseParam(_pVal->as<Param>());
        case NamedValue::LOCAL:
        case NamedValue::GLOBAL:
            return traverseVariable(_pVal->as<Variable>());
    }

    ENTER(NamedValue, _pVal);
    TRAVERSE(Type, NamedValueType, _pVal->getType(), _pVal, NamedValue, setType);
    EXIT();
}

bool Visitor::traverseEnumValue(const std::shared_ptr<EnumValue> &_pVal) {
    ENTER(EnumValue, _pVal);
    if (getRole() != R_EnumValueDecl) {
        TRAVERSE(Type, EnumValueType, _pVal->getType(), _pVal, NamedValue, setType);
    }
    EXIT();
}

bool Visitor::traverseParam(const std::shared_ptr<Param> &_pVal) {
    ENTER(Param, _pVal);
    TRAVERSE(Type, ParamType, _pVal->getType(), _pVal, NamedValue, setType);
    EXIT();
}

bool Visitor::traverseVariable(const std::shared_ptr<Variable> &_pVal) {
    ENTER(Variable, _pVal);
    TRAVERSE(Type, VariableType, _pVal->getType(), _pVal, NamedValue, setType);
    EXIT();
}

// Expressions.

bool Visitor::traverseExpression(const std::shared_ptr<Expression> &_pExpr) {
    switch (_pExpr->getKind()) {
    case Expression::WILD:
        return traverseWild(_pExpr->as<Wild>());
    case Expression::LITERAL:
        return traverseLiteral(_pExpr->as<Literal>());
    case Expression::VAR:
        return traverseVariableReference(_pExpr->as<VariableReference>());
    case Expression::PREDICATE:
        return traversePredicateReference(_pExpr->as<PredicateReference>());
    case Expression::UNARY:
        return traverseUnary(_pExpr->as<Unary>());
    case Expression::BINARY:
        return traverseBinary(_pExpr->as<Binary>());
    case Expression::TERNARY:
        return traverseTernary(_pExpr->as<Ternary>());
    case Expression::TYPE:
        return traverseTypeExpr(_pExpr->as<TypeExpr>());
    case Expression::COMPONENT:
        return traverseComponent(_pExpr->as<Component>());
    case Expression::FUNCTION_CALL:
        return traverseFunctionCall(_pExpr->as<FunctionCall>());
    case Expression::FORMULA_CALL:
        return traverseFormulaCall(_pExpr->as<FormulaCall>());
    case Expression::LAMBDA:
        return traverseLambda(_pExpr->as<Lambda>());
    case Expression::BINDER:
        return traverseBinder(_pExpr->as<Binder>());
    case Expression::FORMULA:
        return traverseFormula(_pExpr->as<Formula>());
    case Expression::CONSTRUCTOR:
        return traverseConstructor(_pExpr->as<Constructor>());
    case Expression::CAST:
        return traverseCastExpr(_pExpr->as<CastExpr>());
    }

    return true;
}

bool Visitor::traverseWild(const std::shared_ptr<Wild> &_pExpr) {
    ENTER(Wild, _pExpr);
    EXIT();
}

bool Visitor::traverseLiteral(const std::shared_ptr<Literal> &_pExpr) {
    ENTER(Literal, _pExpr);
    EXIT();
}

bool Visitor::traverseVariableReference(const std::shared_ptr<VariableReference> &_pExpr) {
    ENTER(VariableReference, _pExpr);
    EXIT();
}

bool Visitor::traversePredicateReference(const std::shared_ptr<PredicateReference> &_pExpr) {
    ENTER(PredicateReference, _pExpr);
    EXIT();
}

bool Visitor::traverseUnary(const std::shared_ptr<Unary> &_pExpr) {
    ENTER(Unary, _pExpr);
    TRAVERSE(Expression, UnarySubexpression, _pExpr->getExpression(), _pExpr, Unary, setExpression);
    EXIT();
}

bool Visitor::traverseBinary(const std::shared_ptr<Binary> &_pExpr) {
    ENTER(Binary, _pExpr);
    TRAVERSE(Expression, BinarySubexpression, _pExpr->getLeftSide(), _pExpr, Binary, setLeftSide);
    TRAVERSE(Expression, BinarySubexpression, _pExpr->getRightSide(), _pExpr, Binary, setRightSide);
    EXIT();
}

bool Visitor::traverseTernary(const std::shared_ptr<Ternary> &_pExpr) {
    ENTER(Ternary, _pExpr);
    TRAVERSE(Expression, TernarySubexpression, _pExpr->getIf(), _pExpr, Ternary, setIf);
    TRAVERSE(Expression, TernarySubexpression, _pExpr->getThen(), _pExpr, Ternary, setThen);
    TRAVERSE(Expression, TernarySubexpression, _pExpr->getElse(), _pExpr, Ternary, setElse);
    EXIT();
}

bool Visitor::traverseTypeExpr(const std::shared_ptr<TypeExpr> &_pExpr) {
    ENTER(TypeExpr, _pExpr);
    TRAVERSE(Type, TypeExprValue, _pExpr->getContents(), _pExpr, TypeExpr, setContents);
    EXIT();
}

bool Visitor::traverseComponent(const std::shared_ptr<Component> &_pExpr) {
    switch (_pExpr->getComponentKind()) {
    case Component::ARRAY_PART:
        return traverseArrayPartExpr(_pExpr->as<ArrayPartExpr>());
    case Component::STRUCT_FIELD:
        return traverseFieldExpr(_pExpr->as<FieldExpr>());
    case Component::MAP_ELEMENT:
        return traverseMapElementExpr(_pExpr->as<MapElementExpr>());
    case Component::LIST_ELEMENT:
        return traverseListElementExpr(_pExpr->as<ListElementExpr>());
    case Component::REPLACEMENT:
        return traverseReplacement(_pExpr->as<Replacement>());
    case Component::RECOGNIZER:
        return traverseRecognizerExpr(_pExpr->as<RecognizerExpr>());
    case Component::ACCESSOR:
        return traverseAccessorExpr(_pExpr->as<AccessorExpr>());
    }

    return true;
}

bool Visitor::traverseArrayPartExpr(const std::shared_ptr<ArrayPartExpr> &_pExpr) {
    ENTER(ArrayPartExpr, _pExpr);
    TRAVERSE(Expression, ArrayPartObject, _pExpr->getObject(), _pExpr, Component, setObject);
    TRAVERSE_COL(Expression, ArrayPartIndex, _pExpr->getIndices());
    EXIT();
}

bool Visitor::traverseFieldExpr(const std::shared_ptr<FieldExpr> &_pExpr) {
    ENTER(FieldExpr, _pExpr);
    TRAVERSE(Expression, FieldObject, _pExpr->getObject(), _pExpr, Component, setObject);
    EXIT();
}

bool Visitor::traverseMapElementExpr(const std::shared_ptr<MapElementExpr> &_pExpr) {
    ENTER(MapElementExpr, _pExpr);
    TRAVERSE(Expression, MapElementObject, _pExpr->getObject(), _pExpr, Component, setObject);
    TRAVERSE(Expression, MapElementIndex, _pExpr->getIndex(), _pExpr, MapElementExpr, setIndex);
    EXIT();
}

bool Visitor::traverseListElementExpr(const std::shared_ptr<ListElementExpr> &_pExpr) {
    ENTER(ListElementExpr, _pExpr);
    TRAVERSE(Expression, ListElementObject, _pExpr->getObject(), _pExpr, Component, setObject);
    TRAVERSE(Expression, ListElementIndex, _pExpr->getIndex(), _pExpr, ListElementExpr, setIndex);
    EXIT();
}

bool Visitor::traverseReplacement(const std::shared_ptr<Replacement> &_pExpr) {
    ENTER(Replacement, _pExpr);
    TRAVERSE(Expression, ReplacementObject, _pExpr->getObject(), _pExpr, Component, setObject);
    TRAVERSE(Constructor, ReplacementValue, _pExpr->getNewValues(), _pExpr, Replacement, setNewValues);
    EXIT();
}

bool Visitor::traverseRecognizerExpr(const std::shared_ptr<RecognizerExpr> &_pExpr) {
    ENTER(RecognizerExpr, _pExpr);
    TRAVERSE(Expression, RecognizerExpression, _pExpr->getObject(), _pExpr, Component, setObject);
    EXIT();
}

bool Visitor::traverseAccessorExpr(const std::shared_ptr<AccessorExpr> &_pExpr) {
    ENTER(AccessorExpr, _pExpr);
    TRAVERSE(Expression, AccessorExpression, _pExpr->getObject(), _pExpr, Component, setObject);
    EXIT();
}

bool Visitor::traverseFunctionCall(const std::shared_ptr<FunctionCall> &_pExpr) {
    ENTER(FunctionCall, _pExpr);
    TRAVERSE(Expression, FunctionCallee, _pExpr->getPredicate(), _pExpr, FunctionCall, setPredicate);
    TRAVERSE_COL(Expression, FunctionCallArgs, _pExpr->getArgs());
    EXIT();
}

bool Visitor::traverseFormulaCall(const std::shared_ptr<FormulaCall> &_pExpr) {
    ENTER(FormulaCall, _pExpr);
    TRAVERSE_COL(Expression, FormulaCallArgs, _pExpr->getArgs());
    EXIT();
}

bool Visitor::traverseLambda(const std::shared_ptr<Lambda> &_pExpr) {
    ENTER(Lambda, _pExpr);
    auto predicate = std::make_shared<AnonymousPredicate>(_pExpr->getPredicate());
    if (!_traverseAnonymousPredicate(predicate))
        return false;
    EXIT();
}

bool Visitor::traverseBinder(const std::shared_ptr<Binder> &_pExpr) {
    ENTER(Binder, _pExpr);
    TRAVERSE(Expression, BinderCallee, _pExpr->getPredicate(), _pExpr, Binder, setPredicate);
    TRAVERSE_COL(Expression, BinderArgs, _pExpr->getArgs());
    EXIT();
}

bool Visitor::traverseFormula(const std::shared_ptr<Formula> &_pExpr) {
    ENTER(Formula, _pExpr);
    TRAVERSE_COL(NamedValue, FormulaBoundVariable, _pExpr->getBoundVariables());
    TRAVERSE(Expression, Subformula, _pExpr->getSubformula(), _pExpr, Formula, setSubformula);
    EXIT();
}

bool Visitor::traverseConstructor(const std::shared_ptr<Constructor> &_pExpr) {
    switch (_pExpr->getConstructorKind()) {
    case Constructor::STRUCT_FIELDS:
        return traverseStructConstructor(_pExpr->as<StructConstructor>());
    case Constructor::ARRAY_ELEMENTS:
        return traverseArrayConstructor(_pExpr->as<ArrayConstructor>());
    case Constructor::SET_ELEMENTS:
        return traverseSetConstructor(_pExpr->as<SetConstructor>());
    case Constructor::MAP_ELEMENTS:
        return traverseMapConstructor(_pExpr->as<MapConstructor>());
    case Constructor::LIST_ELEMENTS:
        return traverseListConstructor(_pExpr->as<ListConstructor>());
    case Constructor::ARRAY_ITERATION:
        return traverseArrayIteration(_pExpr->as<ArrayIteration>());
    case Constructor::UNION_CONSTRUCTOR:
        return traverseUnionConstructor(_pExpr->as<UnionConstructor>());
    }

    return true;
}

bool Visitor::traverseStructConstructor(const std::shared_ptr<StructConstructor> &_pExpr) {
    ENTER(StructConstructor, _pExpr);
    TRAVERSE_COL(StructFieldDefinition, StructFieldDef, *_pExpr);
    EXIT();
}

bool Visitor::traverseArrayConstructor(const std::shared_ptr<ArrayConstructor> &_pExpr) {
    ENTER(ArrayConstructor, _pExpr);
    TRAVERSE_COL(ElementDefinition, ArrayElementDef, *_pExpr);
    EXIT();
}

bool Visitor::traverseSetConstructor(const std::shared_ptr<SetConstructor> &_pExpr) {
    ENTER(SetConstructor, _pExpr);
    TRAVERSE_COL(Expression, SetElementDef, *_pExpr);
    EXIT();
}

bool Visitor::traverseMapConstructor(const std::shared_ptr<MapConstructor> &_pExpr) {
    ENTER(MapConstructor, _pExpr);
    TRAVERSE_COL(ElementDefinition, MapElementDef, *_pExpr);
    EXIT();
}

bool Visitor::traverseListConstructor(const std::shared_ptr<ListConstructor> &_pExpr) {
    ENTER(ListConstructor, _pExpr);
    TRAVERSE_COL(Expression, ListElementDef, *_pExpr);
    EXIT();
}

bool Visitor::traverseArrayIteration(const std::shared_ptr<ArrayIteration> &_pExpr) {
    ENTER(ArrayIteration, _pExpr);
    TRAVERSE_COL(NamedValue, ArrayIterator, _pExpr->getIterators());
    TRAVERSE(Expression, ArrayIterationDefault, _pExpr->getDefault(), _pExpr, ArrayIteration, setDefault);
    TRAVERSE_COL(ArrayPartDefinition, ArrayIterationPart, *_pExpr);
    EXIT();
}

bool Visitor::traverseUnionConstructor(const std::shared_ptr<UnionConstructor> &_pExpr) {
    ENTER(UnionConstructor, _pExpr);
    TRAVERSE_COL(VariableDeclaration, UnionCostructorVarDecl, _pExpr->getDeclarations());
    TRAVERSE_COL(StructFieldDefinition, UnionCostructorParam, *_pExpr);
    EXIT();
}

bool Visitor::traverseCastExpr(const std::shared_ptr<CastExpr> &_pExpr) {
    ENTER(CastExpr, _pExpr);
    TRAVERSE(TypeExpr, CastToType, _pExpr->getToType(), _pExpr, CastExpr, setToType);
    TRAVERSE(Expression, CastParam, _pExpr->getExpression(), _pExpr, CastExpr, setExpression);
    EXIT();
}

// Statements.

bool Visitor::traverseStatement(const std::shared_ptr<Statement> &_pStmt) {
    switch (_pStmt->getKind()) {
    case Statement::BLOCK:
        return traverseBlock(_pStmt->as<Block>());
    case Statement::PARALLEL_BLOCK:
        return traverseParallelBlock(_pStmt->as<ParallelBlock>());
    case Statement::JUMP:
        return traverseJump(_pStmt->as<Jump>());
    case Statement::ASSIGNMENT:
        return traverseAssignment(_pStmt->as<Assignment>());
    case Statement::MULTIASSIGNMENT:
        return traverseMultiassignment(_pStmt->as<Multiassignment>());
    case Statement::CALL:
        return traverseCall(_pStmt->as<Call>());
    case Statement::SWITCH:
        return traverseSwitch(_pStmt->as<Switch>());
    case Statement::IF:
        return traverseIf(_pStmt->as<If>());
    case Statement::FOR:
        return traverseFor(_pStmt->as<For>());
    case Statement::WHILE:
        return traverseWhile(_pStmt->as<While>());
    case Statement::BREAK:
        return traverseBreak(_pStmt->as<Break>());
    case Statement::WITH:
        return traverseWith(_pStmt->as<With>());
    case Statement::RECEIVE:
        return traverseReceive(_pStmt->as<Receive>());
    case Statement::SEND:
        return traverseSend(_pStmt->as<Send>());
    case Statement::TYPE_DECLARATION:
        return traverseTypeDeclaration(_pStmt->as<TypeDeclaration>());
    case Statement::VARIABLE_DECLARATION:
        return traverseVariableDeclaration(_pStmt->as<VariableDeclaration>());
    case Statement::FORMULA_DECLARATION:
        return traverseFormulaDeclaration(_pStmt->as<FormulaDeclaration>());
    case Statement::LEMMA_DECLARATION:
        return traverseLemmaDeclaration(_pStmt->as<LemmaDeclaration>());
    case Statement::PREDICATE_DECLARATION:
        return traversePredicate(_pStmt->as<Predicate>());
    case Statement::VARIABLE_DECLARATION_GROUP:
        return traverseVariableDeclarationGroup(_pStmt->as<VariableDeclarationGroup>());
    }

    ENTER(Statement, _pStmt);
    TRAVERSE(Label, StmtLabel, _pStmt->getLabel(), _pStmt, Statement, setLabel);
    EXIT();
}

bool Visitor::traverseBlock(const std::shared_ptr<Block> &_pStmt) {
    ENTER(Block, _pStmt);
    TRAVERSE(Label, StmtLabel, _pStmt->getLabel(), _pStmt, Statement, setLabel);
    TRAVERSE_COL(Statement, Stmt, *_pStmt);
    EXIT();
}

bool Visitor::traverseParallelBlock(const std::shared_ptr<ParallelBlock> &_pStmt) {
    ENTER(ParallelBlock, _pStmt);
    TRAVERSE(Label, StmtLabel, _pStmt->getLabel(), _pStmt, Statement, setLabel);
    TRAVERSE_COL(Statement, Stmt, *_pStmt);
    EXIT();
}

bool Visitor::traverseJump(const std::shared_ptr<Jump> &_pStmt) {
    ENTER(Jump, _pStmt);
    TRAVERSE(Label, StmtLabel, _pStmt->getLabel(), _pStmt, Statement, setLabel);
    EXIT();
}

bool Visitor::traverseAssignment(const std::shared_ptr<Assignment> &_pStmt) {
    ENTER(Assignment, _pStmt);
    TRAVERSE(Label, StmtLabel, _pStmt->getLabel(), _pStmt, Statement, setLabel);
    TRAVERSE(Expression, LValue, _pStmt->getLValue(), _pStmt, Assignment, setLValue);
    TRAVERSE(Expression, RValue, _pStmt->getExpression(), _pStmt, Assignment, setExpression);
    EXIT();
}

bool Visitor::traverseMultiassignment(const std::shared_ptr<Multiassignment> &_pStmt) {
    ENTER(Multiassignment, _pStmt);
    TRAVERSE(Label, StmtLabel, _pStmt->getLabel(), _pStmt, Statement, setLabel);
    TRAVERSE_COL(Expression, LValue, _pStmt->getLValues());
    TRAVERSE_COL(Expression, RValue, _pStmt->getExpressions());
    EXIT();
}

bool Visitor::traverseCall(const std::shared_ptr<Call> &_pStmt) {
    ENTER(Call, _pStmt);
    TRAVERSE(Label, StmtLabel, _pStmt->getLabel(), _pStmt, Statement, setLabel);
    TRAVERSE(Expression, PredicateCallee, _pStmt->getPredicate(), _pStmt, Call, setPredicate);
    TRAVERSE_COL(VariableDeclaration, PredicateVarDecl, _pStmt->getDeclarations());
    TRAVERSE_COL(Expression, PredicateCallArgs, _pStmt->getArgs());

    for (size_t i = 0; i < _pStmt->getBranches().size(); ++i) {

        auto br = std::make_shared<CallBranch>(*_pStmt->getBranches().get(i));
        TRAVERSE(Statement, PredicateCallBranchHandler, br->getHandler(), br, CallBranch, setHandler);
        TRAVERSE_COL(Expression, PredicateCallBranchResults, *br);
    }

    EXIT();
}

bool Visitor::traverseSwitch(const std::shared_ptr<Switch> &_pStmt) {
    ENTER(Switch, _pStmt);
    TRAVERSE(Label, StmtLabel, _pStmt->getLabel(), _pStmt, Statement, setLabel);
    TRAVERSE(VariableDeclaration, SwitchParamDecl, _pStmt->getParamDecl(), _pStmt, Switch, setParamDecl);
    TRAVERSE(Expression, SwitchArg, _pStmt->getArg(), _pStmt, Switch, setArg);
    TRAVERSE(Statement, SwitchDefault, _pStmt->getDefault(), _pStmt, Switch, setDefault);
    TRAVERSE_COL(SwitchCase, SwitchCase, *_pStmt);
    EXIT();
}

bool Visitor::traverseIf(const std::shared_ptr<If> &_pStmt) {
    ENTER(If, _pStmt);
    TRAVERSE(Label, StmtLabel, _pStmt->getLabel(), _pStmt, Statement, setLabel);
    TRAVERSE(Expression, IfArg, _pStmt->getArg(), _pStmt, If, setArg);
    TRAVERSE(Statement, IfBody, _pStmt->getBody(), _pStmt, If, setBody);
    TRAVERSE(Statement, IfElse, _pStmt->getElse(), _pStmt, If, setElse);
    EXIT();
}

bool Visitor::traverseFor(const std::shared_ptr<For> &_pStmt) {
    ENTER(For, _pStmt);
    TRAVERSE(Label, StmtLabel, _pStmt->getLabel(), _pStmt, Statement, setLabel);
    TRAVERSE(VariableDeclaration, ForIterator, _pStmt->getIterator(), _pStmt, For, setIterator);
    TRAVERSE(Expression, ForInvariant, _pStmt->getInvariant(), _pStmt, For, setInvariant);
    TRAVERSE(Statement, ForIncrement, _pStmt->getIncrement(), _pStmt, For, setIncrement);
    TRAVERSE(Statement, ForBody, _pStmt->getBody(), _pStmt, For, setBody);
    EXIT();
}

bool Visitor::traverseWhile(const std::shared_ptr<While> &_pStmt) {
    ENTER(While, _pStmt);
    TRAVERSE(Label, StmtLabel, _pStmt->getLabel(), _pStmt, Statement, setLabel);
    TRAVERSE(Expression, WhileInvariant, _pStmt->getInvariant(), _pStmt, While, setInvariant);
    TRAVERSE(Statement, WhileBody, _pStmt->getBody(), _pStmt, While, setBody);
    EXIT();
}

bool Visitor::traverseBreak(const std::shared_ptr<Break> &_pStmt) {
    ENTER(Break, _pStmt);
    TRAVERSE(Label, StmtLabel, _pStmt->getLabel(), _pStmt, Statement, setLabel);
    EXIT();
}

bool Visitor::traverseWith(const std::shared_ptr<With> &_pStmt) {
    ENTER(With, _pStmt);
    TRAVERSE(Label, StmtLabel, _pStmt->getLabel(), _pStmt, Statement, setLabel);
    TRAVERSE_COL(Expression, WithArg, _pStmt->getArgs());
    TRAVERSE(Statement, WithBody, _pStmt->getBody(), _pStmt, With, setBody);
    EXIT();
}

bool Visitor::traverseReceive(const std::shared_ptr<Receive> &_pStmt) {
    ENTER(Receive, _pStmt);
    TRAVERSE(Label, StmtLabel, _pStmt->getLabel(), _pStmt, Statement, setLabel);
    TRAVERSE(Expression, ReceiveTimeout, _pStmt->getTimeout(), _pStmt, Receive, setTimeout);
    TRAVERSE(Statement, ReceiveTimeoutHandler, _pStmt->getTimeoutHandler(), _pStmt, Receive, setTimeoutHandler);
    TRAVERSE_COL(MessageHandler, ReceiveHandler, *_pStmt);
    EXIT();
}

bool Visitor::traverseSend(const std::shared_ptr<Send> &_pStmt) {
    ENTER(Send, _pStmt);
    TRAVERSE(Label, StmtLabel, _pStmt->getLabel(), _pStmt, Statement, setLabel);
    TRAVERSE_COL(Expression, SendArgs, _pStmt->getArgs());
    EXIT();
}

bool Visitor::traverseTypeDeclaration(const std::shared_ptr<TypeDeclaration> &_pStmt) {
    ENTER(TypeDeclaration, _pStmt);
    TRAVERSE(Label, StmtLabel, _pStmt->getLabel(), _pStmt, Statement, setLabel);
    TRAVERSE(Type, TypeDeclBody, _pStmt->getType(), _pStmt, TypeDeclaration, setType);
    EXIT();
}

bool Visitor::traverseVariableDeclaration(const std::shared_ptr<VariableDeclaration> &_pStmt) {
    ENTER(VariableDeclaration, _pStmt);
    TRAVERSE(Label, StmtLabel, _pStmt->getLabel(), _pStmt, Statement, setLabel);
    TRAVERSE(Variable, VarDeclVar, _pStmt->getVariable(), _pStmt, VariableDeclaration, setVariable);
    TRAVERSE(Expression, VarDeclInit, _pStmt->getValue(), _pStmt, VariableDeclaration, setValue);
    EXIT();
}

bool Visitor::traverseFormulaDeclaration(const std::shared_ptr<FormulaDeclaration> &_pStmt) {
    ENTER(FormulaDeclaration, _pStmt);
    TRAVERSE(Label, StmtLabel, _pStmt->getLabel(), _pStmt, Statement, setLabel);
    TRAVERSE_COL(NamedValue, FormulaDeclParams, _pStmt->getParams());
    TRAVERSE(Type, FormulaDeclResultType, _pStmt->getResultType(), _pStmt, FormulaDeclaration, setResultType);
    TRAVERSE(Expression, FormulaDeclBody, _pStmt->getFormula(), _pStmt, FormulaDeclaration, setFormula);
    TRAVERSE(Expression, FormulaDeclMeasure, _pStmt->getMeasure(), _pStmt, FormulaDeclaration, setMeasure);
    EXIT();
}

bool Visitor::traverseLemmaDeclaration(const std::shared_ptr<LemmaDeclaration> &_pStmt) {
    ENTER(LemmaDeclaration, _pStmt);
    TRAVERSE(Label, StmtLabel, _pStmt->getLabel(), _pStmt, Statement, setLabel);
    TRAVERSE(Expression, LemmaDeclBody, _pStmt->getProposition(), _pStmt, LemmaDeclaration, setProposition);
    EXIT();
}

bool Visitor::traversePredicate(const std::shared_ptr<Predicate> &_pStmt) {
    ENTER(Predicate, _pStmt);
    TRAVERSE(Label, StmtLabel, _pStmt->getLabel(), _pStmt, Statement, setLabel);

    if (!_traverseAnonymousPredicate(_pStmt))
        return false;

    EXIT();
}

bool Visitor::traverseVariableDeclarationGroup(const std::shared_ptr<VariableDeclarationGroup> &_pStmt) {
    ENTER(VariableDeclarationGroup, _pStmt);
    TRAVERSE(Label, StmtLabel, _pStmt->getLabel(), _pStmt, Statement, setLabel);
    TRAVERSE_COL(VariableDeclaration, VarDeclGroupElement, *_pStmt);
    EXIT();
}

// Misc.

bool Visitor::traverseUnionConstructorDeclaration(const std::shared_ptr<UnionConstructorDeclaration> &_pCons) {
    ENTER(UnionConstructorDeclaration, _pCons);
    TRAVERSE(Type, UnionConsFields, _pCons->getFields(), _pCons, UnionConstructorDeclaration, setFields);
    EXIT();
}

bool Visitor::traverseStructFieldDefinition(const std::shared_ptr<StructFieldDefinition> &_pCons) {
    ENTER(StructFieldDefinition, _pCons);
    TRAVERSE(Expression, StructFieldValue, _pCons->getValue(), _pCons, StructFieldDefinition, setValue);
    EXIT();
}

bool Visitor::traverseElementDefinition(const std::shared_ptr<ElementDefinition> &_pCons) {
    ENTER(ElementDefinition, _pCons);
    TRAVERSE(Expression, ElementIndex, _pCons->getIndex(), _pCons, ElementDefinition, setIndex);
    TRAVERSE(Expression, ElementValue, _pCons->getValue(), _pCons, ElementDefinition, setValue);
    EXIT();
}

bool Visitor::traverseArrayPartDefinition(const std::shared_ptr<ArrayPartDefinition> &_pCons) {
    ENTER(ArrayPartDefinition, _pCons);
    TRAVERSE_COL(Expression, ArrayPartCond, _pCons->getConditions());
    TRAVERSE(Expression, ArrayPartValue, _pCons->getExpression(), _pCons, ArrayPartDefinition, setExpression);
    EXIT();
}

bool Visitor::traverseLabel(const std::shared_ptr<Label> &_pLabel) {
    ENTER(Label, _pLabel);
    EXIT();
}

bool Visitor::traverseSwitchCase(const std::shared_ptr<SwitchCase> &_pCase) {
    ENTER(SwitchCase, _pCase);
    TRAVERSE_COL(Expression, SwitchCaseValue, _pCase->getExpressions());
    TRAVERSE(Statement, SwitchCaseBody, _pCase->getBody(), _pCase, SwitchCase, setBody);
    EXIT();
}

bool Visitor::_traverseAnonymousPredicate(const std::shared_ptr<AnonymousPredicate> &_pDecl) {
    TRAVERSE_COL(Param, PredicateInParam, _pDecl->getInParams());

    for (size_t i = 0; i < _pDecl->getOutParams().size(); ++i) {
        auto br = std::make_shared<Branch>(*_pDecl->getOutParams().get(i));

        TRAVERSE(Label, PredicateBranchLabel, br->getLabel(), br, Branch, setLabel);
        TRAVERSE(Formula, PredicateBranchPreCondition, br->getPreCondition(), br, Branch, setPreCondition);
        TRAVERSE(Formula, PredicateBranchPostCondition, br->getPostCondition(), br, Branch, setPostCondition);
        TRAVERSE_COL(Param, PredicateOutParam, *br);
    }

    TRAVERSE(Formula, PredicatePreCondition, _pDecl->getPreCondition(), _pDecl, AnonymousPredicate, setPreCondition);
    TRAVERSE(Formula, PredicatePostCondition, _pDecl->getPostCondition(), _pDecl, AnonymousPredicate, setPostCondition);
    TRAVERSE(Expression, PredicateMeasure, _pDecl->getMeasure(), _pDecl, AnonymousPredicate, setMeasure);
    TRAVERSE(Block, PredicateBody, _pDecl->getBlock(), _pDecl, AnonymousPredicate, setBlock);

    return true;
}

bool Visitor::_traverseDeclarationGroup(const std::shared_ptr<DeclarationGroup> &_pDecl) {
    TRAVERSE_COL(Predicate, PredicateDecl, _pDecl->getPredicates());
    TRAVERSE_COL(TypeDeclaration, TypeDecl, _pDecl->getTypes());
    TRAVERSE_COL(VariableDeclaration, VarDecl, _pDecl->getVariables());
    TRAVERSE_COL(FormulaDeclaration, FormulaDecl, _pDecl->getFormulas());
    TRAVERSE_COL(LemmaDeclaration, LemmaDecl, _pDecl->getLemmas());
    TRAVERSE_COL(Message, MessageDecl, _pDecl->getMessages());
    TRAVERSE_COL(Process, ProcessDecl, _pDecl->getProcesses());
    return true;
}

bool Visitor::traverseDeclarationGroup(const std::shared_ptr<DeclarationGroup> &_pType) {
    return true;
}

bool Visitor::traverseModule(const std::shared_ptr<Module> &_pModule) {
    ENTER(Module, _pModule);

    TRAVERSE_COL(Param, ModuleParam, _pModule->getParams());
    TRAVERSE_COL(Module, ModuleDecl, _pModule->getModules());

    if (!_traverseDeclarationGroup(_pModule))
        return false;

    TRAVERSE_COL(Class, ClassDecl, _pModule->getClasses());

    EXIT();
}

bool Visitor::traverseClass(const std::shared_ptr<Class> &_pClass) {
    ENTER(Class, _pClass);

    if (!_traverseDeclarationGroup(_pClass))
        return false;

    EXIT();
}

bool Visitor::traverseMessage(const std::shared_ptr<Message> &_pMessage) {
    ENTER(Message, _pMessage);
    TRAVERSE_COL(Param, MessageParam, _pMessage->getParams());
    EXIT();
}

bool Visitor::traverseProcess(const std::shared_ptr<Process> &_pProcess) {
    ENTER(Process, _pProcess);
    TRAVERSE_COL(Param, ProcessInParam, _pProcess->getInParams());

    for (size_t i = 0; i < _pProcess->getOutParams().size(); ++i) {
        auto br = std::make_shared<Branch>(*_pProcess->getOutParams().get(i));

        TRAVERSE(Label, ProcessBranchLabel, br->getLabel(), br, Branch, setLabel);
        TRAVERSE(Formula, ProcessBranchPreCondition, br->getPreCondition(), br, Branch, setPreCondition);
        TRAVERSE(Formula, ProcessBranchPostCondition, br->getPostCondition(), br, Branch, setPostCondition);
        TRAVERSE_COL(Param, ProcessOutParam, *br);
    }

    TRAVERSE(Block, ProcessBody, _pProcess->getBlock(), _pProcess, Process, setBlock);
    EXIT();
}

bool Visitor::traverseMessageHandler(const std::shared_ptr<MessageHandler> &_pMessageHandler) {
    ENTER(MessageHandler, _pMessageHandler);
    TRAVERSE(Statement, MessageHandlerBody, _pMessageHandler->getBody(), _pMessageHandler, MessageHandler, setBody);
    EXIT();
}
