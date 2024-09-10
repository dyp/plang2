/// \file backend_c.cpp
///


#include <iostream>
#include <sstream>
#include <string>
#include <map>

#include "utils.h"
#include "llir.h"
#include "backend_c.h"

using namespace llir;

namespace backend {

class CGenerator {
public:
    CGenerator() :
        m_pParent(NULL), /*m_os(_os),*/ m_nLevel(0), m_pChild(NULL), m_nParam(0),
        m_nConst(0), m_nLabel(0) {}
    CGenerator(CGenerator & _parent) :
        m_pParent(& _parent), /*m_os(_parent.m_os),*/ m_nLevel(_parent.m_nLevel + 1),
        m_pChild(NULL), m_nParam(0), m_nConst(0), m_nLabel(0) {}
    ~CGenerator();

    CGenerator * addChild();
    CGenerator * getChild();

    std::wostream & generateModule(std::wostream & _os, const Module & _module);
    std::wostream & generateFunction(std::wostream & _os, const Function & _function);
    std::wostream & generateType(std::wostream & _os, const Type & _type);
    std::wostream & generateStructType(std::wostream & _os, const StructType & _struct);
    std::wostream & generateTypeDef(std::wostream & _os, const Type & _type);
    std::wostream & generateConst(std::wostream & _os, const Constant & _const);

    std::wostream & generateInstruction(std::wostream & _os, const Instruction & _instr, bool _bInline, bool _bLast);
    std::wostream & generateBinary(std::wostream & _os, const Binary & _instr);
    std::wostream & generateUnary(std::wostream & _os, const Unary & _instr);
    std::wostream & generateLiteral(std::wostream & _os, const Literal & _lit);
    std::wostream & generateOperand(std::wostream & _os, const Operand & _op);
    std::wostream & generateIf(std::wostream & _os, const If & _instr);
    std::wostream & generateWhile(std::wostream & _os, const While & _instr);
    std::wostream & generateCall(std::wostream & _os, const Call & _instr);
    std::wostream & generateField(std::wostream & _os, const Field & _instr);
    std::wostream & generateCast(std::wostream & _os, const Cast & _instr);
    std::wostream & generateCopy(std::wostream & _os, const Copy & _instr);
    std::wostream & generateSwitch(std::wostream & _os, const Switch & _instr);

    std::wstring resolveType(const void * _p) { return resolveName(& CGenerator::m_types, _p); }

    std::wstring resolveVariable(const void * _p) {
        std::wstring s = resolveName(& CGenerator::m_args, _p);
        return s.empty() ? resolveName(& CGenerator::m_vars, _p) : s;
    }

    std::wstring resolveLabel(const Label & _label);

    bool canInline(const Instruction & _instr) const;
    //const Instruction * getInlineByResult(const Auto<Variable> & _var) const;

private:
    CGenerator * m_pParent;
    //std::wostream & m_os;
    int m_nLevel;
    CGenerator * m_pChild;
    int m_nParam;
    int m_nConst;
    int m_nLabel;

    typedef std::map<const void *, std::wstring> name_map_t;
    name_map_t m_args;
    name_map_t m_vars;
    name_map_t m_types;
    name_map_t m_labels;

    typedef std::pair<std::wstring, const Instruction *> inline_instr_t;
    typedef std::map<const Variable *, inline_instr_t> inline_map_t;
    inline_map_t m_inlinedInstrs;


    std::wstring fmtIndent(const std::wstring & _s) {
        std::wstring res;

        for (int i = 0; i < m_nLevel; ++ i)
            res += L"  ";

        return res + _s;
    }

    std::wstring resolveName(name_map_t CGenerator::* _map, const void * _p);
};

CGenerator::~CGenerator() {
    if (m_pChild)
        delete m_pChild;
}

CGenerator * CGenerator::getChild() {
    if (! m_pChild)
        return addChild();
    return m_pChild;
}

CGenerator * CGenerator::addChild() {
    if (m_pChild)
        delete m_pChild;
    m_pChild = new CGenerator(* this);
    return m_pChild;
}

std::wstring CGenerator::resolveName(name_map_t CGenerator::* _map, const void * _p) {
    name_map_t::iterator iName = (this->*_map).find(_p);

    if (iName != (this->*_map).end())
        return iName->second;
    else if (m_pParent)
        return m_pParent->resolveName(_map, _p);
    else
        return L"";
}

std::wstring CGenerator::resolveLabel(const Label & _label) {
    if (m_pParent)
        return m_pParent->resolveLabel(_label);

    std::wstring strName = resolveName(& CGenerator::m_labels, & _label);

    if (strName.empty()) {
        strName = fmtInt(m_nLabel ++, L"___l%llu");
        m_labels[& _label] = strName;
    }

    return strName;
}

bool CGenerator::canInline(const Instruction & _instr) const {
    if (_instr.getLabel() && _instr.getLabel()->getUsageCount() > 0)
        return false;

    if (_instr.getKind() == Instruction::UNARY) {
        switch (((const Unary &) _instr).getUnaryKind()) {
            case Unary::PTR:
            case Unary::LOAD:
                return true;
            default:
                return false;
        }
    }

    if (_instr.getResultUsageCount() != 1)
        return false;

    switch (_instr.getKind()) {
        case Instruction::BINARY:
        case Instruction::SELECT:
        case Instruction::FIELD:
        case Instruction::CAST:
            return true;
    }

    return false;
}

/*const Instruction * CGenerator::getInlineByResult(const Auto<Variable> & _var) const {
    inline_map_t::const_iterator iInstr = m_inlinedInstrs.find(_var.ptr());

    return iInstr != m_inlinedInstrs.end() ? iInstr->second : NULL;
}*/

std::wostream & CGenerator::generateModule(std::wostream & _os, const Module & _module) {

    for (Functions::const_iterator iFunc = _module.functions().begin(); iFunc != _module.functions().end(); ++ iFunc)
        m_vars[iFunc->ptr()] = (* iFunc)->getName();

    for (Functions::const_iterator iFunc = _module.usedBuiltins().begin(); iFunc != _module.usedBuiltins().end(); ++ iFunc)
        m_vars[iFunc->ptr()] = (* iFunc)->getName();

    _os << "/* Auto-generated by PLang compiler */\n";
    _os << "#include <plang.h>\n";
    _os << "\n";

    /*for (Consts::const_iterator iConst = _module.consts().begin(); iConst != _module.consts().end(); ++ iConst)
        getChild()->generate(** iConst);*/

    for (Consts::const_iterator iConst = _module.consts().begin(); iConst != _module.consts().end(); ++ iConst)
        getChild()->generateConst(_os, ** iConst);

    _os << "\n";

    for (Types::const_iterator iType = _module.types().begin(); iType != _module.types().end(); ++ iType) {
        getChild()->generateTypeDef(_os, ** iType);
        _os << "\n";
    }

    for (Functions::const_iterator iFunc = _module.functions().begin(); iFunc != _module.functions().end(); ++ iFunc) {
        getChild()->generateFunction(_os, ** iFunc);
        _os << "\n";
    }

    //m_os << "}\n";

    return _os;
}

std::wostream & CGenerator::generateLiteral(std::wostream & _os, const Literal & _lit) {
    switch (_lit.getKind()) {
        case Literal::NUMBER:
            return _os << L"(" << _lit.getNumber().toString() << L")";
        case Literal::BOOL:
            return _os << (_lit.getBool() ? "true" : "false");
        /*case Type::Char:
            return _os << "\'" << _lit.getChar() << "\'";*/
        case Literal::CHAR:
            return _os << "L\'" << _lit.getChar() << "\'";
        case Literal::STRING:
            return _os << "\"" << _lit.getString().c_str() << "\"";
        /*case Type::WString:
            return _os << "L\"" << _lit.getWString() << "\"";*/
    }

    return _os;
}

std::wostream & CGenerator::generateOperand(std::wostream & _os, const Operand & _op) {
    switch (_op.getKind()) {
        case Operand::LITERAL:
            return generateLiteral(_os, _op.getLiteral());
        case Operand::VARIABLE: {
            inline_map_t::iterator iInstr = m_inlinedInstrs.find(_op.getVariable().ptr());
            //const Instruction * pInstr = getInlineByResult(_op.getVariable().ptr());

            if (iInstr != m_inlinedInstrs.end()) {
                _os /*<< L"("*/ << iInstr->second.first /*<< L")"*/;
                //const Instruction * pInstr = iInstr->second;
                iInstr->second.second->decResultUsageCount();
                if (iInstr->second.second->getResultUsageCount() == 0)
                    m_inlinedInstrs.erase(iInstr);
                //_op.getVariable()->getLastInit
                /*_os << L"(";
                generate(_os, * pInstr, true);
                _os << L")";*/
            } else {
                _op.getVariable()->setUsed(true);
                _os << resolveVariable(_op.getVariable().ptr());
            }

            return _os;
        }
        case Operand::LABEL:
            return _os << resolveLabel(* _op.getLabel());
    }

    return _os;
}

std::wostream & CGenerator::generateBinary(std::wostream & _os, const Binary & _instr) {

    if (_instr.getBinaryKind() == Binary::JMZ || _instr.getBinaryKind() == Binary::JNZ) {
        _os << L"if (";

        if (_instr.getBinaryKind() == Binary::JMZ) {
            _os << L"! (";
            generateOperand(_os, _instr.getOp1());
            _os << L")";
        } else
            generateOperand(_os, _instr.getOp1());

        _os << L") goto ";
        generateOperand(_os, _instr.getOp2());

        return _os;
    } else if (_instr.getBinaryKind() == Binary::STORE) {
        _os << L"(* ";
        generateOperand(_os, _instr.getOp1());
        _os << L") = ";
        generateOperand(_os, _instr.getOp2());

        return _os;
    }

    switch (_instr.getBinaryKind()) {
        case Binary::SET:
            generateOperand(_os, _instr.getOp1());
            _os << L" = ";
            generateOperand(_os, _instr.getOp2());
            return _os;
    }

    std::wstring strInstr = L"!!!";

    _os << L"(";
//    _os << L"(";
    generateOperand(_os, _instr.getOp1());
//    _os << L")";

    switch (_instr.getBinaryKind()) {
        case Binary::ADD: strInstr = L"+"; break;
        case Binary::SUB: strInstr = L"-"; break;
        case Binary::MUL: strInstr = L"*"; break;
        case Binary::DIV: strInstr = L"/"; break;
        case Binary::REM: strInstr = L"%"; break;
        case Binary::SHL: strInstr = L"<<"; break;
        case Binary::SHR: strInstr = L">>"; break;
        case Binary::POW: assert(false && "Unimplemented"); break;
        case Binary::AND: strInstr = L"&"; break;
        case Binary::OR:  strInstr = L"|";  break;
        case Binary::XOR: strInstr = L"^"; break;

        case Binary::OFFSET: strInstr = L"+"; break;

        /*case Binary::BAnd: strInstr = L"band"; break;
        case Binary::BOr:  strInstr = L"bor";  break;
        case Binary::BXor: strInstr = L"bxor"; break;

        case Binary::FAdd: strInstr = L"fadd"; break;
        case Binary::FSub: strInstr = L"fsub"; break;
        case Binary::FMul: strInstr = L"fmul"; break;
        case Binary::FDiv: strInstr = L"fdiv"; break;
        case Binary::FPow: strInstr = L"fpow"; break;

        case Binary::ZAdd: strInstr = L"zadd"; break;
        case Binary::ZSub: strInstr = L"zsub"; break;
        case Binary::ZMul: strInstr = L"zmul"; break;
        case Binary::ZDiv: strInstr = L"zdiv"; break;
        case Binary::ZRem: strInstr = L"zrem"; break;
        case Binary::ZShl: strInstr = L"zshl"; break;
        case Binary::ZShr: strInstr = L"zshr"; break;
        case Binary::ZPow: strInstr = L"zpow"; break;
        case Binary::ZAnd: strInstr = L"zand"; break;
        case Binary::ZOr:  strInstr = L"zor";  break;
        case Binary::ZXor: strInstr = L"zxor"; break;

        case Binary::QAdd: strInstr = L"qadd"; break;
        case Binary::QSub: strInstr = L"qsub"; break;
        case Binary::QMul: strInstr = L"qmul"; break;
        case Binary::QDiv: strInstr = L"qdiv"; break;
        case Binary::QPow: strInstr = L"qpow"; break;*/

        case Binary::EQ:  strInstr = L"==";  break;
        case Binary::NE:  strInstr = L"!=";  break;
        case Binary::LT:  strInstr = L"<";  break;
        case Binary::LTE: strInstr = L"<="; break;
        case Binary::GT:  strInstr = L">";  break;
        case Binary::GTE: strInstr = L">="; break;

        /*case Binary::FEq:  strInstr = L"feq";  break;
        case Binary::FNe:  strInstr = L"fne";  break;
        case Binary::FLt:  strInstr = L"flt";  break;
        case Binary::FLte: strInstr = L"flte"; break;
        case Binary::FGt:  strInstr = L"fgt";  break;
        case Binary::FGte: strInstr = L"fgte"; break;

        case Binary::ZEq:  strInstr = L"zeq";  break;
        case Binary::ZNe:  strInstr = L"zne";  break;
        case Binary::ZLt:  strInstr = L"zlt";  break;
        case Binary::ZLte: strInstr = L"zlte"; break;
        case Binary::ZGt:  strInstr = L"zgt";  break;
        case Binary::ZGte: strInstr = L"zgte"; break;

        case Binary::QEq:  strInstr = L"qeq";  break;
        case Binary::QNe:  strInstr = L"qne";  break;
        case Binary::QLt:  strInstr = L"qlt";  break;
        case Binary::QLte: strInstr = L"qlte"; break;
        case Binary::QGt:  strInstr = L"qgt";  break;
        case Binary::QGte: strInstr = L"qgte"; break;*/

        /*case Binary::Store: strInstr = L"store"; break;
        case Binary::Jmz: strInstr = L"jmz"; break;
        case Binary::Jnz: strInstr = L"jnz"; break;
        case Binary::Offset: strInstr = L"offset"; break;*/
    }

    _os << L" " << strInstr << L" ";
    generateOperand(_os, _instr.getOp2());
    //_os << L")";
    _os << L")";

    return _os;
}

std::wostream & CGenerator::generateUnary(std::wostream & _os, const Unary & _instr) {
//    std::wstring strInstr = L"!!!";

    switch (_instr.getUnaryKind()) {
        case Unary::RETURN:
            _os << L"return ";
            generateOperand(_os, _instr.getOp());
            break;
        case Unary::PTR:
            _os << L"(& ";
            generateOperand(_os, _instr.getOp());
            _os << L")";
            break;
        case Unary::LOAD:
            _os << L"(* ";
            generateOperand(_os, _instr.getOp());
            _os << L")";
            break;
        case Unary::MALLOC:
            _os << L"__p_alloc(";
            generateOperand(_os, _instr.getOp());
            _os << L")";
            break;
        /*case Unary::Free: strInstr = L"free"; break;
        case Unary::Ref: strInstr = L"ref"; break;
        case Unary::Unref: strInstr = L"unref"; break;
        case Unary::UnrefNd: strInstr = L"unrefnd"; break;*/
        case Unary::JMP:
            _os << L"goto ";
            generateOperand(_os, _instr.getOp());
            break;
    }

    return _os;
}

std::wostream & CGenerator::generateCall(std::wostream & _os, const Call & _instr) {
    _os << L"(";
    generateOperand(_os, _instr.getFunction());
    _os << L") (";

    size_t cArgs = _instr.args().size();

    for (Operands::const_iterator iArg = _instr.args().begin(); iArg != _instr.args().end(); ++ iArg, -- cArgs) {
        generateOperand(_os, * iArg);
        if (cArgs > 1)
            _os << L", ";
    }

    _os << L")";

    return _os;
}

std::wostream & CGenerator::generateIf(std::wostream & _os, const If & _instr) {
    _os << L"if (";
    generateOperand(_os, _instr.getCondition()) << L") {\n";

    if (! _instr.brTrue().empty()) {
        Instructions::const_iterator iLast = ::prev(_instr.brTrue().end());
        ++ m_nLevel;
        for (Instructions::const_iterator iInstr = _instr.brTrue().begin(); iInstr != _instr.brTrue().end(); ++ iInstr)
            generateInstruction(_os, ** iInstr, false, iInstr == iLast);
        -- m_nLevel;
    }

    if (! _instr.brFalse().empty()) {
        _os << fmtIndent(L"} else {\n");
        Instructions::const_iterator iLast = ::prev(_instr.brFalse().end());
        ++ m_nLevel;
        for (Instructions::const_iterator iInstr = _instr.brFalse().begin(); iInstr != _instr.brFalse().end(); ++ iInstr)
            generateInstruction(_os, ** iInstr, false, iInstr == iLast);
        -- m_nLevel;
    }

    _os << fmtIndent(L"}");

    return _os;
}

std::wostream & CGenerator::generateWhile(std::wostream & _os, const While & _instr) {
    _os << L"while (";
    generateOperand(_os, _instr.getCondition()) << L") {\n";

    if (! _instr.getBlock().empty()) {
        Instructions::const_iterator iLast = ::prev(_instr.getBlock().end());
        ++ m_nLevel;
        for (Instructions::const_iterator iInstr = _instr.getBlock().begin(); iInstr != _instr.getBlock().end(); ++ iInstr)
            generateInstruction(_os, ** iInstr, false, iInstr == iLast);
        -- m_nLevel;
    }

    _os << fmtIndent(L"}");

    return _os;
}

std::wostream & CGenerator::generateType(std::wostream & _os, const Type & _type) {
    std::wstring strName = resolveType(& _type);

    if (! strName.empty())
        return _os << strName;

    switch (_type.getKind()) {
        case Type::VOID:    return _os << L"void";

        case Type::BOOL:    return _os << L"bool";
        //case Type::Char:    return _os << L"char";
        case Type::WCHAR:   return _os << L"wchar_t";

        case Type::INT8:    return _os << L"int8_t";
        case Type::INT16:   return _os << L"int16_t";
        case Type::INT32:   return _os << L"int32_t";
        case Type::INT64:   return _os << L"int64_t";
        case Type::UINT8:   return _os << L"uint8_t";
        case Type::UINT16:  return _os << L"uint16_t";
        case Type::UINT32:  return _os << L"uint32_t";
        case Type::UINT64:  return _os << L"uint64_t";
        /*case Type::Gmp_z:   return m_os << "gmpz";
        case Type::Gmp_q:   return m_os << "gmpq";
        case Type::String:  return m_os << "string";
        case Type::WString: return m_os << "wstring";
        case Type::Float:   return m_os << "float";
        case Type::Double:  return m_os << "double";
        case Type::Quad:    return m_os << "quad";*/

        case Type::STRUCT:
            return generateStructType(_os, (StructType &) _type);

        case Type::POINTER:
            generateType(_os, * ((PointerType &) _type).getBase());
            _os << L" *";
            return _os;
    }

    return _os << L"t" << _type.getKind();
}

std::wostream & CGenerator::generateField(std::wostream & _os, const Field & _instr) {
    _os << L"& ";
    generateOperand(_os, _instr.getOp()) << L"->f";
    _os << _instr.getIndex();
    return _os;
}

std::wostream & CGenerator::generateCast(std::wostream & _os, const Cast & _instr) {
    _os << L"(";
    generateType(_os, * _instr.getType());
    _os << L") ";
    generateOperand(_os, _instr.getOp());
    return _os;
}

std::wostream & CGenerator::generateCopy(std::wostream & _os, const Copy & _instr) {
    _os << L"memcpy(";
    generateOperand(_os, _instr.getDest());
    _os << L", ";
    generateOperand(_os, _instr.getSrc());
    _os << L", ";
    generateOperand(_os, _instr.getSize());
    _os << L")";
    return _os;
}

std::wostream & CGenerator::generateSwitch(std::wostream & _os, const Switch & _instr) {
    _os << L"switch (";
    generateOperand(_os, _instr.getArg()) << L") {\n";
    ++ m_nLevel;
    for (SwitchCases::const_iterator iCase = _instr.cases().begin(); iCase != _instr.cases().end(); ++ iCase) {
        for (size_t i = 0; i < iCase->values.size(); ++ i) {
            _os << fmtIndent(fmtInt(iCase->values[i], L"case %llu:"));
            _os << L"\n";
        }

        ++ m_nLevel;
        for (Instructions::const_iterator iInstr = iCase->body.begin(); iInstr != iCase->body.end(); ++ iInstr)
            generateInstruction(_os, ** iInstr, false, false);
        _os << fmtIndent(L"break;") << L"\n";
        -- m_nLevel;
    }

    if (! _instr.deflt().empty()) {
        _os << fmtIndent(L"default:\n");
        Instructions::const_iterator iLast = ::prev(_instr.deflt().end());
        ++ m_nLevel;
        for (Instructions::const_iterator iInstr = _instr.deflt().begin(); iInstr != _instr.deflt().end(); ++ iInstr)
            generateInstruction(_os, ** iInstr, false, iInstr == iLast);
        -- m_nLevel;
    }

    -- m_nLevel;

    _os << fmtIndent(L"}");

    return _os;
}

std::wostream & CGenerator::generateInstruction(std::wostream & _os,
        const Instruction & _instr, bool _bInline, bool _bLast)
{
    if (! _bInline && canInline(_instr)) {
        std::wstringstream ss;
        generateInstruction(ss, _instr, true, _bLast);
        m_inlinedInstrs[_instr.getResult().ptr()] = inline_instr_t(ss.str(), & _instr);
        return _os;
    }

    //m_pCurrentInstr = & _instr;

    bool bHasLabel = false;

    if (! _bInline) {
        bHasLabel = _instr.getLabel() && _instr.getLabel()->getUsageCount() > 0;
        if (bHasLabel)
            _os << resolveLabel(* _instr.getLabel()) << L":" << L" /* " << _instr.getLabel()->getUsageCount() << L" */" << std::endl;

        if (_instr.getKind() == Instruction::NOP && ! _bLast)
            return _os;

        _os << fmtIndent(L"");

        if (_instr.getResult()) {
            Variable * pVar = _instr.getResult().ptr();
            pVar->setUsed(true);
            std::wstring strName = resolveVariable(pVar);
            assert(! strName.empty());
            _os << strName << L" = ";
        }
    }

    switch (_instr.getKind()) {
        case Instruction::UNARY:
            generateUnary(_os, (Unary &) _instr);
            break;
        case Instruction::BINARY:
            generateBinary(_os, (Binary &) _instr);
            break;
        /*case Instruction::Select:
            generate((Select &) _instr);
            break;*/
        case Instruction::CALL:
            generateCall(_os, (Call &) _instr);
            break;
        case Instruction::IF:
            generateIf(_os, (If &) _instr);
            break;
        case Instruction::WHILE:
            generateWhile(_os, (While &) _instr);
            break;
        case Instruction::SWITCH:
            generateSwitch(_os, (Switch &) _instr);
            break;
        case Instruction::FIELD:
            generateField(_os, (Field &) _instr);
            break;
        case Instruction::CAST:
            generateCast(_os, (Cast &) _instr);
            break;
        case Instruction::COPY:
            generateCopy(_os, (Copy &) _instr);
            break;
        /*default:
            m_os << L"nop";*/
    }

    /*if (! _instr.getResult().empty() && ! _instr.getResult()->getType().empty()) {
        m_os << "    ## ";
        generate(* _instr.getResult()->getType());
    }*/

    //m_os << fmtIndent(fmtInt(_instr.getKind()));
    if (! _bInline)
        _os << ";\n";

    return _os;
}

std::wostream & CGenerator::generateFunction(std::wostream & _os, const Function & _function) {
    if (_function.getResult()) {
        getChild()->generateType(_os, * _function.getReturnType());
        _os << std::endl;

/*        getChild()->m_vars[_function.getResult().ptr()] = L"r";
        m_os << getChild()->fmtIndent(L".result %r ");
        //const size_t c = sizeof(Type);
        getChild()->generate(* _function.getReturnType()) << "\n"; */
    } else {
        _os << L"void" << std::endl;
    }

    _os << _function.getName();
    _os << L"(" << std::endl;

    addChild();

    size_t cArgs = _function.args().size();

    for (Args::const_iterator iArg = _function.args().begin(); iArg != _function.args().end(); ++ iArg, -- cArgs) {
        std::wstring strName = fmtInt(getChild()->m_nParam ++, L"a%llu");
        getChild()->m_args[iArg->ptr()] = strName;

        _os << getChild()->fmtIndent(L"");
        getChild()->generateType(_os, * (* iArg)->getType()) << " ";
        _os << strName;
        if (cArgs > 1)
            _os << L",";
        _os << "\n";
    }

    _os << L") {" << std::endl;

    if (_function.getResult() && _function.getResult()->getType()->getKind() != Type::VOID) {
        getChild()->m_vars[_function.getResult().ptr()] = L"r";

        /*_os << getChild()->fmtIndent(L"");
        getChild()->generate(_os, * _function.getReturnType()) << L" ";
        _os << L"r;" << std::endl;*/
    }

    int nVar = 0;

    for (Args::const_iterator iVar = _function.locals().begin(); iVar != _function.locals().end(); ++ iVar) {
        std::wstring strName = fmtInt(nVar ++, L"u%llu");
        getChild()->m_vars[iVar->ptr()] = strName;
        (* iVar)->setUsed(false);
    }

    std::wstringstream ss;

    if (! _function.instructions().empty()) {
        Instructions::const_iterator iLast = ::prev(_function.instructions().end());
        for (Instructions::const_iterator iInstr = _function.instructions().begin(); iInstr != _function.instructions().end(); ++ iInstr)
            getChild()->generateInstruction(ss, ** iInstr, false, iInstr == iLast);
    }

    for (name_map_t::iterator iVar = getChild()->m_vars.begin(); iVar != getChild()->m_vars.end(); ++ iVar) {
        const Variable * pVar = (const Variable *) iVar->first;

        if (pVar->isUsed()) {
            _os << getChild()->fmtIndent(L"");
            getChild()->generateType(_os, * pVar->getType()) << " ";
            _os << iVar->second << L";" << std::endl;
        }
    }
/*
    nVar = 0;

    for (Args::const_iterator iVar = _function.locals().begin(); iVar != _function.locals().end(); ++ iVar) {
        if (! (* iVar)->isUsed())
            continue;

        std::wstring strName = fmtInt(nVar ++, L"u%llu");
        _os << getChild()->fmtIndent(L"");
        getChild()->generate(_os, * (* iVar)->getType()) << " ";
        _os << strName << L";" << std::endl;
    }
*/
    _os << std::endl << ss.str() << L"}" << std::endl;

    /*m_os << L"\n" << fmtIndent(L".function ");
    m_os << _function.getName();
    m_os <<  " {\n";
    m_vars[& _function] = _function.getName();

    addChild();

    if (! _function.getResult().empty()) {
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

    m_os << fmtIndent(L"}\n");*/

    return _os;
}

std::wostream & CGenerator::generateTypeDef(std::wostream & _os, const Type & _type) {
    std::wstring strName = fmtInt(m_types.size(), L"td%llu");

    _os << L"typedef ";
    generateType(_os, _type);
    _os << L" " << strName << L";\n\n";

    m_types[& _type] = strName;

    return _os;
}

std::wostream & CGenerator::generateStructType(std::wostream & _os, const StructType & _struct) {
    _os << L"struct {\n";

    size_t cField = 0;

    for (Types::const_iterator iType = _struct.fieldTypes().begin(); iType != _struct.fieldTypes().end(); ++ iType, ++ cField) {
        _os << fmtIndent(L"");
        generateType(_os, ** iType);
        _os << L" " << fmtInt(cField, L"f%llu") << L";\n";
    }

    _os << L"}";

    return _os;
}


std::wostream & CGenerator::generateConst(std::wostream & _os, const Constant & _const) {
    std::wstring strName = fmtInt(m_nConst ++, L"c%llu");

    if (_const.getLiteral()->getKind() == Literal::STRING) {
        _os << L"__P_STRING_CONST(" << strName << L", "
                << _const.getLiteral()->getString().size() <<  L", "
                << "L\"" << _const.getLiteral()->getString() << L"\")";
    } else {
        _os << L"const ";
        generateType(_os, * _const.getType());
        _os << L" " << strName;

        _os << L" = ";

        generateLiteral(_os, * _const.getLiteral());
    }

    /*if (_const.getType()->getKind() == Type::WChar) {
        //_os << _const.getLiteral()->getWString().size() << L" ";
        _os << "L\"" << _const.getLiteral()->getWString() << L'"';
    }*/

    _os << ";\n";
    m_vars[& _const] = strName;

    return _os;
}

void generateC(const Module & _module, std::wostream & _os) {
    CGenerator gen;

    gen.generateModule(_os, _module);
}

};
