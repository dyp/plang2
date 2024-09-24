#include <iostream>
#include <functional>
#include <algorithm>
#include <sstream>

#include "static_typecheck.h"
#include "options.h"
#include "pp_syntax.h"
#include "prettyprinter.h"



bool StaticTypeChecker::checkAccessorExpr(AccessorExpr& accessorExpr) {
    printTypecheckInfo(L"Start check for: ", str(accessorExpr), PRINT_BLUE, 1);
    printTypecheckInfo(L"Static type checking failed", L"", PRINT_RED, 2);
    return false;
}

bool StaticTypeChecker::checkArrayConstructor(ArrayConstructor& arrayConstructor) {
    printTypecheckInfo(L"Start check for: ", str(arrayConstructor), PRINT_BLUE, 1);
    if (std::any_of(arrayConstructor.begin(), arrayConstructor.end(), [](ElementDefinitionPtr x){ 
        return isFresh(x->getValue()->getType());
    })) {
        printTypecheckInfo(L"Static type checking failed", L"", PRINT_RED, 2);
        return false;
    }
    auto arrayBaseType = std::make_shared<Type>(Type::BOTTOM);
    for (const auto& i: arrayConstructor)
        arrayBaseType = getTypeJoin(arrayBaseType, i->getValue()->getType());
    bool haveIndices = std::all_of(arrayConstructor.begin(), arrayConstructor.end(),
                    [](ElementDefinitionPtr x){if (x->getIndex()) return true; return false;});
    if (!haveIndices) {
        const auto upperBound = std::make_shared<Literal>(Number::makeNat(arrayConstructor.size()));
        upperBound->setType(std::make_shared<Type>(Type::NAT, upperBound->getNumber().countBits(false)));
        const auto arrayDimensionType = std::make_shared<Subtype>(std::make_shared<NamedValue>(L"", upperBound->getType()));
        arrayDimensionType->setExpression(std::make_shared<Binary>(Binary::LESS,
                                                     std::make_shared<VariableReference>(arrayDimensionType->getParam()),
                                                     upperBound));
        for (size_t i = 0; i < arrayConstructor.size(); ++i) {
            const auto index = std::make_shared<Literal>(Number::makeNat(i));
            index->setType(std::make_shared<Type>(Type::NAT, index->getNumber().countBits(false)));
            arrayConstructor.get(i)->setIndex(index);
        }
        typeWarning(L"array constructor", arrayBaseType->getKind() != Type::BOTTOM &&
                                         arrayBaseType->getKind() != Type::TOP);
        typeWarning(L"array constructor", arrayDimensionType->getKind() != Type::BOTTOM &&
                                         arrayDimensionType->getKind() != Type::TOP);
        arrayConstructor.setType(std::make_shared<ArrayType>(arrayBaseType, arrayDimensionType));
    } else {
        auto paramType = std::make_shared<Type>(Type::BOTTOM);
        for (const auto& i : arrayConstructor)
            if (i->getIndex()) {
                if(isFresh(i->getIndex()->getType())) {
                    printTypecheckInfo(L"Static type checking failed", L"", PRINT_RED, 2);
                    return false;
                } else
                    paramType = getTypeJoin(paramType, i->getIndex()->getType());
            }
        const auto param = std::make_shared<VariableReference>(L"", std::make_shared<NamedValue>(L"", paramType));
        const auto arrayDimensionType = std::make_shared<Subtype>(param->getTarget());
        ExpressionPtr subtypeExpression = std::make_shared<Literal>(false);
        ExpressionPtr lastIndex = std::make_shared<Literal>(Number::makeNat(0));

        for (const auto& i : arrayConstructor)
            if (i->getIndex()) {
                subtypeExpression = std::make_shared<Binary>(Binary::BOOL_OR, subtypeExpression,
                                               std::make_shared<Binary>(Binary::EQUALS, param, i->getIndex()));
                lastIndex = i->getIndex();
            } else {
                subtypeExpression = std::make_shared<Binary>(Binary::BOOL_OR, subtypeExpression,
                                               std::make_shared<Binary>(Binary::EQUALS, param,
                                                          std::make_shared<Binary>(Binary::ADD, lastIndex,
                                                                     std::make_shared<Literal>(Number::makeNat(1)))));
            }
        arrayDimensionType->setExpression(subtypeExpression);
        typeWarning(L"array constructor", arrayBaseType->getKind() != Type::BOTTOM &&
                                          arrayBaseType->getKind() != Type::TOP);
        typeWarning(L"array constructor", arrayDimensionType->getKind() != Type::BOTTOM &&
                                          arrayDimensionType->getKind() != Type::TOP);
        arrayConstructor.setType(std::make_shared<ArrayType>(arrayBaseType, arrayDimensionType));
    }
    return true;
}

static const TypePtr _getContentsType(const ExpressionPtr _pExpr) {
    return _pExpr->getType() && _pExpr->getType()->getKind() == Type::TYPE
        ? _pExpr->as<TypeExpr>()->getContents()
        : _pExpr->getType();
}

bool StaticTypeChecker::checkArrayIteration(ArrayIteration &_iter) {
    printTypecheckInfo(L"Start check for: ", str(_iter), PRINT_BLUE, 1);
    auto pArrayType = std::make_shared<ArrayType>();
    _iter.setType(pArrayType);

    typeError("array iteration should have >= 1 cases", !_iter.empty());

    auto pBaseType = _getContentsType(_iter.get(0)->getExpression());
    if (isFresh(pBaseType)) {
        printTypecheckInfo(L"Static type checking failed", L"expression type is undefined in array iteration", PRINT_RED, 2);
        return false;
    }
    for (size_t i = 1; i < _iter.size(); ++i) {
        const auto pType = _getContentsType(_iter.get(i)->getExpression());
        if (isFresh(pType)) {
            printTypecheckInfo(L"Static type checking failed", L"expression type is undefined in array iteration", PRINT_RED, 2);
            return false;
        }
        if (!isSubtype(pBaseType, pType)) {
            if (isSubtype(pType, pBaseType)) {
                pBaseType = pType;
            } else {
                typeError("array iteration case expression types should extend type", false);
            }
        }
    }

    for (size_t i = 0; i < _iter.size(); ++i) {
        const auto& conds = _iter.get(i)->getConditions();
        for (size_t j = 0; j < conds.size(); ++j) {
            if (conds.get(j)->getKind() == Expression::CONSTRUCTOR
                && conds.get(j)->as<Constructor>()->getConstructorKind() == Constructor::STRUCT_FIELDS)
            {
                const auto& constructor = *conds.get(j)->as<StructConstructor>();
                typeError("Count of iterators does not match count of fields", _iter.getIterators().size() == constructor.size());

                for (size_t k = 0; k < constructor.size(); ++k) {
                    if (isFresh(_iter.getIterators().get(k)->getType())) {
                        const auto ctorType = _getContentsType(constructor.get(k)->getValue());
                        if (isFresh(ctorType)) {
                            printTypecheckInfo(L"Static type checking failed", L"array ctor index type is undefined", PRINT_RED, 2);
                            return false;
                        }
                    }

                    const auto pIterType = _iter.getIterators().get(k)->getType();
                    const auto ctorType = _getContentsType(constructor.get(k)->getValue());

                    if (pIterType->getKind() != Type::GENERIC) {
                        if (ctorType->getKind() != Type::GENERIC) {
                            typeError("array iteration type mismatch ctor params", isSubtype(pIterType, ctorType) || isSubtype(ctorType, pIterType));//TODO:dyp: maybe we need to set type of ctor from iteration type
                        } else {
                            constructor.get(k)->getValue()->setType(pIterType);
                        }
                    } else {
                        _iter.getIterators().get(k)->setType(ctorType);
                    }
                }

                continue;
            }

            typeError("array iteration should have size 1 for non struct types", _iter.getIterators().size() == 1);

            typeError("array iteration index type is not compatible", isSubtype(_getContentsType(conds.get(j)), _iter.getIterators().get(0)->getType()));

            const auto pIterType = _iter.getIterators().get(0)->getType();

            if (pIterType->getKind() != Type::GENERIC)
                typeError("array iteration type mismatch condition", isSubtype(_getContentsType(conds.get(j)), pIterType));
            else
                _iter.getIterators().get(0)->setType(_getContentsType(conds.get(j)));
        }

        typeError("array iteration base type mismatch expression", isSubtype(pBaseType, _getContentsType(_iter.get(i)->getExpression())));
    }

    if (_iter.getDefault()) {
        const auto pType = _getContentsType(_iter.getDefault());
        if (!isSubtype(pBaseType, pType)) {
            if (isSubtype(pType, pBaseType)) {
                pBaseType = pType;
            } else {
                typeError("array iteration base type mismatch default expression", false);
            }
        }
    }

    std::vector<TypePtr> dimensions;
    for (size_t i = 0; i < _iter.getIterators().size(); ++i)
        dimensions.push_back(_iter.getIterators().get(i)->getType());

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

bool StaticTypeChecker::checkArrayPartExpr(ArrayPartExpr &arrayPartExpr) {
    printTypecheckInfo(L"Start check for: ", str(arrayPartExpr), PRINT_BLUE, 1);
    for (auto i : arrayPartExpr.getIndices()) {
        if (isFresh(i->getType())) {
            const auto objectType = arrayPartExpr.getObject()->getType();
            if (isFresh(objectType)) {
                printTypecheckInfo(L"Static type checking failed", L"Array type is undefined", PRINT_RED, 2);
                return false;
            }
            if (objectType->getKind() != Type::ARRAY) {
                printTypecheckInfo(L"Static type checking failed", L"Array part base type is not array", PRINT_RED, 2);
                return false;
            }
            const auto arrayType = objectType->as<ArrayType>();
            i->setType(arrayType->getDimensionType());
        }
    }

    if (isFresh(arrayPartExpr.getObject()->getType()) ||
        std::any_of(arrayPartExpr.getIndices().begin(), arrayPartExpr.getIndices().end(),
                    [](ExpressionPtr x){return isFresh(x->getType());})) {
        printTypecheckInfo(L"Static type checking failed", L"", PRINT_RED, 2);
        return false;
    }
    if (arrayPartExpr.getObject()->getType()->getKind() == Type::ARRAY) {
        for (const auto& i: arrayPartExpr.getIndices())
            typeError("type of dimension",isSubtype(i->getType(), std::make_shared<Type>(Type::INT, Number::GENERIC)));
        const auto arrayType = arrayPartExpr.getObject()->getType()->as<ArrayType>();
        Collection<Type> dimensionTypes;
        arrayType->getDimensions(dimensionTypes);
        if (arrayType->getDimensionsCount() != arrayPartExpr.getIndices().size())
            typeError("dimension count");
        for (size_t i = 0; i < arrayType->getDimensionsCount(); i++) {
            TypePtr dimensionType = dimensionTypes.get(i);
            typeWarning(L"type of dimension", isContains(arrayPartExpr.getIndices().get(i), dimensionType));
        }
        TypePtr arrayBaseType = arrayType->getBaseType();
        setType(arrayPartExpr, arrayBaseType);
    }
    if (arrayPartExpr.getObject()->getType()->getKind() == Type::MAP) {
        const auto mapType = arrayPartExpr.getObject()->getType()->as<MapType>();
        typeError("map type", arrayPartExpr.getIndices().size() == 1);
        typeError("map type", isSubtype(arrayPartExpr.getIndices().get(0)->getType(), mapType->getIndexType()));
        const auto mapBaseType = mapType->getBaseType();
        setType(arrayPartExpr, mapBaseType);
    }
    if (arrayPartExpr.getObject()->getType()->getKind() == Type::LIST) { 
        typeError("list type", arrayPartExpr.getIndices().size() == 1);
        typeError("list type", isSubtype(arrayPartExpr.getIndices().get(0)->getType(), 
                                         std::make_shared<Type>(Type::NAT, Number::GENERIC)));
        const auto listBaseType = arrayPartExpr.getObject()->getType()->as<ListType>()->getBaseType();
        setType(arrayPartExpr, listBaseType);
    }
    return true;
}

bool StaticTypeChecker::checkArrayType(ArrayType &arrayType) {
    printTypecheckInfo(L"Start check for: ", str(arrayType), PRINT_BLUE, 1);
    Collection<Type> dimensionTypes;
    arrayType.getDimensions(dimensionTypes);
    if (std::any_of(dimensionTypes.begin(), dimensionTypes.end(), isFresh)) {
        printTypecheckInfo(L"Static type checking failed", L"", PRINT_RED, 2);
        return false;
    }
    for (const auto& i : dimensionTypes)
        typeError("array dimension", 
                  isSubtype(i, std::make_shared<Type>(Type::INT, Number::GENERIC)));
    return true;
}

bool StaticTypeChecker::checkAssignment(Assignment &assignment) {
    printTypecheckInfo(L"Start check for: ", str(assignment), PRINT_BLUE, 1);
    TypePtr typeOfLeft = assignment.getLValue()->getType();
    TypePtr typeOfRight = assignment.getExpression()->getType();
    if (isFresh(typeOfLeft) || isFresh(typeOfRight)) {
        printTypecheckInfo(L"Static type checking failed", L"", PRINT_RED, 2);
        return false;
    }
    typeError("assignment", isContains(assignment.getExpression(), typeOfLeft));
    return true;
}

bool StaticTypeChecker::checkBinary(Binary &binary) {
    printTypecheckInfo(L"Start check for: ", str(binary), PRINT_BLUE, 1);
    TypePtr typeOfLeft = binary.getLeftSide()->getType();
    TypePtr typeOfRight = binary.getRightSide()->getType();
    if (isFresh(typeOfLeft) || isFresh(typeOfRight)) {
        printTypecheckInfo(L"Static type checking failed", L"", PRINT_RED, 2);
        return false;
    }
    TypePtr joinType = getTypeJoin(typeOfLeft, typeOfRight);
    switch (binary.getOperator()) {
        case Binary::ADD:
            switch (joinType->getKind()) {
                case Type::REAL:
                case Type::INT:
                case Type::NAT:
                case Type::LIST:
                case Type::SET:
                case Type::ARRAY:
                    setType(binary, joinType);
                    return true;
                default:
                    typeError("binary plus");
            }
        case Binary::SUBTRACT:
            switch (joinType->getKind()) {
                case Type::REAL:
                case Type::INT:
                case Type::SET:
                    setType(binary, joinType);
                    return true;
                case Type::NAT:
                    setType(binary, std::make_shared<Type>(Type::INT, Number::GENERIC));
                    return true;
                default:
                    typeError("binary minus");
            }
        case Binary::MULTIPLY:
        case Binary::DIVIDE:
            switch (joinType->getKind()) {
                case Type::NAT:
                case Type::INT:
                case Type::REAL:
                    setType(binary, joinType);
                    return true;
                default:
                    typeError("binary operation");
            }
        case Binary::LESS:
        case Binary::LESS_OR_EQUALS:
        case Binary::GREATER:
        case Binary::GREATER_OR_EQUALS:
            switch (joinType->getKind()) {
                case Type::NAT:
                case Type::INT:
                case Type::REAL:
                case Type::ENUM:
                case Type::CHAR:
                    setType(binary, std::make_shared<Type>(Type::BOOL));
                    return true;
                default:
                    typeError("binary operation");
            }
        case Binary::IMPLIES:
        case Binary::IFF:
            switch (joinType->getKind()) {
                case Type::BOOL:
                    setType(binary, joinType);
                    return true;
                default:
                    typeError("binary operation");
            }
        case Binary::BOOL_AND:
        case Binary::BOOL_OR:
        case Binary::BOOL_XOR:
        case Binary::BITWISE_AND:
        case Binary::BITWISE_OR:
        case Binary::BITWISE_XOR:
            switch (joinType->getKind()) {
                case Type::SET:
                case Type::NAT:
                case Type::INT:
                case Type::BOOL:
                    setType(binary, joinType);
                    return true;
                default:
                    typeError("binary operation");
            }
        case Binary::REMAINDER:
        case Binary::SHIFT_LEFT:
        case Binary::SHIFT_RIGHT:
            switch (joinType->getKind()) {
                case Type::NAT:
                case Type::INT:
                    setType(binary, joinType);
                    return true;
                default:
                    typeError("binary operation");
            }
        case Binary::EQUALS:
        case Binary::NOT_EQUALS:
            if (isSubtype(typeOfLeft, typeOfRight) || isSubtype(typeOfRight, typeOfLeft)) {
                setType(binary, std::make_shared<Type>(Type::BOOL));
                return true;
            }
            else
                typeError("binary operation");
        case Binary::POWER:
            if ((typeOfRight->getKind() == Type::INT || typeOfRight->getKind() == Type::NAT) &&
                    (typeOfLeft->getKind() == Type::NAT || typeOfLeft->getKind() == Type::INT ||
                            typeOfLeft->getKind() == Type::REAL)) {
                setType(binary, typeOfLeft);
                return true;
            }
            else
                typeError("binary operation");
        case Binary::IN:
            if (typeOfRight->getKind() == Type::SET) { ;
                TypePtr BaseType = typeOfRight->as<SetType>()->getBaseType();
                if (isSubtype(typeOfLeft, BaseType)) {
                    setType(binary, std::make_shared<Type>(Type::BOOL));
                    return true;
                }
            }
            typeError("binary operation");
    }
    return true;
}

bool StaticTypeChecker::checkBinder(Binder &binder) {
    printTypecheckInfo(L"Start check for: ", str(binder), PRINT_BLUE, 1);
    printTypecheckInfo(L"Static type checking failed", L"", PRINT_RED, 2);
    return false;

}

bool StaticTypeChecker::checkCall(Call &call, const ir::Context &context) {
    printTypecheckInfo(L"Start check for: ", str(call), PRINT_BLUE, 1);
    const auto predicateReference = call.getPredicate()->as<PredicateReference>();
    Predicates predicates;
    if(predicateReference->getType() && predicateReference->getType()->getKind() == Type::PREDICATE) {
        const auto predicateType = predicateReference->getType()->as<PredicateType>();
        if ((predicateType->getOutParams().size() != call.getBranches().size()) ||
            (predicateType->getInParams().size() != call.getArgs().size()))
            typeError("predicate call");
        for (size_t i = 0; i < predicateType->getInParams().size(); ++i) {
            if (isFresh(call.getArgs().get(i)->getType()) ||
                isFresh(predicateType->getInParams().get(i)->getType())) {
                printTypecheckInfo(L"Static type checking failed", L"", PRINT_RED, 2);
                return false;
            }
            if (!isContains(call.getArgs().get(i), predicateType->getInParams().get(i)->getType()))
                typeError("predicate call");
        }
        for (size_t i = 0; i < predicateType->getOutParams().size(); ++i) {
            if (predicateType->getOutParams().get(i)->size() != call.getBranches().get(i)->size())
                typeError("predicate call");
            for (size_t k = 0; k < predicateType->getOutParams().get(i)->size(); ++k) {
                if (isFresh(call.getBranches().get(i)->get(k)->getType()) ||
                    isFresh(predicateType->getOutParams().get(i)->get(k)->getType())) {
                    printTypecheckInfo(L"Static type checking failed", L"", PRINT_RED, 2);
                    return false;
                }
                if (!isTypeEqual(call.getBranches().get(i)->get(k)->getType(),
                                 predicateType->getOutParams().get(i)->get(k)->getType())) {
                    typeError("predicate call");
                }
            }
        }
        return true;
    }
    context.getPredicates(predicateReference->getName(), predicates);
    bool hasSuitedPredicate = false;
    enum {
        SUITED,
        UNSUITED,
        POSSIBLE,
    };
    for (size_t i = 0; i < predicates.size(); ++i) {
        PredicatePtr predicate = predicates.get(i);
        if ((predicate->getOutParams().size() != call.getBranches().size()) ||
            (predicate->getInParams().size() != call.getArgs().size()))
            continue;
        int kind = SUITED;
        for (size_t j = 0; j < predicate->getInParams().size(); ++j) {
            if (isFresh(call.getArgs().get(j)->getType()) ||
                isFresh(predicate->getInParams().get(j)->getType())) {
                kind = POSSIBLE;
                continue;
            }
            if (!isContains(call.getArgs().get(j), predicate->getInParams().get(j)->getType())) {
                kind = UNSUITED;
                break;
            }
        }
        if (kind == SUITED or kind == POSSIBLE) {
            for (size_t j = 0; j < predicate->getOutParams().size(); ++j) {
                if (predicate->getOutParams().get(j)->size() != call.getBranches().get(j)->size()) {
                    kind = UNSUITED;
                    break;
                }
                for (size_t k = 0; k < predicate->getOutParams().get(j)->size(); ++k) {
                    if (isFresh(call.getBranches().get(j)->get(k)->getType()) ||
                        isFresh(predicate->getOutParams().get(j)->get(k)->getType())) {
                        kind = POSSIBLE;
                        continue;
                    }
                    if (!isTypeEqual(call.getBranches().get(j)->get(k)->getType(),
                                 predicate->getOutParams().get(j)->get(k)->getType())) {
                        kind = UNSUITED;
                        break;
                    }
                }
                if (kind == UNSUITED)
                    break;
            }
        }
        if (kind == POSSIBLE) {
            printTypecheckInfo(L"Static type checking failed", L"", PRINT_RED, 2);
            return false;
        }
        if (kind == SUITED) {
            if (hasSuitedPredicate) {
                typeError("predicate call");
                return true;
            } else
                hasSuitedPredicate = true;
        }

    }
    typeError("predicate call", hasSuitedPredicate);
    return true;
}

bool StaticTypeChecker::checkCastExpr(CastExpr &castExpr) {
    printTypecheckInfo(L"Start check for: ", str(castExpr), PRINT_BLUE, 1);
    printTypecheckInfo(L"Static type checking failed", L"", PRINT_RED, 2);
    return false;
}

bool StaticTypeChecker::checkFieldExpr(FieldExpr &fieldExpr) {
    printTypecheckInfo(L"Start check for: ", str(fieldExpr), PRINT_BLUE, 1);
    if (isFresh(fieldExpr.getObject()->getType())) {
        printTypecheckInfo(L"Static type checking failed", L"", PRINT_RED, 2);
        return false;
    }
    typeError("field expression", fieldExpr.getObject()->getType()->getKind() == Type::STRUCT);
    const auto fields = fieldExpr.getObject()->getType()->as<StructType>()->getNamesOrd();
    for (size_t i = 0; i < fields->size(); i++) {
        if (fields->get(i)->getName() == fieldExpr.getFieldName()){
            if (isFresh(fields->get(i)->getType())) {
                printTypecheckInfo(L"Static type checking failed", L"", PRINT_RED, 2);
                return false;
            }
            else {
                setType(fieldExpr, fields->get(i)->getType());
                return true;
            }
        }
    }
    typeError("field expression");
    return true;
}

bool StaticTypeChecker::checkFormula(Formula &formula) {
    printTypecheckInfo(L"Start check for: ", str(formula), PRINT_BLUE, 1);
    if (isFresh(formula.getSubformula()->getType())) {
        printTypecheckInfo(L"Static type checking failed", L"", PRINT_RED, 2);
        return false;
    }
    typeError("formula", formula.getSubformula()->getType()->getKind() == Type::BOOL);
    setType(formula, std::make_shared<Type>(Type::BOOL));
    return true;
}

bool StaticTypeChecker::checkFormulaCall(FormulaCall &formulaCall) {
    printTypecheckInfo(L"Start check for: ", str(formulaCall), PRINT_BLUE, 1);
    for (size_t i = 0; i < formulaCall.getArgs().size(); i++) {
        TypePtr argumentType = formulaCall.getArgs().get(i)->getType();
        TypePtr parameterType = formulaCall.getTarget()->getParams().get(i)->getType();
        if (isFresh(argumentType) || isFresh(parameterType)) {
            printTypecheckInfo(L"Static type checking failed", L"", PRINT_RED, 2);
            return false;
        }
        typeError("formula call", isContains(formulaCall.getArgs().get(i), parameterType));
    }
    setType(formulaCall, formulaCall.getTarget()->getResultType());
    return true;
}

bool StaticTypeChecker::checkFunctionCall(FunctionCall &functionCall, const ir::Context &context) {
    printTypecheckInfo(L"Start check for: ", str(functionCall), PRINT_BLUE, 1);
    const auto predicateReference = functionCall.getPredicate()->as<PredicateReference>();
    Predicates funcs;
    context.getPredicates(predicateReference->getName(), funcs);
    TypePtr suitedFunc = nullptr;
    enum {
        SUITED,
        UNSUITED,
        POSSIBLE,
    };
    for (size_t i = 0; i < funcs.size(); ++i) {
        PredicatePtr func = funcs.get(i);
        if ((func->getOutParams().size() != 1) || (func->getOutParams().get(0)->size() != 1) ||
            (func->getInParams().size() != functionCall.getArgs().size()))
            continue;
        int kind = SUITED;
        for (size_t j = 0; j < func->getInParams().size(); ++j) {
            if (isFresh(functionCall.getArgs().get(j)->getType()) ||
                isFresh(func->getInParams().get(j)->getType())) {
                kind = POSSIBLE;
                continue;
            }
            if (!isContains(functionCall.getArgs().get(j), func->getInParams().get(j)->getType())) {
                kind = UNSUITED;
                break;
            }
        }
        if (kind == POSSIBLE) {
            printTypecheckInfo(L"Static type checking failed", L"", PRINT_RED, 2);
            return false;
        }
        if (kind == SUITED) {
            if (suitedFunc) {
                typeError("predicate call");
                return true;
            } else
                suitedFunc = func->getOutParams().get(0)->get(0)->getType();
        }
    }
    typeError("predicate call", bool(suitedFunc));
    setType(functionCall, suitedFunc);
    return true;
}

bool StaticTypeChecker::checkIf(If &conditional) {
    printTypecheckInfo(L"Start check for: ", str(conditional), PRINT_BLUE, 1);
    if (isFresh(conditional.getArg()->getType())) {
        printTypecheckInfo(L"Static type checking failed", L"", PRINT_RED, 2);
        return false;
    }
    typeError("if", conditional.getArg()->getType()->getKind() == Type::BOOL);
    return true;
}

bool StaticTypeChecker::checkLambda(Lambda &lambda) {
    printTypecheckInfo(L"Start check for: ", str(lambda), PRINT_BLUE, 1);
    setType(lambda, lambda.getPredicate()->getType());
    return true;
}

bool StaticTypeChecker::checkListConstructor(ListConstructor &listConstructor) {
    printTypecheckInfo(L"Start check for: ", str(listConstructor), PRINT_BLUE, 1);
    if (std::any_of(listConstructor.begin(), listConstructor.end(), [](ExpressionPtr x){
        return isFresh(x->getType());
    })) {
        printTypecheckInfo(L"Static type checking failed", L"", PRINT_RED, 2);
        return false;
    }
    auto listBaseType = std::make_shared<Type>(Type::BOTTOM);
    for (auto i : listConstructor)
        listBaseType = getTypeJoin(listBaseType, i->getType());
    typeWarning(L"list constructor", listBaseType->getKind() != Type::BOTTOM && listBaseType->getKind() != Type::TOP);
    setType(listConstructor, std::make_shared<ListType>(listBaseType));
    return true;
}

bool StaticTypeChecker::checkLiteral(Literal &literal) {
    printTypecheckInfo(L"Start check for: ", str(literal), PRINT_BLUE, 1);
    switch (literal.getLiteralKind()) {
        case Literal::UNIT:
            setType(literal, std::make_shared<Type>(Type::UNIT));
            break;
        case Literal::NUMBER: {
            const Number &num = literal.getNumber();

            if (num.isNat())
                setType(literal, std::make_shared<Type>(Type::NAT, num.countBits(false)));
            else if (num.isInt())
                setType(literal, std::make_shared<Type>(Type::INT, num.countBits(true)));
            else
                setType(literal, std::make_shared<Type>(Type::REAL, num.countBits(false)));
            break;
        }
        case Literal::BOOL:
            setType(literal, std::make_shared<Type>(Type::BOOL));
            break;
        case Literal::CHAR:
            setType(literal, std::make_shared<Type>(Type::CHAR));
            break;
        case Literal::STRING:
            setType(literal, std::make_shared<Type>(Type::STRING));
            break;
        default:
            break;
    }
    return true;
}

bool StaticTypeChecker::checkMapConstructor(MapConstructor &mapConstructor) {
    printTypecheckInfo(L"Start check for: ", str(mapConstructor), PRINT_BLUE, 1);
    if (std::any_of(mapConstructor.begin(), mapConstructor.end(), [](ElementDefinitionPtr x){
        return isFresh(x->getIndex()->getType()) || isFresh(x->getValue()->getType());
    })) {
        printTypecheckInfo(L"Static type checking failed", L"", PRINT_RED, 2);
        return false;
    }
    auto mapBaseType = std::make_shared<Type>(Type::BOTTOM);
    auto mapIndexType = std::make_shared<Type>(Type::BOTTOM);
    for(const auto& i : mapConstructor) {
        mapBaseType = getTypeJoin(mapBaseType, i->getValue()->getType());
        mapIndexType = getTypeJoin(mapIndexType, i->getIndex()->getType());
    }
    typeWarning(L"map constructor", mapBaseType->getKind() == Type::BOTTOM || mapBaseType->getKind() == Type::TOP);
    typeWarning(L"map constructor", mapIndexType->getKind() == Type::BOTTOM || mapIndexType->getKind() == Type::TOP);
    setType(mapConstructor, std::make_shared<MapType>(mapIndexType, mapBaseType));
    return true;
}

bool StaticTypeChecker::checkPredicateReference(PredicateReference &predicateReference, const ir::Context &context) {
    printTypecheckInfo(L"Start check for: ", str(predicateReference), PRINT_BLUE, 1);
    Predicates predicates;
    context.getPredicates(predicateReference.getName(), predicates);
    if (predicates.size() != 1) {
        printTypecheckInfo(L"Static type checking failed", L"", PRINT_RED, 2);
        return false;
    }
    setType(predicateReference, predicates.get(0)->getType());
    return true;
}

SubtypePtr StaticTypeChecker::checkRange(Range &range) {
    printTypecheckInfo(L"Start check for: ", str(range), PRINT_BLUE, 1);
    const auto rangeMaxType = range.getMax()->getType();
    const auto rangeMinType = range.getMin()->getType();
    if (isFresh(rangeMinType) || isFresh(rangeMaxType)) {
        printTypecheckInfo(L"Static type checking failed", L"", PRINT_RED, 2);
        return nullptr;
    }
    const auto rangeParamType = getTypeJoin(rangeMaxType, rangeMinType);
    typeError("range", rangeParamType->getKind() == Type::INT || rangeParamType->getKind() == Type::NAT||
                       rangeParamType->getKind() == Type::ENUM || rangeParamType->getKind() == Type::CHAR);
    const auto subtype = range.asSubtype();
    subtype->getParam()->setType(rangeParamType);

    return subtype;
}

bool StaticTypeChecker::checkRecognizerExpr(RecognizerExpr &recognizerExpr) {
    printTypecheckInfo(L"Start check for: ", str(recognizerExpr), PRINT_BLUE, 1);
    if (isFresh(recognizerExpr.getObject()->getType())) {
        printTypecheckInfo(L"Static type checking failed", L"", PRINT_RED, 2);
        return false;
    }
    setType(recognizerExpr, std::make_shared<Type>(Type::BOOL));
    UnionConstructorDeclarations constructorDeclarations = 
            recognizerExpr.getObject()->getType()->as<UnionType>()->getConstructors();
    auto it = std::find_if(constructorDeclarations.begin(), constructorDeclarations.end(),
               [recognizerExpr](UnionConstructorDeclarationPtr x){
                   return x->getName() == recognizerExpr.getConstructorName();});
    typeError("recognizer expression", it != constructorDeclarations.end());
    return true;
}

bool StaticTypeChecker::checkReplacement(Replacement &replacement) {
    printTypecheckInfo(L"Start check for: ", str(replacement), PRINT_BLUE, 1);
    TypePtr objectType = replacement.getObject()->getType();
    TypePtr newValuesType = replacement.getNewValues()->getType();
    if (isFresh(objectType) || isFresh(newValuesType)) {
        printTypecheckInfo(L"Static type checking failed", L"", PRINT_RED, 2);
        return false;
    }
    TypePtr replacementType = getTypeJoin(objectType, newValuesType);
    typeWarning(L"replacement", replacementType->getKind() != Type::TOP);
    setType(replacement, replacementType);
    return true;
}

bool StaticTypeChecker::checkSetConstructor(SetConstructor &setConstructor) {
    printTypecheckInfo(L"Start check for: ", str(setConstructor), PRINT_BLUE, 1);
    if (std::any_of(setConstructor.begin(), setConstructor.end(), [](ExpressionPtr x){
        return isFresh(x->getType());
    })) {
        printTypecheckInfo(L"Static type checking failed", L"", PRINT_RED, 2);
        return false;
    }
    auto setBaseType = std::make_shared<Type>(Type::BOTTOM);
    for (const auto& i : setConstructor)
        setBaseType = getTypeJoin(setBaseType, i->getType());
    typeWarning(L"set constructor", setBaseType->getKind() != Type::BOTTOM && setBaseType->getKind() != Type::TOP);
    setType(setConstructor, std::make_shared<SetType>(setBaseType));
    return true;
}

bool StaticTypeChecker::checkStructConstructor(StructConstructor &structConstructor) {
    printTypecheckInfo(L"Start check for: ", str(structConstructor), PRINT_BLUE, 1);
    if (std::any_of(structConstructor.begin(), structConstructor.end(), [](StructFieldDefinitionPtr x){
        return isFresh(x->getValue()->getType());
    })) {
        printTypecheckInfo(L"Static type checking failed", L"", PRINT_RED, 2);
        return false;
    }
    const auto structType = std::make_shared<StructType>();
    for (const auto& i : structConstructor) {
        const auto field = std::make_shared<NamedValue>(i->getName());
        setType(*field, i->getValue()->getType());
        if (i->getName().empty())
            structType->getTypesOrd()->add(field);
        else
            structType->getNamesSet()->add(field);
        i->setField(field);
    }
    setType(structConstructor, structType);
    return true;
}

bool StaticTypeChecker::checkTernary(Ternary &ternary) {
    printTypecheckInfo(L"Start check for: ", str(ternary), PRINT_BLUE, 1);
    TypePtr ifType = ternary.getIf()->getType();
    TypePtr thenType = ternary.getThen()->getType();
    TypePtr elseType = ternary.getElse()->getType();
    if (isFresh(ifType) || isFresh(thenType) || isFresh(elseType)) {
        printTypecheckInfo(L"Static type checking failed", L"", PRINT_RED, 2);
        return false;
    }
    TypePtr ternaryType = getTypeJoin(thenType, elseType);
    typeWarning(L"ternary", ternaryType->getKind() != Type::TOP);
    typeError("ternary", ifType->getKind() == Type::BOOL);
    setType(ternary, ternaryType);
    return true;
}

bool StaticTypeChecker::checkTypeExpr(TypeExpr &typeExpr) {
    printTypecheckInfo(L"Start check for: ", str(typeExpr), PRINT_BLUE, 1);
    if (typeExpr.getContents()->getKind() == Type::RANGE || typeExpr.getContents()->getKind() == Type::SUBTYPE) {
        typeExpr.setType(typeExpr.getContents());
    } else {
        TypeTypePtr typeType = std::make_shared<TypeType>();
        typeType->setDeclaration(std::make_shared<TypeDeclaration>(L"", typeExpr.getContents()));
        typeExpr.setType(typeType);
    }
    return true;
}

bool StaticTypeChecker::checkUnary(Unary &unary) {
    printTypecheckInfo(L"Start check for: ", str(unary), PRINT_BLUE, 1);
    TypePtr exprType = unary.getExpression()->getType();
    if (isFresh(exprType)) {
        printTypecheckInfo(L"Static type checking failed", L"", PRINT_RED, 2);
        return false;
    }
    switch (unary.getOperator()) {
        case Unary::MINUS:
            switch (exprType->getKind()) {
                case Type::INT:
                    setType(unary, std::make_shared<Type>(Type::INT, Number::GENERIC));
                    return true;
                case Type::REAL:
                    setType(unary, std::make_shared<Type>(Type::REAL, Number::GENERIC));
                    return true;
                case Type::NAT:
                    setType(unary, std::make_shared<Type>(Type::INT, Number::GENERIC));
                    return true;
                default:
                    typeError("unary minus");
            }
        case Unary::PLUS:
            switch (exprType->getKind()) {
                case Type::INT:
                    setType(unary, std::make_shared<Type>(Type::INT, Number::GENERIC));
                    return true;
                case Type::REAL:
                    setType(unary, std::make_shared<Type>(Type::REAL, Number::GENERIC));
                    return true;
                case Type::NAT:
                    setType(unary, std::make_shared<Type>(Type::INT, Number::GENERIC));
                    return true;
                default:
                    typeError("unary plus");
            }
        case Unary::BOOL_NEGATE:
            switch (exprType->getKind()) {
                case Type::BOOL:
                    setType(unary, std::make_shared<Type>(Type::BOOL));
                    return true;
                case Type::SET:
                    setType(unary, clone(exprType));
                    return true;
                default:
                    typeError("unary operation");
            }
        case Unary::BITWISE_NEGATE:
            switch (exprType->getKind()) {
                case Type::INT:
                    setType(unary, std::make_shared<Type>(Type::INT, Number::GENERIC));
                    return true;
                case Type::NAT:
                    setType(unary, std::make_shared<Type>(Type::NAT, Number::GENERIC));
                    return true;
                default:
                    typeError("unary operation");
            }
        default:
            typeError("unary operation");
    }
    return true;
}

bool StaticTypeChecker::checkUnionConstructor(UnionConstructor &unionConstructor) {
    printTypecheckInfo(L"Start check for: ", str(unionConstructor), PRINT_BLUE, 1);
    if (std::any_of(unionConstructor.begin(), unionConstructor.end(), [](StructFieldDefinitionPtr x){
        return isFresh(x->getValue()->getType());
    })) {
        printTypecheckInfo(L"Static type checking failed", L"", PRINT_RED, 2);
        return false;
    }
    const auto unionType = std::make_shared<UnionType>();
    const auto constructorDeclaration = std::make_shared<UnionConstructorDeclaration>(unionConstructor.getName());
    unionType->getConstructors().add(constructorDeclaration);
    for (auto i : unionConstructor) {
        const auto field = std::make_shared<NamedValue>(i->getName());
        setType(*field, i->getValue()->getType());
        constructorDeclaration->getStructFields()->getNamesOrd()->add(field);
        i->setField(field);
    }
    setType(unionConstructor, unionType);
    return true;
}

bool StaticTypeChecker::checkVariableDeclaration(VariableDeclaration &variableDeclaration) {
    printTypecheckInfo(L"Start check for: ", str(variableDeclaration), PRINT_BLUE, 1);
    if ((variableDeclaration.getValue() && isFresh(variableDeclaration.getValue()->getType())) ||
            isFresh(variableDeclaration.getVariable()->getType())) {
        printTypecheckInfo(L"Static type checking failed", L"", PRINT_RED, 2);
        return false;
    }
    if (variableDeclaration.getValue())
        typeError("variable declaration", isContains(variableDeclaration.getValue(),
                                          variableDeclaration.getVariable()->getType()));
    return true;
}

bool StaticTypeChecker::checkVariableReference(VariableReference &variableReference) {
    printTypecheckInfo(L"Start check for: ", str(variableReference), PRINT_BLUE, 1);
    setType(variableReference, variableReference.getTarget()->getType());
    return true;
}


void StaticTypeChecker::printTypecheckInfo(std::wstring head, std::wstring msg, int color, int indent) {
    if (Options::instance().bVerbose) {
        for (int i = 0; i < indent; ++i)
            std::wcout << L"  ";
        switch (color) {
            case PRINT_BLUE:
                std::wcout << L"\033[34m";
                break;
            case PRINT_BLACK:
                std::wcout << L"\033[30m";
                break;
            case PRINT_RED:
                std::wcout << L"\033[31m";
                break;
            case PRINT_GREEN:
                std::wcout << L"\033[32m";
                break;
            default:
                std::wcout << L"\033[30m";
                break;
        }
        std::wcout << head << L"\033[30m" << msg << L"\n";
    }
}


TypePtr StaticTypeChecker::getGeneralType(const TypePtr &type) {
    if (type->getKind() == Type::RANGE)
        return getGeneralType(getTypeJoin(type->as<Range>()->getMax()->getType(),
                                          type->as<Range>()->getMin()->getType()));
    if (type->getKind() == Type::SUBTYPE)
        return getGeneralType(type->as<Subtype>()->getParam()->getType());
    return type;
}

bool StaticTypeChecker::isSubtype(const TypePtr &type1, const TypePtr &type2) {
    const auto generalType1 = getGeneralType(type1);
    const auto generalType2 = getGeneralType(type2);
    printTypecheckInfo(L"Subtype check ", str(generalType1) + L" <: " + str(generalType2), PRINT_BLUE, 2);
    if (type1->getKind() == Type::ARRAY && type2->getKind() == Type::ARRAY) {
        const auto arrayType1 = type1->as<ArrayType>();
        const auto arrayType2 = type2->as<ArrayType>();
        Collection<Type> dimension1;
        Collection<Type> dimension2;
        arrayType1->getDimensions(dimension1);
        arrayType1->getDimensions(dimension2);
        if (dimension1.size() != dimension2.size()) {
            printTypecheckInfo(L"Static type checking failed", L"", PRINT_RED, 2);
            return false;
        }
        for (size_t i = 0; i < dimension1.size(); ++i)
            if (!isTypeEqual(dimension1.get(i), dimension2.get(i))) {
                printTypecheckInfo(L"Static type checking failed", L"", PRINT_RED, 2);
                return false;
            }
        return isSubtype(arrayType1->getBaseType(), arrayType2->getBaseType());
    }
    if (type1->getKind() == Type::LIST && type2->getKind() == Type::LIST)
        return isSubtype(type1->as<ListType>()->getBaseType(), type2->as<ListType>()->getBaseType());
    return generalType1->compare(*generalType2, Type::ORD_SUB) ||
           generalType1->compare(*generalType2, Type::ORD_EQUALS);
}

bool StaticTypeChecker::isTypeEqual(const TypePtr &type1, const TypePtr &type2) {
    const auto tJoin = type1->getJoin(type2);
    const auto generalType1 = getGeneralType(type1);
    const auto generalType2 = getGeneralType(type2);
    printTypecheckInfo(L"Check equals ", str(generalType1) + L" <=> " + str(generalType2), PRINT_BLUE, 2);
    return isSubtype(generalType1, generalType2) && isSubtype(generalType1, generalType2);
}

TypePtr StaticTypeChecker::getTypeJoin(const TypePtr &type1, const TypePtr &type2) {
    TypePtr generalType1 = getGeneralType(type1);
    TypePtr generalType2 = getGeneralType(type2);
    TypePtr tJoin = generalType1->getJoin(generalType2);
    printTypecheckInfo(L"Get Join ", str(generalType1) + L" & " + str(generalType2) + L" => " + str(tJoin),
                       PRINT_BLUE, 2);
    return tJoin;
}

bool StaticTypeChecker::isFresh(const TypePtr &type) {
    TypePtr generalType = getGeneralType(type);
    switch (generalType->getKind()) {
        case Type::FRESH:
            return true;
        case Type::GENERIC:
            return true;
        case Type::ARRAY:
            return isFresh(generalType->as<ArrayType>()->getBaseType());
        case Type::RANGE:
            return isFresh(generalType->as<Range>()->getMin()->getType()) || isFresh(type->as<Range>()->getMax()->getType());
        case Type::SUBTYPE:
            return isFresh(generalType->as<Subtype>()->getParam()->getType());
        case Type::MAP:
            return isFresh(generalType->as<MapType>()->getBaseType());
        case Type::LIST:
            return isFresh(generalType->as<ListType>()->getBaseType());
        default:
            return false;
    }
}

void StaticTypeChecker::setType(Node &node, const TypePtr type) {
    if (node.getNodeKind() == Node::NAMED_VALUE)
        node.as<NamedValue>()->setType(clone(type));
    else if (node.getNodeKind() == Node::EXPRESSION)
        node.as<Expression>()->setType(clone(type));
    else
        typeError("impossible to set the type");
    printTypecheckInfo(L"Set Type ", L"[" +str(type) + L"] " + str(node.as<Node>()), PRINT_BLUE, 2);
}

bool StaticTypeChecker::isContains(const ExpressionPtr &expr, const TypePtr &type) {
    return isSubtype(expr->getType(), type);
}

void StaticTypeChecker::typeError(const std::string& msg, bool expr) {
    if (!expr) {
        printTypecheckInfo(L"Type Error ", L"", PRINT_RED, 2);
        throw std::runtime_error(msg);
    }
}

void StaticTypeChecker::typeWarning(const std::wstring& msg, bool expr) {
    if (!expr)
        printTypecheckInfo(L"Type Warning ", msg, PRINT_RED, 2);
}

std::wstring StaticTypeChecker::str(const NodePtr &node) {
    std::wostringstream stream;
    pp::prettyPrintSyntax(node, stream, NULL, true);
    std::wstring wstr = stream.str();
    wstr.pop_back();
    return wstr;
}
