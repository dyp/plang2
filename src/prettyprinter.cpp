/// \file prettyprinter.cpp
///

#include "prettyprinter.h"
#include "utils.h"
#include "ir/statements.h"

#include <iostream>

using namespace ir;

std::map<ir::CType *, std::wstring> m_freshTypes;

class CPrettyPrinter {
public:
    CPrettyPrinter(std::wostream & _os, bool _bCompact = false) : m_os(_os), m_nLevel(0), m_bCompact(_bCompact) {}

    std::wostream & print(const CNamedValue & _val);
    std::wostream & print(const CNamedValues & _vals, const std::wstring & _name);
    std::wostream & print(CParam & _param);
    std::wostream & print(CParams & _params, const std::wstring & _name);
    std::wostream & print(CEnumValue & _val);
    std::wostream & print(CElementDefinition & _elem);
    std::wostream & print(CStructFieldDefinition & _field);
    std::wostream & print(CCollection<CExpression> & _exprs, const std::wstring & _name);

    std::wostream & print(CTypeDeclaration & _typeDecl);
    std::wostream & print(CVariableDeclaration & _varDecl);
    std::wostream & print(CFormulaDeclaration & _formulaDecl);
    std::wostream & print(CBranch & _branch);
    std::wostream & print(CPredicate & _pred);
    std::wostream & print(CProcess & _process);
    std::wostream & print(CModule & _module);

    std::wostream & print(CExpression & _expr);
    std::wostream & print(CStructFieldExpr & _expr);
    std::wostream & print(CArrayConstructor & _expr);
    std::wostream & print(CArrayIteration & _expr);
    std::wostream & print(CStructConstructor & _expr);
    std::wostream & print(CMapConstructor & _expr);
    std::wostream & print(CSetConstructor & _expr);
    std::wostream & print(CListConstructor & _expr);
    std::wostream & print(CLambda & _expr);
    std::wostream & print(CCastExpr & _cast);

    std::wostream & print(CType & _type);
    std::wostream & print(CParameterizedType & _type);
    std::wostream & print(CNamedReferenceType & _type);
    std::wostream & print(CRange & _type);
    std::wostream & print(CArrayType & _type);
    std::wostream & print(CDerivedType & _type, const std::wstring & _name);
    std::wostream & print(CSetType & _type);
    std::wostream & print(CMapType & _type);
    std::wostream & print(CStructType & _type);
    std::wostream & print(CUnionType & _type);
    std::wostream & print(CEnumType & _type);
    std::wostream & print(CSubtype & _type);
    std::wostream & print(CPredicateType & _type);

    std::wostream & print(CStatement & _stmt);
    std::wostream & print(CBlock & _block);
    std::wostream & print(CParallelBlock & _block);
    std::wostream & print(CAssignment & _assgn);
    std::wostream & print(CMultiassignment & _assgn);
    std::wostream & print(CSwitch & _switch);
    std::wostream & print(CIf & _if);
    std::wostream & print(CJump & _jump);
    std::wostream & print(CReceive & _recv);
    std::wostream & print(CSend & _send);
    std::wostream & print(CWith & _with);
    std::wostream & print(CFor & _for);
    std::wostream & print(CWhile & _while);
    std::wostream & print(CBreak & _break);
    std::wostream & print(CCall & _call);

private:
    std::wostream & m_os;
    int m_nLevel;
    bool m_bCompact;


    std::wstring fmtPair(const std::wstring & _strName, const std::wstring & _strValue) {
        if (m_bCompact)
            return _strValue;
        return fmtIndent(_strName) + L": " + fmtQuote(_strValue) + L"\n";
    }

    std::wstring fmtBits(int _bits) {
        std::wstring str;
        switch (_bits) {
            case CNumber::Generic: str = L"generic"; break;
            case CNumber::Native:  str = L"native";  break;
            default:               str = fmtInt(_bits);
        }
        if (m_bCompact)
            return _bits == CNumber::Generic ? L"" : std::wstring(L"(") + str + L")";
        return fmtPair(L"bits", str);
    }

    std::wstring fmtIndent(const std::wstring & _s) {
        std::wstring res;

        for (int i = 0; i < m_nLevel; ++ i)
            res += L"  ";

        return res + _s;
    }
};

static
std::wstring getUnaryOp(int _kind) {
    switch (_kind) {
        case CUnary::Plus:          return L"plus";
        case CUnary::Minus:         return L"minus";
        case CUnary::BoolNegate:    return L"bool_negate";
        case CUnary::BitwiseNegate: return L"bitwise_negate";
        default:                    return L"";
    }
}

static
std::wstring getQuantifier(int _kind) {
    switch (_kind) {
        case CFormula::Universal:   return L"forall";
        case CFormula::Existential: return L"exists";
        default:                    return L"";
    }
}

static
std::wstring getBinaryOp(int _kind) {
    switch (_kind) {
        case CBinary::Implies:         return L"implies";
        case CBinary::Iff:             return L"iff";
        case CBinary::BoolOr:          return L"bool_or";
        case CBinary::BoolXor:         return L"bool_xor";
        case CBinary::BoolAnd:         return L"bool_and";
        case CBinary::Equals:          return L"equals";
        case CBinary::NotEquals:       return L"not_equals";
        case CBinary::Less:            return L"less";
        case CBinary::LessOrEquals:    return L"less_or_equals";
        case CBinary::Greater:         return L"greater";
        case CBinary::GreaterOrEquals: return L"greater_or_equals";
        case CBinary::In:              return L"in";
        case CBinary::ShiftLeft:       return L"shift_left";
        case CBinary::ShiftRight:      return L"shift_right";
        case CBinary::Add:             return L"add";
        case CBinary::Subtract:        return L"subtract";
        case CBinary::Multiply:        return L"multiply";
        case CBinary::Divide:          return L"divide";
        case CBinary::Remainder:       return L"remainder";
        case CBinary::Power:           return L"power";
        default:                       return L"";
    }
}

std::wostream & CPrettyPrinter::print(CStructFieldExpr & _expr) {
    ++ m_nLevel;
    m_os << fmtPair(L"component_kind", L"struct_field");

    m_os << fmtPair(L"field", _expr.getFieldName());

    /*m_os << fmtIndent(L"field:\n");

    print(* _expr.getField());

    m_os << fmtIndent(L"struct_type:\n");
    print(* _expr.getStructType());*/

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CArrayConstructor & _expr) {
    ++ m_nLevel;
    m_os << fmtPair(L"constructor_kind", L"array_elements");

    for (size_t i = 0; i < _expr.size(); ++ i) {
        m_os << fmtIndent(fmtInt(i,  L"element [%d]:\n"));
        print(* _expr.get(i));
    }

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CArrayIteration & _expr) {
    ++ m_nLevel;
    m_os << fmtPair(L"constructor_kind", L"array_iteration");

    print(_expr.getIterators(), L"iterator");

    for (size_t i = 0; i < _expr.size(); ++ i) {
        m_os << fmtIndent(fmtInt(i,  L"part [%d]:\n"));
        ++ m_nLevel;
        print(_expr.get(i)->getConditions(), L"condition");
        m_os << fmtIndent(L"expression:\n");
        print(* _expr.get(i)->getExpression());
        -- m_nLevel;
    }

    if (_expr.getDefault()) {
        m_os << fmtIndent(L"default:\n");
        print(* _expr.getDefault());
    }

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CMapConstructor & _expr) {
    ++ m_nLevel;
    m_os << fmtPair(L"constructor_kind", L"map_elements");

    for (size_t i = 0; i < _expr.size(); ++ i) {
        m_os << fmtIndent(fmtInt(i,  L"element [%d]:\n"));
        print(* _expr.get(i));
    }

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CSetConstructor & _expr) {
    ++ m_nLevel;
    m_os << fmtPair(L"constructor_kind", L"set_elements");

    for (size_t i = 0; i < _expr.size(); ++ i) {
        m_os << fmtIndent(fmtInt(i,  L"element [%d]:\n"));
        print(* _expr.get(i));
    }

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CListConstructor & _expr) {
    ++ m_nLevel;
    m_os << fmtPair(L"constructor_kind", L"list_elements");

    for (size_t i = 0; i < _expr.size(); ++ i) {
        m_os << fmtIndent(fmtInt(i,  L"element [%d]:\n"));
        print(* _expr.get(i));
    }

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CStructConstructor & _expr) {
    ++ m_nLevel;
    m_os << fmtPair(L"constructor_kind", L"struct_fields");

    for (size_t i = 0; i < _expr.size(); ++ i) {
        m_os << fmtIndent(fmtInt(i,  L"field [%d]:\n"));
        print(* _expr.get(i));
    }

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CLambda & _expr) {
    ++ m_nLevel;

    m_os << fmtPair(L"kind", L"lambda");
    print(_expr.getPredicate().getInParams(), L"in_param");

    for (size_t i = 0; i < _expr.getPredicate().getOutParams().size(); ++ i) {
        m_os << fmtIndent(fmtInt(i,  L"branch [%d]:\n"));
        print(* _expr.getPredicate().getOutParams().get(i));
    }

    if (_expr.getPredicate().getBlock()) {
        m_os << fmtIndent(L"body:\n");
        print(* _expr.getPredicate().getBlock());
    }

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CExpression & _expr) {
    ++ m_nLevel;

    if (_expr.getKind() == CExpression::Literal) {
        m_os << fmtPair(L"kind", L"literal");

        CLiteral & lit = * (CLiteral *) (& _expr);
        wchar_t c;

        switch (lit.getLiteralKind()) {
            case CLiteral::Unit:
                m_os << fmtPair(L"literal_kind", L"unit");
                break;
            case CLiteral::Number:
                m_os << fmtPair(L"literal_kind", L"number");
                m_os << fmtPair(L"value", lit.getNumber().toString());
                break;
            case CLiteral::Bool:
                m_os << fmtPair(L"literal_kind", L"bool");
                m_os << fmtPair(L"value", lit.getBool() ? L"true" : L"false");
                break;
            case CLiteral::Char:
                m_os << fmtPair(L"literal_kind", L"char");
                c = lit.getChar();
                m_os << fmtPair(L"value", std::wstring(& c, 1));
                break;
            case CLiteral::String:
                m_os << fmtPair(L"literal_kind", L"string");
                m_os << fmtPair(L"value", lit.getString());
                break;
        }
    } else if (_expr.getKind() == CExpression::Var) {
        m_os << fmtPair(L"kind", L"variable_reference");

        CVariableReference & var = * (CVariableReference *) (& _expr);

        m_os << fmtPair(L"name", var.getName());
    } else if (_expr.getKind() == CExpression::Type) {
        m_os << fmtPair(L"kind", L"type_expression");

        CTypeExpr & var = * (CTypeExpr *) (& _expr);

        m_os << fmtIndent(L"contents:\n");
        print(* var.getContents());
    } else if (_expr.getKind() == CExpression::FunctionCall) {
        m_os << fmtPair(L"kind", L"function_call");

        CFunctionCall & call = * (CFunctionCall *) (& _expr);

        if (call.getPredicate()) {
            m_os << fmtIndent(L"predicate_expr:\n");
            print(* call.getPredicate());
        }

        print(call.getParams(), L"param");
    } else if (_expr.getKind() == CExpression::Binder) {
        m_os << fmtPair(L"kind", L"binder");

        CBinder & binder = * (CBinder *) (& _expr);

        if (binder.getPredicate()) {
            m_os << fmtIndent(L"predicate_expr:\n");
            print(* binder.getPredicate());
        }

        print(binder.getParams(), L"param");
    } else if (_expr.getKind() == CExpression::FormulaCall) {
        m_os << fmtPair(L"kind", L"formula_call");

        CFormulaCall & call = * (CFormulaCall *) (& _expr);

        print(call.getParams(), L"param");
    } else if (_expr.getKind() == CExpression::Predicate) {
        m_os << fmtPair(L"kind", L"predicate_reference");

        CPredicateReference & pred = * (CPredicateReference *) (& _expr);

        if (pred.getTarget())
            m_os << fmtPair(L"name", pred.getTarget()->getName());
        else
            m_os << fmtPair(L"name", pred.getName());
    } else if (_expr.getKind() == CExpression::Component) {
        m_os << fmtPair(L"kind", L"component");

        CComponent & comp = * (CComponent *) (& _expr);

        if (comp.getObject()) {
            m_os << fmtIndent(L"object:\n");
            print(* comp.getObject());
        }

        switch (comp.getComponentKind()) {
            case CComponent::ArrayPart:
                m_os << fmtPair(L"component_kind", L"array_part");
                print(((CArrayPartExpr *) (& _expr))->getIndices(), L"index");
                break;
            case CComponent::Replacement:
                m_os << fmtPair(L"component_kind", L"replacement");
                m_os << fmtIndent(L"new_values:\n");
                print(* ((CReplacement *) (& _expr))->getNewValues());
                break;
            case CComponent::StructField:
                -- m_nLevel;
                print(* (CStructFieldExpr *) (& comp));
                ++ m_nLevel;
                break;
        }
    } else if (_expr.getKind() == CExpression::Constructor) {
        m_os << fmtPair(L"kind", L"constructor");

        CConstructor & cons = * (CConstructor *) (& _expr);

        switch (cons.getConstructorKind()) {
            case CConstructor::ArrayElements:
                print(* (CArrayConstructor *) (& cons));
                break;
            case CConstructor::MapElements:
                print(* (CMapConstructor *) (& cons));
                break;
            case CConstructor::StructFields:
                print(* (CStructConstructor *) (& cons));
                break;
            case CConstructor::SetElements:
                print(* (CSetConstructor *) (& cons));
                break;
            case CConstructor::ListElements:
                print(* (CListConstructor *) (& cons));
                break;
            case CConstructor::ArrayIteration:
                print(* (CArrayIteration *) (& cons));
                break;
        }
    } else if (_expr.getKind() == CExpression::Lambda) {
        -- m_nLevel;
        print(* (CLambda *) (& _expr));
        ++ m_nLevel;
    } else if (_expr.getKind() == CExpression::Cast) {
        -- m_nLevel;
        print(* (CCastExpr *) (& _expr));
        ++ m_nLevel;
    } else if (_expr.getKind() == CExpression::Formula) {
        m_os << fmtPair(L"kind", L"formula");

        CFormula & formula = * (CFormula *) (& _expr);

        m_os << fmtPair(L"quantifier", getQuantifier(formula.getQuantifier()));

        print(formula.getBoundVariables(), L"bound_variable");

        if (formula.getSubformula()) {
            m_os << fmtIndent(L"subformula:\n");
            print(* formula.getSubformula());
        }
    } else if (_expr.getKind() == CExpression::Unary) {
        m_os << fmtPair(L"kind", L"unary");

        CUnary & unary = * (CUnary *) (& _expr);

        m_os << fmtPair(L"operator", getUnaryOp(unary.getOperator()));

        if (unary.getExpression()) {
            m_os << fmtIndent(L"operand:\n");
            print(* unary.getExpression());
        }

        m_os << fmtPair(L"overflow", fmtInt(unary.overflow().get()));
    } else if (_expr.getKind() == CExpression::Binary) {
        m_os << fmtPair(L"kind", L"binary");

        CBinary & binary = * (CBinary *) (& _expr);

        m_os << fmtPair(L"operator", getBinaryOp(binary.getOperator()));

        if (binary.getLeftSide()) {
            m_os << fmtIndent(L"left_operand:\n");
            print(* binary.getLeftSide());
        }

        if (binary.getRightSide()) {
            m_os << fmtIndent(L"right_operand:\n");
            print(* binary.getRightSide());
        }

        m_os << fmtPair(L"overflow", fmtInt(binary.overflow().get()));
    } else if (_expr.getKind() == CExpression::Ternary) {
        m_os << fmtPair(L"kind", L"ternary");

        CTernary & ternary = * (CTernary *) (& _expr);

        if (ternary.getIf()) {
            m_os << fmtIndent(L"if_operand:\n");
            print(* ternary.getIf());
        }

        if (ternary.getThen()) {
            m_os << fmtIndent(L"then_operand:\n");
            print(* ternary.getThen());
        }

        if (ternary.getElse()) {
            m_os << fmtIndent(L"else_operand:\n");
            print(* ternary.getElse());
        }
    }

    if (_expr.getType()) {
        m_os << fmtIndent(L"type:\n");
        print(* _expr.getType());
    }

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(const CNamedValue & _val) {
    if (m_bCompact) {
        print(* _val.getType());
        if (!_val.getName().empty())
            m_os << L" " << _val.getName();
        return m_os;
    }

    ++ m_nLevel;
    m_os << fmtPair(L"name", _val.getName());
    m_os << fmtIndent(L"type:\n");
    print(* _val.getType());
    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CCollection<CExpression> & _exprs, const std::wstring & _name) {
    for (size_t i = 0; i < _exprs.size(); ++ i) {
        if (_exprs.get(i)) {
            m_os << fmtIndent(_name + fmtInt(i,  L" [%d]:\n"));
            print(* _exprs.get(i));
        } else
            m_os << fmtIndent(_name + fmtInt(i,  L" [%d]: nil\n"));
    }

    return m_os;
}

std::wostream & CPrettyPrinter::print(const CNamedValues & _vals, const std::wstring & _name) {
    if (m_bCompact) {
        for (size_t i = 0; i < _vals.size(); ++ i) {
            if (i > 0)
                m_os << L", ";
            print(* _vals.get(i));
        }

        return m_os;
    }

    for (size_t i = 0; i < _vals.size(); ++ i) {
        m_os << fmtIndent(_name + fmtInt(i,  L" [%d]:\n"));
        print(* _vals.get(i));
    }

    return m_os;
}

std::wostream & CPrettyPrinter::print(CParam & _param) {
    print(* (CNamedValue *) (& _param));
    if (! m_bCompact) {
        ++ m_nLevel;
        m_os << fmtPair(L"is_output", (_param.isOutput() ? L"true" : L"false"));
        m_os << fmtPair(L"has_linked", (_param.getLinkedParam() ? L"true" : L"false"));
        -- m_nLevel;
    }
    return m_os;
}

std::wostream & CPrettyPrinter::print(CParams & _params, const std::wstring & _name) {
    if (m_bCompact) {
        for (size_t i = 0; i < _params.size(); ++ i) {
            if (i > 0)
                m_os << L", ";
            print(* _params.get(i));
        }

        return m_os;
    }

    for (size_t i = 0; i < _params.size(); ++ i) {
        m_os << fmtIndent(_name + fmtInt(i,  L" [%d]:\n"));
        print(* _params.get(i));
    }

    return m_os;
}

std::wostream & CPrettyPrinter::print(CEnumValue & _val) {
    ++ m_nLevel;
    m_os << fmtPair(L"name", _val.getName());
    m_os << fmtPair(L"ordinal", fmtInt(_val.getOrdinal()));
    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CElementDefinition & _elem) {
    ++ m_nLevel;

    if (_elem.getIndex()) {
        m_os << fmtIndent(L"index:\n");
        print(* _elem.getIndex());
    }

    if (_elem.getValue()) {
        m_os << fmtIndent(L"value:\n");
        print(* _elem.getValue());
    }

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CStructFieldDefinition & _field) {
    ++ m_nLevel;

    m_os << fmtPair(L"field_name", _field.getName());

    if (_field.getValue()) {
        m_os << fmtIndent(L"value:\n");
        print(* _field.getValue());
    }

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CParameterizedType & _type) {
    ++ m_nLevel;
    m_os << fmtPair(L"kind", L"parameterized") << fmtIndent(L"actual_type:\n");
    print(* _type.getActualType());
    print(_type.getParams(), L"param");
    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CNamedReferenceType & _type) {
    ++ m_nLevel;

    if (! m_bCompact)
        m_os << fmtPair(L"kind", L"named_reference");

    if (_type.getDeclaration()) {
        m_os << fmtPair(L"declaration", _type.getDeclaration()->getName());
    } else if (_type.getVariable()) {
        m_os << fmtPair(L"parameter", _type.getVariable()->getName());
    }

    print(_type.getParams(), L"param");
    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CRange & _type) {
    ++ m_nLevel;
    m_os << fmtPair(L"kind", L"range");

    if (_type.getMin()) {
        m_os << fmtIndent(L"min:\n");
        print(* _type.getMin());
    }

    if (_type.getMax()) {
        m_os << fmtIndent(L"max:\n");
        print(* _type.getMax());
    }

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CDerivedType & _type, const std::wstring & _name) {
    ++ m_nLevel;
    m_os << fmtPair(L"kind", _name);

    if (_type.getBaseType()) {
        m_os << fmtIndent(L"base_type:\n");
        print(* _type.getBaseType());
    }

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CSetType & _type) {
    if (m_bCompact) {
        m_os << L"{";
        print(*_type.getBaseType());
        m_os << L"}";
    } else
        print(* (CDerivedType *) (& _type), L"set");

    return m_os;
}

std::wostream & CPrettyPrinter::print(CMapType & _type) {
    print(* (CDerivedType *) (& _type), L"map");
    ++ m_nLevel;

    if (_type.getIndexType()) {
        m_os << fmtIndent(L"index_type:\n");
        print(* _type.getIndexType());
    }

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CArrayType & _type) {
    print(* (CDerivedType *) (& _type), L"array");
    ++ m_nLevel;

    for (size_t i = 0; i < _type.getDimensions().size(); ++ i) {
        m_os << fmtIndent(fmtInt(i,  L"dimension [%d]:\n"));
        print(* _type.getDimensions().get(i));
    }

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CStructType & _type) {
    ++ m_nLevel;
    if (m_bCompact) {
        m_os << L"(";
        print(_type.getFields(), L"");
        m_os << L")";
    } else {
        m_os << fmtPair(L"kind", L"struct");
        print(_type.getFields(), L"field");
    }
    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CUnionType & _type) {
    ++ m_nLevel;
    m_os << fmtPair(L"kind", L"union");
    if (m_bCompact) {
        m_os << L"(";
        for (size_t i = 0; i < _type.getConstructors().size(); ++ i) {
            const ir::CUnionConstructorDefinition * pCons = _type.getConstructors().get(i);

            if (i > 0)
                m_os << L", ";

            m_os << pCons->getName();

            if (! pCons->getStruct().getFields().empty()) {
                m_os << L"(";
                print(pCons->getStruct().getFields(), std::wstring(L""));
                m_os << L")";
            }
        }
        m_os << L")";
    }
    //print(_type.getConstructors(), L"constructors");
    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CEnumType & _type) {
    ++ m_nLevel;
    m_os << fmtPair(L"kind", L"enum");

    for (size_t i = 0; i < _type.getValues().size(); ++ i) {
        m_os << fmtIndent(fmtInt(i,  L"value [%d]:\n"));
        print(* _type.getValues().get(i));
    }

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CSubtype & _type) {
    ++ m_nLevel;
    m_os << fmtPair(L"kind", L"subtype");

    if (_type.getParam()) {
        m_os << fmtIndent(L"parameter:\n");
        print(* _type.getParam());
    }

    if (_type.getExpression()) {
        m_os << fmtIndent(L"predicate:\n");
        print(* _type.getExpression());
    }

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CPredicateType & _type) {
    if (m_bCompact) {
        m_os << L"predicate(";
        print(_type.getInParams(), L"");
        for (size_t i = 0; i < _type.getOutParams().size(); ++ i) {
            m_os << L" : ";
            print(* _type.getOutParams().get(i));
        }
        m_os << L")";
        return m_os;
    }

    ++ m_nLevel;
    m_os << fmtPair(L"kind", L"predicate");

    print(_type.getInParams(), L"in_param");

    for (size_t i = 0; i < _type.getOutParams().size(); ++ i) {
        m_os << fmtIndent(fmtInt(i,  L"branch [%d]:\n"));
        print(* _type.getOutParams().get(i));
    }

    if (_type.getPreCondition()) {
        m_os << fmtIndent(L"precondition:\n");
        print(* _type.getPreCondition());
    }

    if (_type.getPostCondition()) {
        m_os << fmtIndent(L"postcondition:\n");
        print(* _type.getPostCondition());
    }

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CType & _type) {
    ++ m_nLevel;

    if (& _type == NULL) {
        m_os << fmtPair(L"kind", L"NULL");
        return m_os;
    }

    switch (_type.getKind()) {
        case CType::Unit:    m_os << fmtPair(L"kind", L"unit"); break;
        case CType::Int:     m_os << fmtPair(L"kind", L"int"); break;
        case CType::Nat:     m_os << fmtPair(L"kind", L"nat"); break;
        case CType::Real:    m_os << fmtPair(L"kind", L"real"); break;
        case CType::Bool:    m_os << fmtPair(L"kind", L"bool"); break;
        case CType::Char:    m_os << fmtPair(L"kind", L"char"); break;
        case CType::String:  m_os << fmtPair(L"kind", L"string"); break;
        case CType::Type:    m_os << fmtPair(L"kind", L"type"); break;
        case CType::Generic: m_os << fmtPair(L"kind", L"var"); break;
        case CType::Parameterized:
            print(* (CParameterizedType *) (& _type)); break;
        case CType::NamedReference:
            print(* (CNamedReferenceType *) (& _type)); break;
        case CType::Range:
            print(* (CRange *) (& _type)); break;
        case CType::Array:
            print(* (CArrayType *) (& _type)); break;
        case CType::Map:
            print(* (CMapType *) (& _type)); break;
        case CType::Optional:
            print(* (CDerivedType *) (& _type), L"optional"); break;
        case CType::Seq:
            print(* (CDerivedType *) (& _type), L"seq"); break;
        case CType::Set:
            print(* (CSetType *) (& _type)); break;
        case CType::List:
            print(* (CDerivedType *) (& _type), L"list"); break;
        case CType::Struct:
            print(* (CStructType *) (& _type)); break;
        case CType::Union:
            print(* (CUnionType *) (& _type)); break;
        case CType::Enum:
            print(* (CEnumType *) (& _type)); break;
        case CType::Subtype:
            print(* (CSubtype *) (& _type)); break;
        case CType::Predicate:
            print(* (CPredicateType *) (& _type)); break;
        case CType::Fresh: {
            std::wstring strName = m_freshTypes[& _type];
            if (strName.empty()) {
                const size_t nType = m_freshTypes.size() - 1;
                const int nChar = nType%26;
                const int nNum = nType/26;
                strName = L"A";
                strName[0] += nChar;
                if (nNum > 0)
                    strName += fmtInt(nNum);
                m_freshTypes[& _type] = strName;
            }
            m_os << fmtPair(L"fresh", strName);
            if (! m_bCompact)
                m_os << fmtPair(L"flags", fmtInt(((tc::FreshType &) _type).getFlags(), L"0x%02X")); break;
            //m_os << strName;
            break;
        }

    }

    switch (_type.getKind()) {
        case CType::Int:
        case CType::Nat:
        case CType::Real:
            m_os << fmtBits(_type.getBits());
    }

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CTypeDeclaration & _typeDecl) {
    if (_typeDecl.getType() == NULL)
        return m_os;

    ++ m_nLevel;

    m_os << fmtPair(L"kind", L"type_declaration");
    m_os << fmtPair(L"name", _typeDecl.getName()) << fmtIndent(L"type:\n");
    print(* _typeDecl.getType());

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CVariableDeclaration & _varDecl) {
    if (_varDecl.getVariable().getType() == NULL)
        return m_os;

    ++ m_nLevel;
    m_os << fmtPair(L"kind", L"variable_declaration");
    -- m_nLevel;

    print(_varDecl.getVariable());

    ++ m_nLevel;
    CExpression * pExpr = _varDecl.getValue();

    if (pExpr) {
        m_os << fmtIndent(L"value:\n");
        print(* pExpr);
    }

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CBranch & _branch) {
    ++ m_nLevel;
    if (_branch.getLabel())
        m_os << fmtPair(L"label", _branch.getLabel()->getName());
    print(* (CParams *) (& _branch), L"out_param");

    if (_branch.getPreCondition()) {
        m_os << fmtIndent(L"precondition:\n");
        print(* _branch.getPreCondition());
    }

    if (_branch.getPostCondition()) {
        m_os << fmtIndent(L"postcondition:\n");
        print(* _branch.getPostCondition());
    }

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CStatement & _stmt) {
    if (_stmt.getLabel()) {
        ++ m_nLevel;
        m_os << fmtPair(L"label", _stmt.getLabel()->getName());
        -- m_nLevel;
    }

    switch(_stmt.getKind()) {
        case CStatement::Block:
            return print(* (CBlock *) (& _stmt));
        case CStatement::ParallelBlock:
            return print(* (CParallelBlock *) (& _stmt));
        case CStatement::Assignment:
            return print(* (CAssignment *) (& _stmt));
        case CStatement::Multiassignment:
            return print(* (CMultiassignment *) (& _stmt));
        case CStatement::Switch:
            return print(* (CSwitch *) (& _stmt));
        case CStatement::If:
            return print(* (CIf *) (& _stmt));
        case CStatement::Jump:
            return print(* (CJump *) (& _stmt));
        case CStatement::Receive:
            return print(* (CReceive *) (& _stmt));
        case CStatement::Send:
            return print(* (CSend *) (& _stmt));
        case CStatement::With:
            return print(* (CWith *) (& _stmt));
        case CStatement::For:
            return print(* (CFor *) (& _stmt));
        case CStatement::While:
            return print(* (CWhile *) (& _stmt));
        case CStatement::Break:
            return print(* (CBreak *) (& _stmt));
        case CStatement::Call:
            return print(* (CCall *) (& _stmt));

        case CStatement::TypeDeclaration:
            return print(* (CTypeDeclaration *) (& _stmt));
        case CStatement::VariableDeclaration:
            return print(* (CVariableDeclaration *) (& _stmt));
        case CStatement::PredicateDeclaration:
            return print(* (CPredicate *) (& _stmt));
    }

    ++ m_nLevel;
    m_os << fmtIndent(L"nop\n");
    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CBlock & _block) {
    ++ m_nLevel;

    m_os << fmtPair(L"kind", L"block");

    for (size_t i = 0; i < _block.size(); ++ i) {
        m_os << fmtIndent(fmtInt(i,  L"statement [%d]:\n"));
        print(* _block.get(i));
    }

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CParallelBlock & _block) {
    ++ m_nLevel;

    m_os << fmtPair(L"kind", L"parallel_block");

    for (size_t i = 0; i < _block.size(); ++ i) {
        m_os << fmtIndent(fmtInt(i,  L"statement [%d]:\n"));
        print(* _block.get(i));
    }

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CAssignment & _assgn) {
    ++ m_nLevel;

    m_os << fmtPair(L"kind", L"assignment");
    m_os << fmtIndent(L"l-value:\n");
    print(* _assgn.getLValue());
    m_os << fmtIndent(L"r-value:\n");
    print(* _assgn.getExpression());

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CMultiassignment & _assgn) {
    ++ m_nLevel;

    m_os << fmtPair(L"kind", L"multiassignment");

    print(_assgn.getLValues(), L"l-value");
    print(_assgn.getExpressions(), L"r-value");

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CSwitch & _switch) {
    ++ m_nLevel;

    m_os << fmtPair(L"kind", L"switch");

    m_os << fmtIndent(L"param:\n");
    print(* _switch.getParam());

    for (size_t i = 0; i < _switch.size(); ++ i) {
        m_os << fmtIndent(fmtInt(i,  L"case [%d]:\n"));
        ++ m_nLevel;
        print(_switch.get(i)->getExpressions(), L"condition");
        m_os << fmtIndent(L"body:\n");
        print(* _switch.get(i)->getBody());
        -- m_nLevel;
    }

    if (_switch.getDefault()) {
        m_os << fmtIndent(L"default:\n");
        print(* _switch.getDefault());
    }

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CIf & _if) {
    ++ m_nLevel;

    m_os << fmtPair(L"kind", L"if");
    m_os << fmtIndent(L"param:\n");
    print(* _if.getParam());
    m_os << fmtIndent(L"body:\n");
    print(* _if.getBody());

    if (_if.getElse()) {
        m_os << fmtIndent(L"else:\n");
        print(* _if.getElse());
    }

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CJump & _jump) {
    ++ m_nLevel;

    m_os << fmtPair(L"kind", L"jump");
    m_os << fmtPair(L"target", _jump.getDestination()->getName());

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CReceive & _recv) {
    ++ m_nLevel;

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CSend & _send) {
    ++ m_nLevel;

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CWith & _with) {
    ++ m_nLevel;

    m_os << fmtPair(L"kind", L"with");

    print(_with.getParams(), L"param");
    m_os << fmtIndent(L"body:\n");
    print(* _with.getBody());

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CFor & _for) {
    ++ m_nLevel;

    m_os << fmtPair(L"kind", L"for");

    if (_for.getIterator()) {
        m_os << fmtIndent(L"iterator:\n");
        print(* _for.getIterator());
    }

    if (_for.getInvariant()) {
        m_os << fmtIndent(L"invariant:\n");
        print(* _for.getInvariant());
    }

    if (_for.getIncrement()) {
        m_os << fmtIndent(L"increment:\n");
        print(* _for.getIncrement());
    }

    m_os << fmtIndent(L"body:\n");
    print(* _for.getBody());

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CWhile & _while) {
    ++ m_nLevel;

    m_os << fmtPair(L"kind", L"while");

    m_os << fmtIndent(L"invariant:\n");
    print(* _while.getInvariant());
    m_os << fmtIndent(L"body:\n");
    print(* _while.getBody());

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CBreak & _break) {
    ++ m_nLevel;

    m_os << fmtPair(L"kind", L"break");

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CCall & _call) {
    ++ m_nLevel;

    m_os << fmtPair(L"kind", L"call");

    if (_call.getPredicate()) {
        m_os << fmtIndent(L"predicate_expr:\n");
        print(* _call.getPredicate());
    }

    for (size_t j = 0; j < _call.getDeclarations().size(); ++ j) {
        m_os << fmtIndent(fmtInt(j, L"declaration [%d]:\n"));
        print(* _call.getDeclarations().get(j));
    }

    print(_call.getParams(), L"param");

    for (size_t i = 0; i < _call.getBranches().size(); ++ i) {
        CCallBranch * pBranch = _call.getBranches().get(i);
        m_os << fmtIndent(fmtInt(i, L"branch [%d]:\n"));
        ++ m_nLevel;
        print(* pBranch, L"param");
        -- m_nLevel;
        if (pBranch->getHandler()) {
            ++ m_nLevel;
            m_os << fmtIndent(L"handler:\n");
            print(* pBranch->getHandler());
            -- m_nLevel;
        }
    }

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CCastExpr & _cast) {
    ++ m_nLevel;

    m_os << fmtPair(L"kind", L"cast");

    m_os << fmtIndent(L"expression:\n");
    if (_cast.getExpression())
        print(* _cast.getExpression());
    m_os << fmtIndent(L"to_type:\n");
    if (_cast.getToType())
        print(* _cast.getToType());

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CPredicate & _pred) {
    ++ m_nLevel;

    m_os << fmtPair(L"kind", L"predicate_declaration");
    m_os << fmtPair(L"name", _pred.getName());
    print(_pred.getInParams(), L"in_param");

    for (size_t i = 0; i < _pred.getOutParams().size(); ++ i) {
        m_os << fmtIndent(fmtInt(i,  L"branch [%d]:\n"));
        print(* _pred.getOutParams().get(i));
    }

    if (_pred.getPreCondition()) {
        m_os << fmtIndent(L"precondition:\n");
        print(* _pred.getPreCondition());
    }

    if (_pred.getPostCondition()) {
        m_os << fmtIndent(L"postcondition:\n");
        print(* _pred.getPostCondition());
    }

    if (_pred.getBlock()) {
        m_os << fmtIndent(L"body:\n");
        print(* _pred.getBlock());
    }

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CProcess & _process) {
    ++ m_nLevel;

    m_os << fmtPair(L"name", _process.getName());
    print(_process.getInParams(), L"in_param");

    for (size_t i = 0; i < _process.getOutParams().size(); ++ i) {
        m_os << fmtIndent(fmtInt(i,  L"branch [%d]:\n"));
        print(* _process.getOutParams().get(i));
    }

    if (_process.getBlock()) {
        m_os << fmtIndent(L"body:\n");
        print(* _process.getBlock());
    }

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CFormulaDeclaration & _formulaDecl) {
    ++ m_nLevel;

    m_os << fmtPair(L"name", _formulaDecl.getName());
    print(_formulaDecl.getParams(), L"_param");

    if (_formulaDecl.getFormula()) {
        m_os << fmtIndent(L"formula:\n");
        print(* _formulaDecl.getFormula());
    }

    -- m_nLevel;
    return m_os;
}

std::wostream & CPrettyPrinter::print(CModule & _module) {
    ++ m_nLevel;

    m_os << fmtPair(L"name", _module.getName());

    for (size_t i = 0; i < _module.getImports().size(); ++ i)
        m_os << fmtPair(fmtInt(i,  L"import [%d]"), _module.getImports()[i]);

    for (size_t i = 0; i < _module.getTypes().size(); ++ i) {
        CTypeDeclaration * pDecl = _module.getTypes().get(i);
        m_os << fmtIndent(fmtInt(i,  L"type_declaration [%d]:\n"));
        print(* pDecl);
    }

    for (size_t i = 0; i < _module.getFormulas().size(); ++ i) {
        CFormulaDeclaration * pDecl = _module.getFormulas().get(i);
        m_os << fmtIndent(fmtInt(i,  L"formula_declaration [%d]:\n"));
        print(* pDecl);
    }

    for (size_t i = 0; i < _module.getVariables().size(); ++ i) {
        CVariableDeclaration * pDecl = _module.getVariables().get(i);
        m_os << fmtIndent(fmtInt(i,  L"variable_declaration [%d]:\n"));
        print(* pDecl);
    }

    for (size_t i = 0; i < _module.getPredicates().size(); ++ i) {
        CPredicate * pPred = _module.getPredicates().get(i);
        m_os << fmtIndent(fmtInt(i,  L"predicate_declaration [%d]:\n"));
        print(* pPred);
    }

    for (size_t i = 0; i < _module.getProcesses().size(); ++ i) {
        CProcess * pProcess = _module.getProcesses().get(i);
        m_os << fmtIndent(fmtInt(i,  L"process_declaration [%d]:\n"));
        print(* pProcess);
    }

    -- m_nLevel;
    return m_os;
}

void prettyPrint(tc::Formulas & _constraints, std::wostream & _os) {
    static CPrettyPrinter pp(_os, true);

    _os << L"\n";

    for (tc::Formulas::iterator i = _constraints.begin(); i != _constraints.end(); ++ i) {
        tc::Formula & f = ** i;

        if (! f.is(tc::Formula::Compound)) {
            pp.print(* f.getLhs());
            _os << (f.is(tc::Formula::Equals) ? L" = " :
                (f.is(tc::Formula::Subtype) ? L" <= " : L" < "));
            pp.print(* f.getRhs());
        } else {
            tc::CompoundFormula & cf = (tc::CompoundFormula &) f;

            for (size_t j = 0; j < cf.size(); ++ j) {
                if (j > 0)
                    _os << L"\n  or ";
                _os << L"(";

                tc::Formulas & part = cf.getPart(j);

                for (tc::Formulas::iterator k = part.begin(); k != part.end(); ++ k) {
                    tc::Formula & g = ** k;
                    assert(! g.is(tc::Formula::Compound));
                    if (k != part.begin())
                        _os << L" and ";
                    pp.print(* g.getLhs());
                    _os << (g.is(tc::Formula::Equals) ? L" = " :
                        (g.is(tc::Formula::Subtype) ? L" <= " : L" < "));
                    pp.print(* g.getRhs());
                }
                _os << L")";

                if (! part.substs.empty()) {
                    _os << L"\t|    ";

                    for (tc::FormulaSet::iterator j = part.substs.begin(); j != part.substs.end(); ++ j) {
                        tc::Formula & g = ** j;

                        if (j != part.substs.begin())
                            _os << L", ";
                        assert(g.getLhs() != NULL);
                        assert(g.getRhs() != NULL);

                        pp.print(* g.getLhs());
                        _os << L" -> ";
                        pp.print(* g.getRhs());
                    }
                }
            }
        }
        /*pp.print(* i->lhs());
        _os << L" = ";
        pp.print(i->rhs());*/
        _os << L"\n";
    }

    _os << L"----------\n";

    for (tc::FormulaSet::iterator i = _constraints.substs.begin(); i != _constraints.substs.end(); ++ i) {
        tc::Formula & f = ** i;

        assert(f.getLhs() != NULL);
        assert(f.getRhs() != NULL);

        pp.print(* f.getLhs());
        _os << L" -> ";
        pp.print(* f.getRhs());
        _os << L"\n";
    }
}

void prettyPrint(CModule & _module, std::wostream & _os) {
    CPrettyPrinter pp(_os);
    pp.print(_module);
}

