/// \file backend_c.cpp
///


#include <iostream>
#include <sstream>
#include <string>
#include <map>

#include "utils.h"
#include "llir.h"
#include "backend_c.h"

namespace backend {

class CCGenerator {
public:
    CCGenerator() :
        m_pParent(NULL), /*m_os(_os),*/ m_nLevel(0), m_pChild(NULL), m_nParam(0),
        m_nVar(0), m_nConst(0), m_nLabel(0) {}
    CCGenerator(CCGenerator & _parent) :
        m_pParent(& _parent), /*m_os(_parent.m_os),*/ m_nLevel(_parent.m_nLevel + 1),
        m_pChild(NULL), m_nParam(0), m_nVar(0), m_nConst(0), m_nLabel(0) {}
    ~CCGenerator();

    CCGenerator * addChild();
    CCGenerator * getChild();

    std::wostream & generate(std::wostream & _os, const llir::CModule & _module);
    std::wostream & generate(std::wostream & _os, const llir::CFunction & _function);
    std::wostream & generate(std::wostream & _os, const llir::CType & _type);
    std::wostream & generate(std::wostream & _os, const llir::CStructType & _struct);
    std::wostream & generateTypeDef(std::wostream & _os, const llir::CType & _type);
    std::wostream & generate(std::wostream & _os, const llir::CConstant & _const);

    std::wostream & generate(std::wostream & _os, const llir::CInstruction & _instr, bool _bInline, bool _bLast);
    std::wostream & generate(std::wostream & _os, const llir::CBinary & _instr);
    std::wostream & generate(std::wostream & _os, const llir::CUnary & _instr);
    std::wostream & generate(std::wostream & _os, const llir::CLiteral & _lit);
    std::wostream & generate(std::wostream & _os, const llir::COperand & _op);
    std::wostream & generate(std::wostream & _os, const llir::CIf & _instr);
    std::wostream & generate(std::wostream & _os, const llir::CCall & _instr);
    std::wostream & generate(std::wostream & _os, const llir::CField & _instr);
    std::wostream & generate(std::wostream & _os, const llir::CCast & _instr);
    std::wostream & generate(std::wostream & _os, const llir::CCopy & _instr);
    std::wostream & generate(std::wostream & _os, const llir::CSwitch & _instr);

    std::wstring resolveType(const void * _p) { return resolveName(& CCGenerator::m_types, _p); }

    std::wstring resolveVariable(const void * _p) {
        std::wstring s = resolveName(& CCGenerator::m_args, _p);
        return s.empty() ? resolveName(& CCGenerator::m_vars, _p) : s;
    }

    std::wstring resolveLabel(const llir::CLabel & _label);

    bool canInline(const llir::CInstruction & _instr) const;
    //const llir::CInstruction * getInlineByResult(const Auto<llir::CVariable> & _var) const;

private:
    CCGenerator * m_pParent;
    //std::wostream & m_os;
    int m_nLevel;
    CCGenerator * m_pChild;
    int m_nParam;
    int m_nVar;
    int m_nConst;
    int m_nLabel;

    typedef std::map<const void *, std::wstring> name_map_t;
    name_map_t m_args;
    name_map_t m_vars;
    name_map_t m_types;
    name_map_t m_labels;

    typedef std::pair<std::wstring, const llir::CInstruction *> inline_instr_t;
    typedef std::map<const llir::CVariable *, inline_instr_t> inline_map_t;
    inline_map_t m_inlinedInstrs;


    std::wstring fmtIndent(const std::wstring & _s) {
        std::wstring res;

        for (int i = 0; i < m_nLevel; ++ i)
            res += L"  ";

        return res + _s;
    }

    std::wstring resolveName(name_map_t CCGenerator::* _map, const void * _p);
};

CCGenerator::~CCGenerator() {
    if (m_pChild)
        delete m_pChild;
}

CCGenerator * CCGenerator::getChild() {
    if (! m_pChild)
        return addChild();
    return m_pChild;
}

CCGenerator * CCGenerator::addChild() {
    if (m_pChild)
        delete m_pChild;
    m_pChild = new CCGenerator(* this);
    return m_pChild;
}

std::wstring CCGenerator::resolveName(name_map_t CCGenerator::* _map, const void * _p) {
    name_map_t::iterator iName = (this->*_map).find(_p);

    if (iName != (this->*_map).end())
        return iName->second;
    else if (m_pParent)
        return m_pParent->resolveName(_map, _p);
    else
        return L"";
}

std::wstring CCGenerator::resolveLabel(const llir::CLabel & _label) {
    if (m_pParent)
        return m_pParent->resolveLabel(_label);

    std::wstring strName = resolveName(& CCGenerator::m_labels, & _label);

    if (strName.empty()) {
        strName = fmtInt(m_nLabel ++, L"___l%llu");
        m_labels[& _label] = strName;
    }

    return strName;
}

bool CCGenerator::canInline(const llir::CInstruction & _instr) const {
    if (! _instr.getLabel().empty() && _instr.getLabel()->getUsageCount() > 0)
        return false;

    if (_instr.getKind() == llir::CInstruction::Unary) {
        switch (((const llir::CUnary &) _instr).getUnaryKind()) {
            case llir::CUnary::Ptr:
            case llir::CUnary::Load:
                return true;
            default:
                return false;
        }
    }

    if (_instr.getResultUsageCount() != 1)
        return false;

    switch (_instr.getKind()) {
        case llir::CInstruction::Binary:
        case llir::CInstruction::Select:
        case llir::CInstruction::Field:
        case llir::CInstruction::Cast:
            return true;
    }

    return false;
}

/*const llir::CInstruction * CCGenerator::getInlineByResult(const Auto<llir::CVariable> & _var) const {
    inline_map_t::const_iterator iInstr = m_inlinedInstrs.find(_var.ptr());

    return iInstr != m_inlinedInstrs.end() ? iInstr->second : NULL;
}*/

std::wostream & CCGenerator::generate(std::wostream & _os, const llir::CModule & _module) {

    for (llir::functions_t::const_iterator iFunc = _module.functions().begin(); iFunc != _module.functions().end(); ++ iFunc)
        m_vars[iFunc->ptr()] = (* iFunc)->getName();

    for (llir::functions_t::const_iterator iFunc = _module.usedBuiltins().begin(); iFunc != _module.usedBuiltins().end(); ++ iFunc)
        m_vars[iFunc->ptr()] = (* iFunc)->getName();

    _os << "/* Auto-generated by PLang compiler */\n";
    _os << "#include <plang.h>\n";
    _os << "\n";

    /*for (llir::consts_t::const_iterator iConst = _module.consts().begin(); iConst != _module.consts().end(); ++ iConst)
        getChild()->generate(** iConst);*/

    for (llir::consts_t::const_iterator iConst = _module.consts().begin(); iConst != _module.consts().end(); ++ iConst)
        getChild()->generate(_os, ** iConst);

    _os << "\n";

    for (llir::types_t::const_iterator iType = _module.types().begin(); iType != _module.types().end(); ++ iType) {
        getChild()->generateTypeDef(_os, ** iType);
        _os << "\n";
    }

    for (llir::functions_t::const_iterator iFunc = _module.functions().begin(); iFunc != _module.functions().end(); ++ iFunc) {
        getChild()->generate(_os, ** iFunc);
        _os << "\n";
    }

    //m_os << "}\n";

    return _os;
}

std::wostream & CCGenerator::generate(std::wostream & _os, const llir::CLiteral & _lit) {
    switch (_lit.getKind()) {
        case llir::CLiteral::Number:
            return _os << L"(" << _lit.getNumber().toString() << L")";
        case llir::CLiteral::Bool:
            return _os << (_lit.getBool() ? "true" : "false");
        /*case llir::CType::Char:
            return _os << "\'" << _lit.getChar() << "\'";*/
        case llir::CLiteral::Char:
            return _os << "L\'" << _lit.getChar() << "\'";
        case llir::CLiteral::String:
            return _os << "\"" << _lit.getString().c_str() << "\"";
        /*case llir::CType::WString:
            return _os << "L\"" << _lit.getWString() << "\"";*/
    }

    return _os;
}

std::wostream & CCGenerator::generate(std::wostream & _os, const llir::COperand & _op) {
    switch (_op.getKind()) {
        case llir::COperand::Literal:
            return generate(_os, _op.getLiteral());
        case llir::COperand::Variable: {
            inline_map_t::iterator iInstr = m_inlinedInstrs.find(_op.getVariable().ptr());
            //const llir::CInstruction * pInstr = getInlineByResult(_op.getVariable().ptr());

            if (iInstr != m_inlinedInstrs.end()) {
                _os /*<< L"("*/ << iInstr->second.first /*<< L")"*/;
                //const llir::CInstruction * pInstr = iInstr->second;
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
        case llir::COperand::Label:
            return _os << resolveLabel(* _op.getLabel());
    }

    return _os;
}

std::wostream & CCGenerator::generate(std::wostream & _os, const llir::CBinary & _instr) {

    if (_instr.getBinaryKind() == llir::CBinary::Jmz || _instr.getBinaryKind() == llir::CBinary::Jnz) {
        _os << L"if (";

        if (_instr.getBinaryKind() == llir::CBinary::Jmz) {
            _os << L"! (";
            generate(_os, _instr.getOp1());
            _os << L")";
        } else
            generate(_os, _instr.getOp1());

        _os << L") goto ";
        generate(_os, _instr.getOp2());

        return _os;
    } else if (_instr.getBinaryKind() == llir::CBinary::Store) {
        _os << L"(* ";
        generate(_os, _instr.getOp1());
        _os << L") = ";
        generate(_os, _instr.getOp2());

        return _os;
    }

    switch (_instr.getBinaryKind()) {
        case llir::CBinary::Set:
            generate(_os, _instr.getOp1());
            _os << L" = ";
            generate(_os, _instr.getOp2());
            return _os;
    }

    std::wstring strInstr = L"!!!";

    _os << L"(";
//    _os << L"(";
    generate(_os, _instr.getOp1());
//    _os << L")";

    switch (_instr.getBinaryKind()) {
        case llir::CBinary::Add: strInstr = L"+"; break;
        case llir::CBinary::Sub: strInstr = L"-"; break;
        case llir::CBinary::Mul: strInstr = L"*"; break;
        case llir::CBinary::Div: strInstr = L"/"; break;
        case llir::CBinary::Rem: strInstr = L"%"; break;
        case llir::CBinary::Shl: strInstr = L"<<"; break;
        case llir::CBinary::Shr: strInstr = L">>"; break;
        case llir::CBinary::Pow: assert(false && "Unimplemented"); break;
        case llir::CBinary::And: strInstr = L"&"; break;
        case llir::CBinary::Or:  strInstr = L"|";  break;
        case llir::CBinary::Xor: strInstr = L"^"; break;

        case llir::CBinary::Offset: strInstr = L"+"; break;

        /*case llir::CBinary::BAnd: strInstr = L"band"; break;
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
        case llir::CBinary::QPow: strInstr = L"qpow"; break;*/

        case llir::CBinary::Eq:  strInstr = L"==";  break;
        case llir::CBinary::Ne:  strInstr = L"!=";  break;
        case llir::CBinary::Lt:  strInstr = L"<";  break;
        case llir::CBinary::Lte: strInstr = L"<="; break;
        case llir::CBinary::Gt:  strInstr = L">";  break;
        case llir::CBinary::Gte: strInstr = L">="; break;

        /*case llir::CBinary::FEq:  strInstr = L"feq";  break;
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
        case llir::CBinary::QGte: strInstr = L"qgte"; break;*/

        /*case llir::CBinary::Store: strInstr = L"store"; break;
        case llir::CBinary::Jmz: strInstr = L"jmz"; break;
        case llir::CBinary::Jnz: strInstr = L"jnz"; break;
        case llir::CBinary::Offset: strInstr = L"offset"; break;*/
    }

    _os << L" " << strInstr << L" ";
    generate(_os, _instr.getOp2());
    //_os << L")";
    _os << L")";

    return _os;
}

std::wostream & CCGenerator::generate(std::wostream & _os, const llir::CUnary & _instr) {
//    std::wstring strInstr = L"!!!";

    switch (_instr.getUnaryKind()) {
        case llir::CUnary::Return:
            _os << L"return ";
            generate(_os, _instr.getOp());
            break;
        case llir::CUnary::Ptr:
            _os << L"(& ";
            generate(_os, _instr.getOp());
            _os << L")";
            break;
        case llir::CUnary::Load:
            _os << L"(* ";
            generate(_os, _instr.getOp());
            _os << L")";
            break;
        case llir::CUnary::Malloc:
            _os << L"__p_alloc(";
            generate(_os, _instr.getOp());
            _os << L")";
            break;
        /*case llir::CUnary::Free: strInstr = L"free"; break;
        case llir::CUnary::Ref: strInstr = L"ref"; break;
        case llir::CUnary::Unref: strInstr = L"unref"; break;
        case llir::CUnary::UnrefNd: strInstr = L"unrefnd"; break;*/
        case llir::CUnary::Jmp:
            _os << L"goto ";
            generate(_os, _instr.getOp());
            break;
    }

    return _os;
}

std::wostream & CCGenerator::generate(std::wostream & _os, const llir::CCall & _instr) {
    _os << L"(";
    generate(_os, _instr.getFunction());
    _os << L") (";

    size_t cArgs = _instr.args().size();

    for (llir::operands_t::const_iterator iArg = _instr.args().begin(); iArg != _instr.args().end(); ++ iArg, -- cArgs) {
        generate(_os, * iArg);
        if (cArgs > 1)
            _os << L", ";
    }

    _os << L")";

    return _os;
}

std::wostream & CCGenerator::generate(std::wostream & _os, const llir::CIf & _instr) {
    _os << L"if (";
    generate(_os, _instr.getCondition()) << L") {\n";

    if (! _instr.brTrue().empty()) {
        llir::instructions_t::const_iterator iLast = prev(_instr.brTrue().end());
        ++ m_nLevel;
        for (llir::instructions_t::const_iterator iInstr = _instr.brTrue().begin(); iInstr != _instr.brTrue().end(); ++ iInstr)
            generate(_os, ** iInstr, false, iInstr == iLast);
        -- m_nLevel;
    }

    if (! _instr.brFalse().empty()) {
        _os << fmtIndent(L"} else {\n");
        llir::instructions_t::const_iterator iLast = prev(_instr.brFalse().end());
        ++ m_nLevel;
        for (llir::instructions_t::const_iterator iInstr = _instr.brFalse().begin(); iInstr != _instr.brFalse().end(); ++ iInstr)
            generate(_os, ** iInstr, false, iInstr == iLast);
        -- m_nLevel;
    }

    _os << fmtIndent(L"}");

    return _os;
}

std::wostream & CCGenerator::generate(std::wostream & _os, const llir::CType & _type) {
    std::wstring strName = resolveType(& _type);

    if (! strName.empty())
        return _os << strName;

    switch (_type.getKind()) {
        case llir::CType::Void:    return _os << L"void";

        case llir::CType::Bool:    return _os << L"bool";
        //case llir::CType::Char:    return _os << L"char";
        case llir::CType::WChar:   return _os << L"wchar_t";

        case llir::CType::Int8:    return _os << L"int8_t";
        case llir::CType::Int16:   return _os << L"int16_t";
        case llir::CType::Int32:   return _os << L"int32_t";
        case llir::CType::Int64:   return _os << L"int64_t";
        case llir::CType::UInt8:   return _os << L"uint8_t";
        case llir::CType::UInt16:  return _os << L"uint16_t";
        case llir::CType::UInt32:  return _os << L"uint32_t";
        case llir::CType::UInt64:  return _os << L"uint64_t";
        /*case llir::CType::Gmp_z:   return m_os << "gmpz";
        case llir::CType::Gmp_q:   return m_os << "gmpq";
        case llir::CType::String:  return m_os << "string";
        case llir::CType::WString: return m_os << "wstring";
        case llir::CType::Float:   return m_os << "float";
        case llir::CType::Double:  return m_os << "double";
        case llir::CType::Quad:    return m_os << "quad";*/

        case llir::CType::Struct:
            return generate(_os, (llir::CStructType &) _type);

        case llir::CType::Pointer:
            generate(_os, * ((llir::CPointerType &) _type).getBase());
            _os << L" *";
            return _os;
    }

    return _os << L"t" << _type.getKind();
}

std::wostream & CCGenerator::generate(std::wostream & _os, const llir::CField & _instr) {
    _os << L"& ";
    generate(_os, _instr.getOp()) << L"->f";
    _os << _instr.getIndex();
    return _os;
}

std::wostream & CCGenerator::generate(std::wostream & _os, const llir::CCast & _instr) {
    _os << L"(";
    generate(_os, * _instr.getType());
    _os << L") ";
    generate(_os, _instr.getOp());
    return _os;
}

std::wostream & CCGenerator::generate(std::wostream & _os, const llir::CCopy & _instr) {
    _os << L"memcpy(";
    generate(_os, _instr.getDest());
    _os << L", ";
    generate(_os, _instr.getSrc());
    _os << L", ";
    generate(_os, _instr.getSize());
    _os << L")";
    return _os;
}

std::wostream & CCGenerator::generate(std::wostream & _os, const llir::CSwitch & _instr) {
    _os << L"switch (";
    generate(_os, _instr.getArg()) << L") {\n";
    ++ m_nLevel;
    for (llir::switch_cases_t::const_iterator iCase = _instr.cases().begin(); iCase != _instr.cases().end(); ++ iCase) {
        for (size_t i = 0; i < iCase->values.size(); ++ i) {
            _os << fmtIndent(fmtInt(iCase->values[i], L"case %llu:"));
            _os << L"\n";
        }

        ++ m_nLevel;
        for (llir::instructions_t::const_iterator iInstr = iCase->body.begin(); iInstr != iCase->body.end(); ++ iInstr)
            generate(_os, ** iInstr, false, false);
        _os << fmtIndent(L"break;") << L"\n";
        -- m_nLevel;
    }

    if (! _instr.deflt().empty()) {
        _os << fmtIndent(L"default:\n");
        llir::instructions_t::const_iterator iLast = prev(_instr.deflt().end());
        ++ m_nLevel;
        for (llir::instructions_t::const_iterator iInstr = _instr.deflt().begin(); iInstr != _instr.deflt().end(); ++ iInstr)
            generate(_os, ** iInstr, false, iInstr == iLast);
        -- m_nLevel;
    }

    -- m_nLevel;

    _os << fmtIndent(L"}");

    return _os;
}

std::wostream & CCGenerator::generate(std::wostream & _os,
        const llir::CInstruction & _instr, bool _bInline, bool _bLast)
{
    if (! _bInline && canInline(_instr)) {
        std::wstringstream ss;
        generate(ss, _instr, true, _bLast);
        m_inlinedInstrs[_instr.getResult().ptr()] = inline_instr_t(ss.str(), & _instr);
        return _os;
    }

    //m_pCurrentInstr = & _instr;

    bool bHasLabel = false;

    if (! _bInline) {
        bHasLabel = ! _instr.getLabel().empty() && _instr.getLabel()->getUsageCount() > 0;
        if (bHasLabel)
            _os << resolveLabel(* _instr.getLabel()) << L":" << L" /* " << _instr.getLabel()->getUsageCount() << L" */" << std::endl;

        if (_instr.getKind() == llir::CInstruction::Nop && ! _bLast)
            return _os;

        _os << fmtIndent(L"");

        if (! _instr.getResult().empty()) {
            llir::CVariable * pVar = _instr.getResult().ptr();
            pVar->setUsed(true);
            std::wstring strName = resolveVariable(pVar);
            assert(! strName.empty());
            _os << strName << L" = ";
        }
    }

    switch (_instr.getKind()) {
        case llir::CInstruction::Unary:
            generate(_os, (llir::CUnary &) _instr);
            break;
        case llir::CInstruction::Binary:
            generate(_os, (llir::CBinary &) _instr);
            break;
        /*case llir::CInstruction::Select:
            generate((llir::CSelect &) _instr);
            break;*/
        case llir::CInstruction::Call:
            generate(_os, (llir::CCall &) _instr);
            break;
        case llir::CInstruction::If:
            generate(_os, (llir::CIf &) _instr);
            break;
        case llir::CInstruction::Switch:
            generate(_os, (llir::CSwitch &) _instr);
            break;
        case llir::CInstruction::Field:
            generate(_os, (llir::CField &) _instr);
            break;
        case llir::CInstruction::Cast:
            generate(_os, (llir::CCast &) _instr);
            break;
        case llir::CInstruction::Copy:
            generate(_os, (llir::CCopy &) _instr);
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

std::wostream & CCGenerator::generate(std::wostream & _os, const llir::CFunction & _function) {
    if (! _function.getResult().empty()) {
        getChild()->generate(_os, * _function.getReturnType());
        _os << std::endl;

/*        getChild()->m_vars[_function.getResult().ptr()] = L"r";
        m_os << getChild()->fmtIndent(L".result %r ");
        //const size_t c = sizeof(llir::CType);
        getChild()->generate(* _function.getReturnType()) << "\n"; */
    } else {
        _os << L"void" << std::endl;
    }

    _os << _function.getName();
    _os << L"(" << std::endl;

    addChild();

    size_t cArgs = _function.args().size();

    for (llir::args_t::const_iterator iArg = _function.args().begin(); iArg != _function.args().end(); ++ iArg, -- cArgs) {
        std::wstring strName = fmtInt(getChild()->m_nParam ++, L"a%llu");
        getChild()->m_args[iArg->ptr()] = strName;

        _os << getChild()->fmtIndent(L"");
        getChild()->generate(_os, * (* iArg)->getType()) << " ";
        _os << strName;
        if (cArgs > 1)
            _os << L",";
        _os << "\n";
    }

    _os << L") {" << std::endl;

    if (! _function.getResult().empty() && _function.getResult()->getType()->getKind() != llir::CType::Void) {
        getChild()->m_vars[_function.getResult().ptr()] = L"r";

        /*_os << getChild()->fmtIndent(L"");
        getChild()->generate(_os, * _function.getReturnType()) << L" ";
        _os << L"r;" << std::endl;*/
    }

    int nVar = 0;

    for (llir::args_t::const_iterator iVar = _function.locals().begin(); iVar != _function.locals().end(); ++ iVar) {
        std::wstring strName = fmtInt(nVar ++, L"u%llu");
        getChild()->m_vars[iVar->ptr()] = strName;
        (* iVar)->setUsed(false);
    }

    std::wstringstream ss;

    if (! _function.instructions().empty()) {
        llir::instructions_t::const_iterator iLast = prev(_function.instructions().end());
        for (llir::instructions_t::const_iterator iInstr = _function.instructions().begin(); iInstr != _function.instructions().end(); ++ iInstr)
            getChild()->generate(ss, ** iInstr, false, iInstr == iLast);
    }

    for (name_map_t::iterator iVar = getChild()->m_vars.begin(); iVar != getChild()->m_vars.end(); ++ iVar) {
        const llir::CVariable * pVar = (const llir::CVariable *) iVar->first;

        if (pVar->isUsed()) {
            _os << getChild()->fmtIndent(L"");
            getChild()->generate(_os, * pVar->getType()) << " ";
            _os << iVar->second << L";" << std::endl;
        }
    }
/*
    nVar = 0;

    for (llir::args_t::const_iterator iVar = _function.locals().begin(); iVar != _function.locals().end(); ++ iVar) {
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

    m_os << fmtIndent(L"}\n");*/

    return _os;
}

std::wostream & CCGenerator::generateTypeDef(std::wostream & _os, const llir::CType & _type) {
    std::wstring strName = fmtInt(m_types.size(), L"td%llu");

    _os << L"typedef ";
    generate(_os, _type);
    _os << L" " << strName << L";\n\n";

    m_types[& _type] = strName;

    return _os;
}

std::wostream & CCGenerator::generate(std::wostream & _os, const llir::CStructType & _struct) {
    _os << L"struct {\n";

    size_t cField = 0;

    for (llir::types_t::const_iterator iType = _struct.fieldTypes().begin(); iType != _struct.fieldTypes().end(); ++ iType, ++ cField) {
        _os << fmtIndent(L"");
        generate(_os, ** iType);
        _os << L" " << fmtInt(cField, L"f%llu") << L";\n";
    }

    _os << L"}";

    return _os;
}


std::wostream & CCGenerator::generate(std::wostream & _os, const llir::CConstant & _const) {
    std::wstring strName = fmtInt(m_nConst ++, L"c%llu");

    if (_const.getLiteral()->getKind() == llir::CLiteral::String) {
        _os << L"__P_STRING_CONST(" << strName << L", "
                << _const.getLiteral()->getString().size() <<  L", "
                << "L\"" << _const.getLiteral()->getString() << L"\")";
    } else {
        _os << L"const ";
        generate(_os, * _const.getType());
        _os << L" " << strName;

        _os << L" = ";

        generate(_os, * _const.getLiteral());
    }

    /*if (_const.getType()->getKind() == llir::CType::WChar) {
        //_os << _const.getLiteral()->getWString().size() << L" ";
        _os << "L\"" << _const.getLiteral()->getWString() << L'"';
    }*/

    _os << ";\n";
    m_vars[& _const] = strName;

    return _os;
}

void generateC(const llir::CModule & _module, std::wostream & _os) {
    CCGenerator gen;

    gen.generate(_os, _module);
}

};
