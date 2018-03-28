/// \file prettyprinter.cpp
///

#include "prettyprinter.h"
#include "utils.h"
#include "ir/statements.h"
#include "ir/visitor.h"
#include "pp_syntax.h"

#include <iostream>
#include <sstream>

using namespace ir;

static std::wstring fmtBits(int _bits) {
    switch (_bits) {
        case Number::GENERIC: return L"generic"; break;
        case Number::NATIVE:  return L"native";  break;
        default:              return fmtInt(_bits);
    }
}

static std::wstring fmtUnaryOp(int _kind) {
    switch (_kind) {
        case Unary::PLUS:           return L"plus";
        case Unary::MINUS:          return L"minus";
        case Unary::BOOL_NEGATE:    return L"bool_negate";
        case Unary::BITWISE_NEGATE: return L"bitwise_negate";
        default:                    return L"";
    }
}

static std::wstring fmtBinaryOp(int _kind) {
    switch (_kind) {
        case Binary::IMPLIES:           return L"implies";
        case Binary::IFF:               return L"iff";
        case Binary::BOOL_OR:           return L"bool_or";
        case Binary::BOOL_XOR:          return L"bool_xor";
        case Binary::BOOL_AND:          return L"bool_and";
        case Binary::EQUALS:            return L"equals";
        case Binary::NOT_EQUALS:        return L"not_equals";
        case Binary::LESS:              return L"less";
        case Binary::LESS_OR_EQUALS:    return L"less_or_equals";
        case Binary::GREATER:           return L"greater";
        case Binary::GREATER_OR_EQUALS: return L"greater_or_equals";
        case Binary::IN:                return L"in";
        case Binary::SHIFT_LEFT:        return L"shift_left";
        case Binary::SHIFT_RIGHT:       return L"shift_right";
        case Binary::ADD:               return L"add";
        case Binary::SUBTRACT:          return L"subtract";
        case Binary::MULTIPLY:          return L"multiply";
        case Binary::DIVIDE:            return L"divide";
        case Binary::REMAINDER:         return L"remainder";
        case Binary::POWER:             return L"power";
        default:                        return L"";
    }
}

static std::wstring fmtTypeFormulaOp(int _kind) {
    switch (_kind) {
        case tc::Formula::EQUALS:           return L"=";
        case tc::Formula::SUBTYPE:          return L"<=";
        case tc::Formula::SUBTYPE_STRICT:   return L"<";
        case tc::Formula::COMPARABLE:       return L"~";
        case tc::Formula::INCOMPARABLE:     return L"!~";
        case tc::Formula::NO_JOIN:          return L"!\x2228";
        case tc::Formula::HAS_JOIN:         return L"\x2228";
        case tc::Formula::NO_MEET:          return L"!\x2227";
        case tc::Formula::HAS_MEET:         return L"\x2227";
        default:                            return L"";
    }
}

static std::wstring fmtQuantifier(int _kind) {
    switch (_kind) {
        case Formula::UNIVERSAL:   return L"universal";
        case Formula::EXISTENTIAL: return L"existential";
        default:                   return L"";
    }
}

static std::wstring fmtLiteralKind(int _kind) {
    switch (_kind) {
        case Literal::UNIT:   return L"unit";
        case Literal::NUMBER: return L"number";
        case Literal::BOOL:   return L"bool";
        case Literal::CHAR:   return L"char";
        case Literal::STRING: return L"string";
        default:              return L"";
    }
}

static std::wstring fmtNumber(const Number &_num) {
    return _num.toString();
}

static std::wstring fmtBool(bool _b) {
    return _b ? L"true" : L"false";
}

static std::wstring fmtChar(wchar_t _c) {
    return fmtQuote(std::wstring(&_c, 1));
}

static std::wstring fmtLiteral(const Literal &_lit) {
    switch (_lit.getLiteralKind()) {
        case Literal::UNIT:   return L"nil";
        case Literal::NUMBER: return fmtNumber(_lit.getNumber());
        case Literal::BOOL:   return fmtBool(_lit.getBool());
        case Literal::CHAR:   return fmtChar(_lit.getChar());
        case Literal::STRING: return fmtQuote(_lit.getString());
        default:              return L"";
    }
}

static std::wstring fmtOverflow(const Overflow &_ovf) {
    switch (_ovf.get()) {
        case Overflow::SATURATE: return L"saturate";
        case Overflow::STRICT:   return L"strict";
        case Overflow::RETURN:   return L"return";
        case Overflow::WRAP:     return L"wrap";
        default:                 return L"";
    }
}

static std::wstring fmtLabel(const LabelPtr &_pLabel) {
    return fmtQuote(_pLabel ? _pLabel->getName() : L"");
}

FreshTypeNames g_freshTypes;

void PrettyPrinterBase::setFreshTypeNames(const FreshTypeNames & _names) {
    g_freshTypes = _names;
}

std::wstring PrettyPrinterBase::fmtFreshType(tc::FreshType &_type) {
    std::wstring strName = g_freshTypes[_type.getOrdinal()];

    if (strName.empty()) {
        const size_t nType = _type.getOrdinal();
        const int nChar = nType%26;
        const int nNum = nType/26;

        strName = L"A";
        strName[0] += nChar;

        if (nNum > 0)
            strName += fmtInt(nNum);

        g_freshTypes[_type.getOrdinal()] = strName;
    }

    if (_type.getFlags())
        strName += L" \x02C4\x02C5\x02DF"[_type.getFlags()];

    return strName;
}

std::wstring PrettyPrinterBase::fmtIndent(const std::wstring &_s) {
    std::wstring res;

    for (size_t i = 0; i < getDepth(); ++ i)
        res += L"  ";

    return res + _s;
}

std::wstring PrettyPrinterBase::fmtType(int _kind) {
    switch (_kind) {
        case Type::FRESH:   return fmtFreshType((tc::FreshType &)getNode());
        case Type::BOTTOM:  return L"\x22a5";
        case Type::TOP:     return L"\x22a4";
        case Type::UNIT:    return L"nil";
        case Type::INT:     return L"int";
        case Type::NAT:     return L"nat";
        case Type::REAL:    return L"real";
        case Type::BOOL:    return L"bool";
        case Type::CHAR:    return L"char";
        case Type::STRING:  return L"string";
        case Type::TYPE:    return L"type";
        case Type::GENERIC: return L"var";
        default:            return L"";
    }
}

#define VISITOR(_NODE, ...)                                 \
        virtual bool visit##_NODE(_NODE &_node) {           \
            m_os << fmtIndent(L" : " WIDEN(#_NODE) L"\n");  \
            { __VA_ARGS__ }                                 \
            return true;                                    \
        }

#define HANDLER(_ROLE)                                  \
    virtual int handle##_ROLE(Node &_node) {            \
        m_os << fmtIndent(L"* " WIDEN(#_ROLE) L"\n");   \
        return 0;                                       \
    }

class PrettyPrinterAST: public PrettyPrinterBase {
public:
    PrettyPrinterAST(std::wostream &_os) : PrettyPrinterBase(_os) {}

#define NODE(_Node, _Parent) VISITOR(_Node);
#include "ir/nodes.inl"
#undef NODE

protected:
#define ROLE(_ROLE) HANDLER(_ROLE)
#include "ir/roles.inl"
#undef ROLE
};

class PrettyPrinter: public PrettyPrinterAST {
public:
    PrettyPrinter(std::wostream &_os) : PrettyPrinterAST(_os) {}

    void print(Node &_node) {
        traverseNode(_node);
    }

protected:

#define PROP(_FMT, _NAME)                                                                               \
        do {                                                                                            \
            m_os << fmtIndent(L" `- " WIDEN(#_NAME) L" = ") + fmt##_FMT(_node.get##_NAME()) + L"\n";    \
        } while (0)

#define PROP_VAL(_FMT)                                                      \
        do {                                                                \
            m_os << fmtIndent(L" `- Value = ") + fmt##_FMT(_node) + L"\n";  \
        } while (0)

#define PROP_IS(_NAME)                                                                              \
        do {                                                                                        \
            m_os << fmtIndent(L" `- " WIDEN(#_NAME) L" = ") + fmtBool(_node.is##_NAME()) + L"\n";   \
        } while (0)

    VISITOR(Type,
            if (_node.getKind() <= Type::GENERIC)
                PROP(Type, Kind);
            else
                PROP(Int, Kind);
            if (_node.getKind() >= Type::NAT && _node.getKind() <= Type::REAL)
                PROP(Bits, Bits);
    );

    VISITOR(NamedReferenceType,
            PROP(Quote, Name);
    );

    VISITOR(VariableReference,
            PROP(Quote, Name);
    );

    VISITOR(PredicateReference,
            PROP(Quote, Name);
    );

    VISITOR(Unary,
            PROP(UnaryOp, Operator);
            PROP(Overflow, Overflow);
    );

    VISITOR(Binary,
            PROP(BinaryOp, Operator);
            PROP(Overflow, Overflow);
    );

    VISITOR(FieldExpr,
            PROP(Quote, FieldName);
    );

    VISITOR(StructFieldDefinition,
            PROP(Quote, Name);
    );

    VISITOR(UnionConstructorDeclaration,
            PROP(Quote, Name);
            PROP(Int, Ordinal);
    );

    VISITOR(FormulaCall,
            PROP(Quote, Name);
    );

    VISITOR(Formula,
            PROP(Quantifier, Quantifier);
    );

    VISITOR(NamedValue,
            if (_node.getKind() != NamedValue::PREDICATE_PARAMETER ||
                    static_cast<const Param &>(_node).isUsed())
                PROP(Quote, Name);
    );

    VISITOR(EnumValue,
            PROP(Int, Ordinal);
    );

    VISITOR(Expression,
            VISITOR_TRAVERSE(Type, ExprType, _node.getType(), _node, Expression, setType);
            return true;
    );

    VISITOR(Param,
            PROP(Bool, LinkedParam);
            PROP_IS(Output);
    );

    VISITOR(Literal,
            PROP(LiteralKind, LiteralKind);
            PROP_VAL(Literal);
    );

    VISITOR(Predicate,
            PROP(Quote, Name);
    );

    VISITOR(FormulaDeclaration,
            PROP(Quote, Name);
    );

    VISITOR(TypeDeclaration,
            PROP(Quote, Name);
    );

    VISITOR(Module,
            PROP(Quote, Name);
    );

    VISITOR(Label,
            PROP(Quote, Name);
    );

    VISITOR(Jump,
            PROP(Label, Destination);
    );
};

void prettyPrint(Module & _module, std::wostream & _os) {
    PrettyPrinter pp(_os);
    Param::updateUsed(_module);
    pp.traverseModule(_module);
}

void print(ir::Node &_node, std::wostream &_os) {
    PrettyPrinter pp(_os);
    Param::updateUsed(_node);
    pp.traverseNode(_node);
}

class PrettyPrinterTcContext : public pp::PrettyPrinterSyntax {
public:
    PrettyPrinterTcContext(std::wostream &_os) :
        PrettyPrinterSyntax(_os, true, 0), m_cInd(1)
    {}

    template<class T>
    void collectConditions(T _constraints) {
        for (auto& i: _constraints.getConditions())
            m_conditions.insert({m_cInd, i});

        if (!_constraints.getConditions().empty()) {
            m_os << L" [" << m_cInd << L"]";
            ++m_cInd;
        }
    }

    void print(const tc::CompoundFormula& _formula) {
        for (size_t i = 0; i < _formula.size(); ++i) {
            if (i > 0)
                m_os << L"\n  or ";

            m_os << setInline(true) << L"(";

            const tc::Formulas &part = _formula.getPart(i);
            tc::ContextStack::push(_formula.getPartPtr(i));

            for (tc::Formulas::iterator j = part.begin(); j != part.end(); ++j) {
                tc::Formula &g = **j;

                assert(!g.is(tc::Formula::COMPOUND));

                if (j != part.begin())
                    m_os << L" and ";

                PrettyPrinterSyntax::print(g.getLhs());
                m_os << L" " << fmtTypeFormulaOp(g.getKind()) << L" ";
                PrettyPrinterSyntax::print(g.getRhs());

                collectConditions(g);
            }

            tc::ContextStack::pop();
            m_os << setInline(false) << L")";

            collectConditions(part);
        }
    }

    void print(const tc::Formula &_formula) {
        if (!_formula.is(tc::Formula::COMPOUND)) {
            m_os << setInline(true);
            PrettyPrinterSyntax::print(_formula.getLhs());
            m_os << L" " << fmtTypeFormulaOp(_formula.getKind()) << L" ";
            PrettyPrinterSyntax::print(_formula.getRhs());
            m_os << setInline(false);
        } else
            print((const tc::CompoundFormula&)_formula);

        collectConditions(_formula);

        m_os << L"\n";
    }

    void print(tc::Context &_constraints) {
        m_os << setInline(false) << L"\n";

        size_t c = 0;

        for (tc::Formulas::iterator i = _constraints->begin(); i != _constraints->end(); ++i, ++c) {
            tc::Formula &f = **i;

            if (!f.is(tc::Formula::COMPOUND))
                m_os << c << L":  ";

            print(f);
        }

        for (auto& i: _constraints.getConditions())
            m_conditions.insert({0, i});

        if (!m_conditions.empty())
            m_os << L"----------\n";

        for (auto& i: m_conditions) {
            if (i.first != 0)
                m_os << L"[" << i.first << L"] ";
            m_os << setInline(true);
            PrettyPrinterSyntax::print(i.second);
            m_os << setInline(false) << L"\n";
        }

        m_os << L"----------\n";

        for (tc::Formulas::iterator i = _constraints.pSubsts->begin(); i != _constraints.pSubsts->end(); ++i) {
            tc::Formula &f = **i;

            assert(f.getLhs());
            assert(f.getRhs());

            m_os << setInline(true);
            PrettyPrinterSyntax::print(f.getLhs());
            m_os << L" -> ";
            PrettyPrinterSyntax::print(f.getRhs());
            m_os << setInline(false) << L"\n";
        }
    }

private:
    std::multimap<size_t, ExpressionPtr> m_conditions;
    size_t m_cInd;
};

void prettyPrint(tc::Context &_constraints, std::wostream &_os) {
    PrettyPrinterTcContext(_os).print(_constraints);
}

void prettyPrint(const tc::Formula &_formula, std::wostream &_os, bool _bNewLine) {
    pp::PrettyPrinterSyntax pp(_os, true, 0);

    if (!_formula.is(tc::Formula::COMPOUND)) {
        pp.print(_formula.getLhs());
        _os << L" " << fmtTypeFormulaOp(_formula.getKind()) << L" ";
        pp.print(_formula.getRhs());
    } else {
        tc::CompoundFormula &cf = (tc::CompoundFormula &)_formula;

        for (size_t j = 0; j < cf.size(); ++j) {
            if (j > 0)
                _os << L"\n  or ";

            _os << L"(";

            tc::Formulas &part = cf.getPart(j);

            tc::ContextStack::push(cf.getPartPtr(j));

            for (tc::Formulas::iterator k = part.begin(); k != part.end(); ++k) {
                tc::Formula &g = **k;

                assert(!g.is(tc::Formula::COMPOUND));

                if (k != part.begin())
                    _os << L" and ";

                pp.print(g.getLhs());
                _os << L" " << fmtTypeFormulaOp(g.getKind()) << L" ";
                pp.print(g.getRhs());
            }

            tc::ContextStack::pop();
            _os << L")";
        }
    }

    if (_bNewLine)
        _os << L"\n";
}
