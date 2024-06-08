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

class Base : public std::enable_shared_from_this<Base> {
protected:
    // Override Counted's deleted copy constructor to allow copying
    // preserving Counted's internal fields.
    Base(const Base &_other) {}
    Base &operator =(const Base &_other) { return *this; }
    Base() = default;
public:
    template <class _Class>
    std::shared_ptr<const _Class> as() const {
        return std::static_pointer_cast<const _Class>(shared_from_this());
    }

    template <class _Class>
    std::shared_ptr<_Class> as() {
        return std::static_pointer_cast<_Class>(shared_from_this());
    }
};

using TypePtr = std::shared_ptr<class Type>;

class Type : public Base {
public:
    enum {
        UNDEFINED,

        // Primitive types.
        VOID,

        GMP_Z, GMP_Q,

        FUNCTION,

        STRUCT,

        // Integer types.
        INTMASK = 0x01000000,
        INT8, INT16, INT32, INT64, UINT8, UINT16, UINT32, UINT64,
        BOOL, WCHAR,

        // Floating point types.
        FLOATMASK = 0x02000000,
        FLOAT, DOUBLE, QUAD,

        POINTER,
    };

    Type() : m_kind(UNDEFINED) {}

    Type(int _kind) : m_kind(_kind) {}

    virtual int getKind() const { return m_kind; }

    static size_t sizeOf(int _kind) {
        switch (_kind) {
            case GMP_Z:
                return sizeof(mpz_t);
            case GMP_Q:
                return sizeof(mpq_t);
            case INT8:
            case UINT8:
            case BOOL:
                return 1;
            case INT16:
            case UINT16:
                return 2;
            case INT32:
            case UINT32:
                return 4;
            case INT64:
            case UINT64:
                return 8;
            case WCHAR:
                return 4;
            case FLOAT:
                return 4;
            case DOUBLE:
                return 8;
            case QUAD:
                return 16;
            case POINTER:
            case FUNCTION:
                return sizeof(void *);
            default:
                return 0;
        }
    }

    virtual size_t sizeOf() const { return sizeOf(m_kind); }

    virtual bool operator < (const Type & _other) const {
        return getKind() < _other.getKind();
    }

private:
    int m_kind;
};

using Types = std::vector<TypePtr>;
using FunctionTypePtr = std::shared_ptr<class FunctionType>;

class FunctionType : public Type {
public:
    FunctionType(const TypePtr & _returnType) : Type(FUNCTION), m_returnType(_returnType) {}

    const TypePtr & getReturnType() const { return m_returnType; }

    Types & argTypes() { return m_argTypes; }

    virtual bool operator < (const Type & _other) const {
        assert(false && "Unimplemented");
        return getKind() < _other.getKind();
    }

private:
    TypePtr m_returnType;
    Types m_argTypes;
};

using StructTypePtr = std::shared_ptr<class StructType>;

class StructType : public Type {
public:
    StructType() : Type(STRUCT), m_strName(L"") {}
    StructType(const std::wstring & _strName) : Type(STRUCT), m_strName(_strName) {}

    const std::wstring & getName() const { return m_strName; }

    Types & fieldTypes() { return m_fieldTypes; }
    const Types & fieldTypes() const { return m_fieldTypes; }

    virtual size_t sizeOf() const {
        // assume 4-byte field alignment.
        size_t cSize = 0;
        for (const auto& iType : m_fieldTypes) {
            const size_t cFieldSize = iType->sizeOf();
            cSize += cFieldSize%4 == 0 ? cFieldSize : (cFieldSize/4 + 1)*4;
        }
        return std::max((size_t) 1, cSize);
    }

    virtual bool operator < (const Type & _other) const {
        if (getKind() < _other.getKind())
            return true;
        if (getKind() > _other.getKind())
            return false;

        const StructType & other = (const StructType &) _other;

        if (m_fieldTypes.size() < other.m_fieldTypes.size())
            return true;
        if (m_fieldTypes.size() > other.m_fieldTypes.size())
            return false;

        auto iType = m_fieldTypes.cbegin();
        auto iType2 = other.m_fieldTypes.cbegin();

        for (; iType != m_fieldTypes.cend(); ++ iType, ++ iType2) {
            if (**iType < **iType2)
                return true;
            if (**iType2 < **iType)
                return false;
        }

        return false;
    }

private:
    Types m_fieldTypes;
    std::wstring m_strName;
};

class PointerType : public Type {
public:
    PointerType(const TypePtr & _baseType) : Type(POINTER), m_baseType(_baseType) {}
    const TypePtr & getBase() const { return m_baseType; }

    virtual bool operator < (const Type & _other) const {
        return getKind() < _other.getKind() ||
                (getKind() == _other.getKind() && getBase() < ((const PointerType &) _other).getBase());
    }

private:
    TypePtr m_baseType;
};

using InstructionPtr = std::shared_ptr<class Instruction>;
using VariablePtr = std::shared_ptr<class Variable>;

class Variable : public Base {
public:
    Variable() : m_bAlive(true), m_bUsed(false) {}
    Variable(const TypePtr & _type) : m_type(_type), m_bAlive(true), m_bUsed(false) {}

    const TypePtr & getType() const { return m_type; }
    void setType(const TypePtr & _type) { m_type = _type; }

    InstructionPtr getLastUse() const { return m_lastUse; }
    void setLastUse(const InstructionPtr _instr) { m_lastUse = _instr; }

    InstructionPtr getLastInit() const { return m_lastInit; }
    void setLastInit(const InstructionPtr _instr) { m_lastInit = _instr; }

//    bool isValid() const { return m_type->getKind() != Type::Undefined; }

    bool isAlive() const { return m_bAlive; }
    void setAlive(bool _bAlive) { m_bAlive = _bAlive; }

    bool isUsed() const { return m_bUsed; }
    void setUsed(bool _bUsed) const { m_bUsed = _bUsed; }

    bool operator ==(const Variable & _other) const { return this == & _other; }
    bool operator < (const Variable & _other) const { return this < & _other; }

protected:
    TypePtr m_type;
    InstructionPtr m_lastUse;
    InstructionPtr m_lastInit;
    bool m_bAlive;
    mutable bool m_bUsed;
};

using LiteralPtr = std::shared_ptr<class Literal>;

typedef std::list<LiteralPtr> Literals;

class Literal : public Base {
public:
    enum {
        NUMBER,
        BOOL,
        STRING,
        CHAR,
        STRUCT
    };

    Literal() : m_kind(NUMBER) {}
    Literal(int _kind, const TypePtr & _type) : m_kind(_kind), m_type(_type) {}
    Literal(const TypePtr & _pType, const Number & _number) : m_kind(NUMBER), m_type(_pType), m_number(_number) {}

    int getKind() const { return m_kind; }

    const TypePtr & getType() const { return m_type; }

    const Number & getNumber() const { return m_number; }

    void setNumber(const Number & _number) {
        //m_type = new Type(Type::Int32);
        m_kind = NUMBER;
        assert(m_type);
        m_number = _number;
    }

    wchar_t getBool() const { return m_number.getInt() != 0; }
    void setBool(bool _b) {
        m_kind = BOOL;
        _setNumber(Type::BOOL, _b ? 1 : 0);
    }

    wchar_t getChar() const { return (wchar_t) m_number.getInt(); }
    void setChar(wchar_t _c) {
        m_kind = CHAR;
        _setNumber(Type::WCHAR, _c);
    }

    const std::wstring & getString() const { return m_wstring; }
    void setString(const std::wstring & _str) {
        m_kind = STRING;
        m_type = std::make_shared<Type>(Type::WCHAR);
        m_wstring = _str;
    }

    Literals & fields() { return m_fields; }
    const Literals & fields() const { return m_fields; }

private:
    int m_kind;
    TypePtr m_type;
    Number m_number;
    std::wstring m_wstring;
    std::string m_string;
    Literals m_fields;

    void _setNumber(int _kind, int64_t _value) {
        m_type = std::make_shared<Type>(_kind);
        m_number = Number::makeInt(_value);
    }
};

using ConstantPtr = std::shared_ptr<class Constant>;

class Constant : public Variable {
public:
    Constant(const LiteralPtr & _literal) :
        Variable(_literal->getType()), m_literal(_literal) {}

    LiteralPtr getLiteral() const { return m_literal; }

private:
    LiteralPtr m_literal;
};

using Consts = std::list<ConstantPtr>;
using LabelPtr = std::shared_ptr<class Label>;

class Label : public Base {
public:
    Label() : m_cUsageCount(0) {}

//    virtual ~Label() {}
    size_t getUsageCount() const { return m_cUsageCount; }
    void setUsageCount(size_t _cUsageCount) { m_cUsageCount = _cUsageCount; }
    void incUsageCount() { ++ m_cUsageCount; }
    void decUsageCount() { -- m_cUsageCount; }

    bool operator ==(const Label & _other) const { return this == & _other; }
    bool operator < (const Label & _other) const { return this < & _other; }

private:
    size_t m_cUsageCount;
};

using InstructionPtr = std::shared_ptr<class Instruction>;

class Instruction : public Base {
public:
    enum {
        UNDEFINED,
        NOP,
        UNARY,
        BINARY,
        CALL,
        BUILTIN,
        IF,
        WHILE,
        SWITCH,
        SELECT,
        FIELD,
        CAST,
        COPY
    };

    Instruction() : m_var(NULL), m_label(NULL), m_cResultUsageCount(0) {}

    virtual int getKind() const { return NOP; };
    VariablePtr getResult() const { return m_var; }
    void setResult(const VariablePtr & _var) { m_var = _var; }
    LabelPtr getLabel() const { return m_label; }
    void setLabel(const LabelPtr & _label) { m_label = _label; }

    bool operator ==(const Instruction & _other) const { return this == & _other; }

    size_t getResultUsageCount() const { return m_cResultUsageCount; }
    void setResultUsageCount(size_t _cResultUsageCount) const { m_cResultUsageCount = _cResultUsageCount; }
    void incResultUsageCount() const { ++ m_cResultUsageCount; }
    void decResultUsageCount() const { -- m_cResultUsageCount; }

protected:
    VariablePtr m_var;
    LabelPtr m_label;
    mutable size_t m_cResultUsageCount;
};

typedef std::list<VariablePtr> Args;
typedef std::list<InstructionPtr> Instructions;

using FunctionPtr = std::shared_ptr<class Function>;

class Function : public Variable {
public:
    Function(const std::wstring & _strName, const TypePtr & _returnType)
        : Variable(std::make_shared<Type>(Type::FUNCTION)), m_strName(_strName), m_result(std::make_shared<Variable>(_returnType)) {}

    const std::wstring & getName() const { return m_strName; }

    const TypePtr & getReturnType() const { return m_result->getType(); }

    const VariablePtr& getResult() const { return m_result; }

    Args & args() { return m_args; }
    const Args & args() const { return m_args; }

    Instructions & instructions() { return m_instructions; }
    const Instructions & instructions() const { return m_instructions; }

    Args & locals() { return m_locals; }
    const Args & locals() const { return m_locals; }

private:
    std::wstring m_strName;
    //TypePtr m_returnType;
    VariablePtr m_result;
    Args m_args, m_locals;
    Instructions m_instructions;
};

using Functions = std::list<FunctionPtr>;

class Module : public Base {
public:

    Functions & functions() { return m_functions; }
    const Functions & functions() const { return m_functions; }

    Types & types() { return m_types; }
    const Types & types() const { return m_types; }

    Consts & consts() { return m_consts; }
    const Consts & consts() const { return m_consts; }

    Functions & usedBuiltins() { return m_usedBuiltins; }
    const Functions & usedBuiltins() const { return m_usedBuiltins; }

private:
    Functions m_functions;
    Functions m_usedBuiltins;
    Types m_types;
    Consts m_consts;
};

class Operand : public Base {
public:
    enum {
        EMPTY,
        LITERAL,
        VARIABLE,
        LABEL
    };

    Operand() : m_kind(EMPTY) {}
    Operand(const Literal & _literal) : m_kind(LITERAL), m_literal(_literal), m_var(NULL), m_label(NULL) {}
    Operand(VariablePtr _var) : m_kind(VARIABLE), m_var(_var), m_label(NULL) {}
    Operand(LabelPtr _label) : m_kind(LABEL), m_var(NULL), m_label(_label) {}

    int getKind() const { return m_kind; }

    const VariablePtr getVariable() const { return m_var; }
    const Literal & getLiteral() const { return m_literal; }
    const LabelPtr getLabel() const { return m_label; }

    void setVariable(const VariablePtr & _var) { m_var = _var; }
    void setLabel(const LabelPtr & _label) { m_label = _label; }

    const TypePtr getType() const {
        if (m_kind == LITERAL) return m_literal.getType();
        if (m_kind == VARIABLE) return m_var->getType();
        return std::make_shared<Type>(Type::UNDEFINED);
    }

    bool empty() const { return m_kind == EMPTY; }

private:
    int m_kind;
    Literal m_literal;
    VariablePtr m_var;
    LabelPtr m_label;
};

class Unary : public Instruction {
public:
    enum {
        NOT,
        RETURN,
        LOAD,
        PTR,
        MALLOC,
        FREE,
        JMP,
        REF,
        UNREF,
        UNREFND,
    };

    Unary(int _kind, const Operand & _op = Operand()) : m_kind(_kind), m_op(_op) {
        TypePtr type;
        switch (_kind) {
            case LOAD:
                assert(_op.getType()->getKind() == Type::POINTER);
                type = ((PointerType &) * _op.getType()).getBase();//TODO:dyp: fix
                break;
            case PTR:
                type = std::make_shared<PointerType>(_op.getType());
                break;
            case NOT:
                type = _op.getType();
                break;
            case MALLOC:
                type = std::make_shared<PointerType>(std::make_shared<Type>(Type::VOID));
                break;
        }

        if (type)
            m_var = std::make_shared<Variable>(type);
    }

    virtual int getKind() const { return UNARY; }
    virtual int getUnaryKind() const { return m_kind; }
    const Operand & getOp() const { return m_op; }
    Operand & getOp() { return m_op; }

private:
    int m_kind;
    Operand m_op;
};

class Binary : public Instruction {
public:
    enum {
        SET,
        STORE,
        JMZ,
        JNZ,
        OFFSET,
        QINIT,

        REFPROC,
        UNREFPROC,

        ARITHMMASK = 0x01000000,

        // Bitwise logical operations.
        AND, OR, XOR,
        BAND, BOR, BXOR,
        ZAND, ZOR, ZXOR,

        // Integer arithmetic.
        ADD, SUB, MUL, DIV, REM, SHL, SHR, POW,

        // Floating point arithmetic.
        FADD, FSUB, FMUL, FDIV, FPOW,

        // GMP integer arithmetic.
        ZADD, ZSUB, ZMUL, ZDIV, ZREM, ZSHL, ZSHR, ZPOW,

        // GMP rational arithmetic.
        QADD, QSUB, QMUL, QDIV, QPOW,

        CMPMASK = 0x02000000,

        // Integer comparison.
        EQ, NE, LT, LTE, GT, GTE,

        // Floating point comparison.
        FEQ, FNE, FLT, FLTE, FGT, FGTE, FNAN, FNORM, FFIN,

        // GMP integer comparison.
        ZEQ, ZNE, ZLT, ZLTE, ZGT, ZGTE,

        // GMP rational comparison.
        QEQ, QNE, QLT, QLTE, QGT, QGTE,
    };

    Binary(int _kind, const Operand & _op1, const Operand & _op2) : m_kind(_kind), m_op1(_op1), m_op2(_op2) {
        TypePtr type;
        if (_kind & ARITHMMASK)
            type = _op1.getType();
        else if (_kind & CMPMASK)
            type = std::make_shared<Type>(Type::BOOL);
        else if (_kind == OFFSET)
            type = _op1.getType();
        else if (_kind == QINIT)
            type = std::make_shared<Type>(Type::GMP_Q);

        if (type)
            m_var = std::make_shared<Variable>(type);
    }

    virtual int getKind() const { return Instruction::BINARY; }
    virtual int getBinaryKind() const { return m_kind; }

    const Operand & getOp1() const { return m_op1; }
    Operand & getOp1() { return m_op1; }
    const Operand & getOp2() const { return m_op2; }
    Operand & getOp2() { return m_op2; }

private:
    int m_kind;
    Operand m_op1, m_op2;
};

class Field : public Instruction {
public:
    Field(const Operand & _op, size_t _cIndex) : m_cIndex(_cIndex), m_op(_op) {
        assert(_op.getType()->getKind() == Type::POINTER);
        const Type & type = * ((PointerType &) * _op.getType()).getBase(); //TODO:dyp: fix
        assert(type.getKind() == Type::STRUCT);
        m_var = std::make_shared<Variable>(std::make_shared<PointerType>(((StructType &) type).fieldTypes()[_cIndex]));//TODO:dyp: fix
    }

    size_t getIndex() const { return m_cIndex; }
    virtual int getKind() const { return Instruction::FIELD; }
    const Operand & getOp() const { return m_op; }
    Operand & getOp() { return m_op; }

private:
    size_t m_cIndex;
    Operand m_op;
};

typedef std::list<Operand> Operands;

class Call : public Instruction {
public:
    Call(const Operand & _function, const FunctionTypePtr & _type) : m_function(_function), m_type(_type) {
        if (m_type->getReturnType() && m_type->getReturnType()->getKind() != Type::VOID) {
            m_var = std::make_shared<Variable>(m_type->getReturnType());
        }
    }

    virtual int getKind() const { return Instruction::CALL; }
    Operands & args() { return m_args; }
    const Operands & args() const { return m_args; }

    const Operand & getFunction() const { return m_function; }

private:
    Operand m_function;
    FunctionTypePtr m_type;
    Operands m_args;
};

class Builtin : public Instruction {
public:
    Builtin(const std::string & _name, const TypePtr & _type) : m_name(_name) {
        m_var = std::make_shared<Variable>(_type);
    }

    virtual int getKind() const { return Instruction::BUILTIN; }
    Operands & args() { return m_args; }
    const Operands & args() const { return m_args; }

    const std::string & getName() const { return m_name; }

private:
    std::string m_name;
    Operands m_args;
};

using IfPtr = std::shared_ptr<class If>;

class If : public Instruction {
public:
    If(const Operand & _condition) : m_condition(_condition) {}

    virtual int getKind() const { return Instruction::IF; }
    const Operand & getCondition() const { return m_condition; }
    Operand & getCondition() { return m_condition; }
    void setCondition(const Operand & _op) { m_condition = _op; }

    Instructions & brTrue() { return m_brTrue; }
    const Instructions & brTrue() const { return m_brTrue; }
    Instructions & brFalse() { return m_brFalse; }
    const Instructions & brFalse() const { return m_brFalse; }

private:
    Operand m_condition;
    Instructions m_brTrue, m_brFalse;
};

class While : public Instruction {
public:
    While(const Operand & _condition) : m_condition(_condition) {}

    virtual int getKind() const { return Instruction::WHILE; }
    const Operand & getCondition() const { return m_condition; }
    Operand & getCondition() { return m_condition; }
    void setCondition(const Operand & _op) { m_condition = _op; }

    Instructions & getBlock() { return m_block; }
    const Instructions & getBlock() const { return m_block; }

private:
    Operand m_condition;
    Instructions m_block;
};

class Cast : public Instruction {
public:
    Cast(const Operand & _op, const TypePtr & _type) :
        m_op(_op), m_type(_type)
    {
        m_var = std::make_shared<Variable>(m_type);
    }

    virtual int getKind() const { return Instruction::CAST; }
    const Operand & getOp() const { return m_op; }
    Operand & getOp() { return m_op; }
    TypePtr getType() const { return m_type; }

private:
    Operand m_op;
    TypePtr m_type;
};

class Select : public Instruction {
public:
    Select(const Operand & _condition, const Operand & _opTrue, const Operand & _opFalse) :
        m_condition(_condition), m_opTrue(_opTrue), m_opFalse(_opFalse)
    {
        m_var = std::make_shared<Variable>(_opTrue.getType());
    }

    virtual int getKind() const { return Instruction::SELECT; }
    const Operand & getCondition() const { return m_condition; }
    Operand & getCondition() { return m_condition; }
    const Operand & getTrue() const { return m_opTrue; }
    Operand & getTrue() { return m_opTrue; }
    const Operand & getFalse() const { return m_opFalse; }
    Operand & getFalse() { return m_opFalse; }

private:
    Operand m_condition, m_opTrue, m_opFalse;
};

class Copy : public Instruction {
public:
    Copy(const Operand & _dest, const Operand & _src, const Operand & _size) :
        m_dest(_dest), m_src(_src), m_size(_size)
    {}

    virtual int getKind() const { return Instruction::COPY; }
    const Operand & getDest() const { return m_dest; }
    Operand & getDest() { return m_dest; }
    const Operand & getSrc() const { return m_src; }
    Operand & getSrc() { return m_src; }
    const Operand & getSize() const { return m_size; }
    Operand & getSize() { return m_size; }

private:
    Operand m_dest, m_src, m_size;
};

struct SwitchCase {
    std::vector<int64_t> values;
    Instructions body;
};

typedef std::list<SwitchCase> SwitchCases;

class Switch : public Instruction {
public:
    Switch(const Operand & _arg) : m_arg(_arg) {}

    virtual int getKind() const { return Instruction::SWITCH; }
    const Operand & getArg() const { return m_arg; }
    Operand & getArg() { return m_arg; }

    SwitchCases & cases() { return m_cases; }
    const SwitchCases & cases() const { return m_cases; }

    Instructions & deflt() { return m_deflt; }
    const Instructions & deflt() const { return m_deflt; }

private:
    Operand m_arg;
    SwitchCases m_cases;
    Instructions m_deflt;
};

void translate(Module & _dest, const ir::ModulePtr & _from);

};

#endif // LLIR_H_
