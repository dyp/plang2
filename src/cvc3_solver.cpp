/// \file cvc3_solver.cpp
///

#include <memory>
#include <map>

#include "cvc3_solver.h"
#include "cvc3/vc.h"
#include "cvc3/expr.h"
#include "cvc3/type.h"
#include "cvc3/variable.h"
#include "cvc3/command_line_flags.h"
#include "ir/visitor.h"
#include "node_analysis.h"
#include "utils.h"

namespace CVC3 {

typedef std::shared_ptr<CVC3::ValidityChecker> ValidityCheckerPtr;
typedef std::shared_ptr<CVC3::Variable> VariablePtr;
typedef std::shared_ptr<CVC3::Expr> ExprPtr;
typedef std::shared_ptr<CVC3::Type> TypePtr;
typedef std::shared_ptr<CVC3::Op> OpPtr;

}

namespace cvc3 {

using namespace ir;

class Solver {
public:
    Solver() :
        m_pValidityChecker(CVC3::ValidityChecker::create()), m_cVars(0), m_cFormulas(0), m_cTypes(0)
    {}

    CVC3::ExprPtr translateExpr(const Expression& _expr);
    CVC3::TypePtr translateType(const Type& _type, bool _bNativeBool = false);
    CVC3::ValidityChecker& getContext() { return *m_pValidityChecker; }

private:
    CVC3::ValidityCheckerPtr m_pValidityChecker;
    std::map<TypePtr, CVC3::TypePtr> m_types;
    std::map<NamedValuePtr, CVC3::ExprPtr> m_variables;
    std::map<FormulaDeclarationPtr, CVC3::OpPtr> m_formulas;
    size_t m_cVars, m_cFormulas, m_cTypes;

    CVC3::TypePtr _getNatType();
    CVC3::TypePtr _getInfiniteArray(const Type& _baseType);
    CVC3::TypePtr _getType(const TypePtr& _pType);
    CVC3::ExprPtr _declareVariable(const NamedValuePtr& _pValue);
    CVC3::OpPtr _declareFormula(const FormulaDeclarationPtr& _pFormula);
    std::string _makeNewTypeName();
    std::string _makeIdentifier(std::wstring _strIdentifier);
    std::string _makeNewVarName(const std::wstring& _strOrigName);
    CVC3::ExprPtr _makeLambdaExpr(const CVC3::ExprPtr& _pVar, const CVC3::ExprPtr& _pPred);
    bool _isTuple(const StructConstructor& _expr) const;

    // Expressions.
    CVC3::ExprPtr _translateLiteral(const Literal& _expr);
    CVC3::ExprPtr _translateVariableReference(const VariableReference& _expr);
    CVC3::ExprPtr _translateUnary(const Unary& _expr);
    CVC3::ExprPtr _translateBinary(const Binary& _expr);
    CVC3::ExprPtr _translateTernary(const Ternary& _expr);
    CVC3::ExprPtr _translateFormula(const Formula& _expr);
    CVC3::ExprPtr _translateFormulaCall(const FormulaCall& _expr);

    // Components.
    CVC3::ExprPtr _translateArrayPartExpr(const ArrayPartExpr& _expr);
    CVC3::ExprPtr _translateFieldExpr(const FieldExpr& _expr);
    CVC3::ExprPtr _translateMapElementExpr(const MapElementExpr& _expr);
    CVC3::ExprPtr _translateListElementExpr(const ListElementExpr& _expr);
    CVC3::ExprPtr _translateComponent(const Component& _expr);

    // Replacements.
    CVC3::ExprPtr _translateArrayReplacement(const Expression& _expr, const ArrayConstructor& _constr);
    CVC3::ExprPtr _translateMapReplacement(const Expression& _expr, const MapConstructor& _constr);
    CVC3::ExprPtr _translateStructReplacement(const Expression& _expr, const StructConstructor& _constr);
    CVC3::ExprPtr _translateReplacement(const Replacement& _expr);

    // Constructors.
    CVC3::ExprPtr _translateStructConstructor(const StructConstructor& _expr);
    CVC3::ExprPtr _translateTupleConstructor(const StructConstructor& _expr);
    CVC3::ExprPtr _translateArrayConstructor(const ArrayConstructor& _expr);
    CVC3::ExprPtr _translateSetConstructor(const SetConstructor& _expr);
    CVC3::ExprPtr _translateMapConstructor(const MapConstructor& _expr);
    CVC3::ExprPtr _translateListConstructor(const ListConstructor& _expr);

    template<typename T>
    CVC3::ExprPtr _translateFiniteArray(const T& _expr);

    template<typename T1, typename T2>
    CVC3::ExprPtr _translateInfiniteArray(const T1& _expr);

    CVC3::ExprPtr _translateConstructor(const Constructor& _expr);

    // Types.
    CVC3::TypePtr _translateEnumType(const EnumType& _enum);
    CVC3::TypePtr _translateStructType(const StructType& _struct);
    CVC3::TypePtr _translateUnionType(const UnionType& _union);
    CVC3::TypePtr _translateArrayType(const ArrayType& _array);
    CVC3::TypePtr _translateMapType(const MapType& _map);
    CVC3::TypePtr _translateSubtype(const Subtype& _subtype);
    CVC3::TypePtr _translateRange(const Range& _range);
    CVC3::TypePtr _translateBranch(const Branch& _branch);
    CVC3::TypePtr _translatePredicateType(const PredicateType& _pred);
    CVC3::TypePtr _translateNamedReferenceType(const NamedReferenceType& _type);
};

#define NEW(_TYPE, _OPERATOR) \
    CVC3:: _TYPE##Ptr(new CVC3:: _TYPE ( _OPERATOR ))

CVC3::ExprPtr Solver::translateExpr(const Expression& _expr) {
    switch (_expr.getKind()) {
        case Expression::LITERAL:
            return _translateLiteral((Literal &)_expr);
        case Expression::VAR:
            return _translateVariableReference((VariableReference &)_expr);
        // There is no PredicateReference.
        case Expression::UNARY:
            return _translateUnary((Unary &)_expr);
        case Expression::BINARY:
            return _translateBinary((Binary &)_expr);
        case Expression::TERNARY:
            return _translateTernary((Ternary &)_expr);
        // There is no TypeExpr.
        case Expression::COMPONENT:
            return _translateComponent((Component &)_expr);
        // There is no FunctionCall.
        case Expression::FORMULA_CALL:
            return _translateFormulaCall((FormulaCall &)_expr);
        // There is no Lambda, Binder.
        case Expression::FORMULA:
            return _translateFormula((Formula &)_expr);
        case Expression::CONSTRUCTOR:
            return _translateConstructor((Constructor &)_expr);
        case Expression::CAST:
            return translateExpr(*((CastExpr &)_expr).getExpression());
        default:
            throw CVC3::Exception("Error translate expression with kind: " + intToStr(_expr.getKind()));
    }

    return nullptr;
}

CVC3::TypePtr Solver::translateType(const Type& _type, bool _bNativeBool) {
    CVC3::TypePtr pType = _getType(&_type);

    if (pType)
        return pType;

    switch (_type.getKind()) {
        // There is no UnitType.
        case Type::NAT:
            pType = _getNatType();
            break;
        case Type::INT:
            pType = NEW(Type, m_pValidityChecker->intType());
            break;
        case Type::REAL:
            pType = NEW(Type, m_pValidityChecker->realType());
            break;
        case Type::BOOL:
            pType = NEW(Type, _bNativeBool
                ? m_pValidityChecker->boolType()
                : m_pValidityChecker->bitvecType(1));
            break;
        // TODO: String, Char.
        // There is no TypeType.
        case Type::ENUM:
            pType = _translateEnumType((EnumType&)_type);
            break;
        case Type::STRUCT:
            pType = _translateStructType((StructType&)_type);
            break;
        case Type::UNION:
            pType = _translateUnionType((UnionType&)_type);
            break;
        case Type::ARRAY:
            pType = _translateArrayType((ArrayType&)_type);
            break;
        case Type::LIST:
            pType = _getInfiniteArray(*((ListType&)_type).getBaseType());
            break;
        case Type::SET:
            pType = _getInfiniteArray(*((SetType&)_type).getBaseType());
            break;
        case Type::MAP:
            pType = _translateMapType((MapType&)_type);
            break;
        case Type::SUBTYPE:
            pType = _translateSubtype((Subtype&)_type);
            break;
        case Type::RANGE:
            pType = _translateRange((Range&)_type);
            break;
        case Type::PREDICATE:
            pType = _translatePredicateType((PredicateType&)_type);
            break;
        // There is no ParametrizedType.
        case Type::NAMED_REFERENCE:
            pType = _translateNamedReferenceType((NamedReferenceType&)_type);
            break;
        default:
            throw CVC3::Exception("Error translate type with kind: " + intToStr(_type.getKind()));
    }

    m_types.insert({&_type, pType});
    return pType;
}

CVC3::TypePtr Solver::_getNatType() {
    CVC3::TypePtr pResult = NEW(Type, m_pValidityChecker->lookupType("NAT"));

    if (pResult->isNull()) {
        CVC3::TypePtr pType = NEW(Type, m_pValidityChecker->intType());
        CVC3::ExprPtr
            pBound = NEW(Expr, m_pValidityChecker->ratExpr("0")),
            pVar = NEW(Expr, m_pValidityChecker->varExpr(_makeNewVarName(L"x"), *pType)),
            pPred = NEW(Expr, m_pValidityChecker->geExpr(*pVar, *pBound)),
            pWitness = NEW(Expr, m_pValidityChecker->ratExpr("1"));

        pResult = NEW(Type, m_pValidityChecker->subtypeType(*_makeLambdaExpr(pVar, pPred), *pWitness));
        pResult = NEW(Type, m_pValidityChecker->createType("NAT", *pResult));
    }

    return pResult;
}

CVC3::TypePtr Solver::_getInfiniteArray(const Type& _baseType) {
    CVC3::TypePtr
        pInt = NEW(Type, m_pValidityChecker->intType()),
        pDim = NEW(Type, *_getNatType()),
        pBase = translateType(_baseType),
        pArrayType = NEW(Type, m_pValidityChecker->arrayType(*pDim, *pBase));

    return NEW(Type, m_pValidityChecker->recordType("size", *pInt, "elements", *pArrayType));
}

CVC3::TypePtr Solver::_getType(const TypePtr& _pType) {
    std::map<TypePtr, CVC3::TypePtr>::iterator it = m_types.find(_pType);
    return it != m_types.end() ? it->second : NULL;
}

CVC3::ExprPtr Solver::_declareVariable(const NamedValuePtr& _pValue) {
    if (!_pValue->getType())
        throw CVC3::Exception("Error translate NULL variable");
    if (!_pValue)
        throw CVC3::Exception("Error translate variable with NULL type");

    std::map<NamedValuePtr, CVC3::ExprPtr>::iterator it = m_variables.find(_pValue);
    if (it != m_variables.end())
        return it->second;

    const std::string strName = _makeNewVarName(_pValue->getName());
    CVC3::TypePtr pType = translateType(*_pValue->getType());
    CVC3::ExprPtr pVar = NEW(Expr, m_pValidityChecker->varExpr(strName, *pType));
    m_variables.insert({_pValue, pVar});

    return pVar;
}

CVC3::OpPtr Solver::_declareFormula(const FormulaDeclarationPtr& _pFormula) {
    std::map<FormulaDeclarationPtr, CVC3::OpPtr>::iterator it = m_formulas.find(_pFormula);
    if (it != m_formulas.end())
        return it->second;



    CVC3::TypePtr
        pTypeRan = NEW(Type, *translateType(*_pFormula->getResultType(), true));

    std::vector<CVC3::Expr> vars;
    std::vector<CVC3::Type> typeDom;
    for (size_t i = 0; i < _pFormula->getParams().size(); ++i) {
        const NamedValue& value = *_pFormula->getParams().get(i);
        typeDom.push_back(*translateType(*value.getType()));
        vars.push_back(*_declareVariable(&value));
    }

    const std::string
        strName = _makeIdentifier(_pFormula->getName()) + "_" + intToStr(++m_cFormulas);
    CVC3::ExprPtr
        pBody = NEW(Expr, *translateExpr(*_pFormula->getFormula()));
    CVC3::TypePtr
        pFunType = NEW(Type, m_pValidityChecker->funType(typeDom, *pTypeRan));
    CVC3::OpPtr
        pLambda = NEW(Op, m_pValidityChecker->lambdaExpr(vars, *pBody)),
        pOp = NEW(Op, m_pValidityChecker->createOp(strName, *pFunType, pLambda->getExpr()));

    m_formulas.insert({_pFormula, pOp});
    return pOp;

}

std::string Solver::_makeNewTypeName() {
    std::string strName;

    do {
        strName = "t_" + intToStr(m_cTypes++);
    } while (!m_pValidityChecker->lookupType(strName).isNull());

    return strName;
}

std::string Solver::_makeIdentifier(std::wstring _strIdentifier) {
    return strNarrow(cyrillicToASCII(_strIdentifier));
}

std::string Solver::_makeNewVarName(const std::wstring& _strOrigName) {
    return (!_strOrigName.empty() ? _makeIdentifier(_strOrigName) : "empty") + "_" + intToStr(++m_cVars);
}

CVC3::ExprPtr Solver::_makeLambdaExpr(const CVC3::ExprPtr& _pVar, const CVC3::ExprPtr& _pPred) {
    std::vector<CVC3::Expr> params{*_pVar};

    CVC3::OpPtr
        pLambda = NEW(Op, m_pValidityChecker->lambdaExpr(params, *_pPred));

    return NEW(Expr, pLambda->getExpr());
}

bool Solver::_isTuple(const StructConstructor& _expr) const {
    for (size_t i = 0; i < _expr.size(); ++i)
        if (!_expr.get(i)->getName().empty())
            return false;
    return true;
}

CVC3::ExprPtr Solver::_translateLiteral(const Literal& _expr) {
    switch (_expr.getLiteralKind()) {
        // There is no Unit.
        case Literal::NUMBER:
            return NEW(Expr, m_pValidityChecker->ratExpr(_makeIdentifier(_expr.getNumber().toString())));
        case Literal::BOOL:
            return NEW(Expr, _expr.getBool()
                ? m_pValidityChecker->trueExpr()
                : m_pValidityChecker->falseExpr());
        // TODO: Char.
        case Literal::STRING:
            return NEW(Expr, m_pValidityChecker->stringExpr(strNarrow(_expr.getString())));
        default:
            throw CVC3::Exception("Error translate literal kind " + intToStr(_expr.getKind()));
    }

    return nullptr;
}

CVC3::ExprPtr Solver::_translateVariableReference(const VariableReference& _expr) {
    return _declareVariable(_expr.getTarget());
}

CVC3::ExprPtr Solver::_translateUnary(const Unary& _expr) {
    CVC3::ExprPtr pSubExpr = NEW(Expr, *translateExpr(*_expr.getExpression()));
    switch(_expr.getOperator()) {
        case Unary::PLUS:
            return pSubExpr;
        case Unary::MINUS:
            return NEW(Expr, m_pValidityChecker->uminusExpr(*pSubExpr));
        case Unary::BOOL_NEGATE:
            return NEW(Expr, m_pValidityChecker->notExpr(*pSubExpr));
        case Unary::BITWISE_NEGATE:
            return NEW(Expr, m_pValidityChecker->newBVNegExpr(*pSubExpr));
        default:
            throw CVC3::Exception("Error translate unary with operator: " + intToStr(_expr.getOperator()));
    }

    return nullptr;
}

static inline bool _isBool(const Binary& _expr) {
    return _expr.getLeftSide() && _expr.getLeftSide()->getType() &&
        _expr.getLeftSide()->getType()->getKind() == Type::BOOL &&
        _expr.getRightSide() && _expr.getRightSide()->getType() &&
        _expr.getRightSide()->getType()->getKind() == Type::BOOL;
}

CVC3::ExprPtr Solver::_translateBinary(const Binary& _expr) {
    CVC3::ExprPtr
        pLeft = NEW(Expr, *translateExpr(*_expr.getLeftSide())),
        pRight = NEW(Expr, *translateExpr(*_expr.getRightSide()));

    switch (_expr.getOperator()) {
        // TODO: "+" for sets.
        case Binary::ADD:
            return NEW(Expr, m_pValidityChecker->plusExpr(*pLeft, *pRight));
        case Binary::SUBTRACT:
            return NEW(Expr, m_pValidityChecker->minusExpr(*pLeft, *pRight));
        case Binary::MULTIPLY:
            return NEW(Expr, m_pValidityChecker->multExpr(*pLeft, *pRight));
        case Binary::DIVIDE:
            return NEW(Expr, m_pValidityChecker->divideExpr(*pLeft, *pRight));
        // TODO: REMAINDER.
        case Binary::POWER:
            return NEW(Expr, m_pValidityChecker->powExpr(*pLeft, *pRight));
        // TODO: SHIFT_LEFT, SHIFT_RIGHT, IN.
        case Binary::LESS:
            return NEW(Expr, m_pValidityChecker->ltExpr(*pLeft, *pRight));
        case Binary::LESS_OR_EQUALS:
            return NEW(Expr, m_pValidityChecker->leExpr(*pLeft, *pRight));
        case Binary::GREATER:
            return NEW(Expr, m_pValidityChecker->gtExpr(*pLeft, *pRight));
        case Binary::GREATER_OR_EQUALS:
            return NEW(Expr, m_pValidityChecker->geExpr(*pLeft, *pRight));
        case Binary::EQUALS:
            return _isBool(_expr)
                ? NEW(Expr, m_pValidityChecker->iffExpr(*pLeft, *pRight))
                : NEW(Expr, m_pValidityChecker->eqExpr(*pLeft, *pRight));
        case Binary::NOT_EQUALS: {
            CVC3::ExprPtr pEq = NEW(Expr, m_pValidityChecker->eqExpr(*pLeft, *pRight));
            return NEW(Expr, m_pValidityChecker->notExpr(*pEq));
        }
        case Binary::BOOL_AND:
            return NEW(Expr, m_pValidityChecker->andExpr(*pLeft, *pRight));
        case Binary::BOOL_OR:
            return NEW(Expr, m_pValidityChecker->orExpr(*pLeft, *pRight));
        case Binary::BOOL_XOR:
            return NEW(Expr, m_pValidityChecker->newBVXorExpr(*pLeft, *pRight));
        case Binary::BITWISE_AND:
            return NEW(Expr, m_pValidityChecker->newBVAndExpr(*pLeft, *pRight));
        case Binary::BITWISE_OR:
            return NEW(Expr, m_pValidityChecker->newBVOrExpr(*pLeft, *pRight));
        case Binary::BITWISE_XOR:
            return NEW(Expr, m_pValidityChecker->newBVXorExpr(*pLeft, *pRight));
        case Binary::IMPLIES:
            return NEW(Expr, m_pValidityChecker->impliesExpr(*pLeft, *pRight));
        case Binary::IFF:
            return NEW(Expr, m_pValidityChecker->iffExpr(*pLeft, *pRight));
        default:
            throw CVC3::Exception("Error translate binary operator " + intToStr(_expr.getOperator()));
    }

    return nullptr;
}

CVC3::ExprPtr Solver::_translateTernary(const Ternary& _expr) {
    CVC3::ExprPtr
        pIf = translateExpr(*_expr.getIf()),
        pThen = translateExpr(*_expr.getThen()),
        pElse = translateExpr(*_expr.getElse());
    return NEW(Expr, m_pValidityChecker->iteExpr(*pIf, *pThen, *pElse));
}

CVC3::ExprPtr Solver::_translateArrayPartExpr(const ArrayPartExpr& _expr) {
    CVC3::ExprPtr
        pArrayPart = translateExpr(*_expr.getObject());

    for (int i = _expr.getIndices().size() - 1; i >= 0; --i) {
        CVC3::ExprPtr pIndex = translateExpr(*_expr.getIndices().get(i));
        pArrayPart = NEW(Expr, m_pValidityChecker->readExpr(*pArrayPart, *pIndex));
    }

    return pArrayPart;
}

CVC3::ExprPtr Solver::_translateFieldExpr(const FieldExpr& _expr) {
    std::string strField = _makeIdentifier(_expr.getFieldName());
    CVC3::ExprPtr pRecord = translateExpr(*_expr.getObject());
    return NEW(Expr, m_pValidityChecker->recSelectExpr(*pRecord, strField));
}

CVC3::ExprPtr Solver::_translateMapElementExpr(const MapElementExpr& _expr) {
    CVC3::ExprPtr
        pIndex = translateExpr(*_expr.getIndex()),
        pArray = translateExpr(*_expr.getObject());
    return NEW(Expr, m_pValidityChecker->readExpr(*pArray, *pIndex));
}

CVC3::ExprPtr Solver::_translateListElementExpr(const ListElementExpr& _expr) {
    CVC3::ExprPtr
        pIndex = translateExpr(*_expr.getIndex()),
        pArray = NEW(Expr, m_pValidityChecker->recSelectExpr(*translateExpr(*_expr.getObject()), "elements"));
    return NEW(Expr, m_pValidityChecker->readExpr(*pArray, *pIndex));
}

CVC3::ExprPtr Solver::_translateArrayReplacement(const Expression& _expr, const ArrayConstructor& _constr) {
    CVC3::ExprPtr pArrayReplacement = translateExpr(_expr);

    for (size_t i = 0; i < _constr.size(); ++i) {
        assert(_constr.get(i)->getIndex());
        CVC3::ExprPtr
            pIndex = translateExpr(*_constr.get(i)->getIndex()),
            pValue = translateExpr(*_constr.get(i)->getValue());
        pArrayReplacement = NEW(Expr, m_pValidityChecker->writeExpr(*pArrayReplacement, *pIndex, *pValue));
    }

    return pArrayReplacement;
}

CVC3::ExprPtr Solver::_translateMapReplacement(const Expression& _expr, const MapConstructor& _constr) {
    CVC3::ExprPtr pMapReplacement = translateExpr(_expr);

    for (size_t i = 0; i < _constr.size(); ++i) {
        assert(_constr.get(i)->getIndex());
        CVC3::ExprPtr
            pIndex = translateExpr(*_constr.get(i)->getIndex()),
            pValue = translateExpr(*_constr.get(i)->getValue());
        pMapReplacement = NEW(Expr, m_pValidityChecker->writeExpr(*pMapReplacement, *pIndex, *pValue));
    }

    return pMapReplacement;
}

CVC3::ExprPtr Solver::_translateStructReplacement(const Expression& _expr, const StructConstructor& _constr) {
    std::vector<std::string> fields;
    std::vector<CVC3::Expr> exprs;
    std::set<std::wstring> initialized;

    const StructType& structType = *_expr.getType().as<StructType>();

    for (size_t i = 0; i < _constr.size(); ++i) {
        if (_constr.get(i)->getName().empty())
            continue;

        initialized.insert(_constr.get(i)->getName());

        fields.push_back(_makeIdentifier(_constr.get(i)->getName()));
        exprs.push_back(*translateExpr(*_constr.get(i)->getValue()));
    }

    CVC3::ExprPtr pRecord = translateExpr(_expr);

    for (size_t i = 0; i < structType.getNamesOrd().size(); ++i) {
        const NamedValue& field = *structType.getNamesOrd().get(i);
        if (initialized.find(field.getName()) != initialized.end())
            continue;

        const std::string strFieldName = _makeIdentifier(field.getName());
        CVC3::ExprPtr pSelectExpr = NEW(Expr, m_pValidityChecker->recSelectExpr(*pRecord, strFieldName));

        fields.push_back(strFieldName);
        exprs.push_back(*pSelectExpr);
    }

    return NEW(Expr, m_pValidityChecker->recordExpr(fields, exprs));
}

CVC3::ExprPtr Solver::_translateReplacement(const Replacement& _expr) {
    const Constructor& constr = *_expr.getNewValues();
    switch (constr.getConstructorKind()) {
        // TODO: ArrayIterator.
        case Constructor::ARRAY_ELEMENTS:
            return _translateArrayReplacement(*_expr.getObject(), *_expr.getNewValues().as<ArrayConstructor>());
        case Constructor::STRUCT_FIELDS:
            return _translateStructReplacement(*_expr.getObject(), *_expr.getNewValues().as<StructConstructor>());
        case Constructor::MAP_ELEMENTS:
            return _translateMapReplacement(*_expr.getObject(), *_expr.getNewValues().as<MapConstructor>());
        default:
            throw CVC3::Exception("Error translate replacement with constructor kind: " +
                intToStr(constr.getConstructorKind()));
    }

    return nullptr;
}

CVC3::ExprPtr Solver::_translateComponent(const Component& _expr) {
    switch (_expr.getComponentKind()) {
        case Component::ARRAY_PART:
            return _translateArrayPartExpr((ArrayPartExpr&)_expr);
        case Component::STRUCT_FIELD:
            return _translateFieldExpr((FieldExpr&)_expr);
        // There is no UnionAlternativeReference.
        case Component::MAP_ELEMENT:
            return _translateMapElementExpr((MapElementExpr&)_expr);
        case Component::LIST_ELEMENT:
            return _translateListElementExpr((ListElementExpr&)_expr);
        case Component::REPLACEMENT:
            return _translateReplacement((Replacement&)_expr);
        default:
            throw CVC3::Exception("Error translate component with kind: " + intToStr(_expr.getComponentKind()));
    }

    return nullptr;
}

CVC3::ExprPtr Solver::_translateFormula(const Formula& _expr) {
    std::vector<CVC3::Expr> vars;
    for (size_t i = 0; i < _expr.getBoundVariables().size(); ++i) {
        CVC3::ExprPtr pVar = _declareVariable(_expr.getBoundVariables().get(i));
        if (!pVar)
            return nullptr;
        vars.push_back(*pVar);
    }

    CVC3::ExprPtr
        pBody = translateExpr(*_expr.getSubformula()),
        pFormula = NEW(Expr, _expr.getQuantifier() == Formula::EXISTENTIAL
            ? m_pValidityChecker->existsExpr(vars, *pBody)
            : m_pValidityChecker->forallExpr(vars, *pBody));

    return pFormula;
}

CVC3::ExprPtr Solver::_translateFormulaCall(const FormulaCall& _expr) {
    if (_expr.getTarget()->getParams().empty())
        return translateExpr(*_expr.getTarget()->getFormula());
    CVC3::OpPtr pOp = _declareFormula(_expr.getTarget());

    std::vector<CVC3::Expr> args;
    for (size_t i = 0; i < _expr.getArgs().size(); ++i)
        args.push_back(*translateExpr(*_expr.getArgs().get(i)));

    return NEW(Expr, m_pValidityChecker->funExpr(*pOp, args));
}

CVC3::ExprPtr Solver::_translateStructConstructor(const StructConstructor& _expr) {
    std::vector<std::string> fields;
    std::vector<CVC3::Expr> exprs;
    for (size_t i = 0; i < _expr.size(); ++i) {
        const StructFieldDefinition& field = *_expr.get(i);
        if (field.getName().empty() && !field.getField())
            continue;
        fields.push_back(_makeIdentifier(field.getName().empty()
            ? field.getField()->getName()
            : field.getName()));
        exprs.push_back(*translateExpr(*field.getValue()));
    }
    return NEW(Expr, m_pValidityChecker->recordExpr(fields, exprs));
}

CVC3::ExprPtr Solver::_translateTupleConstructor(const StructConstructor& _expr) {
    std::vector<CVC3::Expr> exprs;
    for (size_t i = 0; i < _expr.size(); ++i)
        exprs.push_back(*translateExpr(*_expr.get(i)->getValue()));
    return NEW(Expr, m_pValidityChecker->tupleExpr(exprs));
}

template<typename T>
CVC3::ExprPtr Solver::_translateFiniteArray(const T& _expr) {
    CVC3::TypePtr pType = translateType(*_expr.getType());
    CVC3::ExprPtr pCons = NEW(Expr, m_pValidityChecker->varExpr(_makeNewVarName(L"cons"), *pType));

    for (size_t i = 0; i < _expr.size(); ++i) {
        if (!_expr.get(i)->getIndex())
            continue;
        CVC3::ExprPtr
            pIndex = translateExpr(*_expr.get(i)->getIndex()),
            pValue = translateExpr(*_expr.get(i)->getValue());
        pCons = NEW(Expr, m_pValidityChecker->writeExpr(*pCons, *pIndex, *pValue));
    }

    return pCons;
}

template<typename T1, typename T2>
CVC3::ExprPtr Solver::_translateInfiniteArray(const T1& _expr) {
    TypePtr pExprType = _expr.getType();

    CVC3::TypePtr
        pIndex = NEW(Type, m_pValidityChecker->intType()),
        pBase = translateType(*pExprType.as<T2>()->getBaseType()),
        pType = NEW(Type, m_pValidityChecker->arrayType(*pIndex, *pBase));

    CVC3::ExprPtr
        pCons = NEW(Expr, m_pValidityChecker->varExpr(_makeNewVarName(L"cons"), *pType)),
        pSize = NEW(Expr, m_pValidityChecker->ratExpr(_expr.size()));

    for (size_t i = 0; i < _expr.size(); ++i) {
        CVC3::ExprPtr
            pIndex = NEW(Expr, m_pValidityChecker->ratExpr(i)),
            pValue = translateExpr(*_expr.get(i));
        pCons = NEW(Expr, m_pValidityChecker->writeExpr(*pCons, *pIndex, *pValue));
    }

    return NEW(Expr, m_pValidityChecker->recordExpr("elements", *pCons, "size", *pSize));
}

CVC3::ExprPtr Solver::_translateArrayConstructor(const ArrayConstructor& _expr) {
    assert(_expr.getType() && _expr.getType()->getKind() == Type::ARRAY);
    return _translateFiniteArray(_expr);
}

CVC3::ExprPtr Solver::_translateSetConstructor(const SetConstructor& _expr) {
    assert(_expr.getType() && _expr.getType()->getKind() == Type::SET);
    return _translateInfiniteArray<SetConstructor, SetType>(_expr);
}

CVC3::ExprPtr Solver::_translateMapConstructor(const MapConstructor& _expr) {
    assert(_expr.getType() && _expr.getType()->getKind() == Type::MAP);
    return _translateFiniteArray(_expr);
}

CVC3::ExprPtr Solver::_translateListConstructor(const ListConstructor& _expr) {
    assert(!_expr.getType() && _expr.getType()->getKind() == Type::LIST);
    return _translateInfiniteArray<ListConstructor, ListType>(_expr);
}

CVC3::ExprPtr Solver::_translateConstructor(const Constructor& _expr) {
    switch (_expr.getConstructorKind()) {
        case Constructor::STRUCT_FIELDS: {
            const StructConstructor& cons = (const StructConstructor&)_expr;
            return _isTuple(cons) ?
                _translateTupleConstructor(cons):
                _translateStructConstructor(cons);
        }
        case Constructor::ARRAY_ELEMENTS:
            return _translateArrayConstructor((ArrayConstructor&)_expr);
        case Constructor::SET_ELEMENTS:
            return _translateSetConstructor((SetConstructor&)_expr);
        case Constructor::MAP_ELEMENTS:
            return _translateMapConstructor((MapConstructor&)_expr);
        case Constructor::LIST_ELEMENTS:
            return _translateListConstructor((ListConstructor&)_expr);
        // TODO: ARRAY_ITERATION, UNION_CONSTRUCTOR.
        default:
            throw CVC3::Exception("Error translate constructor with kind: " +
                intToStr(_expr.getConstructorKind()));
    }

    return nullptr;
}

CVC3::TypePtr Solver::_translateEnumType(const EnumType& _enum) {
    const std::string strName = _makeNewTypeName();

    std::vector<std::string> constructors;
    std::vector<std::vector<std::string>> selectors;
    std::vector<std::vector<CVC3::Expr>> types;

    const size_t cSize = _enum.getValues().size();
    constructors.reserve(cSize);
    selectors.reserve(cSize);
    types.reserve(cSize);

    for (size_t i = 0; i < cSize; ++i) {
        constructors.push_back(_makeIdentifier(_enum.getValues().get(i)->getName()));
        selectors.push_back(std::vector<std::string>());
        types.push_back(std::vector<CVC3::Expr>());
    }

    m_pValidityChecker->dataType(strName, constructors, selectors, types);
    return NEW(Type, m_pValidityChecker->lookupType(strName));
}

CVC3::TypePtr Solver::_translateStructType(const StructType& _struct) {
    std::vector<std::string> names;
    std::vector<CVC3::Type> types;

    const size_t cSize = _struct.getNamesOrd().size() + _struct.getNamesSet().size();
    names.reserve(cSize);
    types.reserve(cSize);

    for (size_t i = 0; i < _struct.getNamesOrd().size(); ++i) {
        names.push_back(_makeIdentifier(_struct.getNamesOrd().get(i)->getName()));
        types.push_back(*translateType(*_struct.getNamesOrd().get(i)->getType()));
    }
    for (size_t i = 0; i < _struct.getNamesSet().size(); ++i) {
        names.push_back(_makeIdentifier(_struct.getNamesSet().get(i)->getName()));
        types.push_back(*translateType(*_struct.getNamesSet().get(i)->getType()));
    }

    return NEW(Type, m_pValidityChecker->recordType(names, types));
}

CVC3::TypePtr Solver::_translateUnionType(const UnionType& _union) {
    const std::string strName = _makeNewTypeName();

    std::vector<std::string> constructors;
    std::vector<std::vector<std::string>> selectors;
    std::vector<std::vector<CVC3::Expr>> types;

    const size_t cSize = _union.getConstructors().size();
    constructors.reserve(cSize);
    selectors.reserve(cSize);
    types.reserve(cSize);

    for (size_t i = 0; i < _union.getConstructors().size(); ++i) {
        const UnionConstructorDeclaration& cons = *_union.getConstructors().get(i);

        constructors.push_back(_makeIdentifier(cons.getName()));
        selectors.push_back(std::vector<std::string>());
        types.push_back(std::vector<CVC3::Expr>());

        const NamedValuesPtr pFields = cons.getStructFields() ?
            cons.getStructFields()->mergeFields() : NamedValuesPtr();

        if (!pFields)
            continue;

        const size_t cSubSize = pFields->size();
        selectors.back().reserve(cSubSize);
        types.back().reserve(cSubSize);

        for (size_t j = 0; j < cSubSize; ++j) {
            const NamedValue& field = *pFields->get(j);
            selectors.back().push_back(_makeIdentifier(field.getName()));
            types.back().push_back(translateType(*field.getType())->getExpr());
        }
    }

    m_pValidityChecker->dataType(strName, constructors, selectors, types);
    return NEW(Type, m_pValidityChecker->lookupType(strName));
}

CVC3::TypePtr Solver::_translateArrayType(const ArrayType& _array) {
    CVC3::TypePtr
        pDim = translateType(*_array.getDimensionType()),
        pBase = translateType(*_array.getBaseType());

    return NEW(Type, m_pValidityChecker->arrayType(*pDim, *pBase));
}

CVC3::TypePtr Solver::_translateMapType(const MapType& _map) {
    CVC3::TypePtr
        pDim = translateType(*_map.getIndexType()),
        pBase = translateType(*_map.getBaseType());

    return NEW(Type, m_pValidityChecker->arrayType(*pDim, *pBase));
}

CVC3::TypePtr Solver::_translateSubtype(const Subtype& _subtype) {
    CVC3::ExprPtr
        pPred = translateExpr(*_subtype.getExpression()),
        pVar = _declareVariable(_subtype.getParam()),
        pLambda = _makeLambdaExpr(pVar, pPred);
    return NEW(Type, m_pValidityChecker->subtypeType(*pLambda, CVC3::Expr()));
}

CVC3::TypePtr Solver::_translateRange(const Range& _range) {
    CVC3::ExprPtr
        pLeft = translateExpr(*_range.getMin()),
        pRight = translateExpr(*_range.getMax());
    return NEW(Type, m_pValidityChecker->subrangeType(*pLeft, *pRight));
}

CVC3::TypePtr Solver::_translateBranch(const Branch& _branch) {
    if (_branch.size() == 1)
        return NEW(Type, *translateType(*_branch.get(0)->getType(), true));

    std::vector<CVC3::Type> types;
    types.reserve(_branch.size());

    for (size_t i = 0; i < _branch.size(); ++i)
        types.push_back(*translateType(*_branch.get(i)->getType()));

    return NEW(Type, m_pValidityChecker->tupleType(types));
}

CVC3::TypePtr Solver::_translatePredicateType(const PredicateType& _pred) {
    std::vector<CVC3::Type> typeDom;
    typeDom.reserve(_pred.getInParams().size());

    for (size_t i = 0; i < _pred.getInParams().size(); ++i)
        typeDom.push_back(*translateType(*_pred.getInParams().get(i)->getType()));

    if (_pred.getOutParams().empty())
        return NEW(Type, m_pValidityChecker->funType(typeDom, CVC3::Type()));
    if (_pred.getOutParams().size() == 1)
        return NEW(Type, m_pValidityChecker->funType(typeDom, *_translateBranch(*_pred.getOutParams().get(0))));

    const std::string strName = _makeNewTypeName();

    std::vector<std::string> constructors;
    std::vector<std::vector<std::string>> selectors;
    std::vector<std::vector<CVC3::Expr>> types;

    const size_t cSize = _pred.getOutParams().size();
    constructors.reserve(cSize);
    selectors.reserve(cSize);
    types.reserve(cSize);

    for (size_t i = 0; i < _pred.getOutParams().size(); ++i) {
        const Branch& branch = *_pred.getOutParams().get(i);

        constructors.push_back(_makeIdentifier(branch.getLabel()->getName()));
        selectors.push_back(std::vector<std::string>());
        types.push_back(std::vector<CVC3::Expr>());

        const size_t cSubSize = branch.size();
        selectors.back().reserve(cSubSize);
        types.back().reserve(cSubSize);

        for (size_t j = 0; j < branch.size(); ++j) {
            selectors.back().push_back(_makeIdentifier(branch.get(j)->getName()));
            types.back().push_back(translateType(*branch.get(j)->getType())->getExpr());
        }
    }

    m_pValidityChecker->dataType(strName, constructors, selectors, types);
    return NEW(Type, m_pValidityChecker->funType(typeDom, m_pValidityChecker->lookupType(strName)));
}

CVC3::TypePtr Solver::_translateNamedReferenceType(const NamedReferenceType& _type) {
    const Type& decl = *_type.getDeclaration()->getType();
    CVC3::TypePtr pType = _getType(&_type);

    if (!pType) {
        const std::string strName = _makeNewTypeName();
        CVC3::TypePtr pDecl = translateType(decl);
        m_pValidityChecker->createType(strName, *pDecl);
        pType = NEW(Type, m_pValidityChecker->lookupType(strName));
    }

    return pType;
}

std::wstring fmtResult(CVC3::QueryResult _result, bool _bValid) {
    switch (_result) {
        case CVC3::SATISFIABLE:
            return (_bValid ? L"Invalid" : L"Satisfiable");
        case CVC3::UNSATISFIABLE:
            return (_bValid ? L"Valid\n" : L"Unsatisfiable\n");
        case CVC3::ABORT:
            return L"Abort\n";
        case CVC3::UNKNOWN:
            return L"Unknown\n";
    }

    return L"";
}

CVC3::QueryResult checkValidity(const ExpressionPtr& _pExpr) {
    Solver solver;
    CVC3::ExprPtr pExpr = solver.translateExpr(*_pExpr);
    return solver.getContext().query(*pExpr);
}

class ModuleChecker : public Visitor, Solver {
public:
    ModuleChecker(QueryResult& _result, bool _bRewriteStatus = false) :
        Visitor(CHILDREN_FIRST), m_bRewriteStatus(_bRewriteStatus), m_result(_result)
    {}

    virtual bool visitLemmaDeclaration(LemmaDeclaration& _lemma) {
        if (!_lemma.getProposition() || na::containsBannedNodes(_lemma.getProposition()))
            return true;

        CVC3::ExprPtr pExpr;

        try {
            pExpr = translateExpr(*_lemma.getProposition());
        } catch (CVC3::Exception& ex) {
            std::wcerr << strWiden(ex.toString()) << L"\n\n";
            return true;
        }

        CVC3::QueryResult result;

        try {
            result = getContext().query(*pExpr);
        } catch (CVC3::Exception& ex) {
            std::wcerr << strWiden(ex.toString()) << L"\n\n";
            return true;
        }

        m_result.insert({&_lemma, result});

        if (!m_bRewriteStatus)
            return true;

        switch (result) {
            case CVC3::VALID:
                _lemma.setStatus(LemmaDeclaration::VALID);
                break;
            case CVC3::INVALID:
                _lemma.setStatus(LemmaDeclaration::INVALID);
                break;
            default:
                break;
        }

        return true;
    }

private:
    bool m_bRewriteStatus;
    QueryResult &m_result;
};

void checkValidity(const ir::ModulePtr& _pModule, QueryResult& _result) {
    if (!_pModule)
        return;
    ModuleChecker(_result).traverseNode(*_pModule);
}

void checkValidity(const ir::ModulePtr& _pModule) {
    if (!_pModule)
        return;
    QueryResult result;
    ModuleChecker(result, true).traverseNode(*_pModule);
}

void printImage(const ir::Expression& _expr, std::ostream& _os) {
    Solver solver;
    CVC3::ExprPtr pExpr = solver.translateExpr(_expr);
    solver.getContext().printExpr(*pExpr, _os);
}

void printImage(const ir::Type& _type, std::ostream& _os) {
    Solver solver;
    CVC3::TypePtr pType = solver.translateType(_type);
    _os << pType->toString() << "\n";
}

#undef NEW

} // namespace cvc3
