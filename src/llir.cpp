/// \file llir.cpp
///

#include <map>
#include <iostream>

#include <assert.h>

#include "ir/statements.h"
#include "llir.h"
#include "utils.h"
#include "llir_process.h"

namespace llir {

class Translator {
public:
    Translator(Translator * _pParent) :
        m_pParent(_pParent),
        m_pModule(_pParent ? _pParent->m_pModule : NULL),
        m_pFunction(_pParent ? _pParent->m_pFunction : NULL)
     {}
    ~Translator();

    Translator * addChild();

    Auto<Variable> resolveVariable(const void * _pVar, bool & _bReference);
    int resolveLabel(const ir::Label *);
    void addVariable(const void * _pOrig, Auto<Variable> _pNew, bool _bReference = false);

    void addType(const ir::Type * _pIRType, const Auto<Type> & _pLLIRType);
    Auto<Type> resolveType(const ir::Node * _pType);

    void addBuiltin(const std::wstring & _strName, const Auto<Function> & _pFunc);
    Auto<Function> resolveBuiltin(const std::wstring & _strName);

    void addComparator(const ir::Type * _pType, const Auto<Function> & _pFunc);
    Auto<Function> resolveComparator(const ir::Type * _pType);

    void translate(const ir::Module & _module, Module & _dest);

    Auto<Type> translate(const ir::Type & _type);
    Auto<FunctionType> translate(const ir::PredicateType & _type);
    Auto<Type> translate(const ir::NamedReferenceType & _type);
    Auto<StructType> translate(const ir::StructType & _type);
    Auto<StructType> translate(const ir::UnionType & _type);
    Auto<StructType> translate(const ir::UnionConstructorDeclaration &_cons);
    Auto<Function> translate(const ir::Predicate & _pred);
    void translate(const ir::Statement & _stmt, Instructions & _instrs);
    void translate(const ir::Block & _stmt, Instructions & _instrs);
    void translate(const ir::If & _stmt, Instructions & _instrs);
    void translate(const ir::Assignment & _stmt, Instructions & _instrs);
    void translateAsssignment(const ir::NamedValue * _pLHS, const ir::Expression * _pExpr, Instructions & _instrs);
    void translate(const ir::Call & _stmt, Instructions & _instrs);
    void translate(const ir::VariableDeclaration & _stmt, Instructions & _instrs);
    void translate(const ir::Jump & _stmt, Instructions & _instrs);
    void translate(const ir::Switch & _stmt, Instructions & _instrs);
    void translateSwitchInt(const ir::Switch & _stmt, const Operand & _arg,
            Instructions & _instrs);
    void translateSwitchUnion(const ir::Switch & _stmt, const Operand & _arg,
            Instructions & _instrs);
    Operand translate(const ir::Expression & _expr, Instructions & _instrs);
    Operand translate(const ir::Binary & _expr, Instructions & _instrs);
    Operand translate(const ir::FunctionCall & _expr, Instructions & _instrs);
    Operand translate(const ir::VariableReference & _expr, Instructions & _instrs);
    Operand translate(const ir::PredicateReference & _expr, Instructions & _instrs);
    Operand translate(const ir::Literal & _expr, Instructions & _instrs);
    Operand translateStringLiteral(const std::wstring & _str, Instructions & _instrs);
    Operand translate(const ir::Ternary & _expr, Instructions & _instrs);
    Operand translate(const ir::Component & _expr, Instructions & _instrs);
    Operand translate(const ir::FieldExpr & _expr, Instructions & _instrs);
    Operand translate(const ir::UnionAlternativeExpr & _expr, Instructions & _instrs);
    Operand translate(const ir::Constructor & _expr, Instructions & _instrs);
    Operand translateEq(const ir::Type * _pType, const Operand & _lhs, const Operand & _rhs, Instructions & _instrs);
    Operand translateEqUnion(const ir::UnionType * _pType, const Operand & _lhs, const Operand & _rhs, Instructions & _instrs);
    Operand translateEqStruct(const ir::NamedValues &_fields, const Operand & _lhs, const Operand & _rhs, Instructions & _instrs);

    void initStruct(const ir::StructConstructor & _expr, Instructions & _instrs, const ir::NamedValues &_fields, const Operand & _ptr);

    Operand translate(const ir::StructConstructor & _expr, Instructions & _instrs);
    Operand translate(const ir::UnionConstructor & _expr, Instructions & _instrs);
    Operand translateSwitchCond(const ir::Expression & _expr, const Operand & _arg,
            Instructions & _instrs);

    // Builtins.
    bool translateBuiltin(const ir::Call & _stmt, Instructions & _instrs);
    void translatePrint(const ir::Call & _stmt, Instructions & _instrs);
    void translatePrintExpr(const ir::Expression & _expr, Instructions & _instrs);

    //void insertRefs(Instructions & _instrs);
    //void insertUnrefs();

private:
    typedef std::map<const void *, std::pair<Auto<Variable>, bool> > variable_map_t;
    typedef std::map<const ir::Label *, int> label_numbers_t;
    typedef std::map<const ir::Node *, Auto<Type> > type_map_t;
    typedef std::map<const ir::Type *, Auto<Function> > compare_map_t;
    typedef std::map<std::wstring, Auto<Function> > builtin_map_t;

    Auto<StructType> m_unionType;
    Translator * m_pParent;
    Module * m_pModule;
    Auto<Function> m_pFunction;
    std::list<Translator *> m_children;
    variable_map_t m_vars;
    label_numbers_t m_labels;
    type_map_t m_types;
    Args m_ptrs;
    compare_map_t m_comparators;
    builtin_map_t m_builtins;
};

Translator::~Translator() {
    for (std::list<Translator *>::iterator iChild = m_children.begin(); iChild != m_children.end(); ++ iChild)
        delete * iChild;
}

void Translator::addVariable(const void * _pOrig, Auto<Variable> _pNew, bool _bReference) {
    m_vars[_pOrig] = std::make_pair(_pNew, _bReference);
}

Translator * Translator::addChild() {
    m_children.push_back(new Translator(this));
    return m_children.back();
}

Auto<Variable> Translator::resolveVariable(const void * _pVar, bool & _bReference) {
    variable_map_t::iterator iVar = m_vars.find(_pVar);

    if (iVar != m_vars.end()) {
        _bReference = iVar->second.second;
        return iVar->second.first;
    } else if (m_pParent)
        return m_pParent->resolveVariable(_pVar, _bReference);
    else
        return NULL;
}

void Translator::addType(const ir::Type * _pIRType, const Auto<Type> & _pLLIRType) {
    if (! m_pParent)
        m_types[_pIRType] = _pLLIRType;
    else
        m_pParent->addType(_pIRType, _pLLIRType);
}

Auto<Type> Translator::resolveType(const ir::Node * _pType) {
    if (m_pParent)
        return m_pParent->resolveType(_pType);

    type_map_t::iterator iType = m_types.find(_pType);

    return (iType == m_types.end()) ? NULL : iType->second;
}

void Translator::addBuiltin(const std::wstring & _strName, const Auto<Function> & _pFunc) {
    if (! m_pParent)
        m_builtins[_strName] = _pFunc;
    else
        m_pParent->addBuiltin(_strName, _pFunc);
}

Auto<Function> Translator::resolveBuiltin(const std::wstring & _strName) {
    if (m_pParent)
        return m_pParent->resolveBuiltin(_strName);

    builtin_map_t::iterator iFunc = m_builtins.find(_strName);

    return (iFunc == m_builtins.end()) ? NULL : iFunc->second;
}

void Translator::addComparator(const ir::Type * _pType, const Auto<Function> & _pFunc) {
    if (! m_pParent)
        m_comparators[_pType] = _pFunc;
    else
        m_pParent->addComparator(_pType, _pFunc);
}

Auto<Function> Translator::resolveComparator(const ir::Type * _pType) {
    if (m_pParent)
        return m_pParent->resolveComparator(_pType);

    compare_map_t::iterator iFunc = m_comparators.find(_pType);

    return (iFunc == m_comparators.end()) ? NULL : iFunc->second;
}

int Translator::resolveLabel(const ir::Label * _pLabel) {
    label_numbers_t::iterator iLabel = m_labels.find(_pLabel);

    if (iLabel != m_labels.end())
        return iLabel->second;
    else if (m_pParent)
        return m_pParent->resolveLabel(_pLabel);
    else
        return -1;
}

Auto<FunctionType> Translator::translate(const ir::PredicateType & _type) {
    const size_t cBranches = _type.getOutParams().size();
    Auto<Type> returnType;
    ir::NamedValue * pResult = NULL;

    if (cBranches == 1 && _type.getOutParams().get(0)->size() == 1) {
        // Trivial predicate: one branch, one output parameter.
        pResult = _type.getOutParams().get(0)->get(0);
        returnType = translate(* pResult->getType());
    } else if (cBranches < 2) {
        // Pass output params as pointers.
        returnType = new Type(Type::VOID);
    } else {
        // Pass output params as pointers, return branch index.
        returnType = new Type(Type::INT32);
    }

    Auto<FunctionType> pFunc = new FunctionType(returnType);

    for (size_t i = 0; i < _type.getInParams().size(); ++ i) {
        ir::NamedValue * pVar = _type.getInParams().get(i);
        pFunc->argTypes().push_back(translate(* pVar->getType()));
    }

    if (! pResult) {
        for (size_t i = 0; i < _type.getOutParams().size(); ++ i) {
            ir::Branch & branch = * _type.getOutParams().get(i);
            for (size_t j = 0; j < branch.size(); ++ j) {
                ir::NamedValue * pVar = branch.get(j);
                pFunc->argTypes().push_back(new PointerType(translate(* pVar->getType())));
            }
        }
    }

    return pFunc;
}

Auto<Type> Translator::translate(const ir::NamedReferenceType & _type) {
    if (_type.getDeclaration()) {
        return translate(* _type.getDeclaration()->getType());
    }
    assert(false);
    return NULL;
}

Auto<StructType> Translator::translate(const ir::StructType & _type) {
    Auto<StructType> structType = Auto<StructType> (resolveType(& _type));

    if (! structType.empty())
        return structType;

    structType = new StructType();

    assert(_type.getNamesSet().empty());

    for (size_t j = 0; j < 2; ++j)
        for (size_t i = 0; i < _type.getAllFields()[j].size(); ++ i)
            structType->fieldTypes().push_back(translate(*_type.getAllFields()[j].get(i)->getType()));

    m_pModule->types().push_back(structType);
    addType(& _type, structType);

    return structType;
}

Auto<StructType> Translator::translate(const ir::UnionType & _type) {
    if (m_pParent)
        return m_pParent->translate(_type);

    if (m_unionType.empty()) {
        m_unionType = new StructType();
        m_unionType->fieldTypes().push_back(new Type(Type::UINT32));
        m_unionType->fieldTypes().push_back(new PointerType(new Type(Type::VOID)));
        m_pModule->types().push_back(m_unionType);
    }

    addType(& _type, m_unionType);

    return m_unionType;
}

Auto<StructType> Translator::translate(const ir::UnionConstructorDeclaration &_cons) {
    Auto<StructType> structType = Auto<StructType> (resolveType(& _cons));

/*    if (! structType.empty())
        return structType;

    structType = new StructType();

    for (size_t i = 0; i < _type.getFields().size(); ++ i) {
        structType->fieldTypes().push_back(translate(* _type.getFields().get(i)->getType()));
    }

    m_pModule->types().push_back(structType);
    addType(& _type, structType);
*/
    return structType;
}

Auto<Type> Translator::translate(const ir::Type & _type) {
    const int nBits = _type.getBits();

    switch (_type.getKind()) {
        case ir::Type::INT:
            switch (nBits) {
                case Number::GENERIC: return new Type(Type::GMP_Z);
                case Number::NATIVE: return new Type(Type::INT32);
                default:
                    if (nBits <= 8) return new Type(Type::INT8);
                    if (nBits <= 16) return new Type(Type::INT16);
                    if (nBits <= 32) return new Type(Type::INT32);
                    if (nBits <= 64) return new Type(Type::INT64);
                    return new Type(Type::GMP_Z);
            }
        case ir::Type::NAT:
            switch (nBits) {
                case Number::GENERIC: return new Type(Type::GMP_Z);
                case Number::NATIVE: return new Type(Type::UINT32);
                default:
                    if (nBits <= 8) return new Type(Type::UINT8);
                    if (nBits <= 16) return new Type(Type::UINT16);
                    if (nBits <= 32) return new Type(Type::UINT32);
                    if (nBits <= 64) return new Type(Type::UINT64);
                    return new Type(Type::GMP_Z);
            }
        case ir::Type::REAL:
            switch (nBits) {
                case Number::NATIVE: return new Type(Type::DOUBLE);
                case Number::GENERIC: return new Type(Type::GMP_Q);
                default:
                    if (nBits <= (int) sizeof(float)) return new Type(Type::FLOAT);
                    if (nBits <= (int) sizeof(double)) return new Type(Type::DOUBLE);
                    if (nBits <= (int) sizeof(long double)) return new Type(Type::QUAD);
                    return new Type(Type::GMP_Q);
            }
        case ir::Type::BOOL: return new Type(Type::BOOL);
        case ir::Type::CHAR: return new Type(Type::WCHAR);
        case ir::Type::STRING: return new PointerType(new Type(Type::WCHAR));
        case ir::Type::PREDICATE: return translate((ir::PredicateType &) _type);
        case ir::Type::NAMED_REFERENCE: return translate((ir::NamedReferenceType &) _type);
        case ir::Type::STRUCT: return translate((ir::StructType &) _type);
        case ir::Type::UNION: return translate((ir::UnionType &) _type);

        default: return new Type();
    }
}

static int _selectInstr(int _opKind, int _instrInt, int _instrFloat, int _instrZ, int _instrQ) {
    int nInstr = -1;
    if (_opKind & Type::INTMASK) nInstr =_instrInt;
    else if (_opKind & Type::FLOATMASK) nInstr =_instrFloat;
    else if (_opKind == Type::GMP_Z) nInstr =_instrZ;
    else if (_opKind == Type::GMP_Q) nInstr =_instrQ;
    //assert(nInstr >= 0);
    return nInstr;
}

static
int _selectBinaryInstr(int _op, const Auto<llir::Type> _pType) {
    switch (_op) {
        case ir::Binary::ADD:
            return _selectInstr(_pType->getKind(), Binary::ADD, Binary::FADD, Binary::ZADD, Binary::QADD);
        case ir::Binary::SUBTRACT:
            return _selectInstr(_pType->getKind(), Binary::SUB, Binary::FSUB, Binary::ZSUB, Binary::ZADD);
        case ir::Binary::MULTIPLY:
            return _selectInstr(_pType->getKind(), Binary::MUL, Binary::FMUL, Binary::ZMUL, Binary::QMUL);
        case ir::Binary::DIVIDE:
            return _selectInstr(_pType->getKind(), Binary::DIV, Binary::FDIV, Binary::ZDIV, Binary::QDIV);
        case ir::Binary::POWER:
            return _selectInstr(_pType->getKind(), Binary::POW, Binary::FPOW, Binary::ZPOW, Binary::QPOW);

        case ir::Binary::REMAINDER:
            return _selectInstr(_pType->getKind(), Binary::REM, -1, Binary::ZREM, -1);
        case ir::Binary::SHIFT_LEFT:
            return _selectInstr(_pType->getKind(), Binary::SHL, -1, Binary::ZSHL, -1);
        case ir::Binary::SHIFT_RIGHT:
            return _selectInstr(_pType->getKind(), Binary::SHR, -1, Binary::ZSHR, -1);

        case ir::Binary::EQUALS:
            return _selectInstr(_pType->getKind(), Binary::EQ, Binary::FEQ, Binary::ZEQ, Binary::QEQ);
        case ir::Binary::NOT_EQUALS:
            return _selectInstr(_pType->getKind(), Binary::NE, Binary::FNE, Binary::ZNE, Binary::QNE);
        case ir::Binary::LESS:
            return _selectInstr(_pType->getKind(), Binary::LT, Binary::FLT, Binary::ZLT, Binary::QLT);
        case ir::Binary::LESS_OR_EQUALS:
            return _selectInstr(_pType->getKind(), Binary::LTE, Binary::FLTE, Binary::ZLTE, Binary::QLTE);
        case ir::Binary::GREATER:
            return _selectInstr(_pType->getKind(), Binary::GT, Binary::FGT, Binary::ZGT, Binary::QGT);
        case ir::Binary::GREATER_OR_EQUALS:
            return _selectInstr(_pType->getKind(), Binary::GTE, Binary::FGTE, Binary::ZGTE, Binary::QGTE);

        // TODO: use boolean instructions when type inference gets implemented.
        case ir::Binary::BOOL_AND:
        case ir::Binary::BITWISE_AND:
            return _selectInstr(_pType->getKind(), Binary::AND, -1, Binary::ZAND, -1);
        case ir::Binary::BOOL_OR:
        case ir::Binary::BITWISE_OR:
            return _selectInstr(_pType->getKind(), Binary::OR, -1, Binary::ZOR, -1);
        case ir::Binary::BOOL_XOR:
        case ir::Binary::BITWISE_XOR:
            return _selectInstr(_pType->getKind(), Binary::XOR, -1, Binary::ZXOR, -1);
    }

    return -1;
}

Operand Translator::translateEqUnion(const ir::UnionType * _pType, const Operand & _lhs, const Operand & _rhs, Instructions & _instrs) {
    Auto<Function> func = resolveComparator(_pType);
    Auto<FunctionType> funcType;

    if (! func.empty()) {
        funcType = func->getType();
        Auto<Variable> funcVar = func;
        Auto<Call> call = new Call(funcVar, funcType);

        call->args().push_back(_lhs);
        call->args().push_back(_rhs);
        _instrs.push_back(call);

        return Operand(call->getResult());
    }

    static int nCmpCount = 0;

    func = new Function(std::wstring(L"cmp_") + fmtInt(nCmpCount ++), new Type(Type::BOOL));
    funcType = new FunctionType(new Type(Type::BOOL));
    funcType->argTypes().push_back(_lhs.getType());
    funcType->argTypes().push_back(_rhs.getType());
    func->args().push_back(new Variable(_lhs.getType()));
    func->args().push_back(new Variable(_rhs.getType()));
    func->setType(funcType);
    addComparator(_pType, func);
    m_pModule->functions().push_back(func);

    Instructions & instrs = func->instructions();
    Translator trans(this);
    Auto<Label> labelEnd = new Label();

    trans.m_pFunction = func;

    instrs.push_back(new Unary(Unary::PTR, Operand(func->args().front())));

    Operand lPtr = Operand(instrs.back()->getResult());

    instrs.push_back(new Unary(Unary::PTR, Operand(func->args().back())));

    Operand rPtr = Operand(instrs.back()->getResult());

    instrs.push_back(new Field(lPtr, 0));
    instrs.push_back(new Unary(Unary::LOAD, Operand(instrs.back()->getResult())));

    Operand lTag = Operand(instrs.back()->getResult());

    instrs.push_back(new Field(rPtr, 0));
    instrs.push_back(new Unary(Unary::LOAD, Operand(instrs.back()->getResult())));

    Operand rTag = Operand(instrs.back()->getResult());

    instrs.push_back(new Binary(Binary::EQ, lTag, rTag));

    if (_pType->getConstructors().empty()) {
        instrs.push_back(new Unary(Unary::RETURN, instrs.back()->getResult()));
    } else {
        Auto<Variable> varResult = func->getResult();

        instrs.push_back(new Binary(Binary::SET, varResult, instrs.back()->getResult()));
        instrs.push_back(new Binary(Binary::JMZ, varResult, Operand(labelEnd)));

        instrs.push_back(new Field(lPtr, 1));
        lPtr = Operand(instrs.back()->getResult());
        instrs.push_back(new Field(rPtr, 1));
        rPtr = Operand(instrs.back()->getResult());

        Auto<Switch> sw = new Switch(lTag);

        for (size_t i = 0; i < _pType->getConstructors().size(); ++ i) {
            sw->cases().push_back(SwitchCase());

            SwitchCase & swCase = sw->cases().back();
            Instructions & body = swCase.body;

            const ir::UnionConstructorDeclaration * pCons = _pType->getConstructors().get(i);
            const ir::NamedValues &fields = pCons->getFields();
//            const ir::StructType & dataType = pCons->getStruct();

            swCase.values.push_back(i);

            if (fields.size() == 0) {
                body.push_back(new Binary(Binary::SET, varResult,
                        Operand(Literal(new Type(Type::BOOL), Number::makeInt(1)))));
                continue;
            }

            if (fields.size() == 1) {
                const ir::Type * pFieldType = resolveBaseType(fields.get(0)->getType());
                Auto<Type> fieldType = translate(* pFieldType);
                Operand l = lPtr, r = rPtr;

                if (fieldType->sizeOf() > Type::sizeOf(Type::POINTER)) {
                    body.push_back(new Unary(Unary::LOAD, lPtr));
                    l = Operand(body.back()->getResult());
                    body.push_back(new Unary(Unary::LOAD, rPtr));
                    r = Operand(body.back()->getResult());
                }

                body.push_back(new Cast(l, new PointerType(fieldType)));
                body.push_back(new Unary(Unary::LOAD, Operand(body.back()->getResult())));
                l = Operand(body.back()->getResult());
                body.push_back(new Cast(r, new PointerType(fieldType)));
                body.push_back(new Unary(Unary::LOAD, Operand(body.back()->getResult())));
                r = Operand(body.back()->getResult());

                Operand eq = trans.translateEq(pFieldType, l, r, body);

                body.push_back(new Binary(Binary::SET, varResult, eq));
                continue;
            }

            Auto<StructType> st = translate(*pCons);
            Operand l = lPtr, r = rPtr;

            body.push_back(new Unary(Unary::LOAD, lPtr));
            body.push_back(new Cast(Operand(body.back()->getResult()), new PointerType(st)));
            body.push_back(new Unary(Unary::LOAD, Operand(body.back()->getResult())));
            l = Operand(body.back()->getResult());
            body.push_back(new Unary(Unary::LOAD, rPtr));
            body.push_back(new Cast(Operand(body.back()->getResult()), new PointerType(st)));
            body.push_back(new Unary(Unary::LOAD, Operand(body.back()->getResult())));
            r = Operand(body.back()->getResult());

            Operand eq = trans.translateEqStruct(fields, l, r, body);

            body.push_back(new Binary(Binary::SET, varResult, eq));
        }

        instrs.push_back(sw);
        instrs.push_back(new Instruction());
        instrs.back()->setLabel(labelEnd);
    }

    instrs.push_back(new Unary(Unary::RETURN, func->getResult()));

    processLL<MarkEOLs>(* func);
    processLL<CountLabels>(* func);
    processLL<PruneJumps>(* func);
    processLL<CollapseReturns>(* func);
    processLL<RecycleVars>(* func);

    Auto<Variable> funcVar = func;
    Auto<Call> call = new Call(funcVar, funcType);

    call->args().push_back(_lhs);
    call->args().push_back(_rhs);
    _instrs.push_back(call);

    return Operand(call->getResult());
}

Operand Translator::translateEqStruct(const ir::NamedValues &_fields, const Operand & _lhs, const Operand & _rhs, Instructions & _instrs) {
    if (_fields.empty())
        return Operand(Literal(new Type(Type::BOOL), Number::makeInt(1)));

    Auto<Label> labelEnd = new Label();
    Auto<Variable> varResult = new Variable(new Type(Type::BOOL));

    addVariable(NULL, varResult, false);
    m_pFunction->locals().push_back(varResult);

    _instrs.push_back(new Unary(Unary::PTR, _lhs));
    Operand lPtr = Operand(_instrs.back()->getResult());
    _instrs.push_back(new Unary(Unary::PTR, _rhs));
    Operand rPtr = Operand(_instrs.back()->getResult());

    for (size_t i = 0; i < _fields.size(); ++ i) {
        const ir::Type * pFieldType = resolveBaseType(_fields.get(i)->getType());

        _instrs.push_back(new Field(lPtr, i));
        _instrs.push_back(new Unary(Unary::LOAD, Operand(_instrs.back()->getResult())));
        Operand lField = Operand(_instrs.back()->getResult());
        _instrs.push_back(new Field(rPtr, i));
        _instrs.push_back(new Unary(Unary::LOAD, Operand(_instrs.back()->getResult())));
        Operand rField = Operand(_instrs.back()->getResult());
        Operand cmp = translateEq(pFieldType, lField, rField, _instrs);

        _instrs.push_back(new Binary(Binary::SET, varResult, cmp));
        _instrs.push_back(new Binary(Binary::JMZ, cmp, Operand(labelEnd)));
    }

    _instrs.push_back(new Instruction()); // nop
    _instrs.back()->setLabel(labelEnd);

    return Operand(varResult);
}

Operand Translator::translateEq(const ir::Type * _pType, const Operand & _lhs, const Operand & _rhs, Instructions & _instrs) {
    std::wcerr << L"!! _pType->getKind() = " << _pType->getKind() << std::endl;
    switch (_pType->getKind()) {
        case ir::Type::UNION:
            return translateEqUnion((const ir::UnionType *) _pType, _lhs, _rhs, _instrs);
        case ir::Type::STRUCT: {
            const ir::StructType *pStruct = (const ir::StructType *)_pType;
            if (!pStruct->getNamesOrd().empty())
                return translateEqStruct(pStruct->getNamesOrd(), _lhs, _rhs, _instrs);
            if (!pStruct->getTypesOrd().empty())
                return translateEqStruct(pStruct->getTypesOrd(), _lhs, _rhs, _instrs);
        }
    }

    const int nInstr = _selectInstr(_lhs.getType()->getKind(), Binary::EQ, Binary::FEQ, Binary::ZEQ, Binary::QEQ);

    if (nInstr < 0) {
        assert(false && "Cannot compare values of this type");
    }

    _instrs.push_back(new Binary(nInstr, _lhs, _rhs));

    return Operand(_instrs.back()->getResult());
}

Operand Translator::translate(const ir::Binary & _expr, Instructions & _instrs) {
    Operand left = translate(* _expr.getLeftSide(), _instrs);
    Operand right = translate(* _expr.getRightSide(), _instrs);

    if (_expr.getOperator() == ir::Binary::EQUALS)
        return translateEq(resolveBaseType(_expr.getLeftSide()->getType()), left, right, _instrs);

    int nInstr = _selectBinaryInstr(_expr.getOperator(), left.getType());

    assert (nInstr >= 0);

    _instrs.push_back(new Binary(nInstr, left, right));

    return Operand(_instrs.back()->getResult());
}

Operand Translator::translate(const ir::Component & _expr, Instructions & _instrs) {
    switch (_expr.getComponentKind()) {
        case ir::Component::STRUCT_FIELD:
            return translate((ir::FieldExpr &) _expr, _instrs);
        case ir::Component::UNION_ALTERNATIVE:
            return translate((ir::UnionAlternativeExpr &) _expr, _instrs);
    }

    assert(false);
}

Operand Translator::translate(const ir::FieldExpr & _expr, Instructions & _instrs) {
/*    const ir::StructType * pStruct = _expr.getStructType();
    Auto<StructType> st = translate(* pStruct);
    Operand object = translate(* _expr.getObject(), _instrs);

    _instrs.push_back(new Unary(Unary::Ptr, object));
    _instrs.push_back(new Field(Operand(_instrs.back()->getResult()), _expr.getFieldIdx()));
    _instrs.push_back(new Unary(Unary::Load, Operand(_instrs.back()->getResult())));

    return Operand(_instrs.back()->getResult());
*/
    return Operand();
}

Operand Translator::translate(const ir::UnionAlternativeExpr & _expr, Instructions & _instrs) {
    const ir::UnionType * pUnion = _expr.getUnionType();
    Auto<StructType> st = translate(* pUnion);
    Operand object = translate(* _expr.getObject(), _instrs);

    _instrs.push_back(new Unary(Unary::PTR, object));
    _instrs.push_back(new Field(Operand(_instrs.back()->getResult()), 1));
    _instrs.push_back(new Unary(Unary::LOAD, Operand(_instrs.back()->getResult())));

    const ir::UnionConstructorDeclaration * pCons = _expr.getConstructor();
//    const ir::StructType & dataType = pCons->getStruct();

    if (pCons->getFields().size() > 1) {
        // Treat it as a struct.
        assert(false);
    } else {
        // Treat it as a plain value.
        const ir::NamedValue * pField = _expr.getField();
        Auto<Type> type = translate(* pField->getType());

        if (type->sizeOf() <= Type::sizeOf(Type::POINTER)) {
            _instrs.push_back(new Cast(Operand(_instrs.back()->getResult()), type));
        } else
            assert(false);
    }

    return Operand(_instrs.back()->getResult());
}

Operand Translator::translate(const ir::FunctionCall & _expr, Instructions & _instrs) {
    assert(_expr.getPredicate()->getType()->getKind() == ir::Type::PREDICATE);

    Operand function = translate(* _expr.getPredicate(), _instrs);
    Auto<FunctionType> type = translate(* (ir::PredicateType *) _expr.getPredicate()->getType());
    Call * pCall = new Call(function, type);

    for (size_t i = 0; i < _expr.getArgs().size(); ++ i)
        pCall->args().push_back(translate(* _expr.getArgs().get(i), _instrs));

    _instrs.push_back(pCall);

    return Operand(pCall->getResult());
}

Operand Translator::translateStringLiteral(const std::wstring & _str, Instructions & _instrs) {
    m_pModule->consts().push_back(new Constant(new Literal(/*Literal::String, new Type(Type::WChar)*/)));
    m_pModule->consts().back()->setType(new Type(Type::WCHAR));
    m_pModule->consts().back()->getLiteral()->setString(_str);

    Auto<Variable> var = m_pModule->consts().back();

    return var;
}

Operand Translator::translate(const ir::Literal & _expr, Instructions & _instrs) {
    if (_expr.getLiteralKind() == ir::Literal::STRING) {
        return translateStringLiteral(_expr.getString(), _instrs);
    } else {
        Literal lit(Literal::NUMBER, translate(* _expr.getType()));

        switch (_expr.getLiteralKind()) {
            case ir::Literal::NUMBER: lit.setNumber(_expr.getNumber()); break;
            case ir::Literal::BOOL:   lit.setBool(_expr.getBool()); break;
            case ir::Literal::CHAR:   lit.setChar(_expr.getChar()); break;
        }

        return Operand(lit);
    }
}

Operand Translator::translate(const ir::VariableReference & _expr, Instructions & _instrs) {
    bool bReference = false;
    Auto<Variable> pVar = resolveVariable(_expr.getTarget(), bReference);

    assert(! pVar.empty());

    if (bReference) {
        _instrs.push_back(new Unary(Unary::LOAD, Operand(pVar)));
        return Operand(_instrs.back()->getResult());
    }

    return Operand(pVar);
}

Operand Translator::translate(const ir::PredicateReference & _expr, Instructions & _instrs) {
    bool bReference = false;
    Auto<Variable> pVar = resolveVariable(_expr.getTarget(), bReference);

    assert(! pVar.empty());

    if (bReference) {
        _instrs.push_back(new Unary(Unary::LOAD, Operand(pVar)));
        return Operand(_instrs.back()->getResult());
    }

    return Operand(pVar);
}

static
bool _isSimpleExpr(const ir::Expression & _expr) {
    switch (_expr.getKind()) {
        case ir::Expression::LITERAL:
        case ir::Expression::VAR:
            return true;
    }

    return false;
}

Operand Translator::translate(const ir::Ternary & _expr, Instructions & _instrs) {
    Operand cond = translate(* _expr.getIf(), _instrs);

    if (_isSimpleExpr(* _expr.getThen()) && _isSimpleExpr(* _expr.getElse())) {
        Operand op1 = translate(* _expr.getThen(), _instrs);
        Operand op2 = translate(* _expr.getElse(), _instrs);

        _instrs.push_back(new Select(cond, op1, op2));

        return Operand(_instrs.back()->getResult());
    }

    If * pIf = new If(cond);
    Auto<Type> type = translate(* _expr.getThen()->getType());
    Auto<Variable> var = new Variable(type);

    _instrs.push_back(pIf);
    m_pFunction->locals().push_back(var);
    Operand op1 = translate(* _expr.getThen(), pIf->brTrue());
    Operand op2 = translate(* _expr.getElse(), pIf->brFalse());
    pIf->brTrue().push_back(new Binary(Binary::SET, Operand(var), op1));
    pIf->brFalse().push_back(new Binary(Binary::SET, Operand(var), op2));

    return Operand(var);
}

void Translator::initStruct(const ir::StructConstructor & _expr, Instructions & _instrs,
        const ir::NamedValues &_fields, const Operand & _ptr)
{
    for (size_t i = 0; i < _expr.size(); ++ i) {
        ir::StructFieldDefinition * pFieldDef = _expr.get(i);
        ir::Type * pFieldType = (ir::Type *) ir::resolveBaseType(_fields.get(i)->getType());

        // FIXME: assumes constant order of constructor values.

        // Should be done by middle end.
        if (! pFieldDef->getValue()->getType())
            pFieldDef->getValue()->setType(pFieldType, false);

        Operand rhs = translate(* _expr.get(i)->getValue(), _instrs);

        _instrs.push_back(new Field(_ptr, i));
        _instrs.push_back(new Binary(Binary::STORE,
                Operand(_instrs.back()->getResult()),
                rhs));
    }
}

Operand Translator::translate(const ir::StructConstructor & _expr, Instructions & _instrs) {
    assert(_expr.getType());
    assert(_expr.getType()->getKind() == ir::Type::STRUCT);

    Auto<Type> type = translate(* _expr.getType());
    Auto<Variable> var = new Variable(type);
    Auto<Unary> ptr = new Unary(Unary::PTR, Operand(var));

    m_pFunction->locals().push_back(var);
    _instrs.push_back(ptr);

    const ir::StructType *pStruct = (const ir::StructType *)_expr.getType();

    if (!pStruct->getNamesOrd().empty())
        initStruct(_expr, _instrs, pStruct->getNamesOrd(), Operand(ptr->getResult()));
    else if (!pStruct->getTypesOrd().empty())
        initStruct(_expr, _instrs, pStruct->getTypesOrd(), Operand(ptr->getResult()));

    return Operand(var);
}

Operand Translator::translate(const ir::UnionConstructor & _expr, Instructions & _instrs) {
    assert(_expr.getType());
    assert(_expr.isComplete());

    ir::UnionConstructorDeclaration * pProto = _expr.getPrototype();
    Auto<Type> type = translate(* _expr.getType());
    Auto<Variable> var = new Variable(type);
    Auto<Unary> ptr = new Unary(Unary::PTR, Operand(var));

    m_pFunction->locals().push_back(var);
    _instrs.push_back(ptr);
    _instrs.push_back(new Field(Operand(ptr->getResult()), 0));
    _instrs.push_back(new Binary(Binary::STORE,
            Operand(_instrs.back()->getResult()),
            Operand(Literal(new Type(Type::UINT32), Number::makeInt(pProto->getOrdinal())))));

//    const ir::StructType & dataType = pProto->getStruct();

    if (pProto->getFields().size() > 1) {
        Auto<StructType> st = translate(*pProto);

        _instrs.push_back(new Unary(Unary::MALLOC,
                Operand(Literal(new Type(Type::UINT64), Number::makeInt(st->sizeOf())))));
        _instrs.push_back(new Cast(_instrs.back()->getResult(), new PointerType(st)));

        Operand opBuf(_instrs.back()->getResult());

        _instrs.push_back(new Field(Operand(ptr->getResult()), 1));
        _instrs.push_back(new Cast(Operand(_instrs.back()->getResult()), new PointerType(new PointerType(st))));
        _instrs.push_back(new Binary(Binary::STORE,
                Operand(_instrs.back()->getResult()), opBuf));

        initStruct(_expr, _instrs, pProto->getFields(), opBuf);
    } else if (!pProto->getFields().empty()) {
        ir::StructFieldDefinition * pFieldDef = _expr.get(0);
        ir::Type * pFieldType = (ir::Type *) ir::resolveBaseType(pProto->getFields().get(0)->getType());
        Auto<Type> ft = translate(* pFieldType);

        // Should be done by middle end.
        if (! pFieldDef->getValue()->getType())
            pFieldDef->getValue()->setType(pFieldType, false);

        Operand rhs = translate(* _expr.get(0)->getValue(), _instrs);

        if (ft->sizeOf() > Type::sizeOf(Type::POINTER)) {
            _instrs.push_back(new Unary(Unary::MALLOC,
                    Operand(Literal(new Type(Type::UINT64), Number::makeInt(ft->sizeOf())))));
            _instrs.push_back(new Cast(Operand(ptr->getResult()), new PointerType(ft)));

            Operand opBuf(_instrs.back()->getResult());

            _instrs.push_back(new Binary(Binary::STORE, opBuf, rhs));
            rhs = opBuf;
        }

        _instrs.push_back(new Field(Operand(ptr->getResult()), 1));
        _instrs.push_back(new Cast(Operand(_instrs.back()->getResult()), new PointerType(ft)));
        _instrs.push_back(new Binary(Binary::STORE,
                Operand(_instrs.back()->getResult()), rhs));
    }

    return Operand(var);
}


Operand Translator::translate(const ir::Constructor & _expr, Instructions & _instrs) {
    switch (_expr.getConstructorKind()) {
        case ir::Constructor::STRUCT_FIELDS:
            return translate((ir::StructConstructor &) _expr, _instrs);
        case ir::Constructor::UNION_CONSTRUCTOR:
            return translate((ir::UnionConstructor &) _expr, _instrs);
    }

    assert(false);
}

Operand Translator::translate(const ir::Expression & _expr, Instructions & _instrs) {
    switch (_expr.getKind()) {
        case ir::Expression::LITERAL:
            return translate((ir::Literal &) _expr, _instrs);
        case ir::Expression::VAR:
            return translate((ir::VariableReference &) _expr, _instrs);
        case ir::Expression::PREDICATE:
            return translate((ir::PredicateReference &) _expr, _instrs);
        case ir::Expression::BINARY:
            return translate((ir::Binary &) _expr, _instrs);
        case ir::Expression::FUNCTION_CALL:
            return translate((ir::FunctionCall &) _expr, _instrs);
        case ir::Expression::TERNARY:
            return translate((ir::Ternary &) _expr, _instrs);
        case ir::Expression::COMPONENT:
            return translate((ir::Component &) _expr, _instrs);
        case ir::Expression::CONSTRUCTOR:
            return translate((ir::Constructor &) _expr, _instrs);
    }

    assert(false);
    return Operand();
}

void Translator::translate(const ir::If & _stmt, Instructions & _instrs) {
    Operand cond = translate(* _stmt.getArg(), _instrs);
    If * pIf = new If(cond);

    _instrs.push_back(pIf);
    translate(* _stmt.getBody(), pIf->brTrue());
    if (_stmt.getElse())
        translate(* _stmt.getElse(), pIf->brFalse());
}

void Translator::translateAsssignment(const ir::NamedValue * _pLHS,
        const ir::Expression * _pExpr, Instructions & _instrs)
{
    bool bReference = false;
    Auto<Variable> pVar = resolveVariable(_pLHS, bReference);

    assert(! pVar.empty());

    Operand left = Operand(pVar); //translate(* _stmt.getLValue(), _instrs);
    Operand right = translate(* _pExpr, _instrs);

    if (_pExpr->getType()->getKind() == ir::Type::STRING) {
        // Copy string.
        _instrs.push_back(new Cast(right,
                new PointerType(new Type(Type::UINT32))));
        _instrs.push_back(new Binary(Binary::OFFSET,
                Operand(_instrs.back()->getResult()),
                Operand(Literal(new Type(Type::INT32), Number::makeInt(-1)))));

        Operand opSrc (_instrs.back()->getResult());

        _instrs.push_back(new Unary(Unary::LOAD,
                Operand(_instrs.back()->getResult())));
        _instrs.push_back(new Binary(Binary::MUL,
                Operand(_instrs.back()->getResult()),
                Literal(new Type(Type::UINT32), Number(
                        Number::makeInt(Type::sizeOf(Type::WCHAR))))));
        _instrs.push_back(new Binary(Binary::ADD,
                Operand(_instrs.back()->getResult()),
                Literal(new Type(Type::UINT32), Number::makeInt(
                        Type::sizeOf(Type::WCHAR) + Type::sizeOf(Type::UINT32)))));

        Operand opSz (_instrs.back()->getResult());

        _instrs.push_back(new Unary(Unary::MALLOC, opSz));

        Operand opBuf (_instrs.back()->getResult());

        _instrs.push_back(new Copy(opBuf, opSrc, opSz));
        _instrs.push_back(new Cast(opBuf,
                new PointerType(new Type(Type::UINT32))));
        _instrs.push_back(new Binary(Binary::OFFSET,
                Operand(_instrs.back()->getResult()),
                Literal(new Type(Type::INT32), Number::makeInt(1))));
        _instrs.push_back(new Cast(Operand(_instrs.back()->getResult()),
                new PointerType(new Type(Type::WCHAR))));

        right = Operand(_instrs.back()->getResult());
    }

    if (bReference)
        _instrs.push_back(new Binary(Binary::STORE, left, right));
    else
        _instrs.push_back(new Binary(Binary::SET, left, right));
}

void Translator::translate(const ir::Assignment & _stmt, Instructions & _instrs) {
    assert(_stmt.getLValue()->getKind() == ir::Expression::VAR);

    const ir::VariableReference * pLValue = (const ir::VariableReference *) _stmt.getLValue();

    translateAsssignment(pLValue->getTarget(), _stmt.getExpression(), _instrs);
}

void Translator::translate(const ir::Jump & _stmt, Instructions & _instrs) {
    Literal num(Literal::NUMBER, new Type(Type::INT32));
    const int nLabel = resolveLabel(_stmt.getDestination());
    num.setNumber(Number::makeInt(nLabel));
    _instrs.push_back(new Unary(Unary::RETURN, Operand(num)));
}

bool Translator::translateBuiltin(const ir::Call & _stmt, Instructions & _instrs) {
    ir::Expression * pExpr = _stmt.getPredicate();

    if (pExpr->getKind() != ir::Expression::PREDICATE)
        return false;

    ir::PredicateReference * pPredRef = (ir::PredicateReference *) pExpr;

    if (! pPredRef->getTarget()->isBuiltin())
        return false;

    const std::wstring & name = pPredRef->getTarget()->getName();

    if (name == L"print") {
        translatePrint(_stmt, _instrs);
        return true;
    }

    return false;
}

void Translator::translatePrintExpr(const ir::Expression & _expr, Instructions & _instrs) {
    std::wstring strFunc;
    Auto<Type> type = translate(* _expr.getType());

    switch (type->getKind()) {
        case llir::Type::BOOL:    strFunc = L"__p_printBool"; break;
        case llir::Type::WCHAR:   strFunc = L"__p_printWChar"; break;
        case llir::Type::INT8:    strFunc = L"__p_printInt8"; break;
        case llir::Type::INT16:   strFunc = L"__p_printInt16"; break;
        case llir::Type::INT32:   strFunc = L"__p_printInt32"; break;
        case llir::Type::INT64:   strFunc = L"__p_printInt64"; break;
        case llir::Type::UINT8:   strFunc = L"__p_printUInt8"; break;
        case llir::Type::UINT16:  strFunc = L"__p_printUInt16"; break;
        case llir::Type::UINT32:  strFunc = L"__p_printUInt32"; break;
        case llir::Type::UINT64:  strFunc = L"__p_printUInt64"; break;
        /*case llir::Type::Gmp_z:   strFunc = m_os << "gmpz";
        case llir::Type::Gmp_q:   strFunc = m_os << "gmpq";
        case llir::Type::String:  strFunc = m_os << "string";
        case llir::Type::WString: strFunc = m_os << "wstring";
        case llir::Type::Float:   strFunc = m_os << "float";
        case llir::Type::Double:  strFunc = m_os << "double";
        case llir::Type::Quad:    strFunc = m_os << "quad";*/

        /*case llir::Type::Struct:
            return generate(_os, (llir::StructType &) _type);*/

        case llir::Type::POINTER:
            if (((PointerType &) * type).getBase()->getKind() == Type::WCHAR) {
                strFunc = L"__p_printWString";
                break;
            }
        default:
            strFunc = L"__p_printPtr";
    }

    Auto<Function> pFunc = resolveBuiltin(strFunc);
    Auto<FunctionType> pFuncType;

    if (pFunc.empty()) {
        pFuncType = new FunctionType(new Type(Type::VOID));
        pFuncType->argTypes().push_back(type);
        pFunc = new Function(strFunc, new Type(Type::VOID));
        pFunc->setType(pFuncType);
        addBuiltin(strFunc, pFunc);
        m_pModule->usedBuiltins().push_back(pFunc);
    } else
        pFuncType = pFunc->getType();

    Auto<Variable> pFuncVar = pFunc;
    Auto<Call> pCall = new Call(Operand(pFuncVar), pFuncType);

    pCall->args().push_back(translate(_expr, _instrs));
    _instrs.push_back(pCall);
}

void Translator::translatePrint(const ir::Call & _stmt, Instructions & _instrs) {
    for (size_t i = 0; i < _stmt.getArgs().size(); ++ i) {
        const ir::Expression * pParam = _stmt.getArgs().get(i);
        translatePrintExpr(* pParam, _instrs);
    }
}

void Translator::translate(const ir::Call & _stmt, Instructions & _instrs) {
    assert(_stmt.getPredicate()->getType()->getKind() == ir::Type::PREDICATE);

    for (size_t i = 0; i < _stmt.getDeclarations().size(); ++ i)
        translate(* _stmt.getDeclarations().get(i), _instrs);

    if (translateBuiltin(_stmt, _instrs))
        return;

    Operand function = translate(* _stmt.getPredicate(), _instrs);
    ir::PredicateType & predType = * (ir::PredicateType *) _stmt.getPredicate()->getType();
    Auto<FunctionType> type = translate(predType);
    Call * pCall = new Call(function, type);
    Types::const_iterator iType = type->argTypes().begin();
    int cBranch = -1;
    size_t cParam = 0;

    for (size_t i = 0; i < type->argTypes().size(); ++ i, ++ iType) {
        if (i < predType.getInParams().size()) {
            pCall->args().push_back(translate(* _stmt.getArgs().get(i), _instrs));
        } else {
            while (cBranch < 0 || cParam >= _stmt.getBranches().get(cBranch)->size()) {
                ++ cBranch;
                cParam = 0;
                assert(cBranch < (int) _stmt.getBranches().size());
            }

            Operand op = translate(* _stmt.getBranches().get(cBranch)->get(cParam), _instrs);
            _instrs.push_back(new Unary(Unary::PTR, op));
            pCall->args().push_back(Operand(_instrs.back()->getResult()));
        }
    }

    _instrs.push_back(pCall);

    if (_stmt.getBranches().size() > 1) {
        Switch * pSwitch = new Switch(Operand(pCall->getResult()));

        for (size_t i = 0; i < _stmt.getBranches().size(); ++ i) {
            ir::Statement * pHandler = _stmt.getBranches().get(i)->getHandler();
            if (pHandler) {
                pSwitch->cases().push_back(SwitchCase());
                SwitchCase & switchCase = pSwitch->cases().back();
                switchCase.values.push_back(i);
                translate(* pHandler, switchCase.body);
            }
        }

        _instrs.push_back(pSwitch);
    }
}

void Translator::translate(const ir::Block & _stmt, Instructions & _instrs) {
    Translator * pTranslator = addChild();
    for (size_t i = 0; i < _stmt.size(); ++ i)
        pTranslator->translate(* _stmt.get(i), _instrs);
}

void Translator::translate(const ir::VariableDeclaration & _stmt, Instructions & _instrs) {
    Auto<Type> type = translate(* _stmt.getVariable()->getType());
    m_pFunction->locals().push_back(new Variable(type));
    Auto<Variable> var = m_pFunction->locals().back();
    addVariable(_stmt.getVariable(), var);

    if (_stmt.getValue())
        translateAsssignment(_stmt.getVariable(), _stmt.getValue(), _instrs);
}

Operand Translator::translateSwitchCond(const ir::Expression & _expr, const Operand & _arg,
        Instructions & _instrs)
{
    if (_expr.getKind() == ir::Expression::TYPE) {
        const ir::TypeExpr & te = (const ir::TypeExpr &) _expr;
        assert(te.getContents()->getKind() == ir::Type::RANGE);
        const ir::Range & range = * (ir::Range *) te.getContents();

        Operand opMin = translate(* range.getMin(), _instrs);
        Operand opMax = translate(* range.getMax(), _instrs);
        Binary * pGte = new Binary(Binary::GTE, _arg, opMin);
        Binary * pLte = new Binary(Binary::LTE, _arg, opMax);
        _instrs.push_back(pGte);
        _instrs.push_back(pLte);
        _instrs.push_back(new Binary(Binary::BAND,
                Operand(pGte->getResult()),
                Operand(pLte->getResult())));
        return Operand(_instrs.back()->getResult());
    } else {
        Operand rhs = translate(_expr, _instrs);
        _instrs.push_back(new Binary(Binary::EQ, _arg, rhs));
        return Operand(_instrs.back()->getResult());
    }
}

void Translator::translateSwitchInt(const ir::Switch & _stmt,
        const Operand & _arg, Instructions & _instrs)
{
    assert(_arg.getType()->getKind() & Type::INTMASK);

    Auto<Switch> pSwitch = new Switch(_arg);

    std::vector<SwitchCase *> cases(_stmt.size());
    If * pPrevIf = NULL, * pFirstIf = NULL;
    Instructions prefix;

    for (size_t i = 0; i < _stmt.size(); ++ i) {
        const ir::Collection<ir::Expression> & exprs = _stmt.get(i)->getExpressions();
        SwitchCase * pCase = NULL;
        If * pIf = NULL;
        size_t cComplexConditions = 0;

        for (size_t j = 0; j < exprs.size(); ++ j) {
            const ir::Expression & expr = * exprs.get(j);

            if (expr.getKind() != ir::Expression::LITERAL) {
                ++ cComplexConditions;
                continue;
            }

            if (! pCase) {
                pSwitch->cases().push_back(SwitchCase());
                pCase = & pSwitch->cases().back();
            }

            pCase->values.push_back(((const ir::Literal &) expr).getNumber().getInt());
        }

        for (size_t j = 0; cComplexConditions > 0 && j < exprs.size(); ++ j) {
            const ir::Expression & expr = * exprs.get(j);

            if (expr.getKind() == ir::Expression::LITERAL)
                continue;

            -- cComplexConditions;

            if (pCase) {
                if (pCase->body.empty())
                    pCase->body.push_back(new Instruction());

                if (pCase->body.front()->getLabel().empty())
                    pCase->body.front()->setLabel(new Label());

                Operand cond = translateSwitchCond(expr, _arg, _instrs);

                _instrs.push_back(new Binary(Binary::JNZ,
                        cond, Operand(pCase->body.front()->getLabel())));
            } else {
                Instructions & instrs = pPrevIf ? pPrevIf->brFalse() : prefix;
                Operand cond = translateSwitchCond(expr, _arg, instrs);

                if (pIf) {
                    instrs.push_back(new Binary(Binary::BOR,
                            cond, pIf->getCondition()));
                    pIf->setCondition(Operand(instrs.back()->getResult()));
                } else {
                    pIf = new If(Operand(instrs.back()->getResult()));
                    translate(* _stmt.get(i)->getBody(), pIf->brTrue());
                }
            }
        }

        if (pIf) {
            if (! pFirstIf)
                pFirstIf = pIf;
            (pPrevIf ? pPrevIf->brFalse() : prefix).push_back(pIf);
            pPrevIf = pIf;
        }

        cases[i] = pCase;

        if (! pCase)
            continue;

        translate(* _stmt.get(i)->getBody(), pCase->body);
    }

    _instrs.insert(_instrs.end(), prefix.begin(), prefix.end());

    if (! pSwitch->cases().empty()) {
        if (_stmt.getDefault())
            translate(* _stmt.getDefault(), pSwitch->deflt());
        (pPrevIf ? pPrevIf->brFalse() : _instrs).push_back(pSwitch);
    } else if (_stmt.getDefault())
        translate(* _stmt.getDefault(), pPrevIf ? pPrevIf->brFalse() : _instrs);
}

void Translator::translateSwitchUnion(const ir::Switch & _stmt,
        const Operand & _arg, Instructions & _instrs)
{
    const ir::Type * pParamType = resolveBaseType(_stmt.getArg()->getType());

    assert(pParamType->getKind() == ir::Type::UNION);

    const ir::UnionType * pUnion = (const ir::UnionType *) pParamType;
    std::vector<SwitchCase *> cases;

    cases.assign(_stmt.size(), NULL);

    _instrs.push_back(new Unary(Unary::PTR, _arg));

    Operand opStructPtr = Operand(_instrs.back()->getResult());

    _instrs.push_back(new Field(opStructPtr, 0));
    _instrs.push_back(new Unary(Unary::LOAD, Operand(_instrs.back()->getResult())));

    Auto<Switch> pSwitch = new Switch(Operand(_instrs.back()->getResult()));

    _instrs.push_back(new Field(opStructPtr, 1));
    _instrs.push_back(new Unary(Unary::LOAD, Operand(_instrs.back()->getResult())));

    Operand opContents = Operand(_instrs.back()->getResult());

    std::vector<SwitchCase *> caseIdx(pUnion->getConstructors().size());
    std::vector<Auto<Label> > caseLabels(pUnion->getConstructors().size());
    std::vector<bool> isConstructor;

    // Prepare map.
    for (size_t i = 0; i < _stmt.size(); ++ i) {
        const ir::Collection<ir::Expression> & exprs = _stmt.get(i)->getExpressions();

        for (size_t j = 0; j < exprs.size(); ++ j) {
            const ir::Expression * pExpr = exprs.get(j);
            bool bIsConstructor = false;

            if (pExpr->getKind() == ir::Expression::CONSTRUCTOR &&
                    ((const ir::Constructor *) pExpr)->getConstructorKind() == ir::Constructor::UNION_CONSTRUCTOR)
            {
                const ir::UnionConstructor * pCons = (const ir::UnionConstructor *) pExpr;

                if (! pCons->isComplete() || pCons->size() == 0) {
                    const ir::UnionConstructorDeclaration * pProto = pCons->getPrototype();
                    const size_t cOrd = pProto->getOrdinal();

                    if (caseIdx[cOrd] == NULL) {
                        pSwitch->cases().push_back(SwitchCase());
                        pSwitch->cases().back().values.push_back(cOrd);
                        caseIdx[cOrd] = & pSwitch->cases().back();
                        caseLabels[cOrd] = new Label;
                    }

                    bIsConstructor = true;
                }
            }

            isConstructor.push_back(bIsConstructor);
        }
    }

    size_t cExprIdx = 0;
    Instructions * pInstrs = & _instrs;

    for (size_t i = 0; i < _stmt.size(); ++ i) {
        const ir::Collection<ir::Expression> & exprs = _stmt.get(i)->getExpressions();
        SwitchCase * pMainCase = NULL;
        Auto<Label> labelBody = new Label();

        for (size_t j = 0; j < exprs.size(); ++ j, ++ cExprIdx) {
            if (! isConstructor[cExprIdx])
                continue;

            // Got union constructor expression.
            const ir::UnionConstructor * pCons = (const ir::UnionConstructor *) exprs.get(j);;
            const ir::UnionConstructorDeclaration * pProto = pCons->getPrototype();
            SwitchCase * pCase = caseIdx[pCons->getPrototype()->getOrdinal()];

            assert(pCase != NULL);

            if (! pMainCase)
                pMainCase = pCase;

            for (size_t k = 0; k < pCons->getDeclarations().size(); ++ k) {
                const ir::VariableDeclaration * pDecl = pCons->getDeclarations().get(k);
                translate(* pDecl, pCase->body);
            }

            Auto<Label> labelNextCmp = new Label();

            Instructions & body = pCase->body;
            Operand opValue;
            bool bStruct = pProto->getFields().size() > 1;
            bool bPointer = (pProto->getFields().size() == 1);
            const bool bCompare = pCons->getDeclarations().size() < pCons->size();

            if (bPointer) {
                Auto<Type> fieldType = translate(* pProto->getFields().get(0)->getType());
                bPointer = (fieldType->sizeOf() > Type::sizeOf(Type::POINTER));
            }

            if (bCompare || ! pCons->getDeclarations().empty()) {
                if (bStruct) {
                    Auto<StructType> st = translate(*pProto);

                    body.push_back(new Cast(opContents, new PointerType(st)));
                    opValue = Operand(body.back()->getResult());
                } else {
                    Auto<Type> fieldType = translate(* pProto->getFields().get(0)->getType());

                    if (bPointer) {
                        body.push_back(new Cast(opContents, new PointerType(fieldType)));
                        body.push_back(new Unary(Unary::LOAD, body.back()->getResult()));
                    } else
                        body.push_back(new Cast(opContents, fieldType));
                }

                opValue = Operand(body.back()->getResult());
            }

            if (bCompare) {
                for (size_t k = 0; k < pCons->size(); ++ k) {
                    const ir::StructFieldDefinition * pDef = pCons->get(k);

                    if (pDef->getValue()) {
                        // Compare and jump.
                        Operand lhs;

                        if (bStruct) {
                            //body.push_back(new Unary(Unary::Ptr, _arg));
                            body.push_back(new Field(opValue, k));
                            lhs = Operand(body.back()->getResult());
                        } else {
                            assert(k == 0);
                            lhs = opValue;
                        }

                        body.push_back(new Unary(Unary::LOAD, lhs));
                        lhs = Operand(body.back()->getResult());

                        Operand rhs = translate(* pDef->getValue(), body);

                        rhs = translateEq(resolveBaseType(pDef->getValue()->getType()), lhs, rhs, body);
                        body.push_back(new Binary(Binary::JMZ,
                                rhs, Operand(labelNextCmp)));

                    }
                }
            }

            for (size_t k = 0; k < pCons->size(); ++ k) {
                const ir::StructFieldDefinition * pDef = pCons->get(k);

                if (! pDef->getValue()) {
                    // Initialize.
                    Operand opField;
                    Auto<Variable> pFieldVar;

                    if (bStruct) {
                        //body.push_back(new Unary(Unary::Ptr, _arg));
                        body.push_back(new Field(opValue, k));
                        pFieldVar = new Variable(body.back()->getResult()->getType());
                        body.push_back(new Binary(Binary::SET, pFieldVar, Operand(body.back()->getResult())));
                    } else {
                        assert(k == 0);
                        //opField = opValue;
                        pFieldVar = new Variable(opValue.getType());
                        body.push_back(new Binary(Binary::SET, pFieldVar, opValue));
                    }

                    addVariable(pDef->getField(), pFieldVar, bStruct);
                    m_pFunction->locals().push_back(pFieldVar);
                    opField = Operand(pFieldVar);
                    //pFieldVar->setTyoe(opField.getType());
                }
            }

            body.push_back(new Unary(Unary::JMP, Operand(labelBody)));

            if (bCompare) {
                body.push_back(new Instruction()); // nop
                body.back()->setLabel(labelNextCmp);
            }
        }

        cExprIdx -= exprs.size(); // Reset counter for second iteration.

        Operand lhs;

        for (size_t j = 0; j < exprs.size(); ++ j, ++ cExprIdx) {
            if (isConstructor[cExprIdx])
                continue;

            ir::Expression * pCond = exprs.get(j);

            Operand rhs = translate(* pCond, * pInstrs);

            rhs = translateEq(pParamType, _arg, rhs, * pInstrs);

            if (! lhs.empty()) {
                pInstrs->push_back(new Binary(Binary::BOR, lhs, rhs));
                lhs = Operand(pInstrs->back()->getResult());
            } else
                lhs = rhs;
        }

        if (! lhs.empty()) {
            if (pMainCase) {
                pInstrs->push_back(new Binary(Binary::JNZ, lhs, Operand(labelBody)));
            } else {
                If * pIf = new If(lhs);

                pInstrs->push_back(pIf);
                translate(* _stmt.get(i)->getBody(), pIf->brTrue());
                pInstrs = & pIf->brFalse();
            }
        }

        // Actual case body.
        if (pMainCase == NULL || _stmt.get(i)->getBody() == NULL)
            continue;

        Instructions & body = pMainCase->body;

        body.push_back(new Instruction()); // nop
        body.back()->setLabel(labelBody);
        translate(* _stmt.get(i)->getBody(), body);
    }

    if (_stmt.getDefault()) {
        if (pSwitch->cases().empty())
            translate(* _stmt.getDefault(), * pInstrs);
        else
            translate(* _stmt.getDefault(), pSwitch->deflt());
    }

    if (! pSwitch->cases().empty())
        pInstrs->push_back(pSwitch);
}

void Translator::translate(const ir::Switch & _stmt, Instructions & _instrs) {
    if (_stmt.getParamDecl())
        translate(* _stmt.getParamDecl(), _instrs);

    const ir::Type * pParamType = resolveBaseType(_stmt.getArg()->getType());
    Operand arg = translate(* _stmt.getArg(), _instrs);

    if (arg.getType()->getKind() & Type::INTMASK) {
        translateSwitchInt(_stmt, arg, _instrs);
        return;
    } else if (pParamType->getKind() == ir::Type::UNION) {
        translateSwitchUnion(_stmt, arg, _instrs);
        return;
    }

    // Encode as a series of if-s.

    Auto<Switch> pSwitch = new Switch(arg);
    Instructions * pInstrs = & _instrs;

    for (size_t i = 0; i < _stmt.size(); ++ i) {
        const ir::Collection<ir::Expression> & exprs = _stmt.get(i)->getExpressions();

        assert(! exprs.empty());

        Operand lhs = translate(* exprs.get(0), * pInstrs);
        lhs = translateEq(pParamType, arg, lhs, * pInstrs);

        for (size_t j = 1; j < exprs.size(); ++ j) {
            Operand rhs = translate(* exprs.get(j), * pInstrs);
            rhs = translateEq(pParamType, arg, rhs, * pInstrs);

            pInstrs->push_back(new Binary(Binary::BOR, lhs, rhs));
            lhs = Operand(pInstrs->back()->getResult());
        }

        If * pIf = new If(lhs);

        pInstrs->push_back(pIf);
        translate(* _stmt.get(i)->getBody(), pIf->brTrue());
        pInstrs = & pIf->brFalse();
    }

    if (_stmt.getDefault())
        translate(* _stmt.getDefault(), * pInstrs);
}

void Translator::translate(const ir::Statement & _stmt, Instructions & _instrs) {
    switch (_stmt.getKind()) {
        case ir::Statement::IF:
            translate((ir::If &) _stmt, _instrs); break;
        case ir::Statement::ASSIGNMENT:
            translate((ir::Assignment &) _stmt, _instrs); break;
        case ir::Statement::CALL:
            translate((ir::Call &) _stmt, _instrs); break;
        case ir::Statement::VARIABLE_DECLARATION:
            translate((ir::VariableDeclaration &) _stmt, _instrs); break;
        case ir::Statement::BLOCK:
            translate((ir::Block &) _stmt, _instrs); break;
        case ir::Statement::JUMP:
            translate((ir::Jump &) _stmt, _instrs); break;
        case ir::Statement::SWITCH:
            translate((ir::Switch &) _stmt, _instrs); break;
    }
}

Auto<Function> Translator::translate(const ir::Predicate & _pred) {
    Translator * pTranslator = addChild();
    const size_t cBranches = _pred.getOutParams().size();
    Auto<Type> returnType;
    ir::NamedValue * pResult = NULL;

    if (cBranches == 1 && _pred.getOutParams().get(0)->size() == 1) {
        // Trivial predicate: one branch, one output parameter.
        pResult = _pred.getOutParams().get(0)->get(0);
        returnType = pTranslator->translate(* pResult->getType());
    } else if (cBranches < 2) {
        // Pass output params as pointers.
        returnType = new Type(Type::VOID);
    } else {
        // Pass output params as pointers, return branch index.
        returnType = new Type(Type::INT32);
    }

    Auto<Function> pFunction = new Function(_pred.getName(), returnType);

    for (size_t i = 0; i < _pred.getInParams().size(); ++ i) {
        ir::NamedValue * pVar = _pred.getInParams().get(i);
        Auto<Type> type = pTranslator->translate(* pVar->getType());
        pFunction->args().push_back(new Variable(type));
        addVariable(pVar, pFunction->args().back());
    }

    if (pResult) {
        pTranslator->addVariable(pResult, pFunction->getResult());
    } else {
        for (size_t i = 0; i < _pred.getOutParams().size(); ++ i) {
            ir::Branch & branch = * _pred.getOutParams().get(i);
            m_labels[branch.getLabel()] = i;
            for (size_t j = 0; j < branch.size(); ++ j) {
                ir::NamedValue * pVar = branch.get(j);
                Auto<Type> argType = pTranslator->translate(* pVar->getType());
                pFunction->args().push_back(new Variable(new PointerType(argType)));
                addVariable(pVar, pFunction->args().back(), true);
            }
        }
    }

    addVariable(& _pred, pFunction);

    pTranslator->m_pFunction = pFunction;
    pTranslator->translate(* _pred.getBlock(), pFunction->instructions());

    if (returnType->getKind() != Type::VOID && pResult)
        pFunction->instructions().push_back(new Unary(Unary::RETURN, pFunction->getResult()));

    processLL<MarkEOLs>(* pFunction);
    processLL<CountLabels>(* pFunction);
    processLL<PruneJumps>(* pFunction);
    processLL<CollapseReturns>(* pFunction);
    processLL<RecycleVars>(* pFunction);

    return pFunction;
}

/*void Translator::insertRefs(Instructions & _instrs) {
    for (Instructions::iterator iInstr = _instrs.begin(); iInstr != _instrs.end(); ++ iInstr) {
        Instruction & instr = ** iInstr;
        Instructions::iterator iNext = iInstr;
        Operand op;
        ++ iNext;
        switch (instr.getKind()) {
            case Instruction::Unary:
                if (((Unary &) instr).getUnaryKind() == Unary::Ptr)
                    continue;
                break;
            case Instruction::Cast:
                if (((Cast &) instr).getOp().getKind() == Operand::Variable &&
                        ((Cast &) instr).getOp().getVariable()->getLastUse() == * iInstr)
                    continue;
                break;
            case Instruction::Field:
                if (((Field &) instr).getOp().getKind() == Operand::Variable &&
                        ((Field &) instr).getOp().getVariable()->getLastUse() == * iInstr)
                    continue;
                break;
            case Instruction::If:
                insertRefs(((If &) instr).brTrue());
                insertRefs(((If &) instr).brFalse());
                break;
            case Instruction::Switch: {
                Switch & sw = (Switch &) instr;
                for (switch_cases_t::iterator iCase = sw.cases().begin(); iCase != sw.cases().end(); ++ iCase)
                    insertRefs(iCase->body);
                insertRefs(sw.deflt());
                break;
            }
            case Instruction::Binary: {
                Binary & bin = (Binary &) instr;
                switch (((Binary &) instr).getBinaryKind()) {
                    case Binary::Set:
                        if (bin.getOp1().getType()->getKind() == Type::Pointer) {
                            _instrs.insert(iInstr, new Unary(Unary::Unref, bin.getOp1()));
                            op = bin.getOp1();
                        }
                        break;
                    case Binary::Store:
                        if (bin.getOp2().getType()->getKind() == Type::Pointer) {
                            _instrs.insert(iInstr, new Unary(Unary::UnrefNd, bin.getOp1()));
                            op = bin.getOp2();
                        }
                        break;
                    case Binary::Offset:
                        if (bin.getOp1().getKind() == Operand::Variable &&
                                bin.getOp1().getVariable()->getLastUse() == * iInstr)
                            continue;
                        break;
                }
                break;
            }
        }

        if (! instr.getResult().empty() && ! instr.getResult()->getType().empty()
                && instr.getResult()->getType()->getKind() == Type::Pointer)
        {
            m_ptrs.push_back(instr.getResult());
            iInstr = _instrs.insert(iNext, new Unary(Unary::Ref, Operand(instr.getResult())));
        }

        if (op.getKind() != Operand::Empty)
            iInstr = _instrs.insert(iNext, new Unary(Unary::Ref, op));
    }
}

void Translator::insertUnrefs() {
    for (Args::iterator iVar = m_pFunction->locals().begin(); iVar != m_pFunction->locals().end(); ++ iVar) {
        Auto<Variable> pVar = * iVar;
        const Type & type = * pVar->getType();

        switch (type.getKind()) {
            case Type::Pointer:
                m_pFunction->instructions().push_back(
                        new Unary(Unary::Unref, Operand(pVar)));
                break;
        }
    }

    for (Args::iterator iVar = m_ptrs.begin(); iVar != m_ptrs.end(); ++ iVar)
        m_pFunction->instructions().push_back(new Unary(Unary::Unref, Operand(* iVar)));
}
*/
void Translator::translate(const ir::Module & _module, Module & _dest) {
    m_pModule = & _dest;

    for (size_t i = 0; i < _module.getPredicates().size(); ++ i) {
        const ir::Predicate & pred = * _module.getPredicates().get(i);
        if (pred.getBlock() != NULL)
            m_pModule->functions().push_back(translate(pred));
    }
}

void translate(Module & _dest, const ir::Module & _from) {
    Translator translator(NULL);
    translator.translate(_from, _dest);
}

};
