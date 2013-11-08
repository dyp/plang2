/// \file collect_constraints.cpp
///

#include <iostream>
#include <stack>

#include <assert.h>

#include "typecheck.h"
#include "prettyprinter.h"
#include "pp_syntax.h"

#include <ir/statements.h>
#include <ir/builtins.h>
#include <ir/visitor.h>
#include <utils.h>

using namespace ir;

class Collector : public Visitor {
public:
    Collector(tc::Formulas & _constraints, ir::Context & _ctx);
    virtual ~Collector() {}

    virtual bool visitRange(Range &_type);
    virtual bool visitArrayType(ArrayType &_type);
    virtual bool visitVariableReference(VariableReference &_var);
    virtual bool visitPredicateReference(PredicateReference &_ref);
    virtual bool visitFormulaCall(FormulaCall &_call);
    virtual bool visitFunctionCall(FunctionCall &_call);
    virtual bool visitLambda(Lambda &_lambda);
    virtual bool visitBinder(Binder &_binder);
    virtual bool visitCall(Call &_call);
    virtual bool visitLiteral(Literal &_lit);
    virtual bool visitUnary(Unary &_unary);
    virtual bool visitBinary(Binary &_binary);
    virtual bool visitTernary(Ternary &_ternary);
    virtual bool visitFormula(Formula &_formula);
    virtual bool visitArrayPartExpr(ArrayPartExpr &_array);
    virtual bool visitStructConstructor(StructConstructor &_cons);
    virtual bool visitUnionConstructor(UnionConstructor &_cons);
    virtual bool visitListConstructor(ListConstructor &_cons);
    virtual bool visitSetConstructor(SetConstructor &_cons);
    virtual bool visitArrayConstructor(ArrayConstructor &_cons);
    virtual bool visitArrayIteration(ArrayIteration& _iter);
    virtual bool visitMapConstructor(MapConstructor &_cons);
    virtual bool visitFieldExpr(FieldExpr &_field);
    virtual bool visitReplacement(Replacement &_repl);
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
    virtual int handleSubtypeParam(Node &_node);
    virtual int handleSwitchCaseValuePost(Node &_node);

    virtual bool traverseSwitch(Switch &_stmt);

protected:
    tc::FreshTypePtr createFresh(const TypePtr &_pCurrent = NULL);
    tc::FreshTypePtr getKnownType(const TypePtr &_pType);
    void collectParam(const NamedValuePtr &_pParam, int _nFlags);

private:
    tc::Formulas & m_constraints;
    ir::Context & m_ctx;
    std::stack<SwitchPtr> m_switches;
};

Collector::Collector(tc::Formulas & _constraints, ir::Context & _ctx)
    : Visitor(CHILDREN_FIRST), m_constraints(_constraints), m_ctx(_ctx)
{
}

tc::FreshTypePtr Collector::getKnownType(const TypePtr &_pType) {
    if (_pType && _pType->getKind() == Type::NAMED_REFERENCE)
        if (TypePtr pType = _pType.as<NamedReferenceType>()->getDeclaration()->getType())
            if (pType->getKind() == Type::FRESH)
                return pType.as<tc::FreshType>();

    return NULL;
}

tc::FreshTypePtr Collector::createFresh(const TypePtr &_pCurrent) {
    if (tc::FreshTypePtr pFresh = getKnownType(_pCurrent))
        return pFresh;

    return new tc::FreshType();
}

void Collector::collectParam(const NamedValuePtr &_pParam, int _nFlags) {
    TypePtr pType = _pParam->getType();

    if (pType->getKind() == Type::FRESH)
        return;

    if (pType->getKind() == Type::TYPE
        && !pType.as<TypeType>()->getDeclaration()->getType()) {
        TypeTypePtr pTypeType = pType.as<TypeType>();
        pTypeType->getDeclaration()->setType(new tc::FreshType());
        return;
    }

    tc::FreshTypePtr pFresh = createFresh(pType);

    assert(pType);

    if (pType && pType->getKind() != Type::GENERIC && !getKnownType(pType))
        m_constraints.insert(new tc::Formula(tc::Formula::EQUALS, pFresh, pType));

    _pParam->setType(pFresh);
    pFresh->addFlags(_nFlags);
}

static const TypePtr _getContentsType(const ExpressionPtr _pExpr) {
    return _pExpr->getType() && _pExpr->getType()->getKind() == Type::TYPE
        ? _pExpr.as<TypeExpr>()->getContents()
        : _pExpr->getType();
}

static ExpressionPtr _getConditionForIndex(const ExpressionPtr& _pIndex, const VariableReferencePtr& _pVar, bool _bEquality) {
    ExpressionPtr pExpr;
    if (_pIndex->getKind() == Expression::TYPE) {
        const TypePtr pIndexType = _getContentsType(_pIndex);
        if (pIndexType->getKind() != Type::SUBTYPE)
            throw std::runtime_error("Expected subtype or range.");

        SubtypePtr pSubtype = pIndexType.as<Subtype>();

        pExpr = !_bEquality
            ? new Unary(Unary::BOOL_NEGATE, clone(*pSubtype->getExpression()))
            : clone(*pSubtype->getExpression());

        pExpr = Expression::substitute(pExpr, new VariableReference(pSubtype->getParam()), _pVar).as<Expression>();
    }
    else
        pExpr = new Binary(_bEquality ? Binary::EQUALS : Binary::NOT_EQUALS, _pVar, _pIndex);

    return pExpr;
}


bool Collector::visitRange(Range &_type) {
    SubtypePtr pSubtype = _type.asSubtype();
    collectParam(pSubtype->getParam(), tc::FreshType::PARAM_OUT);

    m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
        _type.getMin()->getType(), pSubtype->getParam()->getType()));
    m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
        _type.getMax()->getType(), pSubtype->getParam()->getType()));

    callSetter(pSubtype);
    return true;
}

bool Collector::visitArrayType(ArrayType &_type) {
    // FIXME There is should be finite type.
    m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
        _type.getDimensionType(), new Type(Type::INT, Number::GENERIC)));
    return true;
}

bool Collector::visitVariableReference(VariableReference &_var) {
    _var.setType(_var.getTarget()->getType());
    return true;
}

bool Collector::visitPredicateReference(PredicateReference &_ref) {
    Predicates funcs;

    if (! m_ctx.getPredicates(_ref.getName(), funcs))
        assert(false);

    _ref.setType(createFresh(_ref.getType()));

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

bool Collector::visitFormulaCall(FormulaCall &_call) {
    _call.setType(_call.getTarget()->getResultType());
    for (size_t i = 0; i < _call.getArgs().size(); ++i)
        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
            _call.getArgs().get(i)->getType(), _call.getTarget()->getParams().get(i)->getType()));
    return true;
}

bool Collector::visitFunctionCall(FunctionCall &_call) {
    PredicateTypePtr pType = new PredicateType();

    _call.setType(createFresh(_call.getType()));
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

bool Collector::visitLambda(Lambda &_lambda) {
    _lambda.setType(_lambda.getPredicate().getType());
    return true;
}

bool Collector::visitBinder(Binder &_binder) {
    PredicateTypePtr
        pType = new PredicateType(),
        pPredicateType = new PredicateType();

    _binder.setType(createFresh(_binder.getType()));
    pType->getOutParams().add(new Branch());
    pPredicateType->getOutParams().add(new Branch());

    for (size_t i = 0; i < _binder.getArgs().size(); ++i)
        if (_binder.getArgs().get(i)) {
            pType->getInParams().add(new Param(L"", _binder.getArgs().get(i)->getType()));
            pPredicateType->getInParams().add(new Param(L"", _binder.getArgs().get(i)->getType()));
        } else
            pPredicateType->getInParams().add(new Param(L"", new Type(Type::TOP)));

    if (_binder.getPredicate()->getKind() == Expression::PREDICATE) {
        PredicatePtr pPredicate = _binder.getPredicate().as<PredicateReference>()->getTarget();
        pType->getOutParams().assign(pPredicate->getOutParams());
        pPredicateType->getOutParams().assign(pPredicate->getOutParams());
    }

    m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
            pPredicateType, _binder.getPredicate()->getType()));

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
        default:
            break;
    }

    return true;
}

bool Collector::visitUnary(Unary &_unary) {
    _unary.setType(createFresh(_unary.getType()));

    switch (_unary.getOperator()) {
        case Unary::MINUS:
            // Must contain negative values.
            m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                    new Type(Type::INT, 1), _unary.getType()));
            // no break;
        case Unary::PLUS:
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

                pSet->setBaseType(createFresh());
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
    _binary.setType(createFresh(_binary.getType()));

    switch (_binary.getOperator()) {
        case Binary::ADD:
        case Binary::SUBTRACT:
            //                       x : A + y : B = z :C
            // ----------------------------------------------------------------
            // (A <= B and C = B and A <= real and B <= real and C <= real)
            //   or (B < A and C = A and A <= real and B <= real and C <= real)
            //   or (A <= C and B <= C and C = {D})
            //   or (A <= C and B <= C and C = [[D]])   /* Operator "+" only. */
            //   or (A = T1[D1] & B = T2[D2] & C = T3[D3] & T1 <= T3 & T2 <= T3 & D1 <= D3 & D2 <= D3)   /* Operator "+" only. */
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

                pSet->setBaseType(createFresh());

                part3.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getLeftSide()->getType(),
                        _binary.getType()));
                part3.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getRightSide()->getType(),
                        _binary.getType()));
                part3.insert(new tc::Formula(tc::Formula::EQUALS,
                        _binary.getType(), pSet));

                if (_binary.getOperator() == Binary::ADD) {
                    tc::Formulas & part4 = p->addPart();
                    ListTypePtr pList = new ListType(NULL);

                    pList->setBaseType(createFresh());

                    part4.insert(new tc::Formula(tc::Formula::SUBTYPE,
                            _binary.getLeftSide()->getType(),
                            _binary.getType()));
                    part4.insert(new tc::Formula(tc::Formula::SUBTYPE,
                            _binary.getRightSide()->getType(),
                            _binary.getType()));
                    part4.insert(new tc::Formula(tc::Formula::EQUALS,
                            _binary.getType(), pList));

                    tc::Formulas & part5 = p->addPart();

                    ArrayTypePtr
                        pA = new ArrayType(createFresh(), createFresh()),
                        pB = new ArrayType(createFresh(), createFresh()),
                        pC = new ArrayType(new tc::FreshType(tc::FreshType::PARAM_OUT),
                            new tc::FreshType(tc::FreshType::PARAM_OUT));

                    part5.insert(new tc::Formula(tc::Formula::EQUALS,
                        _binary.getLeftSide()->getType(), pA));
                    part5.insert(new tc::Formula(tc::Formula::EQUALS,
                        _binary.getRightSide()->getType(), pB));
                    part5.insert(new tc::Formula(tc::Formula::EQUALS,
                        _binary.getType(), pC));

                    part5.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        pA->getBaseType(), pC->getBaseType()));
                    part5.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        pB->getBaseType(), pC->getBaseType()));
                    part5.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        pA->getDimensionType(), pC->getDimensionType()));
                    part5.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        pB->getDimensionType(), pC->getDimensionType()));
                }

                m_constraints.insert(p);
            }
            break;

        case Binary::MULTIPLY:
        case Binary::DIVIDE:
        case Binary::REMAINDER:
        case Binary::POWER:
            //         x : A * y : B = z : C
            // -----------------------------------
            // ((A <= B and C = B) or (B < A and C = A)) and A <= real and B <= real and C <= real
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

                pSet->setBaseType(createFresh());

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

                pSet->setBaseType(createFresh());
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

bool Collector::visitTernary(Ternary &_ternary) {
    _ternary.setType(createFresh(_ternary.getType()));
    m_constraints.insert(new tc::Formula(tc::Formula::EQUALS, _ternary.getIf()->getType(), new Type(Type::BOOL)));
    m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE, _ternary.getThen()->getType(), _ternary.getType()));
    m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE, _ternary.getElse()->getType(), _ternary.getType()));

    return true;
}

bool Collector::visitFormula(Formula &_formula) {
    _formula.setType(createFresh(_formula.getType()));
    m_constraints.insert(new tc::Formula(tc::Formula::EQUALS, _formula.getType(), new Type(Type::BOOL)));
    return true;
}

bool Collector::visitArrayPartExpr(ArrayPartExpr &_array) {
    _array.setType(new tc::FreshType(tc::FreshType::PARAM_OUT));

    const MapTypePtr pMapType = new MapType(new Type(Type::BOTTOM), _array.getType());
    const ListTypePtr pListType = new ListType(_array.getType());

    ArrayTypePtr
        pArrayType = new ArrayType(),
        pCurrentArray = pArrayType;

    for (size_t i=0; i<_array.getIndices().size(); ++i) {
        pCurrentArray->setDimensionType(new Type(Type::BOTTOM));
        if (i+1 == _array.getIndices().size())
            continue;
        pCurrentArray->setBaseType(new ArrayType());
        pCurrentArray = pCurrentArray->getBaseType().as<ArrayType>();
    }
    pCurrentArray->setBaseType(_array.getType());

    if (_array.getIndices().size() > 1) {
        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE, _array.getObject()->getType(), pArrayType));
        return true;
    }

    tc::CompoundFormulaPtr pFormula = new tc::CompoundFormula();

    // A = a[i], I i
    // A <= B[_|_, _|_, ...]
    pFormula->addPart().insert(new tc::Formula(tc::Formula::SUBTYPE, _array.getObject()->getType(), pArrayType));

    // or A <= {_|_ : B}
    pFormula->addPart().insert(new tc::Formula(tc::Formula::SUBTYPE, _array.getObject()->getType(), pMapType));

    // or A <= [[B]] and I <= nat
    tc::Formulas& formulas = pFormula->addPart();
    formulas.insert(new tc::Formula(tc::Formula::SUBTYPE, _array.getObject()->getType(), pListType));
    formulas.insert(new tc::Formula(tc::Formula::SUBTYPE, _array.getIndices().get(0)->getType(), new Type(Type::NAT, Number::GENERIC)));

    m_constraints.insert(pFormula);
    return true;
}

bool Collector::visitStructConstructor(StructConstructor &_cons) {
    StructTypePtr pStruct = new StructType();

    for (size_t i = 0; i < _cons.size(); ++i) {
        StructFieldDefinitionPtr pDef = _cons.get(i);
        NamedValuePtr pField = new NamedValue(pDef->getName());

        pField->setType(pDef->getValue()->getType());

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

        pField->setType(pDef->getValue()->getType());
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

    pSet->setBaseType(new tc::FreshType(tc::FreshType::PARAM_OUT));
    _cons.setType(pSet);

    for (size_t i = 0; i < _cons.size(); ++i)
        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                _cons.get(i)->getType(), pSet->getBaseType()));

    return true;
}

bool Collector::visitListConstructor(ListConstructor &_cons) {
    ListTypePtr pList = new ListType(NULL);

    pList->setBaseType(new tc::FreshType(tc::FreshType::PARAM_OUT));
    _cons.setType(pList);

    for (size_t i = 0; i < _cons.size(); ++i)
        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                _cons.get(i)->getType(), pList->getBaseType()));

    return true;
}

bool Collector::visitArrayConstructor(ArrayConstructor &_cons) {
    ArrayTypePtr pArray = new ArrayType(NULL);

    pArray->setBaseType(new tc::FreshType(tc::FreshType::PARAM_OUT));
    _cons.setType(pArray);

    if (_cons.empty()) {
        pArray->setDimensionType(createFresh());
        return true;
    }

    for (size_t i = 0; i < _cons.size(); ++i)
        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
            _cons.get(i)->getValue()->getType(), pArray->getBaseType()));

    const tc::FreshTypePtr pParamType = new tc::FreshType(tc::FreshType::PARAM_OUT);
    SubtypePtr pEnumType = new Subtype(new NamedValue(L"", pParamType));
    bool bHaveIndices = false;

    for (size_t i = 0; i < _cons.size(); ++i)
        if (_cons.get(i)->getIndex()) {
            bHaveIndices = true;
            break;
        }

    if (!bHaveIndices) {
        LiteralPtr pUpperBound = new Literal(Number::makeNat(_cons.size()));

        pUpperBound->setType(new Type(Type::NAT,
                pUpperBound->getNumber().countBits(false)));
        m_constraints.insert(new tc::Formula(tc::Formula::EQUALS,
                pParamType, pUpperBound->getType()));
        pEnumType->setExpression(new Binary(Binary::LESS,
                new VariableReference(pEnumType->getParam()), pUpperBound));

        for (size_t i = 0; i < _cons.size(); ++i) {
            LiteralPtr pIndex = new Literal(Number::makeNat(i));
            pIndex->setType(new Type(Type::NAT, pIndex->getNumber().countBits(false)));
            _cons.get(i)->setIndex(pIndex);
        }
    } else {
        bool bFirst = true;
        size_t nInc = 0;
        for (size_t i = 0; i < _cons.size(); ++i) {
            if (_cons.get(i)->getIndex()) {
                const TypePtr pIndexType = _getContentsType(_cons.get(i)->getIndex());
                m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE, pIndexType, pParamType));
                ExpressionPtr pExpr = _getConditionForIndex(_cons.get(i)->getIndex(),
                        new VariableReference(pEnumType->getParam()), false);

                pEnumType->setExpression(pEnumType->getExpression()
                    ? new Binary(Binary::BOOL_AND, pEnumType->getExpression(), pExpr)
                    : pExpr);

                if (pIndexType->getKind() == Type::SUBTYPE)
                    m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        pIndexType.as<Subtype>()->getParam()->getType(), pEnumType->getParam()->getType()));
            }
            else {
                PredicatePtr pIndexer = Builtins::instance().find(bFirst ? L"zero" : L"inc");
                assert(pIndexer);

                PredicateReferencePtr pReference = new PredicateReference(pIndexer);
                pReference->setType(new PredicateType(pIndexer.as<AnonymousPredicate>()));

                FunctionCallPtr pCallIndexer = new FunctionCall(pReference);
                pCallIndexer->getArgs().add(new TypeExpr(pEnumType));

                if (!bFirst)
                    pCallIndexer->getArgs().add(new Literal(Number(intToStr(++nInc), Number::INTEGER)));

                bFirst = false;
                _cons.get(i)->setIndex(pCallIndexer);
            }
        }

        if (!pEnumType->getExpression())
            pEnumType->setExpression(new Literal(true));
    }

    if (_cons.size() == 1 && _cons.get(0)->getIndex()->getType()) {
        pArray->setDimensionType(_cons.get(0)->getIndex()->getType());
        return true;
    }

    const VariableReferencePtr pParam = new VariableReference(new NamedValue(L"", pParamType));
    SubtypePtr pSubtype = new Subtype(pParam->getTarget());
    pArray->setDimensionType(pSubtype);

    ExpressionPtr pExpr = NULL;
    for (size_t i = 0; i < _cons.size(); ++i) {
        ExpressionPtr pEquals = _getConditionForIndex(_cons.get(i)->getIndex(), pParam, true);
        pExpr = pExpr ? new Binary(Binary::BOOL_OR, pExpr, pEquals) : pEquals;
    }

    pSubtype->setExpression(pExpr);

    return true;
}

bool Collector::visitArrayIteration(ArrayIteration& _iter) {
    ArrayTypePtr pArrayType = new ArrayType();
    _iter.setType(pArrayType);

    std::vector<TypePtr> dimensions;
    const bool bUnknownDimensionType = _iter.getDefault();
    for (size_t i = 0; i < _iter.getIterators().size(); ++i)
        dimensions.push_back(TypePtr(!bUnknownDimensionType ?
                new tc::FreshType(tc::FreshType::PARAM_OUT) : new Type(Type::TOP)));

    TypePtr pBaseType = new tc::FreshType(tc::FreshType::PARAM_OUT);

    for (size_t i = 0; i < _iter.size(); ++i) {
        const Collection<Expression>& conds = _iter.get(i)->getConditions();
        for (size_t j = 0; j < conds.size(); ++j) {
            if (conds.get(j)->getKind() == Expression::CONSTRUCTOR
                && conds.get(j).as<Constructor>()->getConstructorKind() == Constructor::STRUCT_FIELDS)
            {
                const StructConstructor& constructor = *conds.get(j).as<StructConstructor>();
                if (_iter.getIterators().size() != constructor.size())
                    throw std::runtime_error("Count of iterators does not match count of fields.");

                for (size_t k = 0; k < constructor.size(); ++k) {
                    m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _getContentsType(constructor.get(k)->getValue()), dimensions[k]));
                    m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _getContentsType(constructor.get(k)->getValue()), _iter.getIterators().get(k)->getType()));
                }

                continue;
            }

            assert(_iter.getIterators().size() == 1);

            m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                _getContentsType(conds.get(j)), dimensions[0]));
            m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                _getContentsType(conds.get(j)), _iter.getIterators().get(0)->getType()));
        }

        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
            _getContentsType(_iter.get(i)->getExpression()), pBaseType));
    }

    if (_iter.getDefault())
        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
            _getContentsType(_iter.getDefault()), pBaseType));

    for (std::vector<TypePtr>::iterator i = dimensions.begin();; ++i) {
        pArrayType->setDimensionType(*i);

        if (i == --dimensions.end())
            break;

        pArrayType->setBaseType(new ArrayType());
        pArrayType = pArrayType->getBaseType().as<ArrayType>();
    }

    pArrayType->setBaseType(pBaseType);

    return true;
}

bool Collector::visitMapConstructor(MapConstructor &_cons) {
    MapTypePtr pMap = new MapType(NULL, NULL);

    pMap->setBaseType(createFresh());
    pMap->setIndexType(createFresh());
    pMap->getBaseType().as<tc::FreshType>()->setFlags(tc::FreshType::PARAM_OUT);
    pMap->getIndexType().as<tc::FreshType>()->setFlags(tc::FreshType::PARAM_OUT);
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
    tc::FreshTypePtr pFresh = createFresh(_field.getType());
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

bool Collector::visitReplacement(Replacement &_repl) {
    _repl.setType(_repl.getObject()->getType());
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

int Collector::handleSubtypeParam(Node &_node) {
    collectParam((NamedValue *)&_node, 0);
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
    NodeSetter *pSetter = getNodeSetter();
    if (pSetter == NULL)
        return true;

    if (!_type.getDeclaration()->getType())
        _type.getDeclaration()->setType(createFresh());

    TypePtr pDeclType = _type.getDeclaration()->getType();
    if (_type.getArgs().empty()) {
        pSetter->set(pDeclType);
        return true;
    }

    assert(pDeclType->getKind() == Type::PARAMETERIZED);
    ParameterizedTypePtr pOrigType = pDeclType.as<ParameterizedType>();
    TypePtr pType = clone(*pOrigType->getActualType());

    for (size_t i=0; i < pOrigType->getParams().size(); ++i) {
        TypePtr pParamType = pOrigType->getParams().get(i)->getType();
        if (pParamType->getKind() != Type::TYPE)
            pType = Expression::substitute(pType, new VariableReference(pOrigType->getParams().get(i)), _type.getArgs().get(i)).as<Type>();
        else
            pType->rewrite(pParamType, _type.getArgs().get(i).as<TypeExpr>()->getContents());
    }

    pSetter->set(pType);
    return true;
}

bool Collector::visitTypeExpr(TypeExpr &_expr) {
    TypeTypePtr pType = new TypeType();
    pType->setDeclaration(new TypeDeclaration(L"", _expr.getContents()));
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
            _getContentsType(pExpr), m_switches.top()->getArg()->getType()));
    else if (m_switches.top()->getParamDecl())
        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
            _getContentsType(pExpr), m_switches.top()->getParamDecl()->getVariable()->getType()));

    return 0;
}

void tc::collect(tc::Formulas &_constraints, Node &_node, ir::Context &_ctx) {
    Collector collector(_constraints, _ctx);

    tc::ContextStack::push(::ref(&_constraints));
    collector.traverseNode(_node);
    tc::ContextStack::pop();
}
