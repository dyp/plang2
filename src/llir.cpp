/// \file llir.cpp
///

#include <map>
#include <iostream>

#include <assert.h>

#include "ir/statements.h"
#include "llir.h"
#include "utils.h"


namespace ir {

bool isTypeVariable(const CNamedValue * _pVar, const CType * & _pType) {
    if (! _pVar || ! _pVar->getType())
        return false;

    const CType * pType = _pVar->getType();

    if (! pType)
        return false;

    if (pType->getKind() == CType::Type) {
        if (_pType) _pType = pType;
        return true;
    }

    if (pType->getKind() != CType::NamedReference)
        return false;

    const CNamedReferenceType * pRef = (const CNamedReferenceType *) pType;

    if (pRef->getDeclaration() != NULL) {
        if (_pType) _pType = pRef->getDeclaration()->getType();
        return true;
    }

    return false;
}
const CType * resolveBaseType(const CType * _pType) {
    if (! _pType)
        return NULL;

    while (_pType) {
        if (_pType->getKind() == CType::NamedReference) {
            const CNamedReferenceType * pRef = (CNamedReferenceType *) _pType;

            if (pRef->getDeclaration()) {
                _pType = pRef->getDeclaration()->getType();
            } else if (pRef->getVariable()) {
                if (! isTypeVariable(pRef->getVariable(), _pType))
                    return _pType;
            }
        } else if (_pType->getKind() == CType::Parameterized) {
            _pType = ((CParameterizedType *) _pType)->getActualType();
        } else
            break;
    }

    return _pType;
}
};

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

    Auto<CVariable> resolveVariable(const void * _pVar);
    int resolveLabel(const ir::CLabel *);
    void addVariable(const void * _pOrig, Auto<CVariable> _pNew) { m_vars[_pOrig] = _pNew; }

    void addType(const ir::CType * _pIRType, const Auto<CType> & _pLLIRType);
    Auto<CType> resolveType(const ir::CType * _pType);

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
    COperand translate(const ir::CStructConstructor & _expr, instructions_t & _instrs);
    COperand translateSwitchCond(const ir::CExpression & _expr, const COperand & _arg,
            instructions_t & _instrs);

    void markEOLs(instructions_t & _instrs);
    void markEOLs(const operands_t & _ops, Auto<CInstruction> _instr);
    void markEOL(const COperand & _op, Auto<CInstruction> _instr);
    void insertRefs(instructions_t & _instrs);
    void insertUnrefs();

private:
    typedef std::map<const void *, Auto<CVariable> > variable_map_t;
    typedef std::map<const ir::CLabel *, int> label_numbers_t;
    typedef std::map<const ir::CType *, Auto<CType> > type_map_t;

    Auto<CStructType> m_unionType;
    CTranslator * m_pParent;
    CModule * m_pModule;
    Auto<CFunction> m_pFunction;
    std::list<CTranslator *> m_children;
    variable_map_t m_vars;
    label_numbers_t m_labels;
    type_map_t m_types;
    args_t m_ptrs;
};

CTranslator::~CTranslator() {
    for (std::list<CTranslator *>::iterator iChild = m_children.begin(); iChild != m_children.end(); ++ iChild)
        delete * iChild;
}

CTranslator * CTranslator::addChild() {
    m_children.push_back(new CTranslator(this));
    return m_children.back();
}

Auto<CVariable> CTranslator::resolveVariable(const void * _pVar) {
    variable_map_t::iterator iVar = m_vars.find(_pVar);

    if (iVar != m_vars.end())
        return iVar->second;
    else if (m_pParent)
        return m_pParent->resolveVariable(_pVar);
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
        case ir::CType::String: return new CPointerType (new CType(CType::WChar));
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
    assert(nInstr >= 0);
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

COperand CTranslator::translate(const ir::CBinary & _expr, instructions_t & _instrs) {
    COperand left = translate(* _expr.getLeftSide(), _instrs);
    COperand right = translate(* _expr.getRightSide(), _instrs);
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
    const ir::CStructType * pStruct = _expr.getStructType();
    Auto<CStructType> st = translate(* pStruct);
    COperand object = translate(* _expr.getObject(), _instrs);

    _instrs.push_back(new CUnary(CUnary::Ptr, object));
    _instrs.push_back(new CField(COperand(_instrs.back()->getResult()), _expr.getFieldIdx()));
    _instrs.push_back(new CUnary(CUnary::Load, COperand(_instrs.back()->getResult())));

    return COperand(_instrs.back()->getResult());
}

COperand CTranslator::translate(const ir::CUnionAlternativeExpr & _expr, instructions_t & _instrs) {
    const ir::CUnionType * pUnion = _expr.getUnionType();
    Auto<CStructType> st = translate(* pUnion);
    COperand object = translate(* _expr.getObject(), _instrs);

    _instrs.push_back(new CUnary(CUnary::Ptr, object));
    _instrs.push_back(new CField(COperand(_instrs.back()->getResult()), 1));
    _instrs.push_back(new CUnary(CUnary::Load, COperand(_instrs.back()->getResult())));

    const ir::CType * pAltType = pUnion->getAlternatives().get(_expr.getIdx())->getType();
    Auto<CType> type = translate(* pAltType);

    if (type->sizeOf() <= CType::sizeOf(CType::Pointer)) {
        _instrs.push_back(new CCast(COperand(_instrs.back()->getResult()), type));
    } else
        assert(false);

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
    m_pModule->consts().push_back(new CConstant(new CLiteral(new CType(CType::WChar))));
    m_pModule->consts().back()->getLiteral()->setWString(_str);

    Auto<CVariable> var = m_pModule->consts().back();

    _instrs.push_back(new CUnary(CUnary::Ptr, COperand(var)));

    return COperand(_instrs.back()->getResult());
}

COperand CTranslator::translate(const ir::CLiteral & _expr, instructions_t & _instrs) {
    if (_expr.getLiteralKind() == ir::CLiteral::String) {
        return translateStringLiteral(_expr.getString(), _instrs);
    } else {
        CLiteral lit(translate(* _expr.getType()));

        switch (_expr.getLiteralKind()) {
            case ir::CLiteral::Number: lit.setNumber(_expr.getNumber()); break;
            case ir::CLiteral::Bool:   lit.setBool(_expr.getBool()); break;
            case ir::CLiteral::Char:   lit.setWChar(_expr.getChar()); break;
            //case ir::CLiteral::String: lit.setWString(_expr.getString()); break;
        }

        return COperand(lit);
    }
}

COperand CTranslator::translate(const ir::CVariableReference & _expr, instructions_t & _instrs) {
    Auto<CVariable> pVar = resolveVariable(_expr.getTarget());
    assert(! pVar.empty());
    return COperand(pVar);
}

COperand CTranslator::translate(const ir::CPredicateReference & _expr, instructions_t & _instrs) {
    Auto<CVariable> pVar = resolveVariable(_expr.getTarget());
    assert(! pVar.empty());
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

COperand CTranslator::translate(const ir::CStructConstructor & _expr, instructions_t & _instrs) {
    assert(_expr.getType());

    int kind = _expr.getType()->getKind();

    if (_expr.getType()->getKind() == ir::CType::Union) {
        assert(_expr.size() == 1);
        // TODO: move to middle-end.
        ir::CStructFieldDefinition * pField = _expr.get(0);
        if (pField->getName().empty()) {
            // Lookup by type. Unimplemented.
            assert(false);
        } else {
            ir::CUnionType * pUnionType = (ir::CUnionType *) _expr.getType();
            ir::CNamedValues & fields = pUnionType->getAlternatives();
            for (size_t i = 0; i < fields.size(); ++ i)
                if (fields.get(i)->getName() == pField->getName()) {
                    Auto<CType> unionType = translate(* pUnionType);
                    Auto<CType> type = translate(* fields.get(i)->getType());
                    Auto<CVariable> var = new CVariable(unionType);
                    Auto<CUnary> ptr = new CUnary(CUnary::Ptr, COperand(var));

                    m_pFunction->locals().push_back(var);

                    _instrs.push_back(ptr);
                    _instrs.push_back(new CField(COperand(_instrs.back()->getResult()), 0));
                    _instrs.push_back(new CBinary(CBinary::Store,
                            COperand(_instrs.back()->getResult()),
                            COperand(CLiteral(CType::Int32, CNumber((int64_t) i)))));
                    _instrs.push_back(new CField(COperand(ptr->getResult()), 1));

                    Auto<CInstruction> valueField = new CCast(
                            COperand(_instrs.back()->getResult()),
                            new CPointerType(type));

                    _instrs.push_back(valueField);

                    // TODO: this should have been handled by middle-end.
                    ir::CType * pFieldType = (ir::CType *) ir::resolveBaseType(fields.get(i)->getType());
                    pField->getValue()->setType(pFieldType, false);

                    const size_t sz = type->sizeOf();

                    if (sz <= CType::sizeOf(CType::Pointer)) {
                        COperand op = translate(* pField->getValue(), _instrs);

                        _instrs.push_back(new CCast(op, type));
//                        _instrs.push_back(new CCast(COperand(valueField->getResult()),
//                                new CPointerType(op.getType())));
                        _instrs.push_back(new CBinary(CBinary::Store,
                                COperand(valueField->getResult()),
                                COperand(_instrs.back()->getResult())));
                    } else {
                        CUnary * pMalloc = new CUnary(CUnary::Malloc,
                                COperand(CLiteral(CType::UInt64, CNumber((int64_t) sz))));

                        _instrs.push_back(pMalloc);
                        _instrs.push_back(new CCast(COperand(pMalloc->getResult()),
                                new CPointerType(type)));

                        COperand mem = COperand(_instrs.back()->getResult());
                        COperand op = translate(* pField->getValue(), _instrs);

                        _instrs.push_back(new CCast(op, type));
                        _instrs.push_back(new CBinary(CBinary::Store,
                                mem, COperand(_instrs.back()->getResult())));
                        _instrs.push_back(new CBinary(CBinary::Store,
                                COperand(valueField->getResult()), mem));
                    }

                    return COperand(var);
                }
            assert(false); // Alternative not found. WTF?!
        }
    } else {
        assert(_expr.getType()->getKind() == ir::CType::Struct);
        ir::CStructType * pStructType = (ir::CStructType *) _expr.getType();
        Auto<CType> type = translate(* _expr.getType());
        Auto<CVariable> var = new CVariable(type);
        Auto<CUnary> ptr = new CUnary(CUnary::Ptr, COperand(var));

        m_pFunction->locals().push_back(var);
        _instrs.push_back(ptr);

        for (size_t i = 0; i < _expr.size(); ++ i) {
            ir::CStructFieldDefinition * pFieldDef = _expr.get(i);
//            ir::CType * pFieldType = pStructType->getFields().get(i)->getType();
            ir::CType * pFieldType = (ir::CType *) ir::resolveBaseType(pStructType->getFields().get(i)->getType());

            // FIXME: assumes constant order of constructor values.

            if (! pFieldDef->getValue()->getType())
                pFieldDef->getValue()->setType(pFieldType, false);
            //pFieldDef->getValue()->setType(pFieldDef->getField()->getType());

//            pFieldDef

//            _expr.

/*            // TODO: this should have been handled by middle-end.
            ir::CType * pFieldType = (ir::CType *) ir::resolveBaseType(_expr.get(i)->getType());
            pField->getValue()->setType(pFieldType, false); */

            COperand rhs = translate(* _expr.get(i)->getValue(), _instrs);

            _instrs.push_back(new CField(COperand(ptr->getResult()), i));
            _instrs.push_back(new CBinary(CBinary::Store,
                    COperand(_instrs.back()->getResult()),
                    rhs));
        }

        return COperand(var);
    }

//    assert(false);
}

COperand CTranslator::translate(const ir::CConstructor & _expr, instructions_t & _instrs) {
    switch (_expr.getConstructorKind()) {
        case ir::CConstructor::StructFields:
            return translate((ir::CStructConstructor &) _expr, _instrs);
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
//    return COperand();
}

void CTranslator::translate(const ir::CIf & _stmt, instructions_t & _instrs) {
    COperand cond = translate(* _stmt.getParam(), _instrs);
    CIf * pIf = new CIf(cond);

    _instrs.push_back(pIf);
    translate(* _stmt.getBody(), pIf->brTrue());
    if (_stmt.getElse())
        translate(* _stmt.getElse(), pIf->brFalse());
}

void CTranslator::translate(const ir::CAssignment & _stmt, instructions_t & _instrs) {
    COperand left = translate(* _stmt.getLValue(), _instrs);
    COperand right = translate(* _stmt.getExpression(), _instrs);

    if (_stmt.getExpression()->getType()->getKind() == ir::CType::String) {
        // Copy string.
        _instrs.push_back(new CCast(right,
                new CPointerType(new CType(CType::UInt32))));
        _instrs.push_back(new CBinary(CBinary::Offset,
                COperand(_instrs.back()->getResult()),
                COperand(CLiteral(CType::Int32, CNumber((int64_t) -1)))));

        COperand opSrc (_instrs.back()->getResult());

        _instrs.push_back(new CUnary(CUnary::Load,
                COperand(_instrs.back()->getResult())));
        _instrs.push_back(new CBinary(CBinary::Add,
                COperand(_instrs.back()->getResult()),
                CLiteral(CType::UInt32, CNumber(
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
                CLiteral(CType::Int32, CNumber((int64_t) 1))));
        _instrs.push_back(new CCast(COperand(_instrs.back()->getResult()),
                new CPointerType(new CType(CType::WChar))));

        right = COperand(_instrs.back()->getResult());
    }

    _instrs.push_back(new CBinary(CBinary::Set, left, right));
}

void CTranslator::translate(const ir::CJump & _stmt, instructions_t & _instrs) {
    CLiteral num(new CType(CType::Int32));
    const int nLabel = resolveLabel(_stmt.getDestination());
    num.setNumber(CNumber((int64_t) nLabel));
    _instrs.push_back(new CUnary(CUnary::Return, COperand(num)));
}

void CTranslator::translate(const ir::CCall & _stmt, instructions_t & _instrs) {
    assert(_stmt.getPredicate()->getType()->getKind() == ir::CType::Predicate);

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
/*
            assert(iType->getKind() == CType::Pointer);
            CPointerType * ptrType = (CPointerType *) & (* iType);
            CVariable var(ptrType->getBase());
            m_pFunction->locals().push_back(var);
            _instrs.push_back(new CUnary(CUnary::Ptr, COperand(& m_pFunction->locals().back()))); */
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
    Auto<CType> type = translate(* _stmt.getVariable().getType());
    m_pFunction->locals().push_back(new CVariable(type));
    Auto<CVariable> var = m_pFunction->locals().back();
    addVariable(& _stmt.getVariable(), var);

    if (_stmt.getValue()) {
        COperand val = translate(* _stmt.getValue(), _instrs);
        _instrs.push_back(new CBinary(CBinary::Set, COperand(var), val));
    }
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
    assert(_stmt.getParam()->getType()->getKind() == ir::CType::Union);

//    const ir::CUnionType & type = (const ir::CUnionType &) * _stmt.getParam()->getType();

    _instrs.push_back(new CUnary(CUnary::Ptr, _arg));
    _instrs.push_back(new CField(COperand(_instrs.back()->getResult()), 0));
    _instrs.push_back(new CUnary(CUnary::Load, COperand(_instrs.back()->getResult())));

    Auto<CSwitch> pSwitch = new CSwitch(COperand(_instrs.back()->getResult()));

    for (size_t i = 0; i < _stmt.size(); ++ i) {
        const ir::CCollection<ir::CExpression> & exprs = _stmt.get(i)->getExpressions();
        switch_case_t * pCase = NULL;

        for (size_t j = 0; j < exprs.size(); ++ j) {
            const ir::CExpression & expr = * exprs.get(j);

            assert(expr.getKind() == ir::CExpression::Component);
            assert(((const ir::CComponent &) expr).getComponentKind() == ir::CComponent::UnionAlternative);

            const ir::CUnionAlternativeExpr & alt = (const ir::CUnionAlternativeExpr &) expr;

            if (! pCase) {
                pSwitch->cases().push_back(switch_case_t());
                pCase = & pSwitch->cases().back();
            }

            pCase->values.push_back(alt.getIdx());
        }

        assert(pCase);

        translate(* _stmt.get(i)->getBody(), pCase->body);
    }

    if (! pSwitch->cases().empty()) {
        if (_stmt.getDefault())
            translate(* _stmt.getDefault(), pSwitch->deflt());
        _instrs.push_back(pSwitch);
    } else if (_stmt.getDefault())
        translate(* _stmt.getDefault(), _instrs);
}

void CTranslator::translate(const ir::CSwitch & _stmt, instructions_t & _instrs) {
    if (_stmt.getParamDecl())
        translate(* _stmt.getParamDecl(), _instrs);

    COperand arg = translate(* _stmt.getParam(), _instrs);

    if (arg.getType()->getKind() & CType::IntMask) {
        translateSwitchInt(_stmt, arg, _instrs);
        return;
    } else if (_stmt.getParam()->getType()->getKind() == ir::CType::Union) {
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
        pInstrs->push_back(new CBinary(_selectBinaryInstr(ir::CBinary::Equals,
                lhs.getType()), arg, lhs));
        lhs = COperand(pInstrs->back()->getResult());

        for (size_t j = 1; j < exprs.size(); ++ j) {
            //const ir::CExpression & expr = * exprs.get(j);
            COperand rhs = translate(* exprs.get(j), * pInstrs);
            pInstrs->push_back(new CBinary(_selectBinaryInstr(ir::CBinary::Equals,
                    rhs.getType()), arg, rhs));
            rhs = COperand(pInstrs->back()->getResult());

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
                addVariable(pVar, pFunction->args().back());
            }
        }
    }

    addVariable(& _pred, pFunction);

    pTranslator->m_pFunction = pFunction;
    pTranslator->translate(* _pred.getBlock(), pFunction->instructions());

    pTranslator->markEOLs(pFunction->instructions());
    pTranslator->insertRefs(pFunction->instructions());
    pTranslator->insertUnrefs();

    if (returnType->getKind() == CType::Void)
        pFunction->instructions().push_back(new CUnary(CUnary::Return));
    else if (pResult)
        pFunction->instructions().push_back(new CUnary(CUnary::Return, pFunction->getResult()));

    return pFunction;
}

void CTranslator::markEOL(const COperand & _op, Auto<CInstruction> _instr) {
    if (_op.getKind() == COperand::Variable)
        _op.getVariable()->setLastUse(_instr);
}

void CTranslator::markEOLs(const operands_t & _ops, Auto<CInstruction> _instr) {
    for (operands_t::const_iterator iOp = _ops.begin(); iOp != _ops.end(); ++ iOp)
        markEOL(* iOp, _instr);
}

void CTranslator::markEOLs(instructions_t & _instrs) {
    for (instructions_t::iterator iInstr = _instrs.begin(); iInstr != _instrs.end(); ++ iInstr) {
        Auto<CInstruction> pInstr = * iInstr;
        CInstruction & instr = ** iInstr;

        switch (instr.getKind()) {
            case CInstruction::Unary:
                markEOL(((CUnary & ) instr).getOp(), pInstr);
                break;
            case CInstruction::Binary:
                markEOL(((CBinary & ) instr).getOp1(), pInstr);
                markEOL(((CBinary & ) instr).getOp2(), pInstr);
                break;
            case CInstruction::Call:
                markEOLs(((CCall &) instr).args(), pInstr);
                break;
            case CInstruction::If:
                markEOL(((CIf &) instr).getCondition(), pInstr);
                markEOLs(((CIf &) instr).brTrue());
                markEOLs(((CIf &) instr).brFalse());
                break;
            case CInstruction::Switch: {
                CSwitch & sw = (CSwitch &) instr;
                markEOL(sw.getArg(), pInstr);
                for (switch_cases_t::iterator iCase = sw.cases().begin(); iCase != sw.cases().end(); ++ iCase)
                    markEOLs(iCase->body);
                markEOLs(sw.deflt());
                break;
            }
            case CInstruction::Select:
                markEOL(((CSelect & ) instr).getCondition(), pInstr);
                markEOL(((CSelect & ) instr).getTrue(), pInstr);
                markEOL(((CSelect & ) instr).getFalse(), pInstr);
                break;
            case CInstruction::Field:
                markEOL(((CField & ) instr).getOp(), pInstr);
                break;
            case CInstruction::Cast:
                markEOL(((CCast & ) instr).getOp(), pInstr);
                break;
            case CInstruction::Copy:
                markEOL(((CCopy & ) instr).getDest(), pInstr);
                markEOL(((CCopy & ) instr).getSrc(), pInstr);
                markEOL(((CCopy & ) instr).getSize(), pInstr);
                break;
        }
    }
}

void CTranslator::insertRefs(instructions_t & _instrs) {
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
                //iInstr = _instrs.insert(iNext, new CUnary(CUnary::Ref, ((CField &) instr).getOp()));
                //continue;
/*                op = COperand(instr.getResult());
                break;
            case CInstruction::Unary:
                switch (((CUnary &) instr).getUnaryKind()) {
                    case CUnary::Malloc:
                        op = COperand(instr.getResult());
                        break;
                }
                break;*/
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
/*                        _instrs.insert(iInstr, new CUnary(CUnary::Unref, bin.getOp1()));
                        if (bin.getOp1().getType()->getKind() == CType::Pointer &&
                                ((CPointerType &) * bin.getOp1().getType()).getBase()->getKind() == CType::Pointer)
                            op = bin.getOp1(); */
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
