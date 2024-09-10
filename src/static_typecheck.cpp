//
// Created by auzubarev on 04.04.18.
//

#include "static_typecheck.h"
#include "options.h"

void StaticTypeProver::printTypecheckInfo(const std::wstring &_operation, const std::wstring &_msg, bool _result,
                                          size_t _indent) {
    if (Options::instance().bTypeInfo) {
        for (size_t i = 0; i<_indent; ++i)
            std::wcout << L"  ";
        std::wcout << L"\n\033[34m    " << _operation << L"\033[49m" << _msg
                   << _result ? L"\033[32m   Correct\033[49m" : L"\033[31m   Incorrect\033[49m";
    }
}

bool StaticTypeProver::isSubtype(const TypePtr &_t1, const TypePtr &_t2) {
    bool result = _t1->compare(_t2, Type::ORD_SUB);
    printTypecheckInfo(L"Subtype", str(*_t1) + L" <: " + str(*_t2), result);
    return result;
}

bool StaticTypeProver::isTypeEqual(const TypePtr &_t1, const TypePtr &_t2) {
    bool result = _t1->compare(_t2, Type::ORD_EQUALS);
    printTypecheckInfo(L"Equals", str(*_t1) + L" <=> " + str(*_t2), result);
    return result;
}

TypePtr StaticTypeProver::getTypeJoin(const TypePtr &_t1, const TypePtr &_t2) {
    TypePtr tJoin = _t1->getJoin(*_t2);
    printTypecheckInfo(L"Get Join", str(*_t1) + L" & " + str(*_t2) + L" => " + str(*tJoin));
    return tJoin;
}

bool StaticTypeProver::isFresh(const TypePtr &_t) {
    int kind = _t->getKind();
    return (kind == Type::FRESH ||
            kind == Type::ARRAY && isFresh(_t.as<ArrayType>()->getBaseType()) ||
            kind == Type::RANGE && (isFresh(_t.as<Range>()->getMin()->getType()) ||
                                              isFresh(_t.as<Range>()->getMax()->getType())) ||
            kind == Type::SUBTYPE && isFresh(_t.as<Subtype>()->getParam()->getType()) ||
            kind == Type::MAP && isFresh(_t.as<MapType>()->getBaseType()) ||
            kind == Type::LIST && isFresh(_t.as<ListType>()->getBaseType()));
}

void StaticTypeProver::setType(Node &_node, TypePtr &_type) {
    if (_node.getNodeKind() == Node::NAMED_VALUE)
        _node.as<NamedValue>()->setType(_type);
    else if (_node->getNodeKind() == Node::EXPRESSION)
        _node.as<Expression>()->setType(_type);
    else
        typeError(L"impossible to set the type for " + str(_node));
    printTypecheckInfo(L"Set Type", str(*_type) + L": " + str(_node));
}

void StaticTypeProver::isContains(const ExpressionPtr &_expr, const TypePtr &_type) {
    return isSubtype(_expr->getType(), _type);
}

void StaticTypeProver::typeError(std::string _msg, bool _expr) {
    if (!expr) {
        printTypecheckInfo(L"Type Error", _msg);
        throw std::runtime_error(_msg);
    }
}

void StaticTypeProver::typeWarning(std::string _msg, bool _expr) {
    if (!expr)
        printTypecheckInfo(L"Type Error", _msg);
}