/// \file collect_constraints.cpp
///

#include <iostream>
#include <stack>

#include <assert.h>

#include "typecheck.h"
#include "prettyprinter.h"

#include <ir/statements.h>
#include <ir/visitor.h>

using namespace ir;

class Collector : public Visitor {
public:
    Collector(tc::Formulas & _constraints, Context & _ctx, tc::FreshTypes & _types);
    virtual ~Collector() {}

    virtual bool visitVariableReference(VariableReference &_var);
    virtual bool visitPredicateReference(PredicateReference &_ref);
    virtual bool visitFunctionCall(FunctionCall &_call);
    virtual bool visitCall(Call &_call);
    virtual bool visitLiteral(Literal &_lit);
    virtual bool visitUnary(Unary &_unary);
    virtual bool visitBinary(Binary &_binary);
    virtual bool visitStructConstructor(StructConstructor &_cons);
    virtual bool visitUnionConstructor(UnionConstructor &_cons);
    virtual bool visitListConstructor(ListConstructor &_cons);
    virtual bool visitSetConstructor(SetConstructor &_cons);
    virtual bool visitArrayConstructor(ArrayConstructor &_cons);
    virtual bool visitMapConstructor(MapConstructor &_cons);
    virtual bool visitFieldExpr(FieldExpr &_field);
    virtual bool visitCastExpr(CastExpr &_cast);
    virtual bool visitAssignment(Assignment &_assignment);
    virtual bool visitVariableDeclaration(VariableDeclaration &_var);
    virtual bool visitIf(If &_if);

    virtual bool visitTypeExpr(TypeExpr &_expr);

    virtual bool visitNamedReferenceType(NamedReferenceType &_type);

    virtual int handlePredicateInParam(Node &_node);
    virtual int handlePredicateOutParam(Node &_node);
    virtual int handleParameterizedTypeParam(Node &_node);
    virtual int handleVarDeclVar(Node &_node);
    virtual int handleSwitchCaseValuePost(Node &_node);

    virtual bool traverseSwitch(Switch &_stmt);

protected:
    template<typename _Node>
    tc::FreshTypePtr createFreshGeneric(const _Node *_pParam);

    // Need these wrappers since e.g. Binary::setType() is actually Expression::setType() but
    // _pParam won't be implicitly downcast to Expression*.
    tc::FreshTypePtr createFresh(const Expression *_pParam) { return createFreshGeneric(_pParam); }
    tc::FreshTypePtr createFresh(const NamedValue *_pParam) { return createFreshGeneric(_pParam); }
    tc::FreshTypePtr createFresh(const DerivedType *_pParam);
    tc::FreshTypePtr createFresh(const DerivedTypePtr &_pParam) { return createFresh(_pParam.ptr()); }
    tc::FreshTypePtr createFreshIndex(const MapTypePtr &_pParam);

    template<typename _Node>
    void setFreshType(const Auto<_Node> &_pParam, const TypePtr &_pType);

    tc::FreshTypePtr getKnownType(const TypePtr &_pType);

    void collectParam(const NamedValuePtr &_pParam, int _nFlags);

private:
    tc::Formulas & m_constraints;
    Context & m_ctx;
    tc::FreshTypes & m_types;
    std::stack<SwitchPtr> m_switches;
};

Collector::Collector(tc::Formulas & _constraints, Context & _ctx, tc::FreshTypes & _types)
    : Visitor(CHILDREN_FIRST), m_constraints(_constraints), m_ctx(_ctx), m_types(_types)
{
}

template<typename _Node>
void Collector::setFreshType(const Auto<_Node> &_pParam, const TypePtr &_pType) {
    _pParam->setType(_pType);
    if (_pType->getKind() == Type::FRESH)
        m_types.insert(std::make_pair(_pType.as<tc::FreshType>(), tc::createTypeSetter<_Node>(_pParam)));
}

tc::FreshTypePtr Collector::getKnownType(const TypePtr &_pType) {
    if (_pType && _pType->getKind() == Type::NAMED_REFERENCE)
        if (TypePtr pType = _pType.as<NamedReferenceType>()->getDeclaration()->getType())
            if (pType->getKind() == Type::FRESH)
                return pType.as<tc::FreshType>();

    return NULL;
}

template<typename _Node>
tc::FreshTypePtr Collector::createFreshGeneric(const _Node *_pParam) {
    tc::FreshTypePtr pFresh = getKnownType(_pParam->getType());

    if (!pFresh)
        pFresh = new tc::FreshType();

    m_types.insert(std::make_pair(pFresh, tc::createTypeSetter(ref(_pParam))));

    return pFresh;
}

tc::FreshTypePtr Collector::createFresh(const DerivedType *_pParam) {
    tc::FreshTypePtr pType = new tc::FreshType();
    m_types.insert(std::make_pair(pType, tc::createBaseTypeSetter(ref(_pParam))));
    return pType;
}

tc::FreshTypePtr Collector::createFreshIndex(const MapTypePtr &_pParam) {
    tc::FreshTypePtr pType = new tc::FreshType();
    m_types.insert(std::make_pair(pType, tc::createIndexTypeSetter(_pParam)));
    return pType;
}

void Collector::collectParam(const NamedValuePtr &_pParam, int _nFlags) {
    TypePtr pType = _pParam->getType();

    if (pType->getKind() == Type::TYPE) {
        TypeTypePtr pTypeType = pType.as<TypeType>();
        pTypeType->getDeclaration()->setType(new tc::FreshType());
        return;
    }

    tc::FreshTypePtr pFresh = createFresh(_pParam.ptr());

    assert(pType);

    if (pType && pType->getKind() != Type::GENERIC && getKnownType(pType))
        m_constraints.insert(new tc::Formula(tc::Formula::EQUALS, pFresh, pType));

    _pParam->setType(pFresh);
    pFresh->addFlags(_nFlags);
}

bool Collector::visitVariableReference(VariableReference &_var) {
    setFreshType<Expression>(&_var, _var.getTarget()->getType());
    return true;
}

bool Collector::visitPredicateReference(PredicateReference &_ref) {
    Predicates funcs;

    if (! m_ctx.getPredicates(_ref.getName(), funcs))
        assert(false);

    _ref.setType(createFresh(&_ref));

    if (funcs.size() > 1) {
        tc::CompoundFormulaPtr pConstraint = new tc::CompoundFormula();

        for (size_t i = 0; i < funcs.size(); ++ i) {
            PredicatePtr pPred = funcs.get(i);
            tc::Formulas & part = pConstraint->addPart();

            part.insert(new tc::Formula(tc::Formula::EQUALS, pPred->getType(), _ref.getType()));
        }

        m_constraints.insert(pConstraint);
    } else {
        m_constraints.insert(new tc::Formula(tc::Formula::EQUALS, funcs.get(0)->getType(), _ref.getType()));
    }

    return true;
}

bool Collector::visitFunctionCall(FunctionCall &_call) {
    PredicateTypePtr pType = new PredicateType();

    _call.setType(createFresh(&_call));
    pType->getOutParams().add(new Branch());
    pType->getOutParams().get(0)->add(new Param(L"", _call.getType()));

    for (size_t i = 0; i < _call.getArgs().size(); ++ i)
        pType->getInParams().add(new Param(L"", _call.getArgs().get(i)->getType()));

    m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
            _call.getPredicate()->getType(), pType));
//    m_constraints.insert(new tc::Formula(tc::Formula::Equals,
//            _call.getPredicate()->getType(), pType));

    return true;
}

bool Collector::visitCall(Call &_call) {
    PredicateTypePtr pType = new PredicateType();

    for (size_t i = 0; i < _call.getArgs().size(); ++i)
        pType->getInParams().add(new Param(L"", _call.getArgs().get(i)->getType()));

    for (size_t i = 0; i < _call.getBranches().size(); ++i) {
        CallBranch &br = *_call.getBranches().get(i);

        pType->getOutParams().add(new Branch());

        for (size_t j = 0; j < br.size(); ++j)
            pType->getOutParams().get(i)->add(new Param(L"", br.get(j)->getType()));
    }

    m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
            _call.getPredicate()->getType(), pType));

    return true;
}

bool Collector::visitLiteral(Literal &_lit) {
    switch (_lit.getLiteralKind()) {
        case Literal::UNIT:
            _lit.setType(new Type(Type::UNIT));
            break;
        case Literal::NUMBER: {
            const Number &n = _lit.getNumber();

            if (n.isNat())
                _lit.setType(new Type(Type::NAT, n.countBits(false)));
            else if (n.isInt())
                _lit.setType(new Type(Type::INT, n.countBits(true)));
            else
                _lit.setType(new Type(Type::REAL, n.countBits(false)));

            break;
        }
        case Literal::BOOL:
            _lit.setType(new Type(Type::BOOL));
            break;
        case Literal::CHAR:
            _lit.setType(new Type(Type::CHAR));
            break;
        case Literal::STRING:
            _lit.setType(new Type(Type::STRING));
            break;
    }

    return true;
}

bool Collector::visitUnary(Unary &_unary) {
    _unary.setType(createFresh(&_unary));

    switch (_unary.getOperator()) {
        case Unary::PLUS:
        case Unary::MINUS:
            m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                    _unary.getExpression()->getType(), _unary.getType()));
            m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                    _unary.getExpression()->getType(), new Type(Type::REAL, Number::GENERIC)));
            m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                    _unary.getType(), new Type(Type::REAL, Number::GENERIC)));
            break;
        case Unary::BOOL_NEGATE:
            //               ! x : A = y : B
            // ------------------------------------------------
            // (A = bool and B = bool) or (A = {C} and B = {C})
            {
                tc::CompoundFormulaPtr p = new tc::CompoundFormula();

                // Boolean negation.
                tc::Formulas &part1 = p->addPart();

                part1.insert(new tc::Formula(tc::Formula::EQUALS,
                        _unary.getExpression()->getType(), new Type(Type::BOOL)));
                part1.insert(new tc::Formula(tc::Formula::EQUALS,
                        _unary.getType(), new Type(Type::BOOL)));

                // Set negation.
                tc::Formulas &part2 = p->addPart();
                SetTypePtr pSet = new SetType(NULL);

                pSet->setBaseType(createFresh(pSet.as<DerivedType>()));
                part2.insert(new tc::Formula(tc::Formula::EQUALS,
                        _unary.getExpression()->getType(), pSet));
                part2.insert(new tc::Formula(tc::Formula::EQUALS,
                        _unary.getType(), pSet));

                m_constraints.insert(p);
            }

            break;
        case Unary::BITWISE_NEGATE:
            m_constraints.insert(new tc::Formula(tc::Formula::EQUALS,
                    _unary.getExpression()->getType(), _unary.getType()));
            m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                    _unary.getExpression()->getType(), new Type(Type::INT, Number::GENERIC)));
    }

    return true;
}

bool Collector::visitBinary(Binary &_binary) {
    _binary.setType(createFresh(&_binary));

    switch (_binary.getOperator()) {
        case Binary::ADD:
        case Binary::SUBTRACT:
            //                       x : A + y : B = z :C
            // ----------------------------------------------------------------
            // (A <= B and C = B and A <= real and B <= real and C <= real)
            //   or (B < A and C = A and A <= real and B <= real and C <= real)
            //   or (A <= C and B <= C and C = {D})
            //   or (A <= C and B <= C and C = [[D]])   /* Operator "+" only. */
            {
                tc::CompoundFormulaPtr p = new tc::CompoundFormula();
                tc::Formulas & part1 = p->addPart();
                tc::Formulas & part2 = p->addPart();

                // Numeric operations.
                part1.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getLeftSide()->getType(),
                        _binary.getRightSide()->getType()));
                part1.insert(new tc::Formula(tc::Formula::EQUALS,
                        _binary.getType(),
                        _binary.getRightSide()->getType()));
                part1.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getLeftSide()->getType(),
                        new Type(Type::REAL, Number::GENERIC)));
                part1.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getRightSide()->getType(),
                        new Type(Type::REAL, Number::GENERIC)));
                part1.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getType(),
                        new Type(Type::REAL, Number::GENERIC)));
                part2.insert(new tc::Formula(tc::Formula::SUBTYPE_STRICT,
                        _binary.getRightSide()->getType(),
                        _binary.getLeftSide()->getType()));
                part2.insert(new tc::Formula(tc::Formula::EQUALS,
                        _binary.getType(),
                        _binary.getLeftSide()->getType()));
                part2.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getLeftSide()->getType(),
                        new Type(Type::REAL, Number::GENERIC)));
                part2.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getRightSide()->getType(),
                        new Type(Type::REAL, Number::GENERIC)));
                part2.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getType(),
                        new Type(Type::REAL, Number::GENERIC)));

                // Set operations.
                tc::Formulas & part3 = p->addPart();
                SetTypePtr pSet = new SetType(NULL);

                pSet->setBaseType(createFresh(pSet.as<DerivedType>()));

                part3.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getLeftSide()->getType(),
                        _binary.getType()));
                part3.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getRightSide()->getType(),
                        _binary.getType()));
                part3.insert(new tc::Formula(tc::Formula::EQUALS,
                        _binary.getType(), pSet));

                if (_binary.getOperator() == Binary::ADD) {
                    tc::Formulas & part = p->addPart();
                    ListTypePtr pList = new ListType(NULL);

                    pList->setBaseType(createFresh(pList.as<DerivedType>()));

                    part.insert(new tc::Formula(tc::Formula::SUBTYPE,
                            _binary.getLeftSide()->getType(),
                            _binary.getType()));
                    part.insert(new tc::Formula(tc::Formula::SUBTYPE,
                            _binary.getRightSide()->getType(),
                            _binary.getType()));
                    part.insert(new tc::Formula(tc::Formula::EQUALS,
                            _binary.getType(), pList));
                }

                m_constraints.insert(p);
            }
            break;

        case Binary::MULTIPLY:
        case Binary::DIVIDE:
        case Binary::REMAINDER:
        case Binary::POWER:
            //         x : A * y : B = z :C
            // -----------------------------------
            // (A <= B and C = B) or (B < A and C = A)
            {
                tc::CompoundFormulaPtr p = new tc::CompoundFormula();
                tc::Formulas & part1 = p->addPart();
                tc::Formulas & part2 = p->addPart();

                part1.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getLeftSide()->getType(),
                        _binary.getRightSide()->getType()));
                part1.insert(new tc::Formula(tc::Formula::EQUALS,
                        _binary.getType(),
                        _binary.getRightSide()->getType()));
                part2.insert(new tc::Formula(tc::Formula::SUBTYPE_STRICT,
                        _binary.getRightSide()->getType(),
                        _binary.getLeftSide()->getType()));
                part2.insert(new tc::Formula(tc::Formula::EQUALS,
                        _binary.getType(),
                        _binary.getLeftSide()->getType()));
                m_constraints.insert(p);
            }

            // Support only numbers for now.
            m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                    _binary.getLeftSide()->getType(), new Type(Type::REAL, Number::GENERIC)));
            m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                    _binary.getRightSide()->getType(), new Type(Type::REAL, Number::GENERIC)));
            m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                    _binary.getType(), new Type(Type::REAL, Number::GENERIC)));

            break;

        case Binary::SHIFT_LEFT:
        case Binary::SHIFT_RIGHT:
            m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                    _binary.getLeftSide()->getType(), new Type(Type::INT, Number::GENERIC)));
            m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                    _binary.getRightSide()->getType(), new Type(Type::INT, Number::GENERIC)));
            m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                    _binary.getLeftSide()->getType(), _binary.getType()));
            break;

        case Binary::LESS:
        case Binary::LESS_OR_EQUALS:
        case Binary::GREATER:
        case Binary::GREATER_OR_EQUALS:
            m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                    _binary.getLeftSide()->getType(), new Type(Type::REAL, Number::GENERIC)));
            m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                    _binary.getRightSide()->getType(), new Type(Type::REAL, Number::GENERIC)));
            // no break;
        case Binary::EQUALS:
        case Binary::NOT_EQUALS:
            //       (x : A = y : B) : C
            // ----------------------------------
            //   C = bool and (A <= B or B < A)
            m_constraints.insert(new tc::Formula(tc::Formula::EQUALS,
                    _binary.getType(), new Type(Type::BOOL)));

            {
                tc::CompoundFormulaPtr p = new tc::CompoundFormula();

                p->addPart().insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getLeftSide()->getType(),
                        _binary.getRightSide()->getType()));
                p->addPart().insert(new tc::Formula(tc::Formula::SUBTYPE_STRICT,
                        _binary.getRightSide()->getType(),
                        _binary.getLeftSide()->getType()));

                //_binary.h

                m_constraints.insert(p);
            }
            break;

        case Binary::BOOL_AND:
        case Binary::BITWISE_AND:
        case Binary::BOOL_OR:
        case Binary::BITWISE_OR:
        case Binary::BOOL_XOR:
        case Binary::BITWISE_XOR:
            //       (x : A and y : B) : C
            // ----------------------------------
            //   (C = bool and A = bool and B = bool)
            //     or (A <= B and C = B and B <= int)      (bitwise AND)
            //     or (B < A and C = A and A <= int)
            //     or (A <= C and B <= C and C = {D})      (set operations)
            {
                tc::CompoundFormulaPtr p = new tc::CompoundFormula();
                tc::Formulas & part1 = p->addPart();
                tc::Formulas & part2 = p->addPart();
                tc::Formulas & part3 = p->addPart();

                part1.insert(new tc::Formula(tc::Formula::EQUALS,
                        _binary.getLeftSide()->getType(), new Type(Type::BOOL)));
                part1.insert(new tc::Formula(tc::Formula::EQUALS,
                        _binary.getRightSide()->getType(), new Type(Type::BOOL)));
                part1.insert(new tc::Formula(tc::Formula::EQUALS,
                        _binary.getType(), new Type(Type::BOOL)));

                part2.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getLeftSide()->getType(),
                        _binary.getRightSide()->getType()));
                part2.insert(new tc::Formula(tc::Formula::EQUALS,
                        _binary.getType(), _binary.getRightSide()->getType()));
                part2.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getRightSide()->getType(),
                        new Type(Type::INT, Number::GENERIC)));

                part3.insert(new tc::Formula(tc::Formula::SUBTYPE_STRICT,
                        _binary.getRightSide()->getType(),
                        _binary.getLeftSide()->getType()));
                part3.insert(new tc::Formula(tc::Formula::EQUALS,
                        _binary.getType(),
                        _binary.getLeftSide()->getType()));
                part3.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getLeftSide()->getType(),
                        new Type(Type::INT, Number::GENERIC)));

                // Set operations.
                tc::Formulas & part4 = p->addPart();
                SetTypePtr pSet = new SetType(NULL);

                pSet->setBaseType(createFresh(pSet.as<DerivedType>()));

                part4.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getLeftSide()->getType(),
                        _binary.getType()));
                part4.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getRightSide()->getType(),
                        _binary.getType()));
                part4.insert(new tc::Formula(tc::Formula::EQUALS,
                        _binary.getType(), pSet));

                m_constraints.insert(p);
            }
            break;

        case Binary::IMPLIES:
        case Binary::IFF:
            m_constraints.insert(new tc::Formula(tc::Formula::EQUALS,
                    _binary.getLeftSide()->getType(), new Type(Type::BOOL)));
            m_constraints.insert(new tc::Formula(tc::Formula::EQUALS,
                    _binary.getRightSide()->getType(), new Type(Type::BOOL)));
            m_constraints.insert(new tc::Formula(tc::Formula::EQUALS,
                    _binary.getType(), new Type(Type::BOOL)));
            break;

        case Binary::IN:
            {
                SetTypePtr pSet = new SetType(NULL);

                pSet->setBaseType(createFresh(pSet.as<DerivedType>()));
                m_constraints.insert(new tc::Formula(tc::Formula::EQUALS,
                        _binary.getRightSide()->getType(), pSet));
                m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getLeftSide()->getType(), pSet->getBaseType()));
                m_constraints.insert(new tc::Formula(tc::Formula::EQUALS,
                        _binary.getType(), new Type(Type::BOOL)));
            }
            break;
    }

    return true;
}

bool Collector::visitStructConstructor(StructConstructor &_cons) {
    StructTypePtr pStruct = new StructType();

    for (size_t i = 0; i < _cons.size(); ++i) {
        StructFieldDefinitionPtr pDef = _cons.get(i);
        NamedValuePtr pField = new NamedValue(pDef->getName());

        setFreshType(pField, pDef->getValue()->getType());
        if (pDef->getName().empty())
            pStruct->getTypesOrd().add(pField);
        else
            pStruct->getNamesSet().add(pField);
        pDef->setField(pField);
    }

    assert(pStruct->getNamesSet().empty() || pStruct->getTypesOrd().empty());

    _cons.setType(pStruct);

    return true;
}

bool Collector::visitUnionConstructor(UnionConstructor &_cons) {
    UnionTypePtr pUnion = new UnionType();
    UnionConstructorDeclarationPtr pCons = new UnionConstructorDeclaration(_cons.getName());

    pUnion->getConstructors().add(pCons);

    for (size_t i = 0; i < _cons.size(); ++i) {
        StructFieldDefinitionPtr pDef = _cons.get(i);
        NamedValuePtr pField = new NamedValue(pDef->getName());

        setFreshType(pField, pDef->getValue()->getType());
        pCons->getFields().add(pField);
        pDef->setField(pField);
    }

    if (_cons.getType())
        m_constraints.insert(new tc::Formula(tc::Formula::EQUALS, pUnion, _cons.getType()));

    _cons.setType(pUnion);

    return true;
}

bool Collector::visitSetConstructor(SetConstructor &_cons) {
    SetTypePtr pSet = new SetType(NULL);

    pSet->setBaseType(createFresh(pSet.as<DerivedType>()));
    _cons.setType(pSet);

    for (size_t i = 0; i < _cons.size(); ++i)
        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                _cons.get(i)->getType(), pSet->getBaseType()));

    return true;
}

bool Collector::visitListConstructor(ListConstructor &_cons) {
    ListTypePtr pList = new ListType(NULL);

    pList->setBaseType(createFresh(pList.as<DerivedType>()));
    _cons.setType(pList);

    for (size_t i = 0; i < _cons.size(); ++i)
        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                _cons.get(i)->getType(), pList->getBaseType()));

    return true;
}

bool Collector::visitArrayConstructor(ArrayConstructor &_cons) {
    ArrayTypePtr pArray = new ArrayType(NULL);

    pArray->setBaseType(createFresh(pArray.as<DerivedType>()));
    _cons.setType(pArray);

    for (size_t i = 0; i < _cons.size(); ++i)
        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                _cons.get(i)->getValue()->getType(), pArray->getBaseType()));

    return true;
}

bool Collector::visitMapConstructor(MapConstructor &_cons) {
    MapTypePtr pMap = new MapType(NULL, NULL);

    pMap->setBaseType(createFresh(pMap.as<DerivedType>()));
    pMap->setIndexType(createFreshIndex(pMap));
    _cons.setType(pMap);

    for (size_t i = 0; i < _cons.size(); ++i) {
        ElementDefinitionPtr pElement = _cons.get(i);

        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                pElement->getIndex()->getType(), pMap->getIndexType()));
        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                pElement->getValue()->getType(), pMap->getBaseType()));
    }

    return true;
}

bool Collector::visitFieldExpr(FieldExpr &_field) {
    tc::FreshTypePtr pFresh = createFresh(&_field);
    ir::StructTypePtr pStruct = new StructType();
    ir::NamedValuePtr pField = new NamedValue(_field.getFieldName(), pFresh);

    _field.setType(pFresh);
    pStruct->getNamesSet().add(pField);

    if (_field.getObject()->getType()->getKind() == Type::FRESH)
        pFresh->setFlags(_field.getObject()->getType().as<tc::FreshType>()->getFlags());

    // (x : A).foo : B  |-  A <= struct(B foo)
    m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
            _field.getObject()->getType(), pStruct));
    m_constraints.insert(new tc::Formula(tc::Formula::EQUALS,
            _field.getType(), pField->getType()));

    return true;
}

bool Collector::visitCastExpr(CastExpr &_cast) {
    TypePtr pToType = (TypePtr)_cast.getToType()->getContents();

    _cast.setType(pToType);

    if (pToType->getKind() == Type::STRUCT &&
            _cast.getExpression()->getKind() == Expression::CONSTRUCTOR &&
            _cast.getExpression().as<Constructor>()->getConstructorKind() == Constructor::STRUCT_FIELDS)
    {
        // We can cast form tuples to structs explicitly.
        StructTypePtr pStruct = pToType.as<StructType>();
        StructConstructorPtr pFields = _cast.getExpression().as<StructConstructor>();
        bool bSuccess = true;

        // TODO: maybe use default values for fields.
        if (pStruct->getNamesOrd().size() == pFields->size()) {
            for (size_t i = 0; i < pFields->size(); ++i) {
                StructFieldDefinitionPtr pDef = pFields->get(i);
                size_t cOtherIdx = pDef->getName().empty() ? i : pStruct->getNamesOrd().findByNameIdx(pDef->getName());

                if (cOtherIdx == (size_t)-1) {
                    bSuccess = false;
                    break;
                }

                NamedValuePtr pField = pStruct->getNamesOrd().get(cOtherIdx);

                m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        pDef->getValue()->getType(), pField->getType()));
            }
        } else if (pStruct->getTypesOrd().size() == pFields->size()) {
            for (size_t i = 0; i < pFields->size(); ++i) {
                StructFieldDefinitionPtr pDef = pFields->get(i);
                NamedValuePtr pField = pStruct->getTypesOrd().get(i);

                m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        pDef->getValue()->getType(), pField->getType()));
            }
        }

        if (bSuccess)
            return true;
    }

    // (A)(x : B) |- B <= A
    m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
            _cast.getExpression()->getType(), pToType));

    return true;
}

bool Collector::visitAssignment(Assignment &_assignment) {
    // x : A = y : B |- B <= A
    m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
            _assignment.getExpression()->getType(),
            _assignment.getLValue()->getType()));
    return true;
}

int Collector::handleVarDeclVar(Node &_node) {
    collectParam((NamedValue *)&_node, tc::FreshType::PARAM_OUT);
    return 0;
}

bool Collector::visitVariableDeclaration(VariableDeclaration &_var) {
    if (_var.getValue())
        // x : A = y : B |- B <= A
        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                _var.getValue()->getType(),
                _var.getVariable()->getType()));
    return true;
}

bool Collector::visitIf(If &_if) {
    m_constraints.insert(new tc::Formula(tc::Formula::EQUALS,
            _if.getArg()->getType(), new Type(Type::BOOL)));
    return true;
}

int Collector::handlePredicateInParam(Node &_node) {
    collectParam((NamedValue *)&_node, tc::FreshType::PARAM_IN);
    return 0;
}

int Collector::handlePredicateOutParam(Node &_node) {
    collectParam((NamedValue *)&_node, tc::FreshType::PARAM_OUT);
    return 0;
}

int Collector::handleParameterizedTypeParam(Node &_node) {
    collectParam((NamedValue *)&_node, 0);
    return 0;
}

bool Collector::visitNamedReferenceType(NamedReferenceType &_type) {
    // Replace reference type with actual one.
    if (NodeSetter *pSetter = getNodeSetter()) {
        TypeDeclarationPtr pDecl = (TypeDeclarationPtr)_type.getDeclaration();
        TypePtr pType = pDecl->getType();

        if (!_type.getArgs().empty()) {
            assert(pType->getKind() == Type::PARAMETERIZED);
            assert(false && "Not implemented");

            ParameterizedTypePtr pOrigType = pType->clone().as<ParameterizedType>();

            for (size_t i = 0; i < pOrigType->getParams().size(); ++i) {
                TypePtr pParamType = pOrigType->getParams().get(i)->getType();
                TypePtr pReplacement = _type.getArgs().get(i).as<TypeExpr>()->getContents();

                assert(pParamType->getKind() == Type::FRESH);

                pOrigType->rewrite(pParamType, pReplacement);
            }
            pType = pOrigType;
        }

        pSetter->set(pType);
    }

    return true;
}

bool Collector::visitTypeExpr(TypeExpr &_expr) {
    TypeTypePtr pType = new TypeType();
    pType->getDeclaration()->setType(_expr.getContents());
    _expr.setType(pType);
    return true;
}

bool Collector::traverseSwitch(Switch &_stmt) {
    m_switches.push(&_stmt);
    const bool b = Visitor::traverseSwitch(_stmt);
    m_switches.pop();
    return b;
}

int Collector::handleSwitchCaseValuePost(Node &_node) {
    ExpressionPtr pExpr((Expression *)&_node);

    if (m_switches.top()->getArg())
        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
            pExpr->getType(), m_switches.top()->getArg()->getType()));
    else if (m_switches.top()->getParamDecl())
        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
            pExpr->getType(), m_switches.top()->getParamDecl()->getVariable()->getType()));

    return 0;
}

void tc::collect(Formulas & _constraints, Node &_node, Context & _ctx, FreshTypes & _types) {
    Collector Collector(_constraints, _ctx, _types);
    Collector.traverseNode(_node);
}
