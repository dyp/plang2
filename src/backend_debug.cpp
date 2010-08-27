/// \file backend_debug.cpp
///

#include <iostream>
#include <string>
#include <map>

#include "utils.h"
#include "llir.h"
#include "backend_debug.h"

namespace backend {

class CDebugGenerator {
public:
    CDebugGenerator(std::wostream & _os) :
        m_pParent(NULL), m_os(_os), m_nLevel(0), m_pChild(NULL), m_nParam(0),
        m_nVar(0), m_nConst(0), m_nLabel(0) {}
    CDebugGenerator(CDebugGenerator & _parent) :
        m_pParent(& _parent), m_os(_parent.m_os), m_nLevel(_parent.m_nLevel + 1),
        m_pChild(NULL), m_nParam(0), m_nVar(0), m_nConst(0), m_nLabel(0) {}
    ~CDebugGenerator();

    CDebugGenerator * addChild();
    CDebugGenerator * getChild();

    std::wostream & generate(const llir::CModule & _module);
    std::wostream & generate(const llir::CFunction & _function);
    std::wostream & generate(const llir::CType & _type);
    std::wostream & generate(const llir::CStructType & _struct);
    std::wostream & generate(const llir::CVariable & _var);
    std::wostream & generate(const llir::CLiteral & _lit);
    std::wostream & generate(const llir::COperand & _op);
    std::wostream & generateTypeDef(const llir::CType & _type);
    std::wostream & generate(const llir::CInstruction & _instr);
    std::wostream & generate(const llir::CUnary & _instr);
    std::wostream & generate(const llir::CBinary & _instr);
    std::wostream & generate(const llir::CSelect & _instr);
    std::wostream & generate(const llir::CField & _instr);
    std::wostream & generate(const llir::CIf & _instr);
    std::wostream & generate(const llir::CSwitch & _instr);
    std::wostream & generate(const llir::CCall & _instr);
    std::wostream & generate(const llir::CCast & _instr);
    std::wostream & generate(const llir::CConstant & _const);
    std::wostream & generate(const llir::CCopy & _instr);

    std::wstring resolveVariable(const void * _p) { return resolveName(& CDebugGenerator::m_vars, _p); }
    std::wstring resolveType(const void * _p) { return resolveName(& CDebugGenerator::m_types, _p); }

    std::wstring resolveLabel(const llir::CLabel & _label);

private:
    CDebugGenerator * m_pParent;
    std::wostream & m_os;
    int m_nLevel;
    CDebugGenerator * m_pChild;
    int m_nParam;
    int m_nVar;
    int m_nConst;
    int m_nLabel;
    const llir::CInstruction * m_pCurrentInstr;

    typedef std::map<const void *, std::wstring> name_map_t;
    name_map_t m_vars;
    name_map_t m_types;
    name_map_t m_labels;

    std::wstring fmtIndent(const std::wstring & _s) {
        std::wstring res;

        for (int i = 0; i < m_nLevel; ++ i)
            res += L"  ";

        return res + _s;
    }

    std::wstring resolveName(name_map_t CDebugGenerator::* _map, const void * _p);
};

CDebugGenerator::~CDebugGenerator() {
    if (m_pChild)
        delete m_pChild;
}

std::wstring CDebugGenerator::resolveLabel(const llir::CLabel & _label) {
    if (m_pParent)
        return m_pParent->resolveLabel(_label);

    std::wstring strName = resolveName(& CDebugGenerator::m_labels, & _label);

    if (strName.empty()) {
        strName = fmtInt(m_nLabel ++, L"L%llu");
        m_labels[& _label] = strName;
    }

    return strName;
}

CDebugGenerator * CDebugGenerator::getChild() {
    if (! m_pChild)
        return addChild();
    return m_pChild;
}

CDebugGenerator * CDebugGenerator::addChild() {
    if (m_pChild)
        delete m_pChild;
    m_pChild = new CDebugGenerator(* this);
    return m_pChild;
}

std::wstring CDebugGenerator::resolveName(name_map_t CDebugGenerator::* _map, const void * _p) {
    name_map_t::iterator iName = (this->*_map).find(_p);

    if (iName != (this->*_map).end())
        return iName->second;
    else if (m_pParent)
        return m_pParent->resolveName(_map, _p);
    else
        return L"";
}

std::wostream & CDebugGenerator::generate(const llir::CConstant & _const) {
    std::wstring strName = fmtInt(m_nConst ++, L"c%llu");


    m_os << fmtIndent(L".const %") << strName << L" ";


    if (_const.getType()->getKind() == llir::CType::WChar) {
        m_os << _const.getLiteral()->getString().size() << L" ";
        m_os << "L\"" << _const.getLiteral()->getString() << L'"';
    }

    m_os << "\n";
    m_vars[& _const] = strName;

    return m_os;
}

std::wostream & CDebugGenerator::generate(const llir::CModule & _module) {
    m_os << ".module {\n";

    for (llir::consts_t::const_iterator iConst = _module.consts().begin(); iConst != _module.consts().end(); ++ iConst)
        getChild()->generate(** iConst);

    for (llir::types_t::const_iterator iType = _module.types().begin(); iType != _module.types().end(); ++ iType)
        getChild()->generateTypeDef(** iType);

    for (llir::functions_t::const_iterator iFunc = _module.functions().begin(); iFunc != _module.functions().end(); ++ iFunc)
        getChild()->generate(** iFunc);

    m_os << "}\n";

    return m_os;
}

std::wostream & CDebugGenerator::generate(const llir::CVariable & _var) {
    return generate(* _var.getType());
}

std::wostream & CDebugGenerator::generate(const llir::CType & _type) {
    std::wstring strName = resolveType(& _type);

    if (! strName.empty())
        return m_os << strName;

    switch (_type.getKind()) {
        case llir::CType::Void:    return m_os << "void";
        case llir::CType::Gmp_z:   return m_os << "gmpz";
        case llir::CType::Gmp_q:   return m_os << "gmpq";
        /*case llir::CType::String:  return m_os << "string";
        case llir::CType::WString: return m_os << "wstring";*/
        case llir::CType::Int8:    return m_os << "int8";
        case llir::CType::Int16:   return m_os << "int16";
        case llir::CType::Int32:   return m_os << "int32";
        case llir::CType::Int64:   return m_os << "int64";
        case llir::CType::UInt8:   return m_os << "uint8";
        case llir::CType::UInt16:  return m_os << "uint16";
        case llir::CType::UInt32:  return m_os << "uint32";
        case llir::CType::UInt64:  return m_os << "uint64";
        case llir::CType::Bool:    return m_os << "bool";
        //case llir::CType::Char:    return m_os << "char";
        case llir::CType::WChar:   return m_os << "wchar";
        case llir::CType::Float:   return m_os << "float";
        case llir::CType::Double:  return m_os << "double";
        case llir::CType::Quad:    return m_os << "quad";

        case llir::CType::Struct:
            return generate((llir::CStructType &) _type);

        case llir::CType::Pointer:
            m_os << "ptr ( ";
            generate(* ((llir::CPointerType &) _type).getBase());
            return m_os << " )";
    }

    return m_os << "t" << _type.getKind();
}

std::wostream & CDebugGenerator::generate(const llir::CLiteral & _lit) {
    switch (_lit.getType()->getKind()) {
        case llir::CType::Int8:
        case llir::CType::Int16:
        case llir::CType::Int32:
        case llir::CType::Int64:
        case llir::CType::UInt8:
        case llir::CType::UInt16:
        case llir::CType::UInt32:
        case llir::CType::UInt64:
        case llir::CType::Float:
        case llir::CType::Double:
        case llir::CType::Quad:
        case llir::CType::Gmp_z:
        case llir::CType::Gmp_q:
            return m_os << _lit.getNumber().toString();
        case llir::CType::Bool:
            return m_os << (_lit.getBool() ? "true" : "false");
        /*case llir::CType::Char:
            return m_os << "\'" << _lit.getChar() << "\'";
        case llir::CType::WChar:
            return m_os << "L\'" << _lit.getWChar() << "\'";
        case llir::CType::String:
            return m_os << "\"" << _lit.getString().c_str() << "\"";
        case llir::CType::WString:
            return m_os << "L\"" << _lit.getWString() << "\"";*/
    }

    return m_os;
}

std::wostream & CDebugGenerator::generate(const llir::COperand & _op) {
    switch (_op.getKind()) {
        case llir::COperand::Literal:
            return generate(_op.getLiteral());
        case llir::COperand::Variable: {
            m_os << "%" << resolveVariable(& * _op.getVariable());
            if (m_pCurrentInstr == & * _op.getVariable()->getLastUse())
                m_os << ".";
            return m_os;
        }
        case llir::COperand::Label:
            return m_os << "$" << resolveLabel(* _op.getLabel());
    }

    return m_os;
}

std::wostream & CDebugGenerator::generate(const llir::CBinary & _instr) {
    std::wstring strInstr = L"!!!";

    switch (_instr.getBinaryKind()) {
        case llir::CBinary::Add: strInstr = L"add"; break;
        case llir::CBinary::Sub: strInstr = L"sub"; break;
        case llir::CBinary::Mul: strInstr = L"mul"; break;
        case llir::CBinary::Div: strInstr = L"div"; break;
        case llir::CBinary::Rem: strInstr = L"rem"; break;
        case llir::CBinary::Shl: strInstr = L"shl"; break;
        case llir::CBinary::Shr: strInstr = L"shr"; break;
        case llir::CBinary::Pow: strInstr = L"pow"; break;
        case llir::CBinary::And: strInstr = L"and"; break;
        case llir::CBinary::Or:  strInstr = L"or";  break;
        case llir::CBinary::Xor: strInstr = L"xor"; break;

        case llir::CBinary::BAnd: strInstr = L"band"; break;
        case llir::CBinary::BOr:  strInstr = L"bor";  break;
        case llir::CBinary::BXor: strInstr = L"bxor"; break;

        case llir::CBinary::FAdd: strInstr = L"fadd"; break;
        case llir::CBinary::FSub: strInstr = L"fsub"; break;
        case llir::CBinary::FMul: strInstr = L"fmul"; break;
        case llir::CBinary::FDiv: strInstr = L"fdiv"; break;
        case llir::CBinary::FPow: strInstr = L"fpow"; break;

        case llir::CBinary::ZAdd: strInstr = L"zadd"; break;
        case llir::CBinary::ZSub: strInstr = L"zsub"; break;
        case llir::CBinary::ZMul: strInstr = L"zmul"; break;
        case llir::CBinary::ZDiv: strInstr = L"zdiv"; break;
        case llir::CBinary::ZRem: strInstr = L"zrem"; break;
        case llir::CBinary::ZShl: strInstr = L"zshl"; break;
        case llir::CBinary::ZShr: strInstr = L"zshr"; break;
        case llir::CBinary::ZPow: strInstr = L"zpow"; break;
        case llir::CBinary::ZAnd: strInstr = L"zand"; break;
        case llir::CBinary::ZOr:  strInstr = L"zor";  break;
        case llir::CBinary::ZXor: strInstr = L"zxor"; break;

        case llir::CBinary::QAdd: strInstr = L"qadd"; break;
        case llir::CBinary::QSub: strInstr = L"qsub"; break;
        case llir::CBinary::QMul: strInstr = L"qmul"; break;
        case llir::CBinary::QDiv: strInstr = L"qdiv"; break;
        case llir::CBinary::QPow: strInstr = L"qpow"; break;

        case llir::CBinary::Eq:  strInstr = L"eq";  break;
        case llir::CBinary::Ne:  strInstr = L"ne";  break;
        case llir::CBinary::Lt:  strInstr = L"lt";  break;
        case llir::CBinary::Lte: strInstr = L"lte"; break;
        case llir::CBinary::Gt:  strInstr = L"gt";  break;
        case llir::CBinary::Gte: strInstr = L"gte"; break;

        case llir::CBinary::FEq:  strInstr = L"feq";  break;
        case llir::CBinary::FNe:  strInstr = L"fne";  break;
        case llir::CBinary::FLt:  strInstr = L"flt";  break;
        case llir::CBinary::FLte: strInstr = L"flte"; break;
        case llir::CBinary::FGt:  strInstr = L"fgt";  break;
        case llir::CBinary::FGte: strInstr = L"fgte"; break;

        case llir::CBinary::ZEq:  strInstr = L"zeq";  break;
        case llir::CBinary::ZNe:  strInstr = L"zne";  break;
        case llir::CBinary::ZLt:  strInstr = L"zlt";  break;
        case llir::CBinary::ZLte: strInstr = L"zlte"; break;
        case llir::CBinary::ZGt:  strInstr = L"zgt";  break;
        case llir::CBinary::ZGte: strInstr = L"zgte"; break;

        case llir::CBinary::QEq:  strInstr = L"qeq";  break;
        case llir::CBinary::QNe:  strInstr = L"qne";  break;
        case llir::CBinary::QLt:  strInstr = L"qlt";  break;
        case llir::CBinary::QLte: strInstr = L"qlte"; break;
        case llir::CBinary::QGt:  strInstr = L"qgt";  break;
        case llir::CBinary::QGte: strInstr = L"qgte"; break;

        case llir::CBinary::Set: strInstr = L"set"; break;
        case llir::CBinary::Store: strInstr = L"store"; break;
        case llir::CBinary::Jmz: strInstr = L"jmz"; break;
        case llir::CBinary::Jnz: strInstr = L"jnz"; break;
        case llir::CBinary::Offset: strInstr = L"offset"; break;
    }

    m_os << strInstr << " ";
    generate(_instr.getOp1()) << " ";
    generate(_instr.getOp2());

    return m_os;
}

std::wostream & CDebugGenerator::generate(const llir::CCall & _instr) {
    m_os << "call ";
    generate(_instr.getFunction());

    for (llir::operands_t::const_iterator iArg = _instr.args().begin(); iArg != _instr.args().end(); ++ iArg) {
        m_os << " ";
        generate(* iArg);
    }

    return m_os;
}

std::wostream & CDebugGenerator::generate(const llir::CSwitch & _instr) {
    m_os << "switch ";
    generate(_instr.getArg()) << "\n";
    ++ m_nLevel;
    for (llir::switch_cases_t::const_iterator iCase = _instr.cases().begin(); iCase != _instr.cases().end(); ++ iCase) {
        for (size_t i = 0; i < iCase->values.size(); ++ i) {
            m_os << fmtIndent(fmtInt(iCase->values[i], L"case %llu:"));
            m_os << L"\n";
        }

        ++ m_nLevel;
        for (llir::instructions_t::const_iterator iInstr = iCase->body.begin(); iInstr != iCase->body.end(); ++ iInstr) {
            generate(** iInstr);
            m_os << L"\n";
        }
        -- m_nLevel;
    }

    m_os << fmtIndent(L"default:\n");
    ++ m_nLevel;
    for (llir::instructions_t::const_iterator iInstr = _instr.deflt().begin(); iInstr != _instr.deflt().end(); ++ iInstr) {
        generate(** iInstr);
        m_os << L"\n";
    }
    -- m_nLevel;
    -- m_nLevel;

    return m_os;
}

std::wostream & CDebugGenerator::generate(const llir::CIf & _instr) {
    m_os << "if ";
    generate(_instr.getCondition()) << "\n";
    ++ m_nLevel;
    for (llir::instructions_t::const_iterator iInstr = _instr.brTrue().begin(); iInstr != _instr.brTrue().end(); ++ iInstr)
        generate(** iInstr) << "\n";
    -- m_nLevel;

    if (! _instr.brFalse().empty()) {
        m_os << fmtIndent(L"else\n");
        ++ m_nLevel;
        for (llir::instructions_t::const_iterator iInstr = _instr.brFalse().begin(); iInstr != _instr.brFalse().end(); ++ iInstr)
            generate(** iInstr) << "\n";
        -- m_nLevel;
    }

    return m_os;
}

std::wostream & CDebugGenerator::generate(const llir::CUnary & _instr) {
    std::wstring strInstr = L"!!!";

    switch (_instr.getUnaryKind()) {
        case llir::CUnary::Return: strInstr = L"return"; break;
        case llir::CUnary::Ptr: strInstr = L"ptr"; break;
        case llir::CUnary::Load: strInstr = L"load"; break;
        case llir::CUnary::Malloc: strInstr = L"malloc"; break;
        case llir::CUnary::Free: strInstr = L"free"; break;
        case llir::CUnary::Ref: strInstr = L"ref"; break;
        case llir::CUnary::Unref: strInstr = L"unref"; break;
        case llir::CUnary::UnrefNd: strInstr = L"unrefnd"; break;
        case llir::CUnary::Jmp: strInstr = L"jmp"; break;
    }

    m_os << strInstr << " ";
    generate(_instr.getOp());

    return m_os;
}

std::wostream & CDebugGenerator::generate(const llir::CSelect & _instr) {
    m_os << "select ";
    generate(_instr.getCondition()) << " ";
    generate(_instr.getTrue()) << " ";
    generate(_instr.getFalse());
    return m_os;
}

std::wostream & CDebugGenerator::generate(const llir::CCopy & _instr) {
    m_os << "copy ";
    generate(_instr.getDest()) << " ";
    generate(_instr.getSrc()) << " ";
    generate(_instr.getSize());
    return m_os;
}

std::wostream & CDebugGenerator::generate(const llir::CField & _instr) {
    m_os << "field ";
    generate(_instr.getOp()) << " ";
    m_os << _instr.getIndex();
    return m_os;
}

std::wostream & CDebugGenerator::generate(const llir::CCast & _instr) {
    m_os << "cast ";
    generate(_instr.getOp()) << " ";
    generate(* _instr.getType());
    return m_os;
}

std::wostream & CDebugGenerator::generate(const llir::CInstruction & _instr) {
    m_pCurrentInstr = & _instr;

    if (! _instr.getLabel().empty())
        m_os << resolveLabel(* _instr.getLabel()) << L":\n";

    if (! _instr.getResult().empty()) {
        const llir::CVariable * pVar = _instr.getResult().ptr();
        std::wstring strName = resolveVariable(pVar);
        if (strName.empty()) {
            strName = fmtInt(m_nVar ++, L"v%llu");
            m_vars[pVar] = strName;
        }
        m_os << fmtIndent(L"%") << strName << " = ";
    } else
        m_os << fmtIndent(L"");

    switch (_instr.getKind()) {
        case llir::CInstruction::Unary:
            generate((llir::CUnary &) _instr);
            break;
        case llir::CInstruction::Binary:
            generate((llir::CBinary &) _instr);
            break;
        case llir::CInstruction::Select:
            generate((llir::CSelect &) _instr);
            break;
        case llir::CInstruction::Call:
            generate((llir::CCall &) _instr);
            break;
        case llir::CInstruction::If:
            generate((llir::CIf &) _instr);
            break;
        case llir::CInstruction::Switch:
            generate((llir::CSwitch &) _instr);
            break;
        case llir::CInstruction::Field:
            generate((llir::CField &) _instr);
            break;
        case llir::CInstruction::Cast:
            generate((llir::CCast &) _instr);
            break;
        case llir::CInstruction::Copy:
            generate((llir::CCopy &) _instr);
            break;
        //case llir::CInstruction::Nop:
        default:
            m_os << L"nop";
    }

    if (! _instr.getResult().empty() && ! _instr.getResult()->getType().empty()) {
        m_os << "    ## ";
        generate(* _instr.getResult()->getType());
        m_os << "; uc: " << _instr.getResultUsageCount();
    }

    //m_os << fmtIndent(fmtInt(_instr.getKind()));

    return m_os;
}

std::wostream & CDebugGenerator::generate(const llir::CFunction & _function) {
    m_os << L"\n" << fmtIndent(L".function ");
    m_os << _function.getName();
    m_os <<  " {\n";
    m_vars[& _function] = _function.getName();

    addChild();

    if (! _function.getResult().empty()) {
        getChild()->m_vars[_function.getResult().ptr()] = L"r";
        m_os << getChild()->fmtIndent(L".result %r ");
        //const size_t c = sizeof(llir::CType);
        getChild()->generate(* _function.getReturnType()) << "\n";
    }

    for (llir::args_t::const_iterator iArg = _function.args().begin(); iArg != _function.args().end(); ++ iArg) {
        std::wstring strName = fmtInt(getChild()->m_nParam ++);
        getChild()->m_vars[iArg->ptr()] = strName;
        m_os << getChild()->fmtIndent(L".arg %") << strName << " ";
        getChild()->generate(* (* iArg)->getType()) << "\n";
    }

    int nVar = 0;

    for (llir::args_t::const_iterator iVar = _function.locals().begin(); iVar != _function.locals().end(); ++ iVar) {
        std::wstring strName = fmtInt(nVar ++, L"u%llu");
        getChild()->m_vars[iVar->ptr()] = strName;
        m_os << getChild()->fmtIndent(L".var %") << strName << " ";
        getChild()->generate(* (* iVar)->getType()) << "\n";
    }

    for (llir::instructions_t::const_iterator iInstr = _function.instructions().begin(); iInstr != _function.instructions().end(); ++ iInstr) {
        getChild()->generate(** iInstr) << "\n";
    }

    m_os << fmtIndent(L"}\n");

    return m_os;
}

std::wostream & CDebugGenerator::generateTypeDef(const llir::CType & _type) {
    std::wstring strName = fmtInt(m_types.size(), L"td%llu");

    m_os << L"\n" << fmtIndent(L".type ") << strName << L" = ";
    generate(_type);
    m_os << L"\n";

    m_types[& _type] = strName;

    return m_os;
}

std::wostream & CDebugGenerator::generate(const llir::CStructType & _struct) {
    m_os << L"struct (";

    for (llir::types_t::const_iterator iType = _struct.fieldTypes().begin(); iType != _struct.fieldTypes().end(); ++ iType) {
        m_os << L" ";
        generate(** iType);
    }

    m_os << L" )";

    return m_os;
}

void generateDebug(const llir::CModule & _module, std::wostream & _os) {
    CDebugGenerator gen(_os);

    gen.generate(_module);
}

};
