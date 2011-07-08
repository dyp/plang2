/// \file backend_debug.cpp
///

#include <iostream>
#include <string>
#include <map>

#include "utils.h"
#include "llir.h"
#include "backend_debug.h"

using namespace llir;

namespace backend {

class DebugGenerator {
public:
    DebugGenerator(std::wostream & _os) :
        m_pParent(NULL), m_os(_os), m_nLevel(0), m_pChild(NULL), m_nParam(0),
        m_nVar(0), m_nConst(0), m_nLabel(0) {}
    DebugGenerator(DebugGenerator & _parent) :
        m_pParent(& _parent), m_os(_parent.m_os), m_nLevel(_parent.m_nLevel + 1),
        m_pChild(NULL), m_nParam(0), m_nVar(0), m_nConst(0), m_nLabel(0) {}
    ~DebugGenerator();

    DebugGenerator * addChild();
    DebugGenerator * getChild();

    std::wostream & generate(const Module & _module);
    std::wostream & generate(const Function & _function);
    std::wostream & generate(const Type & _type);
    std::wostream & generate(const StructType & _struct);
    std::wostream & generate(const Variable & _var);
    std::wostream & generate(const Literal & _lit);
    std::wostream & generate(const Operand & _op);
    std::wostream & generateTypeDef(const Type & _type);
    std::wostream & generate(const Instruction & _instr);
    std::wostream & generate(const Unary & _instr);
    std::wostream & generate(const Binary & _instr);
    std::wostream & generate(const Select & _instr);
    std::wostream & generate(const Field & _instr);
    std::wostream & generate(const If & _instr);
    std::wostream & generate(const Switch & _instr);
    std::wostream & generate(const Call & _instr);
    std::wostream & generate(const Cast & _instr);
    std::wostream & generate(const Constant & _const);
    std::wostream & generate(const Copy & _instr);

    std::wstring resolveVariable(const void * _p) { return resolveName(& DebugGenerator::m_vars, _p); }
    std::wstring resolveType(const void * _p) { return resolveName(& DebugGenerator::m_types, _p); }

    std::wstring resolveLabel(const Label & _label);

private:
    DebugGenerator * m_pParent;
    std::wostream & m_os;
    int m_nLevel;
    DebugGenerator * m_pChild;
    int m_nParam;
    int m_nVar;
    int m_nConst;
    int m_nLabel;
    const Instruction * m_pCurrentInstr;

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

    std::wstring resolveName(name_map_t DebugGenerator::* _map, const void * _p);
};

DebugGenerator::~DebugGenerator() {
    if (m_pChild)
        delete m_pChild;
}

std::wstring DebugGenerator::resolveLabel(const Label & _label) {
    if (m_pParent)
        return m_pParent->resolveLabel(_label);

    std::wstring strName = resolveName(& DebugGenerator::m_labels, & _label);

    if (strName.empty()) {
        strName = fmtInt(m_nLabel ++, L"L%llu");
        m_labels[& _label] = strName;
    }

    return strName;
}

DebugGenerator * DebugGenerator::getChild() {
    if (! m_pChild)
        return addChild();
    return m_pChild;
}

DebugGenerator * DebugGenerator::addChild() {
    if (m_pChild)
        delete m_pChild;
    m_pChild = new DebugGenerator(* this);
    return m_pChild;
}

std::wstring DebugGenerator::resolveName(name_map_t DebugGenerator::* _map, const void * _p) {
    name_map_t::iterator iName = (this->*_map).find(_p);

    if (iName != (this->*_map).end())
        return iName->second;
    else if (m_pParent)
        return m_pParent->resolveName(_map, _p);
    else
        return L"";
}

std::wostream & DebugGenerator::generate(const Constant & _const) {
    std::wstring strName = fmtInt(m_nConst ++, L"c%llu");


    m_os << fmtIndent(L".const %") << strName << L" ";


    if (_const.getType()->getKind() == Type::WCHAR) {
        m_os << _const.getLiteral()->getString().size() << L" ";
        m_os << "L\"" << _const.getLiteral()->getString() << L'"';
    }

    m_os << "\n";
    m_vars[& _const] = strName;

    return m_os;
}

std::wostream & DebugGenerator::generate(const Module & _module) {
    m_os << ".module {\n";

    for (Consts::const_iterator iConst = _module.consts().begin(); iConst != _module.consts().end(); ++ iConst)
        getChild()->generate(** iConst);

    for (Types::const_iterator iType = _module.types().begin(); iType != _module.types().end(); ++ iType)
        getChild()->generateTypeDef(** iType);

    for (Functions::const_iterator iFunc = _module.functions().begin(); iFunc != _module.functions().end(); ++ iFunc)
        getChild()->generate(** iFunc);

    m_os << "}\n";

    return m_os;
}

std::wostream & DebugGenerator::generate(const Variable & _var) {
    return generate(* _var.getType());
}

std::wostream & DebugGenerator::generate(const Type & _type) {
    std::wstring strName = resolveType(& _type);

    if (! strName.empty())
        return m_os << strName;

    switch (_type.getKind()) {
        case Type::VOID:    return m_os << "void";
        case Type::GMP_Z:   return m_os << "gmpz";
        case Type::GMP_Q:   return m_os << "gmpq";
        /*case Type::String:  return m_os << "string";
        case Type::WString: return m_os << "wstring";*/
        case Type::INT8:    return m_os << "int8";
        case Type::INT16:   return m_os << "int16";
        case Type::INT32:   return m_os << "int32";
        case Type::INT64:   return m_os << "int64";
        case Type::UINT8:   return m_os << "uint8";
        case Type::UINT16:  return m_os << "uint16";
        case Type::UINT32:  return m_os << "uint32";
        case Type::UINT64:  return m_os << "uint64";
        case Type::BOOL:    return m_os << "bool";
        //case Type::Char:    return m_os << "char";
        case Type::WCHAR:   return m_os << "wchar";
        case Type::FLOAT:   return m_os << "float";
        case Type::DOUBLE:  return m_os << "double";
        case Type::QUAD:    return m_os << "quad";

        case Type::STRUCT:
            return generate((StructType &) _type);

        case Type::POINTER:
            m_os << "ptr ( ";
            generate(* ((PointerType &) _type).getBase());
            return m_os << " )";
    }

    return m_os << "t" << _type.getKind();
}

std::wostream & DebugGenerator::generate(const Literal & _lit) {
    switch (_lit.getType()->getKind()) {
        case Type::INT8:
        case Type::INT16:
        case Type::INT32:
        case Type::INT64:
        case Type::UINT8:
        case Type::UINT16:
        case Type::UINT32:
        case Type::UINT64:
        case Type::FLOAT:
        case Type::DOUBLE:
        case Type::QUAD:
        case Type::GMP_Z:
        case Type::GMP_Q:
            return m_os << _lit.getNumber().toString();
        case Type::BOOL:
            return m_os << (_lit.getBool() ? "true" : "false");
        /*case Type::Char:
            return m_os << "\'" << _lit.getChar() << "\'";
        case Type::WChar:
            return m_os << "L\'" << _lit.getWChar() << "\'";
        case Type::String:
            return m_os << "\"" << _lit.getString().c_str() << "\"";
        case Type::WString:
            return m_os << "L\"" << _lit.getWString() << "\"";*/
    }

    return m_os;
}

std::wostream & DebugGenerator::generate(const Operand & _op) {
    switch (_op.getKind()) {
        case Operand::LITERAL:
            return generate(_op.getLiteral());
        case Operand::VARIABLE: {
            m_os << "%" << resolveVariable(& * _op.getVariable());
            if (m_pCurrentInstr == & * _op.getVariable()->getLastUse())
                m_os << ".";
            return m_os;
        }
        case Operand::LABEL:
            return m_os << "$" << resolveLabel(* _op.getLabel());
    }

    return m_os;
}

std::wostream & DebugGenerator::generate(const Binary & _instr) {
    std::wstring strInstr = L"!!!";

    switch (_instr.getBinaryKind()) {
        case Binary::ADD: strInstr = L"add"; break;
        case Binary::SUB: strInstr = L"sub"; break;
        case Binary::MUL: strInstr = L"mul"; break;
        case Binary::DIV: strInstr = L"div"; break;
        case Binary::REM: strInstr = L"rem"; break;
        case Binary::SHL: strInstr = L"shl"; break;
        case Binary::SHR: strInstr = L"shr"; break;
        case Binary::POW: strInstr = L"pow"; break;
        case Binary::AND: strInstr = L"and"; break;
        case Binary::OR:  strInstr = L"or";  break;
        case Binary::XOR: strInstr = L"xor"; break;

        case Binary::BAND: strInstr = L"band"; break;
        case Binary::BOR:  strInstr = L"bor";  break;
        case Binary::BXOR: strInstr = L"bxor"; break;

        case Binary::FADD: strInstr = L"fadd"; break;
        case Binary::FSUB: strInstr = L"fsub"; break;
        case Binary::FMUL: strInstr = L"fmul"; break;
        case Binary::FDIV: strInstr = L"fdiv"; break;
        case Binary::FPOW: strInstr = L"fpow"; break;

        case Binary::ZADD: strInstr = L"zadd"; break;
        case Binary::ZSUB: strInstr = L"zsub"; break;
        case Binary::ZMUL: strInstr = L"zmul"; break;
        case Binary::ZDIV: strInstr = L"zdiv"; break;
        case Binary::ZREM: strInstr = L"zrem"; break;
        case Binary::ZSHL: strInstr = L"zshl"; break;
        case Binary::ZSHR: strInstr = L"zshr"; break;
        case Binary::ZPOW: strInstr = L"zpow"; break;
        case Binary::ZAND: strInstr = L"zand"; break;
        case Binary::ZOR:  strInstr = L"zor";  break;
        case Binary::ZXOR: strInstr = L"zxor"; break;

        case Binary::QADD: strInstr = L"qadd"; break;
        case Binary::QSUB: strInstr = L"qsub"; break;
        case Binary::QMUL: strInstr = L"qmul"; break;
        case Binary::QDIV: strInstr = L"qdiv"; break;
        case Binary::QPOW: strInstr = L"qpow"; break;

        case Binary::EQ:  strInstr = L"eq";  break;
        case Binary::NE:  strInstr = L"ne";  break;
        case Binary::LT:  strInstr = L"lt";  break;
        case Binary::LTE: strInstr = L"lte"; break;
        case Binary::GT:  strInstr = L"gt";  break;
        case Binary::GTE: strInstr = L"gte"; break;

        case Binary::FEQ:  strInstr = L"feq";  break;
        case Binary::FNE:  strInstr = L"fne";  break;
        case Binary::FLT:  strInstr = L"flt";  break;
        case Binary::FLTE: strInstr = L"flte"; break;
        case Binary::FGT:  strInstr = L"fgt";  break;
        case Binary::FGTE: strInstr = L"fgte"; break;

        case Binary::ZEQ:  strInstr = L"zeq";  break;
        case Binary::ZNE:  strInstr = L"zne";  break;
        case Binary::ZLT:  strInstr = L"zlt";  break;
        case Binary::ZLTE: strInstr = L"zlte"; break;
        case Binary::ZGT:  strInstr = L"zgt";  break;
        case Binary::ZGTE: strInstr = L"zgte"; break;

        case Binary::QEQ:  strInstr = L"qeq";  break;
        case Binary::QNE:  strInstr = L"qne";  break;
        case Binary::QLT:  strInstr = L"qlt";  break;
        case Binary::QLTE: strInstr = L"qlte"; break;
        case Binary::QGT:  strInstr = L"qgt";  break;
        case Binary::QGTE: strInstr = L"qgte"; break;

        case Binary::SET: strInstr = L"set"; break;
        case Binary::STORE: strInstr = L"store"; break;
        case Binary::JMZ: strInstr = L"jmz"; break;
        case Binary::JNZ: strInstr = L"jnz"; break;
        case Binary::OFFSET: strInstr = L"offset"; break;
    }

    m_os << strInstr << " ";
    generate(_instr.getOp1()) << " ";
    generate(_instr.getOp2());

    return m_os;
}

std::wostream & DebugGenerator::generate(const Call & _instr) {
    m_os << "call ";
    generate(_instr.getFunction());

    for (Operands::const_iterator iArg = _instr.args().begin(); iArg != _instr.args().end(); ++ iArg) {
        m_os << " ";
        generate(* iArg);
    }

    return m_os;
}

std::wostream & DebugGenerator::generate(const Switch & _instr) {
    m_os << "switch ";
    generate(_instr.getArg()) << "\n";
    ++ m_nLevel;
    for (SwitchCases::const_iterator iCase = _instr.cases().begin(); iCase != _instr.cases().end(); ++ iCase) {
        for (size_t i = 0; i < iCase->values.size(); ++ i) {
            m_os << fmtIndent(fmtInt(iCase->values[i], L"case %llu:"));
            m_os << L"\n";
        }

        ++ m_nLevel;
        for (Instructions::const_iterator iInstr = iCase->body.begin(); iInstr != iCase->body.end(); ++ iInstr) {
            generate(** iInstr);
            m_os << L"\n";
        }
        -- m_nLevel;
    }

    m_os << fmtIndent(L"default:\n");
    ++ m_nLevel;
    for (Instructions::const_iterator iInstr = _instr.deflt().begin(); iInstr != _instr.deflt().end(); ++ iInstr) {
        generate(** iInstr);
        m_os << L"\n";
    }
    -- m_nLevel;
    -- m_nLevel;

    return m_os;
}

std::wostream & DebugGenerator::generate(const If & _instr) {
    m_os << "if ";
    generate(_instr.getCondition()) << "\n";
    ++ m_nLevel;
    for (Instructions::const_iterator iInstr = _instr.brTrue().begin(); iInstr != _instr.brTrue().end(); ++ iInstr)
        generate(** iInstr) << "\n";
    -- m_nLevel;

    if (! _instr.brFalse().empty()) {
        m_os << fmtIndent(L"else\n");
        ++ m_nLevel;
        for (Instructions::const_iterator iInstr = _instr.brFalse().begin(); iInstr != _instr.brFalse().end(); ++ iInstr)
            generate(** iInstr) << "\n";
        -- m_nLevel;
    }

    return m_os;
}

std::wostream & DebugGenerator::generate(const Unary & _instr) {
    std::wstring strInstr = L"!!!";

    switch (_instr.getUnaryKind()) {
        case Unary::RETURN: strInstr = L"return"; break;
        case Unary::PTR: strInstr = L"ptr"; break;
        case Unary::LOAD: strInstr = L"load"; break;
        case Unary::MALLOC: strInstr = L"malloc"; break;
        case Unary::FREE: strInstr = L"free"; break;
        case Unary::REF: strInstr = L"ref"; break;
        case Unary::UNREF: strInstr = L"unref"; break;
        case Unary::UNREFND: strInstr = L"unrefnd"; break;
        case Unary::JMP: strInstr = L"jmp"; break;
    }

    m_os << strInstr << " ";
    generate(_instr.getOp());

    return m_os;
}

std::wostream & DebugGenerator::generate(const Select & _instr) {
    m_os << "select ";
    generate(_instr.getCondition()) << " ";
    generate(_instr.getTrue()) << " ";
    generate(_instr.getFalse());
    return m_os;
}

std::wostream & DebugGenerator::generate(const Copy & _instr) {
    m_os << "copy ";
    generate(_instr.getDest()) << " ";
    generate(_instr.getSrc()) << " ";
    generate(_instr.getSize());
    return m_os;
}

std::wostream & DebugGenerator::generate(const Field & _instr) {
    m_os << "field ";
    generate(_instr.getOp()) << " ";
    m_os << _instr.getIndex();
    return m_os;
}

std::wostream & DebugGenerator::generate(const Cast & _instr) {
    m_os << "cast ";
    generate(_instr.getOp()) << " ";
    generate(* _instr.getType());
    return m_os;
}

std::wostream & DebugGenerator::generate(const Instruction & _instr) {
    m_pCurrentInstr = & _instr;

    if (_instr.getLabel())
        m_os << resolveLabel(* _instr.getLabel()) << L":\n";

    if (_instr.getResult()) {
        const Variable * pVar = _instr.getResult().ptr();
        std::wstring strName = resolveVariable(pVar);
        if (strName.empty()) {
            strName = fmtInt(m_nVar ++, L"v%llu");
            m_vars[pVar] = strName;
        }
        m_os << fmtIndent(L"%") << strName << " = ";
    } else
        m_os << fmtIndent(L"");

    switch (_instr.getKind()) {
        case Instruction::UNARY:
            generate((Unary &) _instr);
            break;
        case Instruction::BINARY:
            generate((Binary &) _instr);
            break;
        case Instruction::SELECT:
            generate((Select &) _instr);
            break;
        case Instruction::CALL:
            generate((Call &) _instr);
            break;
        case Instruction::IF:
            generate((If &) _instr);
            break;
        case Instruction::SWITCH:
            generate((Switch &) _instr);
            break;
        case Instruction::FIELD:
            generate((Field &) _instr);
            break;
        case Instruction::CAST:
            generate((Cast &) _instr);
            break;
        case Instruction::COPY:
            generate((Copy &) _instr);
            break;
        //case Instruction::Nop:
        default:
            m_os << L"nop";
    }

    if (_instr.getResult() && _instr.getResult()->getType()) {
        m_os << "    ## ";
        generate(* _instr.getResult()->getType());
        m_os << "; uc: " << _instr.getResultUsageCount();
    }

    //m_os << fmtIndent(fmtInt(_instr.getKind()));

    return m_os;
}

std::wostream & DebugGenerator::generate(const Function & _function) {
    m_os << L"\n" << fmtIndent(L".function ");
    m_os << _function.getName();
    m_os <<  " {\n";
    m_vars[& _function] = _function.getName();

    addChild();

    if (_function.getResult()) {
        getChild()->m_vars[_function.getResult().ptr()] = L"r";
        m_os << getChild()->fmtIndent(L".result %r ");
        //const size_t c = sizeof(Type);
        getChild()->generate(* _function.getReturnType()) << "\n";
    }

    for (Args::const_iterator iArg = _function.args().begin(); iArg != _function.args().end(); ++ iArg) {
        std::wstring strName = fmtInt(getChild()->m_nParam ++);
        getChild()->m_vars[iArg->ptr()] = strName;
        m_os << getChild()->fmtIndent(L".arg %") << strName << " ";
        getChild()->generate(* (* iArg)->getType()) << "\n";
    }

    int nVar = 0;

    for (Args::const_iterator iVar = _function.locals().begin(); iVar != _function.locals().end(); ++ iVar) {
        std::wstring strName = fmtInt(nVar ++, L"u%llu");
        getChild()->m_vars[iVar->ptr()] = strName;
        m_os << getChild()->fmtIndent(L".var %") << strName << " ";
        getChild()->generate(* (* iVar)->getType()) << "\n";
    }

    for (Instructions::const_iterator iInstr = _function.instructions().begin(); iInstr != _function.instructions().end(); ++ iInstr) {
        getChild()->generate(** iInstr) << "\n";
    }

    m_os << fmtIndent(L"}\n");

    return m_os;
}

std::wostream & DebugGenerator::generateTypeDef(const Type & _type) {
    std::wstring strName = fmtInt(m_types.size(), L"td%llu");

    m_os << L"\n" << fmtIndent(L".type ") << strName << L" = ";
    generate(_type);
    m_os << L"\n";

    m_types[& _type] = strName;

    return m_os;
}

std::wostream & DebugGenerator::generate(const StructType & _struct) {
    m_os << L"struct (";

    for (Types::const_iterator iType = _struct.fieldTypes().begin(); iType != _struct.fieldTypes().end(); ++ iType) {
        m_os << L" ";
        generate(** iType);
    }

    m_os << L" )";

    return m_os;
}

void generateDebug(const Module & _module, std::wostream & _os) {
    DebugGenerator gen(_os);

    gen.generate(_module);
}

};
