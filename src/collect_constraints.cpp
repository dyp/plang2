// \file collect_constraints.cpp
///

#include <iostream>
#include <stack>

#include <assert.h>

#include "typecheck.h"
#include "options.h"
#include "type_lattice.h"

#include <ir/statements.h>
#include <ir/builtins.h>
#include <ir/visitor.h>
#include <utils.h>
#include "static_typecheck.h"

using namespace ir;

class Collector : public Visitor {
public:
    Collector(const tc::FormulasPtr & _constraints, ir::Context & _ctx);
    ~Collector() override = default;

    bool visitRange(const RangePtr &_type) override;
    bool visitArrayType(const ArrayTypePtr &_type) override;
    bool visitVariableReference(const VariableReferencePtr &_var) override;
    bool visitPredicateReference(const PredicateReferencePtr &_ref) override;
    bool visitFormulaCall(const FormulaCallPtr &_call) override;
    bool visitFunctionCall(const FunctionCallPtr &_call) override;
    bool visitLambda(const LambdaPtr &_lambda) override;
    bool visitBinder(const BinderPtr &_binder) override;
    bool visitCall(const CallPtr &_call) override;
    bool visitLiteral(const LiteralPtr &_lit) override;
    bool visitUnary(const UnaryPtr &_unary) override;
    bool visitBinary(const BinaryPtr &_binary) override;
    bool visitTernary(const TernaryPtr &_ternary) override;
    bool visitFormula(const FormulaPtr &_formula) override;
    bool visitArrayPartExpr(const ArrayPartExprPtr &_array) override;
    bool visitRecognizerExpr(const RecognizerExprPtr& _expr) override;
    bool visitAccessorExpr(const AccessorExprPtr& _expr) override;
    bool visitStructConstructor(const StructConstructorPtr &_cons) override;
    bool visitUnionConstructor(const UnionConstructorPtr &_cons) override;
    bool visitListConstructor(const ListConstructorPtr &_cons) override;
    bool visitSetConstructor(const SetConstructorPtr &_cons) override;
    bool visitArrayConstructor(const ArrayConstructorPtr &_cons) override;
    bool visitArrayIteration(const ArrayIterationPtr& _iter) override;
    bool visitMapConstructor(const MapConstructorPtr &_cons) override;
    bool visitFieldExpr(const FieldExprPtr &_field) override;
    bool visitReplacement(const ReplacementPtr &_repl) override;
    bool visitCastExpr(const CastExprPtr &_cast) override;
    bool visitAssignment(const AssignmentPtr &_assignment) override;
    bool visitVariableDeclaration(const VariableDeclarationPtr &_var) override;
    bool visitIf(const IfPtr &_if) override;

    bool visitTypeExpr(const TypeExprPtr &_expr) override;

    int handlePredicateInParam(NodePtr &_node);
    int handlePredicateOutParam(NodePtr &_node);
    int handleFormulaBoundVariable(NodePtr &_node);
    int handleParameterizedTypeParam(NodePtr &_node);
    int handleVarDeclVar(NodePtr &_node);
    int handleSubtypeParam(NodePtr &_node);
    int handleSwitchCaseValuePost(NodePtr &_node);

    bool traverseSwitch(const SwitchPtr &_stmt) override;

    tc::FreshTypePtr createFresh(const TypePtr &_pCurrent = NULL);

protected:
    tc::FreshTypePtr getKnownType(const TypePtr &_pType);
    void collectParam(const NamedValuePtr &_pParam, int _nFlags);

private:
    const tc::FormulasPtr m_constraints;
    ir::Context & m_ctx;
    std::stack<SwitchPtr> m_switches;
};

Collector::Collector(const tc::FormulasPtr & _constraints, ir::Context & _ctx)
    : Visitor(CHILDREN_FIRST), m_constraints(_constraints), m_ctx(_ctx)
{
}

tc::FreshTypePtr Collector::getKnownType(const TypePtr &_pType) {
    if (_pType && _pType->getKind() == Type::NAMED_REFERENCE)
        if (TypePtr pType = _pType->as<NamedReferenceType>()->getDeclaration()->getType())
            if (pType->getKind() == Type::FRESH)
                return pType->as<tc::FreshType>();

    return NULL;
}

tc::FreshTypePtr Collector::createFresh(const TypePtr &_pCurrent) {
    if (tc::FreshTypePtr pFresh = getKnownType(_pCurrent))
        return pFresh;

    return std::make_shared<tc::FreshType>();
}

void Collector::collectParam(const NamedValuePtr &_pParam, int _nFlags) {
    TypePtr pType = _pParam->getType();

    if (pType->getKind() == Type::FRESH)
        return;

    if (pType->getKind() == Type::TYPE
        && !pType->as<TypeType>()->getDeclaration()->getType()) {
        TypeTypePtr pTypeType = pType->as<TypeType>();
        pTypeType->getDeclaration()->setType(std::make_shared<tc::FreshType>());
        return;
    }

    if(!Options::instance().bStaticTypecheck || pType->getKind() == Type::GENERIC) {
        tc::FreshTypePtr pFresh = createFresh(pType);

        assert(pType);

        if (pType && pType->getKind() != Type::GENERIC && !getKnownType(pType))
            m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::EQUALS, pFresh, pType));

        _pParam->setType(pFresh);
        pFresh->addFlags(_nFlags);
    }
}

static const TypePtr _getContentsType(const ExpressionPtr _pExpr) {
    return _pExpr->getType() && _pExpr->getType()->getKind() == Type::TYPE
        ? _pExpr->as<TypeExpr>()->getContents()
        : _pExpr->getType();
}

static ExpressionPtr _getConditionForIndex(const ExpressionPtr& _pIndex, const VariableReferencePtr& _pVar, bool _bEquality) {
    ExpressionPtr pExpr;
    if (_pIndex->getKind() == Expression::TYPE) {
        const TypePtr pIndexType = _getContentsType(_pIndex);
        if (pIndexType->getKind() != Type::SUBTYPE)
            throw std::runtime_error("Expected subtype or range.");

        SubtypePtr pSubtype = pIndexType->as<Subtype>();

        pExpr = !_bEquality
            ? std::make_shared<Unary>(Unary::BOOL_NEGATE, clone(pSubtype->getExpression()))
            : clone(pSubtype->getExpression());

        pExpr = Expression::substitute(pExpr, std::make_shared<VariableReference>(pSubtype->getParam()), _pVar)->as<Expression>();
    }
    else
        pExpr = std::make_shared<Binary>(_bEquality ? Binary::EQUALS : Binary::NOT_EQUALS, _pVar, _pIndex);

    return pExpr;
}


bool Collector::visitRange(const RangePtr &_type) {
    if(Options::instance().bStaticTypecheck) {
        const auto subtype = StaticTypeChecker::checkRange(*_type);//TODO:dyp: fix
        if (subtype) {
            callSetter(subtype);
            return true;
        }
    }

    const auto pSubtype = _type->asSubtype();
    collectParam(pSubtype->getParam(), tc::FreshType::PARAM_OUT);

    m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
        _type->getMin()->getType(), pSubtype->getParam()->getType()));
    m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
        _type->getMax()->getType(), pSubtype->getParam()->getType()));

    callSetter(pSubtype);
    return true;
}

bool Collector::visitArrayType(const ArrayTypePtr &_type) {
    if(Options::instance().bStaticTypecheck && StaticTypeChecker::checkArrayType(*_type))
        return true;
    // FIXME There is should be finite type.
    m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
        _type->getDimensionType(), std::make_shared<Type>(Type::INT, Number::GENERIC)));
    return true;
}

bool Collector::visitVariableReference(const VariableReferencePtr &_var) {
    if(Options::instance().bStaticTypecheck && StaticTypeChecker::checkVariableReference(*_var))
        return true;
    _var->setType(_var->getTarget()->getType());
    return true;
}

bool Collector::visitPredicateReference(const PredicateReferencePtr &_ref) {
    if(Options::instance().bStaticTypecheck && StaticTypeChecker::checkPredicateReference(*_ref, m_ctx))
        return true;
    Predicates funcs;

    if (! m_ctx.getPredicates(_ref->getName(), funcs))
        assert(false);

    _ref->setType(createFresh(_ref->getType()));

    if (funcs.size() > 1) {
        tc::CompoundFormulaPtr pConstraint = std::make_shared<tc::CompoundFormula>();

        for (size_t i = 0; i < funcs.size(); ++ i) {
            PredicatePtr pPred = funcs.get(i);
            tc::Formulas & part = pConstraint->addPart();

            part.insert(std::make_shared<tc::Formula>(tc::Formula::EQUALS, pPred->getType(), _ref->getType()));
        }

        m_constraints->insert(pConstraint);
    } else {
        m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::EQUALS, funcs.get(0)->getType(), _ref->getType()));
    }

    return true;
}

bool Collector::visitFormulaCall(const FormulaCallPtr &_call) {
    if(Options::instance().bStaticTypecheck && StaticTypeChecker::checkFormulaCall(*_call))
        return true;
    _call->setType(_call->getTarget()->getResultType());
    for (size_t i = 0; i < _call->getArgs().size(); ++i)
        m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
            _call->getArgs().get(i)->getType(), _call->getTarget()->getParams().get(i)->getType()));
    return true;
}

bool Collector::visitFunctionCall(const FunctionCallPtr &_call) {
    if(Options::instance().bStaticTypecheck && StaticTypeChecker::checkFunctionCall(*_call, m_ctx))
        return true;
    PredicateTypePtr pType = std::make_shared<PredicateType>();

    _call->setType(createFresh(_call->getType()));
    pType->getOutParams().add(std::make_shared<Branch>());
    pType->getOutParams().get(0)->add(std::make_shared<Param>(L"", _call->getType()));

    for (size_t i = 0; i < _call->getArgs().size(); ++ i)
        pType->getInParams().add(std::make_shared<Param>(L"", _call->getArgs().get(i)->getType()));

    m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
            _call->getPredicate()->getType(), pType));
//    m_constraints->insert(std::make_shared<tc::Formula(tc::Formula::Equals,
//            _call.getPredicate()->getType(), pType));

    return true;
}

bool Collector::visitLambda(const LambdaPtr &_lambda) {
    if(Options::instance().bStaticTypecheck && StaticTypeChecker::checkLambda(*_lambda))
        return true;
    _lambda->setType(_lambda->getPredicate()->getType());
    return true;
}

bool Collector::visitBinder(const BinderPtr &_binder) {
    if(Options::instance().bStaticTypecheck && StaticTypeChecker::checkBinder(*_binder))
        return true;
    const auto
        pType = std::make_shared<PredicateType>(),
        pPredicateType = std::make_shared<PredicateType>();

    _binder->setType(createFresh(_binder->getType()));
    pType->getOutParams().add(std::make_shared<Branch>());
    pPredicateType->getOutParams().add(std::make_shared<Branch>());

    for (size_t i = 0; i < _binder->getArgs().size(); ++i)
        if (_binder->getArgs().get(i)) {
            pType->getInParams().add(std::make_shared<Param>(L"", _binder->getArgs().get(i)->getType()));
            pPredicateType->getInParams().add(std::make_shared<Param>(L"", _binder->getArgs().get(i)->getType()));
        } else
            pPredicateType->getInParams().add(std::make_shared<Param>(L"", std::make_shared<Type>(Type::TOP)));

    if (_binder->getPredicate()->getKind() == Expression::PREDICATE) {
        const auto pPredicate = _binder->getPredicate()->as<PredicateReference>()->getTarget();
        pType->getOutParams().assign(pPredicate->getOutParams());
        pPredicateType->getOutParams().assign(pPredicate->getOutParams());
    }

    m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
            pPredicateType, _binder->getPredicate()->getType()));

    return true;
}

bool Collector::visitCall(const CallPtr &_call) {
    if(Options::instance().bStaticTypecheck && StaticTypeChecker::checkCall(*_call, m_ctx))
        return true;
    PredicateTypePtr pType = std::make_shared<PredicateType>();

    for (size_t i = 0; i < _call->getArgs().size(); ++i)
        pType->getInParams().add(std::make_shared<Param>(L"", _call->getArgs().get(i)->getType()));

    for (size_t i = 0; i < _call->getBranches().size(); ++i) {
        CallBranch &br = *_call->getBranches().get(i);

        pType->getOutParams().add(std::make_shared<Branch>());

        for (size_t j = 0; j < br.size(); ++j)
            pType->getOutParams().get(i)->add(std::make_shared<Param>(L"", br.get(j)->getType()));
    }

    m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
            _call->getPredicate()->getType(), pType));

    return true;
}

bool Collector::visitLiteral(const LiteralPtr &_lit) {
    if(Options::instance().bStaticTypecheck && StaticTypeChecker::checkLiteral(*_lit))
        return true;
    switch (_lit->getLiteralKind()) {
        case Literal::UNIT:
            _lit->setType(std::make_shared<Type>(Type::UNIT));
            break;
        case Literal::NUMBER: {
            const Number &n = _lit->getNumber();

            if (n.isNat())
                _lit->setType(std::make_shared<Type>(Type::NAT, n.countBits(false)));
            else if (n.isInt())
                _lit->setType(std::make_shared<Type>(Type::INT, n.countBits(true)));
            else
                _lit->setType(std::make_shared<Type>(Type::REAL, n.countBits(false)));

            break;
        }
        case Literal::BOOL:
            _lit->setType(std::make_shared<Type>(Type::BOOL));
            break;
        case Literal::CHAR:
            _lit->setType(std::make_shared<Type>(Type::CHAR));
            break;
        case Literal::STRING:
            _lit->setType(std::make_shared<Type>(Type::STRING));
            break;
        default:
            break;
    }

    return true;
}

bool Collector::visitUnary(const UnaryPtr &_unary) {
    if(Options::instance().bStaticTypecheck && StaticTypeChecker::checkUnary(*_unary))
        return true;
    _unary->setType(createFresh(_unary->getType()));

    switch (_unary->getOperator()) {
        case Unary::MINUS:
            // Must contain negative values.
            m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                    std::make_shared<Type>(Type::INT, 1), _unary->getType()));
            // no break;
        case Unary::PLUS:
            m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                    _unary->getExpression()->getType(), _unary->getType()));
            m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                    _unary->getExpression()->getType(), std::make_shared<Type>(Type::REAL, Number::GENERIC)));
            m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                    _unary->getType(), std::make_shared<Type>(Type::REAL, Number::GENERIC)));
            break;
        case Unary::BOOL_NEGATE:
            //               ! x : A = y : B
            // ------------------------------------------------
            // (A = bool and B = bool) or (A = {C} and B = {C})
            {
                const auto p = std::make_shared<tc::CompoundFormula>();

                // Boolean negation.
                tc::Formulas &part1 = p->addPart();

                part1.insert(std::make_shared<tc::Formula>(tc::Formula::EQUALS,
                        _unary->getExpression()->getType(), std::make_shared<Type>(Type::BOOL)));
                part1.insert(std::make_shared<tc::Formula>(tc::Formula::EQUALS,
                        _unary->getType(), std::make_shared<Type>(Type::BOOL)));

                // Set negation.
                tc::Formulas &part2 = p->addPart();
                const auto pSet = std::make_shared<SetType>(createFresh());

                part2.insert(std::make_shared<tc::Formula>(tc::Formula::EQUALS,
                        _unary->getExpression()->getType(), pSet));
                part2.insert(std::make_shared<tc::Formula>(tc::Formula::EQUALS,
                        _unary->getType(), pSet));

                m_constraints->insert(p);
            }

            break;
        case Unary::BITWISE_NEGATE:
            m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::EQUALS,
                    _unary->getExpression()->getType(), _unary->getType()));
            m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                    _unary->getExpression()->getType(), std::make_shared<Type>(Type::INT, Number::GENERIC)));
    }

    return true;
}

bool Collector::visitBinary(const BinaryPtr &_binary) {
    if(Options::instance().bStaticTypecheck && StaticTypeChecker::checkBinary(*_binary))
        return true;
    _binary->setType(createFresh(_binary->getType()));

    switch (_binary->getOperator()) {
        case Binary::ADD:
        case Binary::SUBTRACT:
            //                       x : A + y : B = z :C
            // ----------------------------------------------------------------
            // (A <= C and B <= C and C <= real)
            //   or (A <= C and B <= C and C = {D})
            //   or (A <= C and B <= C and C = [[D]])   /* Operator "+" only. */
            //   or (A = T1[D1] & B = T2[D2] & C = T3[D3] & T1 <= T3 & T2 <= T3 & D1 <= D3 & D2 <= D3)   /* Operator "+" only. */
            {
                const auto p = std::make_shared<tc::CompoundFormula>();
                tc::Formulas & part1 = p->addPart();

                // Numeric operations.
                part1.insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                        _binary->getRightSide()->getType(),
                        _binary->getType()));
                part1.insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                        _binary->getLeftSide()->getType(),
                        _binary->getType()));
                part1.insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                        _binary->getType(),
                        std::make_shared<Type>(Type::REAL, Number::GENERIC)));

                // Set operations.
                tc::Formulas & part2 = p->addPart();
                const auto pSet = std::make_shared<SetType>(createFresh());

                part2.insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                        _binary->getLeftSide()->getType(),
                        _binary->getType()));
                part2.insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                        _binary->getRightSide()->getType(),
                        _binary->getType()));
                part2.insert(std::make_shared<tc::Formula>(tc::Formula::EQUALS,
                        _binary->getType(), pSet));

                if (_binary->getOperator() == Binary::ADD) {
                    tc::Formulas & part3 = p->addPart();
                    const auto pList = std::make_shared<ListType>(createFresh());

                    part3.insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                            _binary->getLeftSide()->getType(),
                            _binary->getType()));
                    part3.insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                            _binary->getRightSide()->getType(),
                            _binary->getType()));
                    part3.insert(std::make_shared<tc::Formula>(tc::Formula::EQUALS,
                            _binary->getType(), pList));

                    tc::Formulas & part4 = p->addPart();

                    ArrayTypePtr
                        pA = std::make_shared<ArrayType>(createFresh(), createFresh()),
                        pB = std::make_shared<ArrayType>(createFresh(), createFresh()),
                        pC = std::make_shared<ArrayType>(std::make_shared<tc::FreshType>(tc::FreshType::PARAM_OUT),
                            std::make_shared<tc::FreshType>(tc::FreshType::PARAM_OUT));

                    part4.insert(std::make_shared<tc::Formula>(tc::Formula::EQUALS,
                        _binary->getLeftSide()->getType(), pA));
                    part4.insert(std::make_shared<tc::Formula>(tc::Formula::EQUALS,
                        _binary->getRightSide()->getType(), pB));
                    part4.insert(std::make_shared<tc::Formula>(tc::Formula::EQUALS,
                        _binary->getType(), pC));

                    part4.insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                        pA->getBaseType(), pC->getBaseType()));
                    part4.insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                        pB->getBaseType(), pC->getBaseType()));
                    part4.insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                        pA->getDimensionType(), pC->getDimensionType()));
                    part4.insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                        pB->getDimensionType(), pC->getDimensionType()));
                }

                m_constraints->insert(p);
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
                const auto p = std::make_shared<tc::CompoundFormula>();
                tc::Formulas & part1 = p->addPart();
                tc::Formulas & part2 = p->addPart();

                part1.insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                        _binary->getLeftSide()->getType(),
                        _binary->getRightSide()->getType()));
                part1.insert(std::make_shared<tc::Formula>(tc::Formula::EQUALS,
                        _binary->getType(),
                        _binary->getRightSide()->getType()));
                part2.insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE_STRICT,
                        _binary->getRightSide()->getType(),
                        _binary->getLeftSide()->getType()));
                part2.insert(std::make_shared<tc::Formula>(tc::Formula::EQUALS,
                        _binary->getType(),
                        _binary->getLeftSide()->getType()));
                m_constraints->insert(p);
            }

            // Support only numbers for now.
            m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                    _binary->getLeftSide()->getType(), std::make_shared<Type>(Type::REAL, Number::GENERIC)));
            m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                    _binary->getRightSide()->getType(), std::make_shared<Type>(Type::REAL, Number::GENERIC)));
            m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                    _binary->getType(), std::make_shared<Type>(Type::REAL, Number::GENERIC)));

            break;

        case Binary::SHIFT_LEFT:
        case Binary::SHIFT_RIGHT:
            m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                    _binary->getLeftSide()->getType(), std::make_shared<Type>(Type::INT, Number::GENERIC)));
            m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                    _binary->getRightSide()->getType(), std::make_shared<Type>(Type::INT, Number::GENERIC)));
            m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                    _binary->getLeftSide()->getType(), _binary->getType()));
            break;

        case Binary::LESS:
        case Binary::LESS_OR_EQUALS:
        case Binary::GREATER:
        case Binary::GREATER_OR_EQUALS:
            m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                    _binary->getLeftSide()->getType(), std::make_shared<Type>(Type::REAL, Number::GENERIC)));
            m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                    _binary->getRightSide()->getType(), std::make_shared<Type>(Type::REAL, Number::GENERIC)));
            // no break;
        case Binary::EQUALS:
        case Binary::NOT_EQUALS:
            //       (x : A = y : B) : C
            // ----------------------------------
            //   C = bool and (A <= B or B < A)
            m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::EQUALS,
                    _binary->getType(), std::make_shared<Type>(Type::BOOL)));

            {
                const auto p = std::make_shared<tc::CompoundFormula>();

                p->addPart().insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                        _binary->getLeftSide()->getType(),
                        _binary->getRightSide()->getType()));
                p->addPart().insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE_STRICT,
                        _binary->getRightSide()->getType(),
                        _binary->getLeftSide()->getType()));

                //_binary.h

                m_constraints->insert(p);
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
                tc::CompoundFormulaPtr p = std::make_shared<tc::CompoundFormula>();
                tc::Formulas & part1 = p->addPart();
                tc::Formulas & part2 = p->addPart();
                tc::Formulas & part3 = p->addPart();

                part1.insert(std::make_shared<tc::Formula>(tc::Formula::EQUALS,
                        _binary->getLeftSide()->getType(), std::make_shared<Type>(Type::BOOL)));
                part1.insert(std::make_shared<tc::Formula>(tc::Formula::EQUALS,
                        _binary->getRightSide()->getType(), std::make_shared<Type>(Type::BOOL)));
                part1.insert(std::make_shared<tc::Formula>(tc::Formula::EQUALS,
                        _binary->getType(), std::make_shared<Type>(Type::BOOL)));

                part2.insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                        _binary->getLeftSide()->getType(),
                        _binary->getRightSide()->getType()));
                part2.insert(std::make_shared<tc::Formula>(tc::Formula::EQUALS,
                        _binary->getType(), _binary->getRightSide()->getType()));
                part2.insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                        _binary->getRightSide()->getType(),
                        std::make_shared<Type>(Type::INT, Number::GENERIC)));

                part3.insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE_STRICT,
                        _binary->getRightSide()->getType(),
                        _binary->getLeftSide()->getType()));
                part3.insert(std::make_shared<tc::Formula>(tc::Formula::EQUALS,
                        _binary->getType(),
                        _binary->getLeftSide()->getType()));
                part3.insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                        _binary->getLeftSide()->getType(),
                        std::make_shared<Type>(Type::INT, Number::GENERIC)));

                // Set operations.
                tc::Formulas & part4 = p->addPart();
                const auto pSet = std::make_shared<SetType>(createFresh());

                part4.insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                        _binary->getLeftSide()->getType(),
                        _binary->getType()));
                part4.insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                        _binary->getRightSide()->getType(),
                        _binary->getType()));
                part4.insert(std::make_shared<tc::Formula>(tc::Formula::EQUALS,
                        _binary->getType(), pSet));

                m_constraints->insert(p);
            }
            break;

        case Binary::IMPLIES:
        case Binary::IFF:
            m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::EQUALS,
                    _binary->getLeftSide()->getType(), std::make_shared<Type>(Type::BOOL)));
            m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::EQUALS,
                    _binary->getRightSide()->getType(), std::make_shared<Type>(Type::BOOL)));
            m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::EQUALS,
                    _binary->getType(), std::make_shared<Type>(Type::BOOL)));
            break;

        case Binary::IN:
            {
                const auto pSet = std::make_shared<SetType>(createFresh());

                m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::EQUALS,
                        _binary->getRightSide()->getType(), pSet));
                m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                        _binary->getLeftSide()->getType(), pSet->getBaseType()));
                m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::EQUALS,
                        _binary->getType(), std::make_shared<Type>(Type::BOOL)));
            }
            break;
    }

    return true;
}

bool Collector::visitTernary(const TernaryPtr &_ternary) {
    if(Options::instance().bStaticTypecheck && StaticTypeChecker::checkTernary(*_ternary))
        return true;
    _ternary->setType(createFresh(_ternary->getType()));
    m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE, _ternary->getIf()->getType(), std::make_shared<Type>(Type::BOOL)));
    m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE, _ternary->getThen()->getType(), _ternary->getType()));
    m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE, _ternary->getElse()->getType(), _ternary->getType()));
    return true;
}

bool Collector::visitFormula(const FormulaPtr &_formula) {
    if(Options::instance().bStaticTypecheck && StaticTypeChecker::checkFormula(*_formula))
        return true;
    _formula->setType(createFresh(_formula->getType()));
    const auto pFresh = std::make_shared<tc::FreshType>(tc::FreshType::PARAM_OUT);

    m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::EQUALS, _formula->getType(), std::make_shared<Type>(Type::BOOL)));
    m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE, pFresh, std::make_shared<Type>(Type::BOOL)));
    m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::EQUALS, pFresh, _formula->getSubformula()->getType()));
    return true;
}

bool Collector::visitArrayPartExpr(const ArrayPartExprPtr &_array) {
    if(Options::instance().bStaticTypecheck && StaticTypeChecker::checkArrayPartExpr(*_array))
        return true;
    _array->setType(std::make_shared<tc::FreshType>(tc::FreshType::PARAM_OUT));

    const auto pMapType = std::make_shared<MapType>(std::make_shared<Type>(Type::BOTTOM), _array->getType());
    const auto pListType = std::make_shared<ListType>(_array->getType());

    const auto pArrayType = std::make_shared<ArrayType>();
    auto pCurrentArray = pArrayType;

    for (size_t i=0; i<_array->getIndices().size(); ++i) {
        pCurrentArray->setDimensionType(std::make_shared<Type>(Type::BOTTOM));
        if (i+1 == _array->getIndices().size())
            continue;
        pCurrentArray->setBaseType(std::make_shared<ArrayType>());
        pCurrentArray = pCurrentArray->getBaseType()->as<ArrayType>();
    }
    pCurrentArray->setBaseType(_array->getType());

    if (_array->getIndices().size() > 1) {
        m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE, _array->getObject()->getType(), pArrayType));
        return true;
    }

    tc::CompoundFormulaPtr pFormula = std::make_shared<tc::CompoundFormula>();

    // A = a[i], I i
    // A <= B[_|_, _|_, ...]
    pFormula->addPart().insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE, _array->getObject()->getType(), pArrayType));

    // or A <= {_|_ : B}
    pFormula->addPart().insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE, _array->getObject()->getType(), pMapType));

    // or A <= [[B]] and I <= nat
    tc::Formulas& formulas = pFormula->addPart();
    formulas.insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE, _array->getObject()->getType(), pListType));
    formulas.insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE, _array->getIndices().get(0)->getType(), std::make_shared<Type>(Type::NAT, Number::GENERIC)));

    m_constraints->insert(pFormula);
    return true;
}

bool Collector::visitRecognizerExpr(const RecognizerExprPtr& _expr) {
    if(Options::instance().bStaticTypecheck && StaticTypeChecker::checkRecognizerExpr(*_expr))
        return true;
    _expr->setType(std::make_shared<Type>(Type::BOOL));

    const auto pUnionType = std::make_shared<UnionType>();
    pUnionType->getConstructors().add(
        std::make_shared<UnionConstructorDeclaration>(_expr->getConstructorName()));

    tc::FreshTypePtr pFields = createFresh();
    pUnionType->getConstructors().get(0)->setFields(pFields);

    m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE, pUnionType, _expr->getObject()->getType()));
    return true;
}

bool Collector::visitAccessorExpr(const AccessorExprPtr& _expr) {
    if(Options::instance().bStaticTypecheck && StaticTypeChecker::checkAccessorExpr(*_expr))
        return true;
    _expr->setType(createFresh(_expr->getType()));

    const auto pUnionType = std::make_shared<UnionType>();
    pUnionType->getConstructors().add(
        std::make_shared<UnionConstructorDeclaration>(_expr->getConstructorName()));
    pUnionType->getConstructors().get(0)->setFields(_expr->getType());

    m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE, pUnionType, _expr->getObject()->getType()));
    return true;
}

bool Collector::visitStructConstructor(const StructConstructorPtr &_cons) {
    if(Options::instance().bStaticTypecheck && StaticTypeChecker::checkStructConstructor(*_cons))
        return true;
    const auto pStruct = std::make_shared<StructType>();

    for (size_t i = 0; i < _cons->size(); ++i) {
        const auto pDef = _cons->get(i);
        const auto pField = std::make_shared<NamedValue>(pDef->getName());

        pField->setType(pDef->getValue()->getType());

        if (pDef->getName().empty())
            pStruct->getTypesOrd()->add(pField);
        else
            pStruct->getNamesSet()->add(pField);
        pDef->setField(pField);
    }

    assert(pStruct->getNamesSet()->empty() || pStruct->getTypesOrd()->empty());

    _cons->setType(pStruct);

    return true;
}

bool Collector::visitUnionConstructor(const UnionConstructorPtr &_cons) {
    if(Options::instance().bStaticTypecheck && StaticTypeChecker::checkUnionConstructor(*_cons))
        return true;
    const auto pUnion = std::make_shared<UnionType>();
    const auto pCons = std::make_shared<UnionConstructorDeclaration>(_cons->getName());

    pUnion->getConstructors().add(pCons);

    for (size_t i = 0; i < _cons->size(); ++i) {
        const auto pDef = _cons->get(i);
        const auto pField = std::make_shared<NamedValue>(pDef->getName());

        pField->setType(pDef->getValue()->getType());
        pCons->getStructFields()->getNamesOrd()->add(pField);
        pDef->setField(pField);
    }

    if (_cons->getType())
        m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::EQUALS, pUnion, _cons->getType()));

    _cons->setType(pUnion);

    return true;
}

bool Collector::visitSetConstructor(const SetConstructorPtr &_cons) {
    if(Options::instance().bStaticTypecheck && StaticTypeChecker::checkSetConstructor(*_cons))
        return true;
    const auto pSet = std::make_shared<SetType>(std::make_shared<tc::FreshType>(tc::FreshType::PARAM_OUT));
    _cons->setType(pSet);

    for (size_t i = 0; i < _cons->size(); ++i)
        m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                _cons->get(i)->getType(), pSet->getBaseType()));

    return true;
}

bool Collector::visitListConstructor(const ListConstructorPtr &_cons) {
    if(Options::instance().bStaticTypecheck && StaticTypeChecker::checkListConstructor(*_cons))
        return true;
    ListTypePtr pList = std::make_shared<ListType>(std::make_shared<tc::FreshType>(tc::FreshType::PARAM_OUT));

    _cons->setType(pList);

    for (size_t i = 0; i < _cons->size(); ++i)
        m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                _cons->get(i)->getType(), pList->getBaseType()));

    return true;
}

bool Collector::visitArrayConstructor(const ArrayConstructorPtr &_cons) {
    if(Options::instance().bStaticTypecheck && StaticTypeChecker::checkArrayConstructor(*_cons))
        return true;
    const auto pArray = std::make_shared<ArrayType>(std::make_shared<tc::FreshType>(tc::FreshType::PARAM_OUT));

    _cons->setType(pArray);

    if (_cons->empty()) {
        pArray->setDimensionType(createFresh());
        return true;
    }

    for (size_t i = 0; i < _cons->size(); ++i)
        m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
            _cons->get(i)->getValue()->getType(), pArray->getBaseType()));

    const auto pParamType = std::make_shared<tc::FreshType>(tc::FreshType::PARAM_OUT);
    const auto pEnumType = std::make_shared<Subtype>(std::make_shared<NamedValue>(L"", pParamType));
    bool bHaveIndices = false;

    for (size_t i = 0; i < _cons->size(); ++i)
        if (_cons->get(i)->getIndex()) {
            bHaveIndices = true;
            break;
        }

    if (!bHaveIndices) {
        const auto pUpperBound = std::make_shared<Literal>(Number::makeNat(_cons->size()));

        pUpperBound->setType(std::make_shared<Type>(Type::NAT,
                pUpperBound->getNumber().countBits(false)));
        m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::EQUALS,
                pParamType, pUpperBound->getType()));
        pEnumType->setExpression(std::make_shared<Binary>(Binary::LESS,
                std::make_shared<VariableReference>(pEnumType->getParam()), pUpperBound));

        for (size_t i = 0; i < _cons->size(); ++i) {
            LiteralPtr pIndex = std::make_shared<Literal>(Number::makeNat(i));
            pIndex->setType(std::make_shared<Type>(Type::NAT, pIndex->getNumber().countBits(false)));
            _cons->get(i)->setIndex(pIndex);
        }
    } else {
        bool bFirst = true;
        size_t nInc = 0;
        for (size_t i = 0; i < _cons->size(); ++i) {
            if (_cons->get(i)->getIndex()) {
                const auto pIndexType = _getContentsType(_cons->get(i)->getIndex());
                m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE, pIndexType, pParamType));
                const auto pExpr = _getConditionForIndex(_cons->get(i)->getIndex(),
                        std::make_shared<VariableReference>(pEnumType->getParam()), false);

                pEnumType->setExpression(pEnumType->getExpression()
                    ? std::make_shared<Binary>(Binary::BOOL_AND, pEnumType->getExpression(), pExpr)
                    : pExpr);

                if (pIndexType->getKind() == Type::SUBTYPE)
                    m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                        pIndexType->as<Subtype>()->getParam()->getType(), pEnumType->getParam()->getType()));
            }
            else {
                PredicatePtr pIndexer = Builtins::instance().find(bFirst ? L"zero" : L"inc");
                assert(pIndexer);

                PredicateReferencePtr pReference = std::make_shared<PredicateReference>(pIndexer);
                pReference->setType(std::make_shared<PredicateType>(pIndexer->as<AnonymousPredicate>()));

                FunctionCallPtr pCallIndexer = std::make_shared<FunctionCall>(pReference);
                pCallIndexer->getArgs().add(std::make_shared<TypeExpr>(pEnumType));

                if (!bFirst)
                    pCallIndexer->getArgs().add(std::make_shared<Literal>(Number(intToStr(++nInc), Number::INTEGER)));

                bFirst = false;
                _cons->get(i)->setIndex(pCallIndexer);
            }
        }

        if (!pEnumType->getExpression())
            pEnumType->setExpression(std::make_shared<Literal>(true));
    }

    if (_cons->size() == 1 && _cons->get(0)->getIndex()->getType()) {
        pArray->setDimensionType(_cons->get(0)->getIndex()->getType());
        return true;
    }

    const VariableReferencePtr pParam = std::make_shared<VariableReference>(std::make_shared<NamedValue>(L"", pParamType));
    SubtypePtr pSubtype = std::make_shared<Subtype>(pParam->getTarget());
    pArray->setDimensionType(pSubtype);

    ExpressionPtr pExpr = NULL;
    for (size_t i = 0; i < _cons->size(); ++i) {
        const auto pEquals = _getConditionForIndex(_cons->get(i)->getIndex(), pParam, true);
        pExpr = pExpr ? std::make_shared<Binary>(Binary::BOOL_OR, pExpr, pEquals) : pEquals;
    }

    pSubtype->setExpression(pExpr);

    return true;
}

bool Collector::visitArrayIteration(const ArrayIterationPtr& _iter) {
    if(Options::instance().bStaticTypecheck && StaticTypeChecker::checkArrayIteration(*_iter))
        return true;
    auto pArrayType = std::make_shared<ArrayType>();
    _iter->setType(pArrayType);

    std::vector<TypePtr> dimensions;
    const bool bUnknownDimensionType = bool(_iter->getDefault());
    for (size_t i = 0; i < _iter->getIterators().size(); ++i)
        dimensions.push_back(!bUnknownDimensionType ?
                std::make_shared<tc::FreshType>(tc::FreshType::PARAM_OUT)->as<Type>() : std::make_shared<Type>(Type::TOP));

    TypePtr pBaseType = std::make_shared<tc::FreshType>(tc::FreshType::PARAM_OUT);

    for (size_t i = 0; i < _iter->size(); ++i) {
        const Collection<Expression>& conds = _iter->get(i)->getConditions();
        for (size_t j = 0; j < conds.size(); ++j) {
            if (conds.get(j)->getKind() == Expression::CONSTRUCTOR
                && conds.get(j)->as<Constructor>()->getConstructorKind() == Constructor::STRUCT_FIELDS)
            {
                const StructConstructor& constructor = *conds.get(j)->as<StructConstructor>();
                if (_iter->getIterators().size() != constructor.size())
                    throw std::runtime_error("Count of iterators does not match count of fields.");

                for (size_t k = 0; k < constructor.size(); ++k) {
                    m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                        _getContentsType(constructor.get(k)->getValue()), dimensions[k]));

                    auto pIterType = _iter->getIterators().get(k)->getType();

                    if (pIterType->getKind() != Type::GENERIC)
                        m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                            _getContentsType(constructor.get(k)->getValue()), pIterType));
                    else
                        _iter->getIterators().get(k)->setType(dimensions[k]);
                }

                continue;
            }

            assert(_iter->getIterators().size() == 1);

            m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                _getContentsType(conds.get(j)), dimensions[0]));

            const auto pIterType = _iter->getIterators().get(0)->getType();

            if (pIterType->getKind() != Type::GENERIC)
                m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                    _getContentsType(conds.get(j)), pIterType));
            else
                _iter->getIterators().get(0)->setType(dimensions[0]);
        }

        m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
            _getContentsType(_iter->get(i)->getExpression()), pBaseType));
    }

    if (_iter->getDefault())
        m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
            _getContentsType(_iter->getDefault()), pBaseType));

    for (std::vector<TypePtr>::iterator i = dimensions.begin();; ++i) {
        pArrayType->setDimensionType(*i);

        if (i == --dimensions.end())
            break;

        pArrayType->setBaseType(std::make_shared<ArrayType>());
        pArrayType = pArrayType->getBaseType()->as<ArrayType>();
    }

    pArrayType->setBaseType(pBaseType);

    return true;
}

bool Collector::visitMapConstructor(const MapConstructorPtr &_cons) {
    if(Options::instance().bStaticTypecheck && StaticTypeChecker::checkMapConstructor(*_cons))
        return true;
    const auto pMap = std::make_shared<MapType>();

    pMap->setBaseType(createFresh());
    pMap->setIndexType(createFresh());
    pMap->getBaseType()->as<tc::FreshType>()->setFlags(tc::FreshType::PARAM_OUT);
    pMap->getIndexType()->as<tc::FreshType>()->setFlags(tc::FreshType::PARAM_OUT);
    _cons->setType(pMap);

    for (size_t i = 0; i < _cons->size(); ++i) {
        ElementDefinitionPtr pElement = _cons->get(i);

        m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                pElement->getIndex()->getType(), pMap->getIndexType()));
        m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                pElement->getValue()->getType(), pMap->getBaseType()));
    }

    return true;
}

bool Collector::visitFieldExpr(const FieldExprPtr &_field) {
    if(Options::instance().bStaticTypecheck && StaticTypeChecker::checkFieldExpr(*_field))
        return true;
    const auto pFresh = createFresh(_field->getType());
    const auto pField = std::make_shared<NamedValue>(_field->getFieldName(), pFresh);
    const auto pStruct = std::make_shared<StructType>();

    _field->setType(pFresh);
    pStruct->getNamesSet()->add(pField);

    if (_field->getObject()->getType()->getKind() == Type::FRESH)
        pFresh->setFlags(_field->getObject()->getType()->as<tc::FreshType>()->getFlags());

    // (x : A).foo : B  |-  A <= struct(B foo)
    m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
            _field->getObject()->getType(), pStruct));
    m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::EQUALS,
            _field->getType(), pField->getType()));

    return true;
}

bool Collector::visitCastExpr(const CastExprPtr &_cast) {
    if(Options::instance().bStaticTypecheck && StaticTypeChecker::checkCastExpr(*_cast))
        return true;
    const auto pToType = _cast->getToType()->getContents()->as<Type>();

    _cast->setType(pToType);

    if (pToType->getKind() == Type::STRUCT &&
            _cast->getExpression()->getKind() == Expression::CONSTRUCTOR &&
            _cast->getExpression()->as<Constructor>()->getConstructorKind() == Constructor::STRUCT_FIELDS)
    {
        // We can cast form tuples to structs explicitly.
        const auto pStruct = pToType->as<StructType>();
        const auto pFields = _cast->getExpression()->as<StructConstructor>();
        bool bSuccess = true;

        // TODO: maybe use default values for fields.
        if (pStruct->getNamesOrd()->size() == pFields->size()) {
            for (size_t i = 0; i < pFields->size(); ++i) {
                StructFieldDefinitionPtr pDef = pFields->get(i);
                size_t cOtherIdx = pDef->getName().empty() ? i : pStruct->getNamesOrd()->findByNameIdx(pDef->getName());

                if (cOtherIdx == (size_t)-1) {
                    bSuccess = false;
                    break;
                }

                NamedValuePtr pField = pStruct->getNamesOrd()->get(cOtherIdx);

                m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                        pDef->getValue()->getType(), pField->getType()));
            }
        } else if (pStruct->getTypesOrd()->size() == pFields->size()) {
            for (size_t i = 0; i < pFields->size(); ++i) {
                const auto pDef = pFields->get(i);
                const auto pField = pStruct->getTypesOrd()->get(i);

                m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                        pDef->getValue()->getType(), pField->getType()));
            }
        }

        if (bSuccess)
            return true;
    }

    // (A)(x : B) |- B <= A
    m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
            _cast->getExpression()->getType(), pToType));

    return true;
}

bool Collector::visitReplacement(const ReplacementPtr &_repl) {
    if(Options::instance().bStaticTypecheck && StaticTypeChecker::checkReplacement(*_repl))
        return true;
    _repl->setType(createFresh(_repl->getType()));

    m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
        _repl->getType(), _repl->getObject()->getType()));
    m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
        _repl->getType(), _repl->getNewValues()->getType()));

    return true;
}

bool Collector::visitAssignment(const AssignmentPtr &_assignment) {
    if(Options::instance().bStaticTypecheck && StaticTypeChecker::checkAssignment(*_assignment))
        return true;
    // x : A = y : B |- B <= A
    m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
            _assignment->getExpression()->getType(),
            _assignment->getLValue()->getType()));
    return true;
}

int Collector::handleVarDeclVar(NodePtr &_node) {
    collectParam(_node->as<NamedValue>(), tc::FreshType::PARAM_OUT);
    return 0;
}

int Collector::handleSubtypeParam(NodePtr &_node) {
    collectParam(_node->as<NamedValue>(), 0);
    return 0;
}

bool Collector::visitVariableDeclaration(const VariableDeclarationPtr &_var) {
    if(Options::instance().bStaticTypecheck && StaticTypeChecker::checkVariableDeclaration(*_var))
        return true;
    if (_var->getValue())
        // x : A = y : B |- B <= A
        m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
                _var->getValue()->getType(),
                _var->getVariable()->getType()));
    return true;
}

bool Collector::visitIf(const IfPtr &_if) {
    if(Options::instance().bStaticTypecheck && StaticTypeChecker::checkIf(*_if))
        return true;
    m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::EQUALS,
            _if->getArg()->getType(), std::make_shared<Type>(Type::BOOL)));
    return true;
}

int Collector::handlePredicateInParam(NodePtr &_node) {
    collectParam(_node->as<NamedValue>(), tc::FreshType::PARAM_IN);
    return 0;
}

int Collector::handlePredicateOutParam(NodePtr &_node) {
    collectParam(_node->as<NamedValue>(), tc::FreshType::PARAM_OUT);
    return 0;
}

int Collector::handleFormulaBoundVariable(NodePtr &_node) {
    collectParam(_node->as<NamedValue>(), tc::FreshType::PARAM_IN);
    return 0;
}

int Collector::handleParameterizedTypeParam(NodePtr &_node) {
    collectParam(_node->as<NamedValue>(), 0);
    return 0;
}

bool Collector::visitTypeExpr(const TypeExprPtr &_expr) {
    if(Options::instance().bStaticTypecheck && StaticTypeChecker::checkTypeExpr(*_expr))
        return true;
    if (_expr->getContents()->getKind() == Type::RANGE || _expr->getContents()->getKind() == Type::SUBTYPE) {
        _expr->setType(_expr->getContents());
    } else {
        const auto pType = std::make_shared<TypeType>();
        pType->setDeclaration(std::make_shared<TypeDeclaration>(L"", _expr->getContents()));
        _expr->setType(pType);
    }
    return true;
}

bool Collector::traverseSwitch(const SwitchPtr &_stmt) {
    m_switches.push(_stmt);
    const bool b = Visitor::traverseSwitch(_stmt);
    m_switches.pop();
    return b;
}

int Collector::handleSwitchCaseValuePost(NodePtr &_node) {
    const auto pExpr = _node->as<Expression>();

    if (m_switches.top()->getArg())
        m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
            _getContentsType(pExpr), m_switches.top()->getArg()->getType()));
    else if (m_switches.top()->getParamDecl())
        m_constraints->insert(std::make_shared<tc::Formula>(tc::Formula::SUBTYPE,
            _getContentsType(pExpr), m_switches.top()->getParamDecl()->getVariable()->getType()));

    return 0;
}

namespace {

class Resolver : public Visitor {
    Collector &m_collector;
    Cloner &m_cloner;
    bool &m_bModified;
public:
    Resolver(Collector &_collector, Cloner &_cloner, bool &_bModified) :
        Visitor(CHILDREN_FIRST), m_collector(_collector), m_cloner(_cloner),
        m_bModified(_bModified)
    {
        m_bModified = false;
    }

    bool visitNamedReferenceType(const NamedReferenceTypePtr &_type) override {
        NodeSetter *pSetter = getNodeSetter();
        if (pSetter == NULL)
            return true;

        for (auto i: m_path) {
            if (i.pNode == _type->getDeclaration())
                return true;
            if (i.pNode && _type->getDeclaration() && _type->getDeclaration()->getType() &&
                *i.pNode == *_type->getDeclaration()->getType())
                return true;
        }

        auto pDeclType = _type->getDeclaration()->getType();

        if (!pDeclType && Options::instance().typeCheck == TC_PREPROCESS)
            return true;

        m_bModified = true;

        if (!pDeclType || (pDeclType->getKind() == Type::FRESH &&
                !m_cloner.isKnown(pDeclType)))
        {
            TypePtr pFresh = m_collector.createFresh();

            _type->getDeclaration()->setType(pFresh);
            tc::ContextStack::top()->namedTypes.insert(std::make_pair(pFresh->as<tc::FreshType>(), _type));

            if (pDeclType)
                m_cloner.inject(pFresh, pDeclType);
            else {
                m_cloner.inject(pFresh);
                pDeclType = pFresh;
            }
        }

        if (_type->getArgs().empty()) {
            pSetter->set(m_cloner.get(pDeclType));
            return true;
        }

        assert(pDeclType->getKind() == Type::PARAMETERIZED);
        ParameterizedTypePtr pOrigType = pDeclType->as<ParameterizedType>();
        TypePtr pType = m_cloner.get(pOrigType->getActualType());

        for (size_t i = 0; i < pOrigType->getParams().size(); ++i) {
            TypePtr pParamType = pOrigType->getParams().get(i)->getType();
            if (pParamType->getKind() != Type::TYPE) {
                ExpressionPtr pParamReference = std::make_shared<VariableReference>(
                        pOrigType->getParams().get(i));
                ExpressionPtr pArgument = _type->getArgs().get(i);
                pType = Expression::substitute(pType, pParamReference, pArgument)->as<Type>();
            } else {
                TypePtr pParamReference = std::make_shared<NamedReferenceType>(
                        pParamType->as<TypeType>()->getDeclaration());
                TypePtr pArgument = _type->getArgs().get(i)->as<TypeExpr>()->getContents();
                pType->rewrite(pParamReference, pArgument);
            }
        }

        pSetter->set(pType);
        return true;
    }
};

static
void _resolveNamedReferenceTypes(const NodePtr &_node, Collector &_collector) {
    Cloner cloner;
    bool bModified = false;

    do
        Resolver(_collector, cloner, bModified).traverseNode(_node);
    while (bModified);
}

}

tc::ContextPtr tc::collect(const tc::FormulasPtr &_constraints, const NodePtr &_node, ir::Context &_ctx) {
    Collector collector(_constraints, _ctx);

    tc::ContextStack::push(_constraints);
    _resolveNamedReferenceTypes(_node, collector);

    if (Options::instance().typeCheck != TC_PREPROCESS)
        collector.traverseNode(_node);

    tc::ContextPtr pContext = tc::ContextStack::top();
    tc::ContextStack::pop();

    return pContext;
}
