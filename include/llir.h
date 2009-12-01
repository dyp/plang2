#ifndef LLIR_H_
#define LLIR_H_

#include <string>
#include <list>

#include <assert.h>
#include <gmp.h>

#include "ir/declarations.h"
#include "utils.h"

namespace llir {

template<typename T>
inline void freeList(std::list<T *> & _list) {
    for (typename std::list<T *>::iterator iItem = _list.begin(); iItem != _list.end(); ++ iItem)
        delete * iItem;
    _list.clear();
}

class CType {
public:
    enum {
        Undefined,

        // Primitive types.
        Void,

        Gmp_z, Gmp_q,

        String, WString,

        Function,

        Struct,

        // Integer types.
        IntMask = 0x01000000,
        Int8, Int16, Int32, Int64, UInt8, UInt16, UInt32, UInt64,
        Bool, Char, WChar,

        // Floating point types.
        FloatMask = 0x02000000,
        Float, Double, Quad,

        Pointer,
    };

    CType() : m_kind(Undefined) {}

    CType(int _kind) : m_kind(_kind) {}

    virtual int getKind() const { return m_kind; }

    static size_t sizeOf(int _kind) {
        switch (_kind) {
            case Gmp_z:
                return sizeof(mpz_t);
            case Gmp_q:
                return sizeof(mpq_t);
            case Int8:
            case UInt8:
            case Bool:
            case Char:
                return 1;
            case Int16:
            case UInt16:
                return 2;
            case Int32:
            case UInt32:
                return 4;
            case Int64:
            case UInt64:
                return 8;
            case WChar:
                return 4;
            case Float:
                return 4;
            case Double:
                return 8;
            case Quad:
                return 16;
            case Pointer:
            case String:
            case WString:
            case Function:
                return sizeof(void *);
            default:
                return 0;
        }
    }

    virtual size_t sizeOf() const { return sizeOf(m_kind); }

private:
    int m_kind;
};

typedef std::vector<Auto<CType> > types_t;

class CFunctionType : public CType {
public:
    CFunctionType(const Auto<CType> & _returnType) : CType(Function), m_returnType(_returnType) {}

    const Auto<CType> & getReturnType() const { return m_returnType; }

    types_t & argTypes() { return m_argTypes; }

private:
    Auto<CType> m_returnType;
    types_t m_argTypes;
};

class CStructType : public CType {
public:
    CStructType() : CType(Struct), m_strName(L"") {}
    CStructType(const std::wstring & _strName) : CType(Struct), m_strName(_strName) {}

    const std::wstring & getName() const { return m_strName; }

    types_t & fieldTypes() { return m_fieldTypes; }
    const types_t & fieldTypes() const { return m_fieldTypes; }

    virtual size_t sizeOf() const {
        // assume 4-byte field alignment.
        size_t cSize = 0;
        for (llir::types_t::const_iterator iType = m_fieldTypes.begin(); iType != m_fieldTypes.end(); ++ iType) {
            const size_t cFieldSize = (* iType)->sizeOf();
            cSize += cFieldSize%4 == 0 ? cFieldSize : (cFieldSize/4 + 1)*4;
        }
        return std::max((size_t) 1, cSize);
    }

private:
    types_t m_fieldTypes;
    std::wstring m_strName;
};

class CPointerType : public CType {
public:
    CPointerType(const Auto<CType> & _baseType) : CType(Pointer), m_baseType(_baseType) {}
    const Auto<CType> & getBase() const { return m_baseType; }

private:
    Auto<CType> m_baseType;
};

class CInstruction;

class CVariable {
public:
    CVariable() {}
    CVariable(const Auto<CType> & _type) : m_type(_type) {}

    const Auto<CType> & getType() const { return m_type; }
    Auto<CInstruction> getLastUse() const { return m_lastUse; }
    void setLastUse(const Auto<CInstruction> _instr) { m_lastUse = _instr; }
//    bool isValid() const { return m_type->getKind() != CType::Undefined; }

protected:
    Auto<CType> m_type;
    Auto<CInstruction> m_lastUse;
};

class CLiteral;

typedef std::list<Auto<CLiteral> > literals_t;

class CLiteral {
public:
    CLiteral() {}
    CLiteral(const Auto<CType> & _type) : m_type(_type) {}
    CLiteral(int _kind, const CNumber & _number) : m_type(new CType(_kind)), m_number(_number) {}

    const Auto<CType> & getType() const { return m_type; }

    const CNumber & getNumber() const { return m_number; }

    void setNumber(const CNumber & _number) {
        //m_type = new CType(CType::Int32);
        assert(! m_type.empty());
        m_number = _number;
    }

    wchar_t getBool() const { return m_number.getInt() != 0; }
    void setBool(bool _b) { _setNumber(CType::Bool, _b ? 1 : 0); }

    wchar_t getWChar() const { return (wchar_t) m_number.getInt(); }
    void setWChar(wchar_t _c) { _setNumber(CType::WChar, _c); }

    wchar_t getChar() const { return (char) m_number.getInt(); }
    void setChar(char _c) { _setNumber(CType::Char, _c); }

    const std::wstring & getWString() const { return m_wstring; }
    void setWString(const std::wstring & _str) {
        m_type = new CType(CType::WString);
        m_wstring = _str;
    }

    const std::string & getString() const { return m_string; }
    void setString(const std::string & _str) {
        m_type = new CType(CType::String);
        m_string = _str;
    }

    literals_t & fields() { return m_fields; }
    const literals_t & fields() const { return m_fields; }

private:
    Auto<CType> m_type;
    CNumber m_number;
    std::wstring m_wstring;
    std::string m_string;
    literals_t m_fields;

    void _setNumber(int _kind, int64_t _value) {
        //m_type = CType(_kind);
        m_number = CNumber(_value);
    }
};

class CConstant : public CVariable {
public:
    CConstant(const Auto<CLiteral> & _literal) :
        CVariable(_literal->getType()), m_literal(_literal) {}

    Auto<CLiteral> getLiteral() const { return m_literal; }

private:
    Auto<CLiteral> m_literal;
};

typedef std::list<Auto<CConstant> > consts_t;

class CLabel {
public:
//    virtual ~CLabel() {}
};

class CInstruction {
public:
    enum {
        Undefined,
        Nop,
        Unary,
        Binary,
        Call,
        If,
        Switch,
        Select,
        Field,
        Cast,
        Copy
    };

    CInstruction() : m_var(NULL), m_label(NULL) {}

    virtual int getKind() const { return Nop; };
    Auto<CVariable> getResult() const { return m_var; }
    Auto<CLabel> getLabel() const { return m_label; }
    void setLabel(const Auto<CLabel> & _label) { m_label = _label; }

protected:
    Auto<CVariable> m_var;
    Auto<CLabel> m_label;
};

typedef std::list<Auto<CVariable> > args_t;
typedef std::list<Auto<CInstruction> > instructions_t;

class CFunction : public CVariable {
public:
    CFunction(const std::wstring & _strName, const Auto<CType> & _returnType)
        : CVariable(new CType(CType::Function)), m_strName(_strName), m_result(new CVariable(_returnType)) {}

    ~CFunction() { /*freeList(m_instructions);*/ }

    const std::wstring & getName() const { return m_strName; }

    const Auto<CType> & getReturnType() const { return m_result->getType(); }

    Auto<CVariable> getResult() const { return m_result; }

    args_t & args() { return m_args; }
    const args_t & args() const { return m_args; }

    instructions_t & instructions() { return m_instructions; }
    const instructions_t & instructions() const { return m_instructions; }

    args_t & locals() { return m_locals; }
    const args_t & locals() const { return m_locals; }

private:
    std::wstring m_strName;
    //Auto<CType> m_returnType;
    Auto<CVariable> m_result;
    args_t m_args, m_locals;
    instructions_t m_instructions;
};

typedef std::list<Auto<CFunction> > functions_t;

class CModule {
public:
    ~CModule() { /*freeList(m_functions);*/ }

    functions_t & functions() { return m_functions; }
    const functions_t & functions() const { return m_functions; }

    types_t & types() { return m_types; }
    const types_t & types() const { return m_types; }

    consts_t & consts() { return m_consts; }
    const consts_t & consts() const { return m_consts; }

private:
    functions_t m_functions;
    types_t m_types;
    consts_t m_consts;
};

class COperand {
public:
    enum {
        Empty,
        Literal,
        Variable,
        Label
    };

    COperand() : m_kind(Empty) {}
    COperand(const CLiteral & _literal) : m_kind(Literal), m_literal(_literal), m_var(NULL), m_label(NULL) {}
    COperand(Auto<CVariable> _var) : m_kind(Variable), m_var(_var), m_label(NULL) {}
    COperand(Auto<CLabel> _label) : m_kind(Label), m_var(NULL), m_label(_label) {}

    int getKind() const { return m_kind; }

    const Auto<CVariable> getVariable() const { return m_var; }
    const CLiteral & getLiteral() const { return m_literal; }
    const Auto<CLabel> getLabel() const { return m_label; }

    const Auto<CType> getType() const {
        if (m_kind == Literal) return m_literal.getType();
        if (m_kind == Variable) return m_var->getType();
        return new CType(CType::Undefined);
    }

private:
    int m_kind;
    CLiteral m_literal;
    Auto<CVariable> m_var;
    Auto<CLabel> m_label;
};

class CUnary : public CInstruction {
public:
    enum {
        Not,
        Return,
        Load,
        Ptr,
        Malloc,
        Free,
        Jmp,
        Ref,
        Unref,
        UnrefNd,
    };

    CUnary(int _kind, const COperand & _op = COperand()) : m_kind(_kind), m_op(_op) {
        Auto<CType> type;
        switch (_kind) {
            case Load:
                assert(_op.getType()->getKind() == CType::Pointer);
                type = ((CPointerType &) * _op.getType()).getBase();
                break;
            case Ptr:
                type = new CPointerType(_op.getType());
                break;
            case Not:
                type = _op.getType();
                break;
            case Malloc:
                type = new CPointerType(new CType(CType::Void));
                break;
        }

        if (! type.empty())
            m_var = new CVariable(type);
    }

    virtual int getKind() const { return Unary; }
    virtual int getUnaryKind() const { return m_kind; }
    const COperand & getOp() const { return m_op; }

private:
    int m_kind;
    COperand m_op;
};

class CBinary : public CInstruction {
public:
    enum {
        Set,
        Store,
        Jmz,
        Jnz,
        Offset,
        QInit,

        RefProc,
        UnrefProc,

        ArithmMask = 0x01000000,

        // Bitwise logical operations.
        And, Or, Xor,
        BAnd, BOr, BXor,
        ZAnd, ZOr, ZXor,

        // Integer arithmetic.
        Add, Sub, Mul, Div, Rem, Shl, Shr, Pow,

        // Floating point arithmetic.
        FAdd, FSub, FMul, FDiv, FPow,

        // GMP integer arithmetic.
        ZAdd, ZSub, ZMul, ZDiv, ZRem, ZShl, ZShr, ZPow,

        // GMP rational arithmetic.
        QAdd, QSub, QMul, QDiv, QPow,

        CmpMask = 0x02000000,

        // Integer comparison.
        Eq, Ne, Lt, Lte, Gt, Gte,

        // Floating point comparison.
        FEq, FNe, FLt, FLte, FGt, FGte, FNaN, FNorm, FFin,

        // GMP integer comparison.
        ZEq, ZNe, ZLt, ZLte, ZGt, ZGte,

        // GMP rational comparison.
        QEq, QNe, QLt, QLte, QGt, QGte,
    };

    CBinary(int _kind, const COperand & _op1, const COperand & _op2) : m_kind(_kind), m_op1(_op1), m_op2(_op2) {
        Auto<CType> type;
        if (_kind & ArithmMask)
            type = _op1.getType();
        else if (_kind & CmpMask)
            type = new CType(CType::Bool);
        else if (_kind == Offset)
            type = _op1.getType();
        else if (_kind == QInit)
            type = new CType(CType::Gmp_q);

        if (! type.empty())
            m_var = new CVariable(type);
    }

    virtual int getKind() const { return Binary; }
    virtual int getBinaryKind() const { return m_kind; }

    const COperand & getOp1() const { return m_op1; }
    const COperand & getOp2() const { return m_op2; }

private:
    int m_kind;
    COperand m_op1, m_op2;
};

class CField : public CInstruction {
public:
    CField(const COperand & _op, size_t _cIndex) : m_cIndex(_cIndex), m_op(_op) {
        assert(_op.getType()->getKind() == CType::Pointer);
        const CType & type = * ((CPointerType &) * _op.getType()).getBase();
        assert(type.getKind() == CType::Struct);
        m_var = new CVariable(new CPointerType(((CStructType &) type).fieldTypes()[_cIndex]));
    }

    size_t getIndex() const { return m_cIndex; }
    virtual int getKind() const { return Field; }
    const COperand & getOp() const { return m_op; }

private:
    size_t m_cIndex;
    COperand m_op;
};

typedef std::list<COperand> operands_t;

class CCall : public CInstruction {
public:
    CCall(const COperand & _function, const Auto<CFunctionType> & _type) : m_function(_function), m_type(_type) {
        m_var = new CVariable(m_type->getReturnType());
    }

    virtual int getKind() const { return Call; }
    operands_t & args() { return m_args; }
    const operands_t & args() const { return m_args; }

    const COperand & getFunction() const { return m_function; }

private:
    COperand m_function;
    Auto<CFunctionType> m_type;
    operands_t m_args;
};

class CIf : public CInstruction {
public:
    CIf(const COperand & _condition) : m_condition(_condition) {}
    virtual ~CIf() {
        /*freeList(m_brTrue);
        freeList(m_brFalse);*/
    }

    virtual int getKind() const { return If; }
    const COperand & getCondition() const { return m_condition; }
    void setCondition(const COperand & _op) { m_condition = _op; }

    instructions_t & brTrue() { return m_brTrue; }
    const instructions_t & brTrue() const { return m_brTrue; }
    instructions_t & brFalse() { return m_brFalse; }
    const instructions_t & brFalse() const { return m_brFalse; }

private:
    COperand m_condition;
    instructions_t m_brTrue, m_brFalse;
};

class CCast : public CInstruction {
public:
    CCast(const COperand & _op, const Auto<CType> & _type) :
        m_op(_op), m_type(_type)
    {
        m_var = new CVariable(m_type);
    }

    virtual int getKind() const { return Cast; }
    const COperand & getOp() const { return m_op; }
    Auto<CType> getType() const { return m_type; }

private:
    COperand m_op;
    Auto<CType> m_type;
};

class CSelect : public CInstruction {
public:
    CSelect(const COperand & _condition, const COperand & _opTrue, const COperand & _opFalse) :
        m_condition(_condition), m_opTrue(_opTrue), m_opFalse(_opFalse)
    {
        m_var = new CVariable(_opTrue.getType());
    }

    virtual int getKind() const { return Select; }
    const COperand & getCondition() const { return m_condition; }
    const COperand & getTrue() const { return m_opTrue; }
    const COperand & getFalse() const { return m_opFalse; }

private:
    COperand m_condition, m_opTrue, m_opFalse;
};

class CCopy : public CInstruction {
public:
    CCopy(const COperand & _dest, const COperand & _src, const COperand & _size) :
        m_dest(_dest), m_src(_src), m_size(_size)
    {}

    virtual int getKind() const { return Copy; }
    const COperand & getDest() const { return m_dest; }
    const COperand & getSrc() const { return m_src; }
    const COperand & getSize() const { return m_size; }

private:
    COperand m_dest, m_src, m_size;
};

struct switch_case_t {
    std::vector<int64_t> values;
    instructions_t body;
};

typedef std::list<switch_case_t> switch_cases_t;

class CSwitch : public CInstruction {
public:
    CSwitch(const COperand & _arg) : m_arg(_arg) {}

    virtual int getKind() const { return Switch; }
    const COperand & getArg() const { return m_arg; }

    switch_cases_t & cases() { return m_cases; }
    const switch_cases_t & cases() const { return m_cases; }

    instructions_t & deflt() { return m_deflt; }
    const instructions_t & deflt() const { return m_deflt; }

private:
    COperand m_arg;
    switch_cases_t m_cases;
    instructions_t m_deflt;
};

void translate(CModule & _dest, const ir::CModule & _from);

};

#endif // LLIR_H_
