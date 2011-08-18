/// \file prettyprinter.cpp
///

#include "prettyprinter.h"
#include "utils.h"
#include "ir/statements.h"
#include "ir/visitor.h"

#include <iostream>

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

std::map<size_t, std::wstring> g_freshTypes;

static std::wstring fmtFreshType(tc::FreshType &_type) {
    std::wstring strName = g_freshTypes[_type.getOrdinal()];

    if (strName.empty()) {
        const size_t nType = g_freshTypes.size() - 1;
        const int nChar = nType%26;
        const int nNum = nType/26;

        strName = L"A";
        strName[0] += nChar;

        if (nNum > 0)
            strName += fmtInt(nNum);

        if (_type.getFlags())
            strName += std::wstring(L":") + fmtInt(_type.getFlags());

        g_freshTypes[_type.getOrdinal()] = strName;
    }

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
        case Type::GENERIC: return L"generic";
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
    pp.traverseModule(_module);
}

void print(ir::Node &_node, std::wostream &_os) {
    PrettyPrinter pp(_os);
    pp.traverseNode(_node);
}

class PrettyPrinterCompact: public PrettyPrinterBase {
public:
    PrettyPrinterCompact(std::wostream &_os, NodePtr _pRoot, int _nFlags) : PrettyPrinterBase(_os), m_pRoot(_pRoot), m_nFlags(_nFlags) {}

    void print(Node &_node) {
        if (&_node == NULL)
            m_os << "NULL";
        else
            traverseNode(_node);
    }

    virtual bool visitNamedValue(NamedValue &_val) {
        if (&_val != m_pRoot.ptr()) {
            if (getLoc().bPartOfCollection && getLoc().cPosInCollection != 0)
                m_os << L", ";
            else if (getLoc().role == R_PredicateTypeOutParam && getLoc().cPosInCollection == 0)
                m_os << L" : ";
        }

        if (getRole() != R_EnumValueDecl)
            traverseType(*_val.getType());

        if (!_val.getName().empty() && getLoc().type != N_Param) {
            if (getRole() != R_EnumValueDecl)
                m_os << L" ";
            m_os << _val.getName();
        }

        return false;
    }

    virtual bool visitType(Type &_type) {
        if ((m_nFlags & PPC_NO_INCOMPLETE_TYPES) && (_type.getKind() == Type::FRESH || _type.getKind() == Type::GENERIC))
            return true;

        if (_type.getKind() == Type::FRESH) {
            m_os << fmtFreshType((tc::FreshType &)_type);;
        } else if (_type.getKind() <= Type::GENERIC) {
            m_os << fmtType(_type.getKind());

            if (_type.getKind() >= Type::NAT && _type.getKind() <= Type::REAL && _type.getBits() != Number::GENERIC) {
                if ((_type.getKind() <= Type::INT && !(m_nFlags & PPC_NO_INT_BITS)) ||
                        ((_type.getKind() == Type::REAL && !(m_nFlags & PPC_NO_REAL_BITS))))
                    m_os << L"(" << fmtBits(_type.getBits()) << L")";
            }
        }

        return true;
    }

    virtual bool visitTypeType(TypeType &_type) {
        m_os << L"<";
        if (TypePtr pType = _type.getDeclaration()->getType())
            traverseType(*pType);
        m_os << L">";
        return false;
    }

    virtual bool visitNamedReferenceType(NamedReferenceType &_type) {
        m_os << _type.getName();
        return true;
    }

    virtual bool visitSetType(SetType &_type) {
        m_os << L"{";
        traverseType(*_type.getBaseType());
        m_os << L"}";
        return false;
    }

    virtual bool visitListType(ListType &_type) {
        m_os << L"[[";
        traverseType(*_type.getBaseType());
        m_os << L"]]";
        return false;
    }

    virtual bool visitMapType(MapType &_type) {
        m_os << L"{";
        traverseType(*_type.getIndexType());
        m_os << L":";
        traverseType(*_type.getBaseType());
        m_os << L"}";
        return false;
    }

    virtual bool visitRange(Range &_type) {
        traverseExpression(*_type.getMin());
        m_os << L"..";
        traverseExpression(*_type.getMax());
        return false;
    }

    virtual bool visitLiteral(Literal &_lit) {
        m_os << fmtLiteral(_lit);
        return false;
    }

    virtual bool visitArrayType(ArrayType &_type) {
        traverseType(*_type.getBaseType());

        if (_type.getDimensions().empty())
            m_os << L"[]";
        else
            for (size_t i = 0; i < _type.getDimensions().size(); ++i) {
                m_os << L"[";
                traverseRange(*_type.getDimensions().get(i));
                m_os << L"]";
            }

        return false;
    }

    virtual bool traverseEnumType(EnumType &_type) {
        m_os << L"(";
        Visitor::traverseEnumType(_type);
        m_os << L")";
        return true;
    }

    virtual bool traverseStructType(StructType &_type) {
        m_os << L"(";
        VISITOR_ENTER(StructType, _type);
        VISITOR_TRAVERSE_COL(NamedValue, StructFieldDecl, _type.getNamesOrd());
        if (!_type.getNamesOrd().empty() && !_type.getTypesOrd().empty())
            m_os << L"; ";
        VISITOR_TRAVERSE_COL(NamedValue, StructFieldDecl, _type.getTypesOrd());

        if (!_type.getNamesSet().empty()) {
            // Ensure sorting for debug purposes (don't reorder source collection though).
            std::map<std::wstring, NamedValuePtr> sortedFieldsMap;
            NamedValues sortedFields;

            for (size_t i = 0; i < _type.getNamesSet().size(); ++i)
                sortedFieldsMap[_type.getNamesSet().get(i)->getName()] = _type.getNamesSet().get(i);

            for (std::map<std::wstring, NamedValuePtr>::iterator i = sortedFieldsMap.begin();
                    i != sortedFieldsMap.end(); ++i)
                sortedFields.add(i->second);

            m_os << L"; ";
            VISITOR_TRAVERSE_COL(NamedValue, StructFieldDecl, sortedFields);
        }
        m_os << L")";
        VISITOR_EXIT();
    }

    virtual bool traverseUnionType(UnionType &_type) {
        m_os << L"(";
        Visitor::traverseUnionType(_type);
        m_os << L")";
        return true;
    }

    virtual bool visitUnionConstructorDeclaration(UnionConstructorDeclaration &_cons) {
        if (&_cons != m_pRoot.ptr() && getLoc().bPartOfCollection && getLoc().cPosInCollection != 0)
            m_os << L", ";

        m_os << _cons.getName() << L"(";
        VISITOR_TRAVERSE_COL(NamedValue, UnionConsField, _cons.getFields());
        m_os << L")";
        return false;
    }

    virtual bool traversePredicateType(PredicateType &_type) {
        m_os << L"predicate(";
        Visitor::traversePredicateType(_type);
        m_os << L")";
        return true;
    }

    virtual bool visitStatement(Statement &_node) {
        return false;
    }

private:
    NodePtr m_pRoot;
    int m_nFlags;
};

void prettyPrint(tc::Context &_constraints, std::wostream &_os) {
    static PrettyPrinterCompact pp(_os, NULL, 0);

    _os << L"\n";

    size_t c = 0;

    for (tc::Formulas::iterator i = _constraints->begin(); i != _constraints->end(); ++i, ++c) {
        tc::Formula &f = **i;

        if (!f.is(tc::Formula::COMPOUND))
            _os << c << ":  ";

        prettyPrint(f, _os);
    }

    _os << L"----------\n";

    for (tc::Formulas::iterator i = _constraints.substs->begin(); i != _constraints.substs->end(); ++i) {
        tc::Formula &f = **i;

        assert(f.getLhs());
        assert(f.getRhs());

        pp.print(*f.getLhs());
        _os << L" -> ";
        pp.print(*f.getRhs());
        _os << L"\n";
    }
}

void prettyPrint(tc::Formula &_formula, std::wostream &_os) {
    static PrettyPrinterCompact pp(_os, NULL, 0);

    if (!_formula.is(tc::Formula::COMPOUND)) {
        pp.print(*_formula.getLhs());
        _os << L" " << fmtTypeFormulaOp(_formula.getKind()) << L" ";
        pp.print(*_formula.getRhs());
    } else {
        tc::CompoundFormula &cf = (tc::CompoundFormula &)_formula;

        for (size_t j = 0; j < cf.size(); ++j) {
            if (j > 0)
                _os << L"\n  or ";

            _os << L"(";

            tc::Formulas &part = cf.getPart(j);

            for (tc::Formulas::iterator k = part.begin(); k != part.end(); ++k) {
                tc::Formula &g = **k;

                assert(!g.is(tc::Formula::COMPOUND));

                if (k != part.begin())
                    _os << L" and ";

                pp.print(*g.getLhs());
                _os << L" " << fmtTypeFormulaOp(g.getKind()) << L" ";
                pp.print(*g.getRhs());
            }

            _os << L")";
        }
    }

    _os << L"\n";
}

void prettyPrintCompact(Node &_node, std::wostream &_os, int _nFlags) {
    PrettyPrinterCompact pp(_os, &_node, _nFlags);
    pp.traverseNode(_node);
}
