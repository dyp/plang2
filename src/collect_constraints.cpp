/// \file collect_constraints.cpp
///

#include <iostream>

#include <assert.h>

#include "typecheck.h"
#include "prettyprinter.h"

#include <ir/statements.h>
#include <ir/visitor.h>

using namespace ir;

class Collector : public Visitor {
public:
    Collector(tc::Formulas & _constraints, Predicate * _pPredicate,
            Context & _ctx, tc::FreshTypes & _types);
    ~Collector() {}

    virtual bool visitVariableReference(VariableReference &_var);
    virtual bool visitPredicateReference(PredicateReference &_ref);
    virtual bool visitFunctionCall(FunctionCall &_call);
    virtual bool visitCall(Call &_call);
    virtual bool visitUnary(Unary &_unary);
    virtual bool visitBinary(Binary &_binary);
    virtual bool visitStructConstructor(StructConstructor &_cons);
    virtual bool visitListConstructor(ListConstructor &_cons);
    virtual bool visitSetConstructor(SetConstructor &_cons);
    virtual bool visitArrayConstructor(ArrayConstructor &_cons);
    virtual bool visitMapConstructor(MapConstructor &_cons);
    virtual bool visitField(StructFieldExpr &_field);
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

protected:
    template<typename Node>
    tc::FreshType * createFreshGeneric(Node * _pParam);

    // Need these wrappers since e.g. Binary::setType() is actually Expression::setType() but
    // _pParam won't be implicitly downcast to Expression*.
    tc::FreshType * createFresh(Expression * _pParam) { return createFreshGeneric(_pParam); }
    tc::FreshType * createFresh(NamedValue * _pParam) { return createFreshGeneric(_pParam); }

    tc::FreshType * createFresh(DerivedType * _pParam);
    tc::FreshType * createFreshIndex(MapType * _pParam);

    template<typename Node>
    void setFreshType(Node * _pParam, Type * _pType);

    tc::FreshType *getKnownType(const Type *_pType);

    void collectParam(NamedValue * _pParam, int _nFlags);

private:
    tc::Formulas & m_constraints;
    Predicate * m_pPredicate;
    Context & m_ctx;
    tc::FreshTypes & m_types;
};

Collector::Collector(tc::Formulas & _constraints,
        Predicate * _pPredicate, Context & _ctx, tc::FreshTypes & _types)
    : Visitor(CHILDREN_FIRST), m_constraints(_constraints), m_pPredicate(_pPredicate), m_ctx(_ctx), m_types(_types)
{
}

template<typename Node>
void Collector::setFreshType(Node * _pParam, Type * _pType) {
    _pParam->setType(_pType);
    if (_pType->getKind() == Type::FRESH)
        m_types.insert(std::make_pair((tc::FreshType *) _pType, tc::createTypeSetter<Node>(_pParam)));
}

tc::FreshType *Collector::getKnownType(const Type *_pType) {
    if (_pType != NULL && _pType->getKind() == Type::NAMED_REFERENCE)
        if (const Type *pType = ((NamedReferenceType *)_pType)->getDeclaration()->getType())
            if (pType->getKind() == Type::FRESH)
                return (tc::FreshType *)pType;

    return NULL;
}

template<typename _Node>
tc::FreshType * Collector::createFreshGeneric(_Node * _pParam) {
    tc::FreshType *pFresh = getKnownType(_pParam->getType());

    if (pFresh == NULL)
        pFresh = new tc::FreshType();

    m_types.insert(std::make_pair(pFresh, tc::createTypeSetter(_pParam)));

    return pFresh;
}

tc::FreshType * Collector::createFresh(DerivedType * _pParam) {
    tc::FreshType *pType = new tc::FreshType();
    m_types.insert(std::make_pair(pType, tc::createBaseTypeSetter(_pParam)));
    return pType;
}

tc::FreshType * Collector::createFreshIndex(MapType * _pParam) {
    tc::FreshType *pType = new tc::FreshType();
    m_types.insert(std::make_pair(pType, tc::createIndexTypeSetter(_pParam)));
    return pType;
}

void Collector::collectParam(NamedValue * _pParam, int _nFlags) {
    Type * pType = _pParam->getType();

    if (pType->getKind() == Type::TYPE) {
        TypeType *pTypeType = (TypeType *)pType;
        pTypeType->getDeclaration()->setType(new tc::FreshType());
        return;
    }

    tc::FreshType *pFresh = createFresh(_pParam);

    assert((pType != NULL));

    if (pType != NULL) {
        _pParam->getType()->setParent(NULL); // Suppress delete.
        if (pType->getKind() != Type::GENERIC && getKnownType(pType) == NULL) {
            m_constraints.insert(new tc::Formula(tc::Formula::EQUALS,
                    pFresh, pType));
        }
    }

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

    _ref.setType(createFresh(&_ref), false);

    if (funcs.size() > 1) {
        tc::CompoundFormula * pConstraint = new tc::CompoundFormula();

        for (size_t i = 0; i < funcs.size(); ++ i) {
            Predicate * pPred = funcs.get(i);
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
    PredicateType *pType = new PredicateType();

    _call.setType(createFresh(&_call), false);
    pType->getOutParams().add(new Branch(), true);
    pType->getOutParams().get(0)->add(new Param(L"", _call.getType(), false), true);

    for (size_t i = 0; i < _call.getArgs().size(); ++ i)
        pType->getInParams().add(new Param(L"", _call.getArgs().get(i)->getType(), false), true);

    m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
            _call.getPredicate()->getType(), pType));
//    m_constraints.insert(new tc::Formula(tc::Formula::Equals,
//            _call.getPredicate()->getType(), pType));

    return true;
}

bool Collector::visitCall(Call &_call) {
    PredicateType *pType = new PredicateType();

    for (size_t i = 0; i < _call.getArgs().size(); ++i)
        pType->getInParams().add(new Param(L"", _call.getArgs().get(i)->getType(), false), true);

    for (size_t i = 0; i < _call.getBranches().size(); ++i) {
        CallBranch &br = *_call.getBranches().get(i);

        pType->getOutParams().add(new Branch(), true);

        for (size_t j = 0; j < br.size(); ++j)
            pType->getOutParams().get(i)->add(new Param(L"", br.get(j)->getType(), false), true);
    }

    m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
            _call.getPredicate()->getType(), pType));

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
                tc::CompoundFormula *p = new tc::CompoundFormula();

                // Boolean negation.
                tc::Formulas &part1 = p->addPart();

                part1.insert(new tc::Formula(tc::Formula::EQUALS,
                        _unary.getExpression()->getType(), new Type(Type::BOOL)));
                part1.insert(new tc::Formula(tc::Formula::EQUALS,
                        _unary.getType(), new Type(Type::BOOL)));

                // Set negation.
                tc::Formulas &part2 = p->addPart();
                SetType *pSet = m_ctx.attach(new SetType(NULL));

                pSet->setBaseType(createFresh(pSet));
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
                tc::CompoundFormula * p = new tc::CompoundFormula();
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
                SetType * pSet = m_ctx.attach(new SetType(NULL));

                pSet->setBaseType(createFresh(pSet));

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
                    ListType * pList = m_ctx.attach(new ListType(NULL));

                    pList->setBaseType(createFresh(pList));

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
                tc::CompoundFormula * p = new tc::CompoundFormula();
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
                tc::CompoundFormula * p = new tc::CompoundFormula();

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
                tc::CompoundFormula * p = new tc::CompoundFormula();
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
                SetType * pSet = m_ctx.attach(new SetType(NULL));

                pSet->setBaseType(createFresh(pSet));

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
                SetType *pSet = m_ctx.attach(new SetType(NULL));

                pSet->setBaseType(createFresh(pSet));
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
    StructType *pStruct = m_ctx.attach(new StructType());

    for (size_t i = 0; i < _cons.size(); ++i) {
        StructFieldDefinition *pDef = _cons.get(i);
        NamedValue *pField = new NamedValue(pDef->getName());

        pDef->setStructType(pStruct);
        setFreshType(pField, pDef->getValue()->getType());
        pStruct->getFields().add(pField);
        pDef->setField(pField);
    }

    _cons.setType(pStruct);

    return true;
}

bool Collector::visitSetConstructor(SetConstructor &_cons) {
    SetType * pSet = m_ctx.attach(new SetType(NULL));

    pSet->setBaseType(createFresh(pSet));
    _cons.setType(pSet);

    for (size_t i = 0; i < _cons.size(); ++i)
        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                _cons.get(i)->getType(), pSet->getBaseType()));

    return true;
}

bool Collector::visitListConstructor(ListConstructor &_cons) {
    ListType *pList = m_ctx.attach(new ListType(NULL));

    pList->setBaseType(createFresh(pList));
    _cons.setType(pList);

    for (size_t i = 0; i < _cons.size(); ++i)
        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                _cons.get(i)->getType(), pList->getBaseType()));

    return true;
}

bool Collector::visitArrayConstructor(ArrayConstructor &_cons) {
    ArrayType *pArray = m_ctx.attach(new ArrayType(NULL));

    pArray->setBaseType(createFresh(pArray));
    _cons.setType(pArray);

    for (size_t i = 0; i < _cons.size(); ++i)
        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                _cons.get(i)->getValue()->getType(), pArray->getBaseType()));

    return true;
}

bool Collector::visitMapConstructor(MapConstructor &_cons) {
    MapType *pMap = m_ctx.attach(new MapType(NULL, NULL));

    pMap->setBaseType(createFresh(pMap));
    pMap->setIndexType(createFreshIndex(pMap));
    _cons.setType(pMap);

    for (size_t i = 0; i < _cons.size(); ++i) {
        ElementDefinition *pElement = _cons.get(i);

        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                pElement->getIndex()->getType(), pMap->getIndexType()));
        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                pElement->getValue()->getType(), pMap->getBaseType()));
    }

    return true;
}

bool Collector::visitField(StructFieldExpr &_field) {
    tc::FreshType *pFresh = createFresh(&_field);
    ir::StructType * pStruct = m_ctx.attach(new StructType());
    ir::NamedValue * pField = new NamedValue(_field.getFieldName(), pFresh, false);

    _field.setType(pFresh);
    pStruct->getFields().add(pField);

    if (_field.getObject()->getType()->getKind() == Type::FRESH)
        pFresh->setFlags(((tc::FreshType *) _field.getObject()->getType())->getFlags());

    // (x : A).foo : B  |-  A <= struct(B foo)
    m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
            _field.getObject()->getType(), pStruct));
    m_constraints.insert(new tc::Formula(tc::Formula::EQUALS,
            _field.getType(), pField->getType()));

    return true;
}

bool Collector::visitCastExpr(CastExpr &_cast) {
    Type *pToType = (Type *)_cast.getToType()->getContents();

    _cast.setType(pToType, false);

    if (pToType->getKind() == Type::STRUCT &&
            _cast.getExpression()->getKind() == Expression::CONSTRUCTOR &&
            ((Constructor *)_cast.getExpression())->getConstructorKind() == Constructor::STRUCT_FIELDS)
    {
        // We can cast form tuples to structs explicitly.
        StructType *pStruct = (StructType *)pToType;
        StructConstructor *pFields = (StructConstructor *)_cast.getExpression();
        bool bSuccess = true;

        // TODO: maybe use default values for fields.
        if (pStruct->getFields().size() == pFields->size()) {
            for (size_t i = 0; i < pFields->size(); ++i) {
                StructFieldDefinition *pDef = pFields->get(i);
                size_t cOtherIdx = pDef->getName().empty() ? i : pStruct->getFields().findByNameIdx(pDef->getName());

                if (cOtherIdx == (size_t)-1) {
                    bSuccess = false;
                    break;
                }

                NamedValue *pField = pStruct->getFields().get(cOtherIdx);

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
    if (_var.getValue() != NULL)
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
        TypeDeclaration *pDecl = (TypeDeclaration *)_type.getDeclaration();
        Type *pType = pDecl->getType();

        if (!_type.getArgs().empty()) {
            assert(pType->getKind() == Type::PARAMETERIZED);
            assert(false && "Not implemented");

            ParameterizedType *pOrigType = (ParameterizedType *)pType->clone();

            for (size_t i = 0; i < pOrigType->getParams().size(); ++i) {
                Type *pParamType = pOrigType->getParams().get(i)->getType();
                Type *pReplacement = ((TypeExpr *)_type.getArgs().get(i))->getContents();

                assert(pParamType->getKind() == Type::FRESH);

                pOrigType->rewrite(pParamType, pReplacement);
            }
            pType = pOrigType;
        }

        pSetter->set((Node *)pType, false);
    }

    return true;
}

bool Collector::visitTypeExpr(TypeExpr &_expr) {
    TypeType *pType = new TypeType();
    pType->getDeclaration()->setType(_expr.getContents(), false);
    _expr.setType(pType);
    return true;
}

void tc::collect(Formulas & _constraints, Predicate & _pred, Context & _ctx, FreshTypes & _types) {
    Collector Collector(_constraints, & _pred, _ctx, _types);
    Collector.traversePredicate(_pred);
}
