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

class CTranslator {
public:
    CTranslator(CTranslator * _pParent) :
        m_pParent(_pParent),
        m_pModule(_pParent ? _pParent->m_pModule : NULL),
        m_pFunction(_pParent ? _pParent->m_pFunction : NULL)
     {}
    ~CTranslator();

    CTranslator * addChild();

    Auto<CVariable> resolveVariable(const void * _pVar, bool & _bReference);
    int resolveLabel(const ir::CLabel *);
    void addVariable(const void * _pOrig, Auto<CVariable> _pNew, bool _bReference = false);

    void addType(const ir::CType * _pIRType, const Auto<CType> & _pLLIRType);
    Auto<CType> resolveType(const ir::CType * _pType);

    void addBuiltin(const std::wstring & _strName, const Auto<CFunction> & _pFunc);
    Auto<CFunction> resolveBuiltin(const std::wstring & _strName);

    void addComparator(const ir::CType * _pType, const Auto<CFunction> & _pFunc);
    Auto<CFunction> resolveComparator(const ir::CType * _pType);

    void translate(const ir::CModule & _module, CModule & _dest);

    Auto<CType> translate(const ir::CType & _type);
    Auto<CFunctionType> translate(const ir::CPredicateType & _type);
    Auto<CType> translate(const ir::CNamedReferenceType & _type);
    Auto<CStructType> translate(const ir::CStructType & _type);
    Auto<CStructType> translate(const ir::CUnionType & _type);
    Auto<CFunction> translate(const ir::CPredicate & _pred);
    void translate(const ir::CStatement & _stmt, instructions_t & _instrs);
    void translate(const ir::CBlock & _stmt, instructions_t & _instrs);
    void translate(const ir::CIf & _stmt, instructions_t & _instrs);
    void translate(const ir::CAssignment & _stmt, instructions_t & _instrs);
    void translateAsssignment(const ir::CNamedValue * _pLHS, const ir::CExpression * _pExpr, instructions_t & _instrs);
    void translate(const ir::CCall & _stmt, instructions_t & _instrs);
    void translate(const ir::CVariableDeclaration & _stmt, instructions_t & _instrs);
    void translate(const ir::CJump & _stmt, instructions_t & _instrs);
    void translate(const ir::CSwitch & _stmt, instructions_t & _instrs);
    void translateSwitchInt(const ir::CSwitch & _stmt, const COperand & _arg,
            instructions_t & _instrs);
    void translateSwitchUnion(const ir::CSwitch & _stmt, const COperand & _arg,
            instructions_t & _instrs);
    COperand translate(const ir::CExpression & _expr, instructions_t & _instrs);
    COperand translate(const ir::CBinary & _expr, instructions_t & _instrs);
    COperand translate(const ir::CFunctionCall & _expr, instructions_t & _instrs);
    COperand translate(const ir::CVariableReference & _expr, instructions_t & _instrs);
    COperand translate(const ir::CPredicateReference & _expr, instructions_t & _instrs);
    COperand translate(const ir::CLiteral & _expr, instructions_t & _instrs);
    COperand translateStringLiteral(const std::wstring & _str, instructions_t & _instrs);
    COperand translate(const ir::CTernary & _expr, instructions_t & _instrs);
    COperand translate(const ir::CComponent & _expr, instructions_t & _instrs);
    COperand translate(const ir::CStructFieldExpr & _expr, instructions_t & _instrs);
    COperand translate(const ir::CUnionAlternativeExpr & _expr, instructions_t & _instrs);
    COperand translate(const ir::CConstructor & _expr, instructions_t & _instrs);
    COperand translateEq(const ir::CType * _pType, const COperand & _lhs, const COperand & _rhs, instructions_t & _instrs);
    COperand translateEqUnion(const ir::CUnionType * _pType, const COperand & _lhs, const COperand & _rhs, instructions_t & _instrs);
    COperand translateEqStruct(const ir::CStructType * _pType, const COperand & _lhs, const COperand & _rhs, instructions_t & _instrs);

    void initStruct(const ir::CStructConstructor & _expr, instructions_t & _instrs, const ir::CType * _pType, const COperand & _ptr);

    COperand translate(const ir::CStructConstructor & _expr, instructions_t & _instrs);
    COperand translate(const ir::CUnionConstructor & _expr, instructions_t & _instrs);
    COperand translateSwitchCond(const ir::CExpression & _expr, const COperand & _arg,
            instructions_t & _instrs);

    // Builtins.
    bool translateBuiltin(const ir::CCall & _stmt, instructions_t & _instrs);
    void translatePrint(const ir::CCall & _stmt, instructions_t & _instrs);
    void translatePrintExpr(const ir::CExpression & _expr, instructions_t & _instrs);

    //void insertRefs(instructions_t & _instrs);
    //void insertUnrefs();

private:
    typedef std::map<const void *, std::pair<Auto<CVariable>, bool> > variable_map_t;
    typedef std::map<const ir::CLabel *, int> label_numbers_t;
    typedef std::map<const ir::CType *, Auto<CType> > type_map_t;
    typedef std::map<const ir::CType *, Auto<CFunction> > compare_map_t;
    typedef std::map<std::wstring, Auto<CFunction> > builtin_map_t;

    Auto<CStructType> m_unionType;
    CTranslator * m_pParent;
    CModule * m_pModule;
    Auto<CFunction> m_pFunction;
    std::list<CTranslator *> m_children;
    variable_map_t m_vars;
    label_numbers_t m_labels;
    type_map_t m_types;
    args_t m_ptrs;
    compare_map_t m_comparators;
    builtin_map_t m_builtins;
};

CTranslator::~CTranslator() {
    for (std::list<CTranslator *>::iterator iChild = m_children.begin(); iChild != m_children.end(); ++ iChild)
        delete * iChild;
}

void CTranslator::addVariable(const void * _pOrig, Auto<CVariable> _pNew, bool _bReference) {
    m_vars[_pOrig] = std::make_pair(_pNew, _bReference);
}

CTranslator * CTranslator::addChild() {
    m_children.push_back(new CTranslator(this));
    return m_children.back();
}

Auto<CVariable> CTranslator::resolveVariable(const void * _pVar, bool & _bReference) {
    variable_map_t::iterator iVar = m_vars.find(_pVar);

    if (iVar != m_vars.end()) {
        _bReference = iVar->second.second;
        return iVar->second.first;
    } else if (m_pParent)
        return m_pParent->resolveVariable(_pVar, _bReference);
    else
        return NULL;
}

void CTranslator::addType(const ir::CType * _pIRType, const Auto<CType> & _pLLIRType) {
    if (! m_pParent)
        m_types[_pIRType] = _pLLIRType;
    else
        m_pParent->addType(_pIRType, _pLLIRType);
}

Auto<CType> CTranslator::resolveType(const ir::CType * _pType) {
    if (m_pParent)
        return m_pParent->resolveType(_pType);

    type_map_t::iterator iType = m_types.find(_pType);

    return (iType == m_types.end()) ? NULL : iType->second;
}

void CTranslator::addBuiltin(const std::wstring & _strName, const Auto<CFunction> & _pFunc) {
    if (! m_pParent)
        m_builtins[_strName] = _pFunc;
    else
        m_pParent->addBuiltin(_strName, _pFunc);
}

Auto<CFunction> CTranslator::resolveBuiltin(const std::wstring & _strName) {
    if (m_pParent)
        return m_pParent->resolveBuiltin(_strName);

    builtin_map_t::iterator iFunc = m_builtins.find(_strName);

    return (iFunc == m_builtins.end()) ? NULL : iFunc->second;
}

void CTranslator::addComparator(const ir::CType * _pType, const Auto<CFunction> & _pFunc) {
    if (! m_pParent)
        m_comparators[_pType] = _pFunc;
    else
        m_pParent->addComparator(_pType, _pFunc);
}

Auto<CFunction> CTranslator::resolveComparator(const ir::CType * _pType) {
    if (m_pParent)
        return m_pParent->resolveComparator(_pType);

    compare_map_t::iterator iFunc = m_comparators.find(_pType);

    return (iFunc == m_comparators.end()) ? NULL : iFunc->second;
}

int CTranslator::resolveLabel(const ir::CLabel * _pLabel) {
    label_numbers_t::iterator iLabel = m_labels.find(_pLabel);

    if (iLabel != m_labels.end())
        return iLabel->second;
    else if (m_pParent)
        return m_pParent->resolveLabel(_pLabel);
    else
        return -1;
}

Auto<CFunctionType> CTranslator::translate(const ir::CPredicateType & _type) {
    const size_t cBranches = _type.getOutParams().size();
    Auto<CType> returnType;
    ir::CNamedValue * pResult = NULL;

    if (cBranches == 1 && _type.getOutParams().get(0)->size() == 1) {
        // Trivial predicate: one branch, one output parameter.
        pResult = _type.getOutParams().get(0)->get(0);
        returnType = translate(* pResult->getType());
    } else if (cBranches < 2) {
        // Pass output params as pointers.
        returnType = new CType(CType::Void);
    } else {
        // Pass output params as pointers, return branch index.
        returnType = new CType(CType::Int32);
    }

    Auto<CFunctionType> pFunc = new CFunctionType(returnType);

    for (size_t i = 0; i < _type.getInParams().size(); ++ i) {
        ir::CNamedValue * pVar = _type.getInParams().get(i);
        pFunc->argTypes().push_back(translate(* pVar->getType()));
    }

    if (! pResult) {
        for (size_t i = 0; i < _type.getOutParams().size(); ++ i) {
            ir::CBranch & branch = * _type.getOutParams().get(i);
            for (size_t j = 0; j < branch.size(); ++ j) {
                ir::CNamedValue * pVar = branch.get(j);
                pFunc->argTypes().push_back(new CPointerType(translate(* pVar->getType())));
            }
        }
    }

    return pFunc;
}

Auto<CType> CTranslator::translate(const ir::CNamedReferenceType & _type) {
    if (_type.getDeclaration()) {
        return translate(* _type.getDeclaration()->getType());
    }
    assert(false);
    return NULL;
}

Auto<CStructType> CTranslator::translate(const ir::CStructType & _type) {
    Auto<CStructType> structType = Auto<CStructType> (resolveType(& _type));

    if (! structType.empty())
        return structType;

    structType = new CStructType();

    for (size_t i = 0; i < _type.getFields().size(); ++ i) {
        structType->fieldTypes().push_back(translate(* _type.getFields().get(i)->getType()));
    }

    m_pModule->types().push_back(structType);
    addType(& _type, structType);

    return structType;
}

Auto<CStructType> CTranslator::translate(const ir::CUnionType & _type) {
    if (m_pParent)
        return m_pParent->translate(_type);

    if (m_unionType.empty()) {
        m_unionType = new CStructType();
        m_unionType->fieldTypes().push_back(new CType(CType::UInt32));
        m_unionType->fieldTypes().push_back(new CPointerType(new CType(CType::Void)));
        m_pModule->types().push_back(m_unionType);
    }

    addType(& _type, m_unionType);

    return m_unionType;
}

Auto<CType> CTranslator::translate(const ir::CType & _type) {
    const int nBits = _type.getBits();

    switch (_type.getKind()) {
        case ir::CType::Int:
            switch (nBits) {
                case CNumber::Generic: return new CType(CType::Gmp_z);
                case CNumber::Native: return new CType(CType::Int32);
                default:
                    if (nBits <= 8) return new CType(CType::Int8);
                    if (nBits <= 16) return new CType(CType::Int16);
                    if (nBits <= 32) return new CType(CType::Int32);
                    if (nBits <= 64) return new CType(CType::Int64);
                    return new CType(CType::Gmp_z);
            }
        case ir::CType::Nat:
            switch (nBits) {
                case CNumber::Generic: return new CType(CType::Gmp_z);
                case CNumber::Native: return new CType(CType::UInt32);
                default:
                    if (nBits <= 8) return new CType(CType::UInt8);
                    if (nBits <= 16) return new CType(CType::UInt16);
                    if (nBits <= 32) return new CType(CType::UInt32);
                    if (nBits <= 64) return new CType(CType::UInt64);
                    return new CType(CType::Gmp_z);
            }
        case ir::CType::Real:
            switch (nBits) {
                case CNumber::Native: return new CType(CType::Double);
                case CNumber::Generic: return new CType(CType::Gmp_q);
                default:
                    if (nBits <= (int) sizeof(float)) return new CType(CType::Float);
                    if (nBits <= (int) sizeof(double)) return new CType(CType::Double);
                    if (nBits <= (int) sizeof(long double)) return new CType(CType::Quad);
                    return new CType(CType::Gmp_q);
            }
        case ir::CType::Bool: return new CType(CType::Bool);
        case ir::CType::Char: return new CType(CType::WChar);
        case ir::CType::String: return new CPointerType(new CType(CType::WChar));
        case ir::CType::Predicate: return translate((ir::CPredicateType &) _type);
        case ir::CType::NamedReference: return translate((ir::CNamedReferenceType &) _type);
        case ir::CType::Struct: return translate((ir::CStructType &) _type);
        case ir::CType::Union: return translate((ir::CUnionType &) _type);

        default: return new CType();
    }
}

static int _selectInstr(int _opKind, int _instrInt, int _instrFloat, int _instrZ, int _instrQ) {
    int nInstr = -1;
    if (_opKind & CType::IntMask) nInstr =_instrInt;
    else if (_opKind & CType::FloatMask) nInstr =_instrFloat;
    else if (_opKind == CType::Gmp_z) nInstr =_instrZ;
    else if (_opKind == CType::Gmp_q) nInstr =_instrQ;
    //assert(nInstr >= 0);
    return nInstr;
}

static
int _selectBinaryInstr(int _op, const Auto<llir::CType> _pType) {
    switch (_op) {
        case ir::CBinary::Add:
            return _selectInstr(_pType->getKind(), CBinary::Add, CBinary::FAdd, CBinary::ZAdd, CBinary::QAdd);
        case ir::CBinary::Subtract:
            return _selectInstr(_pType->getKind(), CBinary::Sub, CBinary::FSub, CBinary::ZSub, CBinary::ZAdd);
        case ir::CBinary::Multiply:
            return _selectInstr(_pType->getKind(), CBinary::Mul, CBinary::FMul, CBinary::ZMul, CBinary::QMul);
        case ir::CBinary::Divide:
            return _selectInstr(_pType->getKind(), CBinary::Div, CBinary::FDiv, CBinary::ZDiv, CBinary::QDiv);
        case ir::CBinary::Power:
            return _selectInstr(_pType->getKind(), CBinary::Pow, CBinary::FPow, CBinary::ZPow, CBinary::QPow);

        case ir::CBinary::Remainder:
            return _selectInstr(_pType->getKind(), CBinary::Rem, -1, CBinary::ZRem, -1);
        case ir::CBinary::ShiftLeft:
            return _selectInstr(_pType->getKind(), CBinary::Shl, -1, CBinary::ZShl, -1);
        case ir::CBinary::ShiftRight:
            return _selectInstr(_pType->getKind(), CBinary::Shr, -1, CBinary::ZShr, -1);

        case ir::CBinary::Equals:
            return _selectInstr(_pType->getKind(), CBinary::Eq, CBinary::FEq, CBinary::ZEq, CBinary::QEq);
        case ir::CBinary::NotEquals:
            return _selectInstr(_pType->getKind(), CBinary::Ne, CBinary::FNe, CBinary::ZNe, CBinary::QNe);
        case ir::CBinary::Less:
            return _selectInstr(_pType->getKind(), CBinary::Lt, CBinary::FLt, CBinary::ZLt, CBinary::QLt);
        case ir::CBinary::LessOrEquals:
            return _selectInstr(_pType->getKind(), CBinary::Lte, CBinary::FLte, CBinary::ZLte, CBinary::QLte);
        case ir::CBinary::Greater:
            return _selectInstr(_pType->getKind(), CBinary::Gt, CBinary::FGt, CBinary::ZGt, CBinary::QGt);
        case ir::CBinary::GreaterOrEquals:
            return _selectInstr(_pType->getKind(), CBinary::Gte, CBinary::FGte, CBinary::ZGte, CBinary::QGte);

        // TODO: use boolean instructions when type inference gets implemented.
        case ir::CBinary::BoolAnd:
        case ir::CBinary::BitwiseAnd:
            return _selectInstr(_pType->getKind(), CBinary::And, -1, CBinary::ZAnd, -1);
        case ir::CBinary::BoolOr:
        case ir::CBinary::BitwiseOr:
            return _selectInstr(_pType->getKind(), CBinary::Or, -1, CBinary::ZOr, -1);
        case ir::CBinary::BoolXor:
        case ir::CBinary::BitwiseXor:
            return _selectInstr(_pType->getKind(), CBinary::Xor, -1, CBinary::ZXor, -1);
    }

    return -1;
}

COperand CTranslator::translateEqUnion(const ir::CUnionType * _pType, const COperand & _lhs, const COperand & _rhs, instructions_t & _instrs) {
    Auto<CFunction> func = resolveComparator(_pType);
    Auto<CFunctionType> funcType;

    if (! func.empty()) {
        funcType = func->getType();
        Auto<CVariable> funcVar = func;
        Auto<CCall> call = new CCall(funcVar, funcType);

        call->args().push_back(_lhs);
        call->args().push_back(_rhs);
        _instrs.push_back(call);

        return COperand(call->getResult());
    }

    static int nCmpCount = 0;

    func = new CFunction(std::wstring(L"cmp_") + fmtInt(nCmpCount ++), new CType(CType::Bool));
    funcType = new CFunctionType(new CType(CType::Bool));
    funcType->argTypes().push_back(_lhs.getType());
    funcType->argTypes().push_back(_rhs.getType());
    func->args().push_back(new CVariable(_lhs.getType()));
    func->args().push_back(new CVariable(_rhs.getType()));
    func->setType(funcType);
    addComparator(_pType, func);
    m_pModule->functions().push_back(func);

    instructions_t & instrs = func->instructions();
    CTranslator trans(this);
    Auto<CLabel> labelEnd = new CLabel();

    trans.m_pFunction = func;

    instrs.push_back(new CUnary(CUnary::Ptr, COperand(func->args().front())));

    COperand lPtr = COperand(instrs.back()->getResult());

    instrs.push_back(new CUnary(CUnary::Ptr, COperand(func->args().back())));

    COperand rPtr = COperand(instrs.back()->getResult());

    instrs.push_back(new CField(lPtr, 0));
    instrs.push_back(new CUnary(CUnary::Load, COperand(instrs.back()->getResult())));

    COperand lTag = COperand(instrs.back()->getResult());

    instrs.push_back(new CField(rPtr, 0));
    instrs.push_back(new CUnary(CUnary::Load, COperand(instrs.back()->getResult())));

    COperand rTag = COperand(instrs.back()->getResult());

    instrs.push_back(new CBinary(CBinary::Eq, lTag, rTag));

    if (_pType->getConstructors().empty()) {
        instrs.push_back(new CUnary(CUnary::Return, instrs.back()->getResult()));
    } else {
        Auto<CVariable> varResult = func->getResult();

        instrs.push_back(new CBinary(CBinary::Set, varResult, instrs.back()->getResult()));
        instrs.push_back(new CBinary(CBinary::Jmz, varResult, COperand(labelEnd)));

        instrs.push_back(new CField(lPtr, 1));
        lPtr = COperand(instrs.back()->getResult());
        instrs.push_back(new CField(rPtr, 1));
        rPtr = COperand(instrs.back()->getResult());

        Auto<CSwitch> sw = new CSwitch(lTag);

        for (size_t i = 0; i < _pType->getConstructors().size(); ++ i) {
            sw->cases().push_back(switch_case_t());

            switch_case_t & swCase = sw->cases().back();
            instructions_t & body = swCase.body;

            const ir::CUnionConstructorDefinition * pCons = _pType->getConstructors().get(i);
            const ir::CStructType & dataType = pCons->getStruct();

            swCase.values.push_back(i);

            if (dataType.getFields().size() == 0) {
                body.push_back(new CBinary(CBinary::Set, varResult,
                        COperand(CLiteral(new CType(CType::Bool), CNumber((int64_t) 1)))));
                continue;
            }

            if (dataType.getFields().size() == 1) {
                const ir::CType * pFieldType = resolveBaseType(dataType.getFields().get(0)->getType());
                Auto<CType> fieldType = translate(* pFieldType);
                COperand l = lPtr, r = rPtr;

                if (fieldType->sizeOf() > CType::sizeOf(CType::Pointer)) {
                    body.push_back(new CUnary(CUnary::Load, lPtr));
                    l = COperand(body.back()->getResult());
                    body.push_back(new CUnary(CUnary::Load, rPtr));
                    r = COperand(body.back()->getResult());
                }

                body.push_back(new CCast(l, new CPointerType(fieldType)));
                body.push_back(new CUnary(CUnary::Load, COperand(body.back()->getResult())));
                l = COperand(body.back()->getResult());
                body.push_back(new CCast(r, new CPointerType(fieldType)));
                body.push_back(new CUnary(CUnary::Load, COperand(body.back()->getResult())));
                r = COperand(body.back()->getResult());

                COperand eq = trans.translateEq(pFieldType, l, r, body);

                body.push_back(new CBinary(CBinary::Set, varResult, eq));
                continue;
            }

            Auto<CStructType> st = translate(dataType);
            COperand l = lPtr, r = rPtr;

            body.push_back(new CUnary(CUnary::Load, lPtr));
            body.push_back(new CCast(COperand(body.back()->getResult()), new CPointerType(st)));
            body.push_back(new CUnary(CUnary::Load, COperand(body.back()->getResult())));
            l = COperand(body.back()->getResult());
            body.push_back(new CUnary(CUnary::Load, rPtr));
            body.push_back(new CCast(COperand(body.back()->getResult()), new CPointerType(st)));
            body.push_back(new CUnary(CUnary::Load, COperand(body.back()->getResult())));
            r = COperand(body.back()->getResult());

            COperand eq = trans.translateEqStruct(& dataType, l, r, body);

            body.push_back(new CBinary(CBinary::Set, varResult, eq));
        }

        instrs.push_back(sw);
        instrs.push_back(new CInstruction());
        instrs.back()->setLabel(labelEnd);
    }

    instrs.push_back(new CUnary(CUnary::Return, func->getResult()));

    processLL<CMarkEOLs>(* func);
    processLL<CCountLabels>(* func);
    processLL<CPruneJumps>(* func);
    processLL<CCollapseReturns>(* func);
    processLL<CRecycleVars>(* func);

    Auto<CVariable> funcVar = func;
    Auto<CCall> call = new CCall(funcVar, funcType);

    call->args().push_back(_lhs);
    call->args().push_back(_rhs);
    _instrs.push_back(call);

    return COperand(call->getResult());
}

COperand CTranslator::translateEqStruct(const ir::CStructType * _pType, const COperand & _lhs, const COperand & _rhs, instructions_t & _instrs) {
    if (_pType->getFields().empty())
        return COperand(CLiteral(new CType(CType::Bool), CNumber((int64_t) 1)));

    Auto<CLabel> labelEnd = new CLabel();
    Auto<CVariable> varResult = new CVariable(new CType(CType::Bool));

    addVariable(NULL, varResult, false);
    m_pFunction->locals().push_back(varResult);

    _instrs.push_back(new CUnary(CUnary::Ptr, _lhs));
    COperand lPtr = COperand(_instrs.back()->getResult());
    _instrs.push_back(new CUnary(CUnary::Ptr, _rhs));
    COperand rPtr = COperand(_instrs.back()->getResult());

    for (size_t i = 0; i < _pType->getFields().size(); ++ i) {
        const ir::CType * pFieldType = resolveBaseType(_pType->getFields().get(i)->getType());

        _instrs.push_back(new CField(lPtr, i));
        _instrs.push_back(new CUnary(CUnary::Load, COperand(_instrs.back()->getResult())));
        COperand lField = COperand(_instrs.back()->getResult());
        _instrs.push_back(new CField(rPtr, i));
        _instrs.push_back(new CUnary(CUnary::Load, COperand(_instrs.back()->getResult())));
        COperand rField = COperand(_instrs.back()->getResult());
        COperand cmp = translateEq(pFieldType, lField, rField, _instrs);

        _instrs.push_back(new CBinary(CBinary::Set, varResult, cmp));
        _instrs.push_back(new CBinary(CBinary::Jmz, cmp, COperand(labelEnd)));
    }

    _instrs.push_back(new CInstruction()); // nop
    _instrs.back()->setLabel(labelEnd);

    return COperand(varResult);
}

COperand CTranslator::translateEq(const ir::CType * _pType, const COperand & _lhs, const COperand & _rhs, instructions_t & _instrs) {
    std::wcerr << L"!! _pType->getKind() = " << _pType->getKind() << std::endl;
    switch (_pType->getKind()) {
        case ir::CType::Union:
            return translateEqUnion((const ir::CUnionType *) _pType, _lhs, _rhs, _instrs);
        case ir::CType::Struct:
            return translateEqStruct((const ir::CStructType *) _pType, _lhs, _rhs, _instrs);
    }

    const int nInstr = _selectInstr(_lhs.getType()->getKind(), CBinary::Eq, CBinary::FEq, CBinary::ZEq, CBinary::QEq);

    if (nInstr < 0) {
        assert(false && "Cannot compare values of this type");
    }

    _instrs.push_back(new CBinary(nInstr, _lhs, _rhs));

    return COperand(_instrs.back()->getResult());
}

COperand CTranslator::translate(const ir::CBinary & _expr, instructions_t & _instrs) {
    COperand left = translate(* _expr.getLeftSide(), _instrs);
    COperand right = translate(* _expr.getRightSide(), _instrs);

    if (_expr.getOperator() == ir::CBinary::Equals)
        return translateEq(resolveBaseType(_expr.getLeftSide()->getType()), left, right, _instrs);

    int nInstr = _selectBinaryInstr(_expr.getOperator(), left.getType());

    assert (nInstr >= 0);

    _instrs.push_back(new CBinary(nInstr, left, right));

    return COperand(_instrs.back()->getResult());
}

COperand CTranslator::translate(const ir::CComponent & _expr, instructions_t & _instrs) {
    switch (_expr.getComponentKind()) {
        case ir::CComponent::StructField:
            return translate((ir::CStructFieldExpr &) _expr, _instrs);
        case ir::CComponent::UnionAlternative:
            return translate((ir::CUnionAlternativeExpr &) _expr, _instrs);
    }

    assert(false);
}

COperand CTranslator::translate(const ir::CStructFieldExpr & _expr, instructions_t & _instrs) {
/*    const ir::CStructType * pStruct = _expr.getStructType();
    Auto<CStructType> st = translate(* pStruct);
    COperand object = translate(* _expr.getObject(), _instrs);

    _instrs.push_back(new CUnary(CUnary::Ptr, object));
    _instrs.push_back(new CField(COperand(_instrs.back()->getResult()), _expr.getFieldIdx()));
    _instrs.push_back(new CUnary(CUnary::Load, COperand(_instrs.back()->getResult())));

    return COperand(_instrs.back()->getResult());
*/
    return COperand();
}

COperand CTranslator::translate(const ir::CUnionAlternativeExpr & _expr, instructions_t & _instrs) {
    const ir::CUnionType * pUnion = _expr.getUnionType();
    Auto<CStructType> st = translate(* pUnion);
    COperand object = translate(* _expr.getObject(), _instrs);

    _instrs.push_back(new CUnary(CUnary::Ptr, object));
    _instrs.push_back(new CField(COperand(_instrs.back()->getResult()), 1));
    _instrs.push_back(new CUnary(CUnary::Load, COperand(_instrs.back()->getResult())));

    const ir::CUnionConstructorDefinition * pCons = _expr.getConstructor();
    const ir::CStructType & dataType = pCons->getStruct();

    if (dataType.getFields().size() > 1) {
        // Treat it as a struct.
        assert(false);
    } else {
        // Treat it as a plain value.
        const ir::CNamedValue * pField = _expr.getField();
        Auto<CType> type = translate(* pField->getType());

        if (type->sizeOf() <= CType::sizeOf(CType::Pointer)) {
            _instrs.push_back(new CCast(COperand(_instrs.back()->getResult()), type));
        } else
            assert(false);
    }

    return COperand(_instrs.back()->getResult());
}

COperand CTranslator::translate(const ir::CFunctionCall & _expr, instructions_t & _instrs) {
    assert(_expr.getPredicate()->getType()->getKind() == ir::CType::Predicate);

    COperand function = translate(* _expr.getPredicate(), _instrs);
    Auto<CFunctionType> type = translate(* (ir::CPredicateType *) _expr.getPredicate()->getType());
    CCall * pCall = new CCall(function, type);

    for (size_t i = 0; i < _expr.getParams().size(); ++ i)
        pCall->args().push_back(translate(* _expr.getParams().get(i), _instrs));

    _instrs.push_back(pCall);

    return COperand(pCall->getResult());
}

COperand CTranslator::translateStringLiteral(const std::wstring & _str, instructions_t & _instrs) {
    m_pModule->consts().push_back(new CConstant(new CLiteral(/*CLiteral::String, new CType(CType::WChar)*/)));
    m_pModule->consts().back()->setType(new CType(CType::WChar));
    m_pModule->consts().back()->getLiteral()->setString(_str);

    Auto<CVariable> var = m_pModule->consts().back();

    return var;
}

COperand CTranslator::translate(const ir::CLiteral & _expr, instructions_t & _instrs) {
    if (_expr.getLiteralKind() == ir::CLiteral::String) {
        return translateStringLiteral(_expr.getString(), _instrs);
    } else {
        CLiteral lit(CLiteral::Number, translate(* _expr.getType()));

        switch (_expr.getLiteralKind()) {
            case ir::CLiteral::Number: lit.setNumber(_expr.getNumber()); break;
            case ir::CLiteral::Bool:   lit.setBool(_expr.getBool()); break;
            case ir::CLiteral::Char:   lit.setChar(_expr.getChar()); break;
        }

        return COperand(lit);
    }
}

COperand CTranslator::translate(const ir::CVariableReference & _expr, instructions_t & _instrs) {
    bool bReference = false;
    Auto<CVariable> pVar = resolveVariable(_expr.getTarget(), bReference);

    assert(! pVar.empty());

    if (bReference) {
        _instrs.push_back(new CUnary(CUnary::Load, COperand(pVar)));
        return COperand(_instrs.back()->getResult());
    }

    return COperand(pVar);
}

COperand CTranslator::translate(const ir::CPredicateReference & _expr, instructions_t & _instrs) {
    bool bReference = false;
    Auto<CVariable> pVar = resolveVariable(_expr.getTarget(), bReference);

    assert(! pVar.empty());

    if (bReference) {
        _instrs.push_back(new CUnary(CUnary::Load, COperand(pVar)));
        return COperand(_instrs.back()->getResult());
    }

    return COperand(pVar);
}

static
bool _isSimpleExpr(const ir::CExpression & _expr) {
    switch (_expr.getKind()) {
        case ir::CExpression::Literal:
        case ir::CExpression::Var:
            return true;
    }

    return false;
}

COperand CTranslator::translate(const ir::CTernary & _expr, instructions_t & _instrs) {
    COperand cond = translate(* _expr.getIf(), _instrs);

    if (_isSimpleExpr(* _expr.getThen()) && _isSimpleExpr(* _expr.getElse())) {
        COperand op1 = translate(* _expr.getThen(), _instrs);
        COperand op2 = translate(* _expr.getElse(), _instrs);

        _instrs.push_back(new CSelect(cond, op1, op2));

        return COperand(_instrs.back()->getResult());
    }

    CIf * pIf = new CIf(cond);
    Auto<CType> type = translate(* _expr.getThen()->getType());
    Auto<CVariable> var = new CVariable(type);

    _instrs.push_back(pIf);
    m_pFunction->locals().push_back(var);
    COperand op1 = translate(* _expr.getThen(), pIf->brTrue());
    COperand op2 = translate(* _expr.getElse(), pIf->brFalse());
    pIf->brTrue().push_back(new CBinary(CBinary::Set, COperand(var), op1));
    pIf->brFalse().push_back(new CBinary(CBinary::Set, COperand(var), op2));

    return COperand(var);
}

void CTranslator::initStruct(const ir::CStructConstructor & _expr, instructions_t & _instrs, const ir::CType * _pType, const COperand & _ptr) {
    const ir::CStructType * pStructType = (const ir::CStructType *) _pType;

    for (size_t i = 0; i < _expr.size(); ++ i) {
        ir::CStructFieldDefinition * pFieldDef = _expr.get(i);
        ir::CType * pFieldType = (ir::CType *) ir::resolveBaseType(pStructType->getFields().get(i)->getType());

        // FIXME: assumes constant order of constructor values.

        // Should be done by middle end.
        if (! pFieldDef->getValue()->getType())
            pFieldDef->getValue()->setType(pFieldType, false);

        COperand rhs = translate(* _expr.get(i)->getValue(), _instrs);

        _instrs.push_back(new CField(_ptr, i));
        _instrs.push_back(new CBinary(CBinary::Store,
                COperand(_instrs.back()->getResult()),
                rhs));
    }
}

COperand CTranslator::translate(const ir::CStructConstructor & _expr, instructions_t & _instrs) {
    assert(_expr.getType());
    assert(_expr.getType()->getKind() == ir::CType::Struct);

    Auto<CType> type = translate(* _expr.getType());
    Auto<CVariable> var = new CVariable(type);
    Auto<CUnary> ptr = new CUnary(CUnary::Ptr, COperand(var));

    m_pFunction->locals().push_back(var);
    _instrs.push_back(ptr);
    initStruct(_expr, _instrs, _expr.getType(), COperand(ptr->getResult()));

    return COperand(var);
}

COperand CTranslator::translate(const ir::CUnionConstructor & _expr, instructions_t & _instrs) {
    assert(_expr.getType());
    assert(_expr.isComplete());

    ir::CUnionConstructorDefinition * pDef = _expr.getDefinition();
    Auto<CType> type = translate(* _expr.getType());
    Auto<CVariable> var = new CVariable(type);
    Auto<CUnary> ptr = new CUnary(CUnary::Ptr, COperand(var));

    m_pFunction->locals().push_back(var);
    _instrs.push_back(ptr);
    _instrs.push_back(new CField(COperand(ptr->getResult()), 0));
    _instrs.push_back(new CBinary(CBinary::Store,
            COperand(_instrs.back()->getResult()),
            COperand(CLiteral(new CType(CType::UInt32), (int64_t) pDef->getOrdinal()))));

    const ir::CStructType & dataType = pDef->getStruct();

    if (dataType.getFields().size() > 1) {
        Auto<CStructType> st = translate(dataType);

        _instrs.push_back(new CUnary(CUnary::Malloc,
                COperand(CLiteral(new CType(CType::UInt64), CNumber((int64_t) st->sizeOf())))));
        _instrs.push_back(new CCast(_instrs.back()->getResult(), new CPointerType(st)));

        COperand opBuf(_instrs.back()->getResult());

        _instrs.push_back(new CField(COperand(ptr->getResult()), 1));
        _instrs.push_back(new CCast(COperand(_instrs.back()->getResult()), new CPointerType(new CPointerType(st))));
        _instrs.push_back(new CBinary(CBinary::Store,
                COperand(_instrs.back()->getResult()), opBuf));

        initStruct(_expr, _instrs, & dataType, opBuf);
    } else if (! dataType.getFields().empty()) {
        ir::CStructFieldDefinition * pFieldDef = _expr.get(0);
        ir::CType * pFieldType = (ir::CType *) ir::resolveBaseType(dataType.getFields().get(0)->getType());
        Auto<CType> ft = translate(* pFieldType);

        // Should be done by middle end.
        if (! pFieldDef->getValue()->getType())
            pFieldDef->getValue()->setType(pFieldType, false);

        COperand rhs = translate(* _expr.get(0)->getValue(), _instrs);

        if (ft->sizeOf() > CType::sizeOf(CType::Pointer)) {
            _instrs.push_back(new CUnary(CUnary::Malloc,
                    COperand(CLiteral(new CType(CType::UInt64), CNumber((int64_t) ft->sizeOf())))));
            _instrs.push_back(new CCast(COperand(ptr->getResult()), new CPointerType(ft)));

            COperand opBuf(_instrs.back()->getResult());

            _instrs.push_back(new CBinary(CBinary::Store, opBuf, rhs));
            rhs = opBuf;
        }

        _instrs.push_back(new CField(COperand(ptr->getResult()), 1));
        _instrs.push_back(new CCast(COperand(_instrs.back()->getResult()), new CPointerType(ft)));
        _instrs.push_back(new CBinary(CBinary::Store,
                COperand(_instrs.back()->getResult()), rhs));
    }

    return COperand(var);
}


COperand CTranslator::translate(const ir::CConstructor & _expr, instructions_t & _instrs) {
    switch (_expr.getConstructorKind()) {
        case ir::CConstructor::StructFields:
            return translate((ir::CStructConstructor &) _expr, _instrs);
        case ir::CConstructor::UnionConstructor:
            return translate((ir::CUnionConstructor &) _expr, _instrs);
    }

    assert(false);
}

COperand CTranslator::translate(const ir::CExpression & _expr, instructions_t & _instrs) {
    switch (_expr.getKind()) {
        case ir::CExpression::Literal:
            return translate((ir::CLiteral &) _expr, _instrs);
        case ir::CExpression::Var:
            return translate((ir::CVariableReference &) _expr, _instrs);
        case ir::CExpression::Predicate:
            return translate((ir::CPredicateReference &) _expr, _instrs);
        case ir::CExpression::Binary:
            return translate((ir::CBinary &) _expr, _instrs);
        case ir::CExpression::FunctionCall:
            return translate((ir::CFunctionCall &) _expr, _instrs);
        case ir::CExpression::Ternary:
            return translate((ir::CTernary &) _expr, _instrs);
        case ir::CExpression::Component:
            return translate((ir::CComponent &) _expr, _instrs);
        case ir::CExpression::Constructor:
            return translate((ir::CConstructor &) _expr, _instrs);
    }

    assert(false);
    return COperand();
}

void CTranslator::translate(const ir::CIf & _stmt, instructions_t & _instrs) {
    COperand cond = translate(* _stmt.getParam(), _instrs);
    CIf * pIf = new CIf(cond);

    _instrs.push_back(pIf);
    translate(* _stmt.getBody(), pIf->brTrue());
    if (_stmt.getElse())
        translate(* _stmt.getElse(), pIf->brFalse());
}

void CTranslator::translateAsssignment(const ir::CNamedValue * _pLHS,
        const ir::CExpression * _pExpr, instructions_t & _instrs)
{
    bool bReference = false;
    Auto<CVariable> pVar = resolveVariable(_pLHS, bReference);

    assert(! pVar.empty());

    COperand left = COperand(pVar); //translate(* _stmt.getLValue(), _instrs);
    COperand right = translate(* _pExpr, _instrs);

    if (_pExpr->getType()->getKind() == ir::CType::String) {
        // Copy string.
        _instrs.push_back(new CCast(right,
                new CPointerType(new CType(CType::UInt32))));
        _instrs.push_back(new CBinary(CBinary::Offset,
                COperand(_instrs.back()->getResult()),
                COperand(CLiteral(new CType(CType::Int32), CNumber((int64_t) -1)))));

        COperand opSrc (_instrs.back()->getResult());

        _instrs.push_back(new CUnary(CUnary::Load,
                COperand(_instrs.back()->getResult())));
        _instrs.push_back(new CBinary(CBinary::Mul,
                COperand(_instrs.back()->getResult()),
                CLiteral(new CType(CType::UInt32), CNumber(
                        (int64_t) CType::sizeOf(CType::WChar)))));
        _instrs.push_back(new CBinary(CBinary::Add,
                COperand(_instrs.back()->getResult()),
                CLiteral(new CType(CType::UInt32), CNumber(
                        (int64_t) (CType::sizeOf(CType::WChar) +
                        CType::sizeOf(CType::UInt32))))));

        COperand opSz (_instrs.back()->getResult());

        _instrs.push_back(new CUnary(CUnary::Malloc, opSz));

        COperand opBuf (_instrs.back()->getResult());

        _instrs.push_back(new CCopy(opBuf, opSrc, opSz));
        _instrs.push_back(new CCast(opBuf,
                new CPointerType(new CType(CType::UInt32))));
        _instrs.push_back(new CBinary(CBinary::Offset,
                COperand(_instrs.back()->getResult()),
                CLiteral(new CType(CType::Int32), CNumber((int64_t) 1))));
        _instrs.push_back(new CCast(COperand(_instrs.back()->getResult()),
                new CPointerType(new CType(CType::WChar))));

        right = COperand(_instrs.back()->getResult());
    }

    if (bReference)
        _instrs.push_back(new CBinary(CBinary::Store, left, right));
    else
        _instrs.push_back(new CBinary(CBinary::Set, left, right));
}

void CTranslator::translate(const ir::CAssignment & _stmt, instructions_t & _instrs) {
    assert(_stmt.getLValue()->getKind() == ir::CExpression::Var);

    const ir::CVariableReference * pLValue = (const ir::CVariableReference *) _stmt.getLValue();

    translateAsssignment(pLValue->getTarget(), _stmt.getExpression(), _instrs);
}

void CTranslator::translate(const ir::CJump & _stmt, instructions_t & _instrs) {
    CLiteral num(CLiteral::Number, new CType(CType::Int32));
    const int nLabel = resolveLabel(_stmt.getDestination());
    num.setNumber(CNumber((int64_t) nLabel));
    _instrs.push_back(new CUnary(CUnary::Return, COperand(num)));
}

bool CTranslator::translateBuiltin(const ir::CCall & _stmt, instructions_t & _instrs) {
    ir::CExpression * pExpr = _stmt.getPredicate();

    if (pExpr->getKind() != ir::CExpression::Predicate)
        return false;

    ir::CPredicateReference * pPredRef = (ir::CPredicateReference *) pExpr;

    if (! pPredRef->getTarget()->isBuiltin())
        return false;

    const std::wstring & name = pPredRef->getTarget()->getName();

    if (name == L"print") {
        translatePrint(_stmt, _instrs);
        return true;
    }

    return false;
}

void CTranslator::translatePrintExpr(const ir::CExpression & _expr, instructions_t & _instrs) {
    std::wstring strFunc;
    Auto<CType> type = translate(* _expr.getType());

    switch (type->getKind()) {
        case llir::CType::Bool:    strFunc = L"__p_printBool"; break;
        case llir::CType::WChar:   strFunc = L"__p_printWChar"; break;
        case llir::CType::Int8:    strFunc = L"__p_printInt8"; break;
        case llir::CType::Int16:   strFunc = L"__p_printInt16"; break;
        case llir::CType::Int32:   strFunc = L"__p_printInt32"; break;
        case llir::CType::Int64:   strFunc = L"__p_printInt64"; break;
        case llir::CType::UInt8:   strFunc = L"__p_printUInt8"; break;
        case llir::CType::UInt16:  strFunc = L"__p_printUInt16"; break;
        case llir::CType::UInt32:  strFunc = L"__p_printUInt32"; break;
        case llir::CType::UInt64:  strFunc = L"__p_printUInt64"; break;
        /*case llir::CType::Gmp_z:   strFunc = m_os << "gmpz";
        case llir::CType::Gmp_q:   strFunc = m_os << "gmpq";
        case llir::CType::String:  strFunc = m_os << "string";
        case llir::CType::WString: strFunc = m_os << "wstring";
        case llir::CType::Float:   strFunc = m_os << "float";
        case llir::CType::Double:  strFunc = m_os << "double";
        case llir::CType::Quad:    strFunc = m_os << "quad";*/

        /*case llir::CType::Struct:
            return generate(_os, (llir::CStructType &) _type);*/

        case llir::CType::Pointer:
            if (((CPointerType &) * type).getBase()->getKind() == CType::WChar) {
                strFunc = L"__p_printWString";
                break;
            }
        default:
            strFunc = L"__p_printPtr";
    }

    Auto<CFunction> pFunc = resolveBuiltin(strFunc);
    Auto<CFunctionType> pFuncType;

    if (pFunc.empty()) {
        pFuncType = new CFunctionType(new CType(CType::Void));
        pFuncType->argTypes().push_back(type);
        pFunc = new CFunction(strFunc, new CType(CType::Void));
        pFunc->setType(pFuncType);
        addBuiltin(strFunc, pFunc);
        m_pModule->usedBuiltins().push_back(pFunc);
    } else
        pFuncType = pFunc->getType();

    Auto<CVariable> pFuncVar = pFunc;
    Auto<CCall> pCall = new CCall(COperand(pFuncVar), pFuncType);

    pCall->args().push_back(translate(_expr, _instrs));
    _instrs.push_back(pCall);
}

void CTranslator::translatePrint(const ir::CCall & _stmt, instructions_t & _instrs) {
    for (size_t i = 0; i < _stmt.getParams().size(); ++ i) {
        const ir::CExpression * pParam = _stmt.getParams().get(i);
        translatePrintExpr(* pParam, _instrs);
    }
}

void CTranslator::translate(const ir::CCall & _stmt, instructions_t & _instrs) {
    assert(_stmt.getPredicate()->getType()->getKind() == ir::CType::Predicate);

    for (size_t i = 0; i < _stmt.getDeclarations().size(); ++ i)
        translate(* _stmt.getDeclarations().get(i), _instrs);

    if (translateBuiltin(_stmt, _instrs))
        return;

    COperand function = translate(* _stmt.getPredicate(), _instrs);
    ir::CPredicateType & predType = * (ir::CPredicateType *) _stmt.getPredicate()->getType();
    Auto<CFunctionType> type = translate(predType);
    CCall * pCall = new CCall(function, type);
    types_t::const_iterator iType = type->argTypes().begin();
    int cBranch = -1;
    size_t cParam = 0;

    for (size_t i = 0; i < type->argTypes().size(); ++ i, ++ iType) {
        if (i < predType.getInParams().size()) {
            pCall->args().push_back(translate(* _stmt.getParams().get(i), _instrs));
        } else {
            while (cBranch < 0 || cParam >= _stmt.getBranches().get(cBranch)->size()) {
                ++ cBranch;
                cParam = 0;
                assert(cBranch < (int) _stmt.getBranches().size());
            }

            COperand op = translate(* _stmt.getBranches().get(cBranch)->get(cParam), _instrs);
            _instrs.push_back(new CUnary(CUnary::Ptr, op));
            pCall->args().push_back(COperand(_instrs.back()->getResult()));
        }
    }

    _instrs.push_back(pCall);

    if (_stmt.getBranches().size() > 1) {
        CSwitch * pSwitch = new CSwitch(COperand(pCall->getResult()));

        for (size_t i = 0; i < _stmt.getBranches().size(); ++ i) {
            ir::CStatement * pHandler = _stmt.getBranches().get(i)->getHandler();
            if (pHandler) {
                pSwitch->cases().push_back(switch_case_t());
                switch_case_t & switchCase = pSwitch->cases().back();
                switchCase.values.push_back(i);
                translate(* pHandler, switchCase.body);
            }
        }

        _instrs.push_back(pSwitch);
    }
}

void CTranslator::translate(const ir::CBlock & _stmt, instructions_t & _instrs) {
    CTranslator * pTranslator = addChild();
    for (size_t i = 0; i < _stmt.size(); ++ i)
        pTranslator->translate(* _stmt.get(i), _instrs);
}

void CTranslator::translate(const ir::CVariableDeclaration & _stmt, instructions_t & _instrs) {
    Auto<CType> type = translate(* _stmt.getVariable()->getType());
    m_pFunction->locals().push_back(new CVariable(type));
    Auto<CVariable> var = m_pFunction->locals().back();
    addVariable(_stmt.getVariable(), var);

    if (_stmt.getValue())
        translateAsssignment(_stmt.getVariable(), _stmt.getValue(), _instrs);
}

COperand CTranslator::translateSwitchCond(const ir::CExpression & _expr, const COperand & _arg,
        instructions_t & _instrs)
{
    if (_expr.getKind() == ir::CExpression::Type) {
        const ir::CTypeExpr & te = (const ir::CTypeExpr &) _expr;
        assert(te.getContents()->getKind() == ir::CType::Range);
        const ir::CRange & range = * (ir::CRange *) te.getContents();

        COperand opMin = translate(* range.getMin(), _instrs);
        COperand opMax = translate(* range.getMax(), _instrs);
        CBinary * pGte = new CBinary(CBinary::Gte, _arg, opMin);
        CBinary * pLte = new CBinary(CBinary::Lte, _arg, opMax);
        _instrs.push_back(pGte);
        _instrs.push_back(pLte);
        _instrs.push_back(new CBinary(CBinary::BAnd,
                COperand(pGte->getResult()),
                COperand(pLte->getResult())));
        return COperand(_instrs.back()->getResult());
    } else {
        COperand rhs = translate(_expr, _instrs);
        _instrs.push_back(new CBinary(CBinary::Eq, _arg, rhs));
        return COperand(_instrs.back()->getResult());
    }
}

void CTranslator::translateSwitchInt(const ir::CSwitch & _stmt,
        const COperand & _arg, instructions_t & _instrs)
{
    assert(_arg.getType()->getKind() & CType::IntMask);

    Auto<CSwitch> pSwitch = new CSwitch(_arg);

    std::vector<switch_case_t *> cases(_stmt.size());
    CIf * pPrevIf = NULL, * pFirstIf = NULL;
    instructions_t prefix;

    for (size_t i = 0; i < _stmt.size(); ++ i) {
        const ir::CCollection<ir::CExpression> & exprs = _stmt.get(i)->getExpressions();
        switch_case_t * pCase = NULL;
        CIf * pIf = NULL;
        size_t cComplexConditions = 0;

        for (size_t j = 0; j < exprs.size(); ++ j) {
            const ir::CExpression & expr = * exprs.get(j);

            if (expr.getKind() != ir::CExpression::Literal) {
                ++ cComplexConditions;
                continue;
            }

            if (! pCase) {
                pSwitch->cases().push_back(switch_case_t());
                pCase = & pSwitch->cases().back();
            }

            pCase->values.push_back(((const ir::CLiteral &) expr).getNumber().getInt());
        }

        for (size_t j = 0; cComplexConditions > 0 && j < exprs.size(); ++ j) {
            const ir::CExpression & expr = * exprs.get(j);

            if (expr.getKind() == ir::CExpression::Literal)
                continue;

            -- cComplexConditions;

            if (pCase) {
                if (pCase->body.empty())
                    pCase->body.push_back(new CInstruction());

                if (pCase->body.front()->getLabel().empty())
                    pCase->body.front()->setLabel(new CLabel());

                COperand cond = translateSwitchCond(expr, _arg, _instrs);

                _instrs.push_back(new CBinary(CBinary::Jnz,
                        cond, COperand(pCase->body.front()->getLabel())));
            } else {
                instructions_t & instrs = pPrevIf ? pPrevIf->brFalse() : prefix;
                COperand cond = translateSwitchCond(expr, _arg, instrs);

                if (pIf) {
                    instrs.push_back(new CBinary(CBinary::BOr,
                            cond, pIf->getCondition()));
                    pIf->setCondition(COperand(instrs.back()->getResult()));
                } else {
                    pIf = new CIf(COperand(instrs.back()->getResult()));
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

void CTranslator::translateSwitchUnion(const ir::CSwitch & _stmt,
        const COperand & _arg, instructions_t & _instrs)
{
    const ir::CType * pParamType = resolveBaseType(_stmt.getParam()->getType());

    assert(pParamType->getKind() == ir::CType::Union);

    const ir::CUnionType * pUnion = (const ir::CUnionType *) pParamType;
    std::vector<switch_case_t *> cases;

    cases.assign(_stmt.size(), NULL);

    _instrs.push_back(new CUnary(CUnary::Ptr, _arg));

    COperand opStructPtr = COperand(_instrs.back()->getResult());

    _instrs.push_back(new CField(opStructPtr, 0));
    _instrs.push_back(new CUnary(CUnary::Load, COperand(_instrs.back()->getResult())));

    Auto<CSwitch> pSwitch = new CSwitch(COperand(_instrs.back()->getResult()));

    _instrs.push_back(new CField(opStructPtr, 1));
    _instrs.push_back(new CUnary(CUnary::Load, COperand(_instrs.back()->getResult())));

    COperand opContents = COperand(_instrs.back()->getResult());

    std::vector<switch_case_t *> caseIdx(pUnion->getConstructors().size());
    std::vector<Auto<CLabel> > caseLabels(pUnion->getConstructors().size());
    std::vector<bool> isConstructor;

    // Prepare map.
    for (size_t i = 0; i < _stmt.size(); ++ i) {
        const ir::CCollection<ir::CExpression> & exprs = _stmt.get(i)->getExpressions();

        for (size_t j = 0; j < exprs.size(); ++ j) {
            const ir::CExpression * pExpr = exprs.get(j);
            bool bIsConstructor = false;

            if (pExpr->getKind() == ir::CExpression::Constructor &&
                    ((const ir::CConstructor *) pExpr)->getConstructorKind() == ir::CConstructor::UnionConstructor)
            {
                const ir::CUnionConstructor * pCons = (const ir::CUnionConstructor *) pExpr;

                if (! pCons->isComplete() || pCons->size() == 0) {
                    const ir::CUnionConstructorDefinition * pDef = pCons->getDefinition();
                    const size_t cOrd = pDef->getOrdinal();

                    if (caseIdx[cOrd] == NULL) {
                        pSwitch->cases().push_back(switch_case_t());
                        pSwitch->cases().back().values.push_back(cOrd);
                        caseIdx[cOrd] = & pSwitch->cases().back();
                        caseLabels[cOrd] = new CLabel;
                    }

                    bIsConstructor = true;
                }
            }

            isConstructor.push_back(bIsConstructor);
        }
    }

    size_t cExprIdx = 0;
    instructions_t * pInstrs = & _instrs;

    for (size_t i = 0; i < _stmt.size(); ++ i) {
        const ir::CCollection<ir::CExpression> & exprs = _stmt.get(i)->getExpressions();
        switch_case_t * pMainCase = NULL;
        Auto<CLabel> labelBody = new CLabel();

        for (size_t j = 0; j < exprs.size(); ++ j, ++ cExprIdx) {
            if (! isConstructor[cExprIdx])
                continue;

            // Got union constructor expression.
            const ir::CUnionConstructor * pCons = (const ir::CUnionConstructor *) exprs.get(j);;
            const ir::CUnionConstructorDefinition * pDef = pCons->getDefinition();
            switch_case_t * pCase = caseIdx[pCons->getDefinition()->getOrdinal()];

            assert(pCase != NULL);

            if (! pMainCase)
                pMainCase = pCase;

            for (size_t k = 0; k < pCons->getDeclarations().size(); ++ k) {
                const ir::CVariableDeclaration * pDecl = pCons->getDeclarations().get(k);
                translate(* pDecl, pCase->body);
            }

            Auto<CLabel> labelNextCmp = new CLabel();

            instructions_t & body = pCase->body;
            COperand opValue;
            bool bStruct = pDef->getStruct().getFields().size() > 1;
            bool bPointer = (pDef->getStruct().getFields().size() == 1);
            const bool bCompare = pCons->getDeclarations().size() < pCons->size();

            if (bPointer) {
                Auto<CType> fieldType = translate(* pDef->getStruct().getFields().get(0)->getType());
                bPointer = (fieldType->sizeOf() > CType::sizeOf(CType::Pointer));
            }

            if (bCompare || ! pCons->getDeclarations().empty()) {
                if (bStruct) {
                    Auto<CStructType> st = translate(pDef->getStruct());

                    body.push_back(new CCast(opContents, new CPointerType(st)));
                    opValue = COperand(body.back()->getResult());
                } else {
                    Auto<CType> fieldType = translate(* pDef->getStruct().getFields().get(0)->getType());

                    if (bPointer) {
                        body.push_back(new CCast(opContents, new CPointerType(fieldType)));
                        body.push_back(new CUnary(CUnary::Load, body.back()->getResult()));
                    } else
                        body.push_back(new CCast(opContents, fieldType));
                }

                opValue = COperand(body.back()->getResult());
            }

            if (bCompare) {
                for (size_t k = 0; k < pCons->size(); ++ k) {
                    const ir::CStructFieldDefinition * pDef = pCons->get(k);

                    if (pDef->getValue()) {
                        // Compare and jump.
                        COperand lhs;

                        if (bStruct) {
                            //body.push_back(new CUnary(CUnary::Ptr, _arg));
                            body.push_back(new CField(opValue, k));
                            lhs = COperand(body.back()->getResult());
                        } else {
                            assert(k == 0);
                            lhs = opValue;
                        }

                        body.push_back(new CUnary(CUnary::Load, lhs));
                        lhs = COperand(body.back()->getResult());

                        COperand rhs = translate(* pDef->getValue(), body);

                        rhs = translateEq(resolveBaseType(pDef->getValue()->getType()), lhs, rhs, body);
                        body.push_back(new CBinary(CBinary::Jmz,
                                rhs, COperand(labelNextCmp)));

                    }
                }
            }

            for (size_t k = 0; k < pCons->size(); ++ k) {
                const ir::CStructFieldDefinition * pDef = pCons->get(k);

                if (! pDef->getValue()) {
                    // Initialize.
                    COperand opField;
                    Auto<CVariable> pFieldVar;

                    if (bStruct) {
                        //body.push_back(new CUnary(CUnary::Ptr, _arg));
                        body.push_back(new CField(opValue, k));
                        pFieldVar = new CVariable(body.back()->getResult()->getType());
                        body.push_back(new CBinary(CBinary::Set, pFieldVar, COperand(body.back()->getResult())));
                    } else {
                        assert(k == 0);
                        //opField = opValue;
                        pFieldVar = new CVariable(opValue.getType());
                        body.push_back(new CBinary(CBinary::Set, pFieldVar, opValue));
                    }

                    addVariable(pDef->getField(), pFieldVar, bStruct);
                    m_pFunction->locals().push_back(pFieldVar);
                    opField = COperand(pFieldVar);
                    //pFieldVar->setTyoe(opField.getType());
                }
            }

            body.push_back(new CUnary(CUnary::Jmp, COperand(labelBody)));

            if (bCompare) {
                body.push_back(new CInstruction()); // nop
                body.back()->setLabel(labelNextCmp);
            }
        }

        cExprIdx -= exprs.size(); // Reset counter for second iteration.

        COperand lhs;

        for (size_t j = 0; j < exprs.size(); ++ j, ++ cExprIdx) {
            if (isConstructor[cExprIdx])
                continue;

            ir::CExpression * pCond = exprs.get(j);

            COperand rhs = translate(* pCond, * pInstrs);

            rhs = translateEq(pParamType, _arg, rhs, * pInstrs);

            if (! lhs.empty()) {
                pInstrs->push_back(new CBinary(CBinary::BOr, lhs, rhs));
                lhs = COperand(pInstrs->back()->getResult());
            } else
                lhs = rhs;
        }

        if (! lhs.empty()) {
            if (pMainCase) {
                pInstrs->push_back(new CBinary(CBinary::Jnz, lhs, COperand(labelBody)));
            } else {
                CIf * pIf = new CIf(lhs);

                pInstrs->push_back(pIf);
                translate(* _stmt.get(i)->getBody(), pIf->brTrue());
                pInstrs = & pIf->brFalse();
            }
        }

        // Actual case body.
        if (pMainCase == NULL || _stmt.get(i)->getBody() == NULL)
            continue;

        instructions_t & body = pMainCase->body;

        body.push_back(new CInstruction()); // nop
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

void CTranslator::translate(const ir::CSwitch & _stmt, instructions_t & _instrs) {
    if (_stmt.getParamDecl())
        translate(* _stmt.getParamDecl(), _instrs);

    const ir::CType * pParamType = resolveBaseType(_stmt.getParam()->getType());
    COperand arg = translate(* _stmt.getParam(), _instrs);

    if (arg.getType()->getKind() & CType::IntMask) {
        translateSwitchInt(_stmt, arg, _instrs);
        return;
    } else if (pParamType->getKind() == ir::CType::Union) {
        translateSwitchUnion(_stmt, arg, _instrs);
        return;
    }

    // Encode as a series of if-s.

    Auto<CSwitch> pSwitch = new CSwitch(arg);
    instructions_t * pInstrs = & _instrs;

    for (size_t i = 0; i < _stmt.size(); ++ i) {
        const ir::CCollection<ir::CExpression> & exprs = _stmt.get(i)->getExpressions();

        assert(! exprs.empty());

        COperand lhs = translate(* exprs.get(0), * pInstrs);
        lhs = translateEq(pParamType, arg, lhs, * pInstrs);

        for (size_t j = 1; j < exprs.size(); ++ j) {
            COperand rhs = translate(* exprs.get(j), * pInstrs);
            rhs = translateEq(pParamType, arg, rhs, * pInstrs);

            pInstrs->push_back(new CBinary(CBinary::BOr, lhs, rhs));
            lhs = COperand(pInstrs->back()->getResult());
        }

        CIf * pIf = new CIf(lhs);

        pInstrs->push_back(pIf);
        translate(* _stmt.get(i)->getBody(), pIf->brTrue());
        pInstrs = & pIf->brFalse();
    }

    if (_stmt.getDefault())
        translate(* _stmt.getDefault(), * pInstrs);
}

void CTranslator::translate(const ir::CStatement & _stmt, instructions_t & _instrs) {
    switch (_stmt.getKind()) {
        case ir::CStatement::If:
            translate((ir::CIf &) _stmt, _instrs); break;
        case ir::CStatement::Assignment:
            translate((ir::CAssignment &) _stmt, _instrs); break;
        case ir::CStatement::Call:
            translate((ir::CCall &) _stmt, _instrs); break;
        case ir::CStatement::VariableDeclaration:
            translate((ir::CVariableDeclaration &) _stmt, _instrs); break;
        case ir::CStatement::Block:
            translate((ir::CBlock &) _stmt, _instrs); break;
        case ir::CStatement::Jump:
            translate((ir::CJump &) _stmt, _instrs); break;
        case ir::CStatement::Switch:
            translate((ir::CSwitch &) _stmt, _instrs); break;
    }
}

Auto<CFunction> CTranslator::translate(const ir::CPredicate & _pred) {
    CTranslator * pTranslator = addChild();
    const size_t cBranches = _pred.getOutParams().size();
    Auto<CType> returnType;
    ir::CNamedValue * pResult = NULL;

    if (cBranches == 1 && _pred.getOutParams().get(0)->size() == 1) {
        // Trivial predicate: one branch, one output parameter.
        pResult = _pred.getOutParams().get(0)->get(0);
        returnType = pTranslator->translate(* pResult->getType());
    } else if (cBranches < 2) {
        // Pass output params as pointers.
        returnType = new CType(CType::Void);
    } else {
        // Pass output params as pointers, return branch index.
        returnType = new CType(CType::Int32);
    }

    Auto<CFunction> pFunction = new CFunction(_pred.getName(), returnType);

    for (size_t i = 0; i < _pred.getInParams().size(); ++ i) {
        ir::CNamedValue * pVar = _pred.getInParams().get(i);
        Auto<CType> type = pTranslator->translate(* pVar->getType());
        pFunction->args().push_back(new CVariable(type));
        addVariable(pVar, pFunction->args().back());
    }

    if (pResult) {
        pTranslator->addVariable(pResult, pFunction->getResult());
    } else {
        for (size_t i = 0; i < _pred.getOutParams().size(); ++ i) {
            ir::CBranch & branch = * _pred.getOutParams().get(i);
            m_labels[branch.getLabel()] = i;
            for (size_t j = 0; j < branch.size(); ++ j) {
                ir::CNamedValue * pVar = branch.get(j);
                Auto<CType> argType = pTranslator->translate(* pVar->getType());
                pFunction->args().push_back(new CVariable(new CPointerType(argType)));
                addVariable(pVar, pFunction->args().back(), true);
            }
        }
    }

    addVariable(& _pred, pFunction);

    pTranslator->m_pFunction = pFunction;
    pTranslator->translate(* _pred.getBlock(), pFunction->instructions());

    if (returnType->getKind() != CType::Void && pResult)
        pFunction->instructions().push_back(new CUnary(CUnary::Return, pFunction->getResult()));

    processLL<CMarkEOLs>(* pFunction);
    processLL<CCountLabels>(* pFunction);
    processLL<CPruneJumps>(* pFunction);
    processLL<CCollapseReturns>(* pFunction);
    processLL<CRecycleVars>(* pFunction);

    return pFunction;
}

/*void CTranslator::insertRefs(instructions_t & _instrs) {
    for (instructions_t::iterator iInstr = _instrs.begin(); iInstr != _instrs.end(); ++ iInstr) {
        CInstruction & instr = ** iInstr;
        instructions_t::iterator iNext = iInstr;
        COperand op;
        ++ iNext;
        switch (instr.getKind()) {
            case CInstruction::Unary:
                if (((CUnary &) instr).getUnaryKind() == CUnary::Ptr)
                    continue;
                break;
            case CInstruction::Cast:
                if (((CCast &) instr).getOp().getKind() == COperand::Variable &&
                        ((CCast &) instr).getOp().getVariable()->getLastUse() == * iInstr)
                    continue;
                break;
            case CInstruction::Field:
                if (((CField &) instr).getOp().getKind() == COperand::Variable &&
                        ((CField &) instr).getOp().getVariable()->getLastUse() == * iInstr)
                    continue;
                break;
            case CInstruction::If:
                insertRefs(((CIf &) instr).brTrue());
                insertRefs(((CIf &) instr).brFalse());
                break;
            case CInstruction::Switch: {
                CSwitch & sw = (CSwitch &) instr;
                for (switch_cases_t::iterator iCase = sw.cases().begin(); iCase != sw.cases().end(); ++ iCase)
                    insertRefs(iCase->body);
                insertRefs(sw.deflt());
                break;
            }
            case CInstruction::Binary: {
                CBinary & bin = (CBinary &) instr;
                switch (((CBinary &) instr).getBinaryKind()) {
                    case CBinary::Set:
                        if (bin.getOp1().getType()->getKind() == CType::Pointer) {
                            _instrs.insert(iInstr, new CUnary(CUnary::Unref, bin.getOp1()));
                            op = bin.getOp1();
                        }
                        break;
                    case CBinary::Store:
                        if (bin.getOp2().getType()->getKind() == CType::Pointer) {
                            _instrs.insert(iInstr, new CUnary(CUnary::UnrefNd, bin.getOp1()));
                            op = bin.getOp2();
                        }
                        break;
                    case CBinary::Offset:
                        if (bin.getOp1().getKind() == COperand::Variable &&
                                bin.getOp1().getVariable()->getLastUse() == * iInstr)
                            continue;
                        break;
                }
                break;
            }
        }

        if (! instr.getResult().empty() && ! instr.getResult()->getType().empty()
                && instr.getResult()->getType()->getKind() == CType::Pointer)
        {
            m_ptrs.push_back(instr.getResult());
            iInstr = _instrs.insert(iNext, new CUnary(CUnary::Ref, COperand(instr.getResult())));
        }

        if (op.getKind() != COperand::Empty)
            iInstr = _instrs.insert(iNext, new CUnary(CUnary::Ref, op));
    }
}

void CTranslator::insertUnrefs() {
    for (args_t::iterator iVar = m_pFunction->locals().begin(); iVar != m_pFunction->locals().end(); ++ iVar) {
        Auto<CVariable> pVar = * iVar;
        const CType & type = * pVar->getType();

        switch (type.getKind()) {
            case CType::Pointer:
                m_pFunction->instructions().push_back(
                        new CUnary(CUnary::Unref, COperand(pVar)));
                break;
        }
    }

    for (args_t::iterator iVar = m_ptrs.begin(); iVar != m_ptrs.end(); ++ iVar)
        m_pFunction->instructions().push_back(new CUnary(CUnary::Unref, COperand(* iVar)));
}
*/
void CTranslator::translate(const ir::CModule & _module, CModule & _dest) {
    m_pModule = & _dest;

    for (size_t i = 0; i < _module.getPredicates().size(); ++ i) {
        const ir::CPredicate & pred = * _module.getPredicates().get(i);
        if (pred.getBlock() != NULL)
            m_pModule->functions().push_back(translate(pred));
    }
}

void translate(CModule & _dest, const ir::CModule & _from) {
    CTranslator translator(NULL);
    translator.translate(_from, _dest);
}

};
