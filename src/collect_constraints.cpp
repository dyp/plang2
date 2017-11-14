/// \file collect_constraints.cpp
///

#include <iostream>
#include <stack>
#include <sstream>

#include <assert.h>

#include "typecheck.h"
#include "options.h"
#include "type_lattice.h"
#include "prettyprinter.h"
#include "pp_syntax.h"
#include "cvc3_solver.h"

#include <ir/statements.h>
#include <ir/builtins.h>
#include <ir/visitor.h>
#include <utils.h>

using namespace ir;

class SMT {
public:
    static bool isContains(ExpressionPtr _expr, TypePtr _type);
    static void addLemma(const ExpressionPtr &_expr);
    static void addTheorem(const ExpressionPtr &_expr);
    static void delLemma(const ExpressionPtr &_expr);
    static void delTheorem(const ExpressionPtr &_expr);
    static bool prove();
    static bool proveTheorem(const ExpressionPtr &_expr);
    static ExpressionPtr cookExpr(const ExpressionPtr &_expr);
    static std::vector<ExpressionPtr> delDupLemmas (const std::vector<ExpressionPtr> &_exprs);
    static std::vector<ExpressionPtr> getLemmas();
    static std::vector<ExpressionPtr> getTheorems();
    static ExpressionPtr createConjunct (const std::vector<ExpressionPtr> &_exprs);
    static VariableReferencePtr getNewVar(const std::wstring _name);
    static std::vector<ExpressionPtr> getSubExpression (const NodePtr &_expr, int _kind1=-1, int _kind2=-1,
                                                        int kind3=-1, int kind4=-1);
    static void changeSubexpr (ExpressionPtr &_expr, const ExpressionPtr &_old, const ExpressionPtr &_new);
    static void setVarType(VariableReferencePtr _var, const TypePtr _type);
private:
    static int nVar;
    static std::map<FormulaDeclarationPtr, FormulaDeclarationPtr> m_formulas;
    static std::map<std::wstring, NamedValuePtr> m_variables;
    static std::map<ExpressionPtr, ExpressionPtr> m_lemmas;
    static std::map<ExpressionPtr, ExpressionPtr> m_theorems;
    static std::vector<ExpressionPtr> m_conditions;
    static ModulePtr m_module;
};

class TC {
public:
    static bool isSubtype(TypePtr _t1, TypePtr _t2);
    static bool isEqual(const TypePtr &_t1, const TypePtr &_t2);
    static bool isKind (TypePtr _type, int _kind);
    static TypePtr getJoin(const TypePtr &_t1, const TypePtr &_t2);
    static void setType (Expression &_expr, const TypePtr &_type);
    static void setType (const NodePtr &_expr, const TypePtr &_type);
    static std::wstring nodeToString (Node &_node);
    static void typeError (std::string _msg, bool _b = false);
    static bool isFresh(const TypePtr &_t);
    static SubtypePtr rangeToSubtype (TypePtr _t);
    static void printInfo (Node &_node);
};

//-----------SMT------------
int SMT::nVar = 0;
std::vector<ExpressionPtr> SMT::m_conditions = std::vector<ExpressionPtr>();
std::map<FormulaDeclarationPtr, FormulaDeclarationPtr> SMT::m_formulas =
        std::map<FormulaDeclarationPtr, FormulaDeclarationPtr>();
std::map<std::wstring, NamedValuePtr> SMT::m_variables = std::map<std::wstring, NamedValuePtr>();
std::map<ExpressionPtr, ExpressionPtr> SMT::m_lemmas = std::map<ExpressionPtr, ExpressionPtr>();
std::map<ExpressionPtr, ExpressionPtr> SMT::m_theorems = std::map<ExpressionPtr, ExpressionPtr>();
ModulePtr SMT::m_module = new Module(L"SMT");

void SMT::delLemma(const ExpressionPtr &_expr){
    std::map<ExpressionPtr, ExpressionPtr>::iterator it = m_lemmas.find(_expr);
    if (it != m_lemmas.end())
        m_lemmas.erase(it);
}

void SMT::addLemma(const ExpressionPtr &_expr) {
    m_conditions = std::vector<ExpressionPtr>();

    ExpressionPtr expr = cookExpr(_expr);
    ExpressionPtr cond = createConjunct(delDupLemmas(m_conditions));
    m_lemmas.insert({_expr, new Binary(Binary::BOOL_AND, expr, cond)});
    if (Options::instance().bTypeInfo)
        std::wcout << L"\n\033[34m    Add lemma \033[49m" + TC::nodeToString(*expr);
}

void SMT::delTheorem(const ExpressionPtr &_expr){
    std::map<ExpressionPtr, ExpressionPtr>::iterator it = m_theorems.find(_expr);
    if (it != m_theorems.end())
        m_theorems.erase(it);
    it = m_lemmas.find(_expr);
    if (it != m_lemmas.end())
        m_lemmas.erase(it);
}

void SMT::addTheorem(const ExpressionPtr &_expr) {
    m_conditions = std::vector<ExpressionPtr>();
    ExpressionPtr expr = cookExpr(_expr).as<Expression>();
    m_theorems.insert({_expr, expr});
    if (Options::instance().bTypeInfo)
        std::wcout << L"\n\033[34m    Add theorem \033[49m" + TC::nodeToString(*expr);
    ExpressionPtr cond = createConjunct(delDupLemmas(m_conditions));
    m_lemmas.insert({_expr, cond});
    if (Options::instance().bTypeInfo)
        std::wcout << L"\n\033[34m    Add lemma \033[49m" + TC::nodeToString(*cond);
}


std::vector<ExpressionPtr> SMT::getSubExpression (const NodePtr &_expr, int _kind1,
                                                  int _kind2, int _kind3, int _kind4) {
    class Searcher : public Visitor {
    public:
        std::vector<ExpressionPtr> exprs;
        int m_kind1, m_kind2, m_kind3, m_kind4;
        Searcher(int i1, int i2, int i3, int i4): m_kind1(i1), m_kind2(i2),
                                                  m_kind3(i3), m_kind4(i4){};
        virtual bool traverseExpression(Expression &expr) {
            int kind = expr.getKind();
            if (m_kind1 == -1) {
                exprs.push_back(&expr);
                return Visitor::traverseExpression(expr);
            }
            if (kind == m_kind1 or kind == m_kind2 or
                kind == m_kind3 or kind == m_kind4)
                exprs.push_back(&expr);
            return Visitor::traverseExpression(expr);
        }
    };
    Searcher s = Searcher(_kind1, _kind2, _kind3, _kind4);
    s.traverseNode(*_expr);
    return s.exprs;
}

VariableReferencePtr SMT::getNewVar(const std::wstring _name) {
    std::map<std::wstring, NamedValuePtr>::iterator it = m_variables.find(_name);
    if (it != m_variables.end()) {
        return new VariableReference(it->second->getName(), it->second);
    } else {
        NamedValuePtr newName = new NamedValue(_name + std::to_wstring(nVar));
        nVar++;
        m_variables.insert({_name, newName});
        m_variables.insert({newName->getName(), newName});
        return new VariableReference(newName->getName(), newName);
    }
}

void SMT::setVarType(VariableReferencePtr _var, const TypePtr _type) {
    _var->setType(_type);
    _var->getTarget()->setType(_type);
}

ExpressionPtr SMT::cookExpr(const ExpressionPtr &_expr) {

    ExpressionPtr expr = clone(_expr);

    //func to var
    for (auto i: getSubExpression(expr, Expression::FUNCTION_CALL)) {
        FunctionCallPtr func = i.as<FunctionCall>();
        VariableReferencePtr var = getNewVar(func->getPredicate().as<PredicateReference>()->getName());
        setVarType(var, func->getType());
        changeSubexpr(expr, func, var);
        ExpressionPtr postCond = cookExpr(func->getPredicate().as<PredicateReference>()
                                                  ->getTarget()->getPostCondition());
        if (postCond) {
            std::wstring resultName = func->getPredicate().as<PredicateReference>()
                    ->getTarget()->getOutParams().get(0)->get(0)->getName();
            for (auto j : getSubExpression(postCond, Expression::VAR)) {
                VariableReferencePtr postVar = i.as<VariableReference>();
                if (postVar->getName() == resultName)
                    postVar->setTarget(var->getTarget());
            }
            m_conditions.push_back(postCond);
            for (size_t j = 0; j < func->getArgs().size(); j++) {
                ExpressionPtr left = func->getArgs().get(j);
                ExpressionPtr right = new VariableReference(func->getPredicate().as<PredicateReference>()->
                        getTarget()->getInParams().get(j));
                ExpressionPtr cond = new Binary(Binary::EQUALS, left, right);
                m_conditions.push_back(cookExpr(cond));
            }
        }
    }

    //formula call
    for (auto i: getSubExpression(expr, Expression::FORMULA_CALL)) {
        FormulaCallPtr fc = i.as<FormulaCall>();
        for (auto arg : fc->getArgs())
            changeSubexpr(arg, arg, cookExpr(arg));
        std::map<FormulaDeclarationPtr, FormulaDeclarationPtr>::iterator it = m_formulas.find(fc->getTarget());
        if (it != m_formulas.end()) {
            fc->setTarget(it->second);
        } else {
            FormulaDeclarationPtr formula = new FormulaDeclaration(fc->getName());
            m_formulas.insert({fc->getTarget(), formula});
            m_formulas.insert({formula, formula});
            for (auto param: fc->getTarget()->getParams()) {
                VariableReferencePtr v = new VariableReference(param->getName(), param);
                formula->getParams().add(cookExpr(v).as<VariableReference>()->getTarget());
            }
            formula->setFormula(cookExpr(fc->getTarget()->getFormula()));
            fc->setTarget(formula);
            m_module->getFormulas().add(formula);
            if (Options::instance().bTypeInfo)
                std::wcout << L"\n\033[34m    CVC4 Add formula \033[49m" + TC::nodeToString(*formula);
        }
    }

    //rem subtype and nat types
    for (auto i: getSubExpression(expr, Expression::VAR)) {
        VariableReferencePtr var = i.as<VariableReference>();
        TypePtr type = var->getType();
        VariableReferencePtr newVar = getNewVar(var->getName());
        switch (type->getKind()) {
            case Type::NAT: {
                setVarType(newVar, new Type(Type::INT, Number::GENERIC));
                LiteralPtr zero = new Literal(Number::makeInt(0));
                zero->setType(new Type(Type::INT, Number::GENERIC));
                m_conditions.push_back(cookExpr(new Binary(Binary::GREATER_OR_EQUALS, newVar, zero)));
                break;
            }
            case Type::RANGE: {
                TypePtr tMin = type.as<Range>()->getMin()->getType();
                TypePtr tMax = type.as<Range>()->getMax()->getType();
                TypePtr tJoin =TC::getJoin(tMin, tMax);
                type = type.as<Range>()->asSubtype();
                type.as<Subtype>()->getParam()->setType(tJoin);
                type.as<Subtype>()->getParam()->setName(L"x");
                // no break
            }

            case Type::SUBTYPE: {
                setVarType(newVar, type.as<Subtype>()->getParam()->getType());
                newVar = cookExpr(newVar).as<VariableReference>();
                ExpressionPtr subtypeExpr = type.as<Subtype>()->getExpression();
                for (auto j : getSubExpression(subtypeExpr, Expression::VAR)) {
                    VariableReferencePtr subtypeVar = j.as<VariableReference>();
                    if (subtypeVar->getTarget() == type.as<Subtype>()->getParam()) {
                        subtypeVar->setName(newVar.as<VariableReference>()->getTarget()->getName());
                        subtypeVar->setType(newVar.as<VariableReference>()->getTarget()->getType());
                        subtypeVar->setTarget(newVar.as<VariableReference>()->getTarget());
                    }
                }
                subtypeExpr = cookExpr(subtypeExpr);
                m_conditions.push_back(subtypeExpr);
                break;
            }
            case Type::ARRAY: {
                ArrayTypePtr tArray = new ArrayType(type.as<ArrayType>()->getBaseType(), new Type(Type::INT,  Number::GENERIC));
                VariableReferencePtr v = getNewVar(L"x");
                setVarType(v, tArray->getBaseType());
                tArray->setBaseType(cookExpr(v)->getType());  //FIXME
                setVarType(newVar, tArray);
                break;
            }
            case Type::CHAR: {
                setVarType(newVar, new Type(Type::INT,  Number::GENERIC));
                break;
            }
            default:
                setVarType(newVar, type);
        }
        changeSubexpr(expr, var, newVar);
    }

    for (auto e: getSubExpression(expr)) {
        TypePtr type = e->getType();
        if (type) {
            switch (type->getKind()) {
                case Type::NAT:
                    e->setType(new Type(Type::INT, Number::GENERIC));
                    break;
                case Type::RANGE: {
                    TypePtr tMin = type.as<Range>()->getMin()->getType();
                    TypePtr tMax = type.as<Range>()->getMax()->getType();
                    TypePtr tJoin =TC::getJoin(tMin, tMax);
                    e->setType(tJoin);
                    break;
                }

                case Type::SUBTYPE:
                    e->setType(type.as<Subtype>()->getParam()->getType());
                    break;
                case Type::ARRAY: {
                    ArrayTypePtr tArray = new ArrayType(type.as<ArrayType>()->getBaseType(), new Type(Type::INT,  Number::GENERIC)); //FIXME
                    VariableReferencePtr var = getNewVar(L"x");
                    setVarType(var, tArray->getBaseType());
                    tArray->setBaseType(cookExpr(var)->getType());
                    e->setType(tArray);
                    break;
                }
                case Type::CHAR: {
                    if (e->getKind() == Expression::LITERAL) {
                        changeSubexpr(expr, e, new Literal(Number::makeInt(0)));
                    }
                    e->setType(new Type(Type::INT,  Number::GENERIC));
                    break;
                }
            }
        }
    }

    for (auto i: getSubExpression(expr, Expression::FORMULA)) {
        FormulaPtr f = i.as<Formula>();
        if (f->getBoundVariables().size()==0)
            SMT::changeSubexpr(expr, i, f->getSubformula());
        else {
            for (size_t j = 0; j<f->getBoundVariables().size(); j++) {
                VariableReferencePtr newVar = getNewVar(f->getBoundVariables().get(j)->getName());
                setVarType(newVar, f->getBoundVariables().get(j)->getType());
                f->getBoundVariables().remove(j);
                f->getBoundVariables().insert(j, newVar->getTarget());
            }
        };
    }

    return expr;
}

bool SMT::proveTheorem(const ExpressionPtr &_expr) {
    addTheorem(_expr);
    bool b = prove();
    delTheorem(_expr);
    return b;
}

ExpressionPtr SMT::createConjunct (const std::vector<ExpressionPtr> &_exprs) {
    if (_exprs.size() == 0)
        return new Literal(true);
    if (_exprs.size() == 1)
        return _exprs.at(0);
    ExpressionPtr expr = _exprs.at(0);
    for (size_t i = 1; i < _exprs.size(); i++)
        expr = new Binary(Binary::BOOL_AND, expr, _exprs.at(i));
    return expr;
}

std::vector<ExpressionPtr> SMT::getLemmas() {
    std::set<std::wstring> s;
    std::vector<ExpressionPtr> exprs;
    for (auto i: m_lemmas)
        if (s.count(TC::nodeToString(*i.second)) == 0){
            s.insert(TC::nodeToString(*i.second));
            exprs.push_back(i.second);
        }
    return exprs;
}

std::vector<ExpressionPtr> SMT::getTheorems() {
    std::set<std::wstring> s;
    std::vector<ExpressionPtr> exprs;
    for (auto i: m_theorems)
        if (s.count(TC::nodeToString(*i.second)) == 0){
            s.insert(TC::nodeToString(*i.second));
            exprs.push_back(i.second);
        }
    return exprs;
}

std::vector<ExpressionPtr> SMT::delDupLemmas (const std::vector<ExpressionPtr> &_exprs) {
    std::set<std::wstring> s;
    std::vector<ExpressionPtr> exprs;
    std::vector<ExpressionPtr> lemmas = getLemmas();
    for (auto i : lemmas)
        s.insert(TC::nodeToString(*i));
    for (auto i : _exprs)
        if (s.count(TC::nodeToString(*i)) == 0){
            s.insert(TC::nodeToString(*i));
            exprs.push_back(i);
        }
    return exprs;
}

bool SMT::prove(){
    ExpressionPtr ant = createConjunct(getLemmas());
    ExpressionPtr con = createConjunct(getTheorems());
    ExpressionPtr impl = new Binary(Binary::IMPLIES, ant, con);
    LemmaDeclarationPtr le = new LemmaDeclaration(impl);
    m_module->getLemmas().add(le);
    int res;
    try {
        cvc3::QueryResult r;
        cvc3::checkValidity(m_module, r);
        if (r.count(le) != 0)
            res = r.at(le);
        else
            res = CVC3::QueryResult::UNKNOWN;

    }
    catch (std::exception e) {
        res = CVC3::QueryResult::UNKNOWN;
    }
    m_module->getLemmas().remove(0);
    if (Options::instance().bTypeInfo) {
        std::wcout.fill( '.' );
        std::wcout << L"\n\033[34m    CVC4 Prove \033[49m" + TC::nodeToString(*impl);
        if (res == CVC3::QueryResult::VALID)
            std::wcout << L"\033[32m   Valid\033[49m";
        else if (res == CVC3::QueryResult::INVALID)
            std::wcout << L"\033[31m   Invalid\033[49m";
        else
            std::wcout << L"\033[34m   Unknown\033[49m";
    }
    return (res == CVC3::QueryResult::VALID or res == CVC3::QueryResult::UNKNOWN);
}

bool SMT::isContains(ExpressionPtr _expr, TypePtr _type) {
    NamedValuePtr nam = new NamedValue(L"ex", _type);
    VariableReferencePtr var = new VariableReference(nam);
    m_conditions = std::vector<ExpressionPtr>();
    ExpressionPtr newVar = cookExpr(var).as<Expression>();
    if (m_conditions.size() == 0)
        return TC::isSubtype(_expr->getType(), _type);
    ExpressionPtr cond = m_conditions.at(0);
    for (size_t i = 1; i < m_conditions.size(); i++)
        cond = new Binary(Binary::BOOL_AND, cond, m_conditions.at(i));
    m_theorems.insert({_expr, cond});
    m_conditions = std::vector<ExpressionPtr>();
    ExpressionPtr expr = new Binary(Binary::EQUALS, newVar, cookExpr(_expr));
    for (auto i : m_conditions)
        expr = new Binary(Binary::BOOL_AND, expr, i);
    m_lemmas.insert({_expr, expr});
    bool b = prove();
    m_theorems.erase(_expr);
    m_lemmas.erase(_expr);
    return b;
}

void SMT::changeSubexpr (ExpressionPtr &_expr, const ExpressionPtr &_old, const ExpressionPtr &_new) {
    _expr = Expression::substitute(_expr, _old, _new).as<Expression>();
}

//-----------TC------------
std::wstring TC::nodeToString (Node &_node) {
    std::wstring s;
    std::wostringstream ss;
    pp::prettyPrintSyntax(_node, ss, NULL, true);
    s = ss.str();
    s.pop_back();
    return s;
}

void TC::printInfo (Node &_node){
    if (Options::instance().bTypeInfo) {
        std::wcout << L"\n\033[32m  Type check: \033[49m" + nodeToString(_node);
    }
}

bool TC::isSubtype(TypePtr _t1, TypePtr _t2) {
    bool b = true;
    int k1 = _t1->getKind();
    int k2 = _t2->getKind();
    if (k1 == Type::RANGE) {
        _t1 = rangeToSubtype(_t1);
        k1 = Type::SUBTYPE;
    }
    if (k2 == Type::RANGE) {
        _t2 = rangeToSubtype(_t2);
        k2 = Type::SUBTYPE;
    }
    if (k1 == Type::ARRAY && k2 == Type::ARRAY) {
        ArrayTypePtr tArray1 = _t1.as<ArrayType>();
        ArrayTypePtr tArray2 = _t2.as<ArrayType>();
        if(tArray1->getDimensionsCount() != tArray2->getDimensionsCount())
            return false;
        Collection<Type> dim1 , dim2;
        tArray1->getDimensions(dim1);
        tArray2->getDimensions(dim2);
        for (size_t i = 0; i<tArray1->getDimensionsCount() && b; i++)
            b = isEqual(dim1.get(i), dim2.get(i));
        if(!b)
            return false;
        return isSubtype(tArray1->getBaseType(), tArray2->getBaseType());
    } else if (k1 ==  Type::SUBTYPE && k2 == Type::SUBTYPE) {
        NamedValuePtr name = new NamedValue(L"sx", _t1);
        return SMT::isContains(new VariableReference(L"sx", name), _t2);
    } else
        b = _t1->compare(*_t2, Type::ORD_SUB) || _t1->compare(*_t2, Type::ORD_EQUALS);
    if (Options::instance().bTypeInfo) {

        std::wcout << L"\n\033[34m    Subtyping \033[49m" + nodeToString(*_t1) + L" <: " + nodeToString(*_t2);
        if (b)
            std::wcout << L"\033[32m   Correct\033[49m";
        else
            std::wcout << L"\033[31m   Incorrect\033[49m";
    }
    return b;
}

bool TC::isEqual(const TypePtr &_t1, const TypePtr &_t2) {
    bool b = isSubtype(_t1, _t2) && isSubtype(_t2, _t1);
    if (Options::instance().bTypeInfo) {
        std::wcout << L"\n\033[34m    Equal \033[49m" + nodeToString(*_t1) + L" = " + nodeToString(*_t2);
        if (b)
            std::wcout << L"\033[32m   Correct\033[49m";
        else
            std::wcout << L"\033[31m   Incorrect\033[49m";
    }
    return b;
}

void TC::typeError (std::string _msg, bool _b) {
    if (!_b)
        throw std::runtime_error(_msg);
}

SubtypePtr TC::rangeToSubtype (TypePtr _t) {
    TypePtr tMin = _t.as<Range>()->getMin()->getType();
    TypePtr tMax = _t.as<Range>()->getMax()->getType();
    TypePtr tJoin =TC::getJoin(tMin, tMax);
    SubtypePtr st = _t.as<Range>()->asSubtype();
    TC::setType(st->getParam(),tJoin);
    for (auto i : SMT::getSubExpression(st->getExpression(), Expression::VAR))
        if (i.as<VariableReference>()->getTarget() == st->getParam())
            i->setType(tJoin);
    return st;
}

void TC::setType (Expression &_expr, const TypePtr &_type) {
    TypePtr type = _type;
    if (_type->getKind() == Type::RANGE)
        type = rangeToSubtype(_type);
    _expr.setType(type);
    if (Options::instance().bTypeInfo) {
        std::wcout.fill( '.' );
        std::wcout << L"\n\033[34m    Set Type \033[49m [" + nodeToString(*type) + L"] " + nodeToString(_expr);
    }
}

void TC::setType (const NodePtr &_expr, const TypePtr &_type) {
    TypePtr type = _type;
    if (_type->getKind() == Type::RANGE)
        type = rangeToSubtype(_type);
    if (_expr->getNodeKind() == Node::NAMED_VALUE)
        _expr.as<NamedValue>()->setType(type);
    else if (_expr->getNodeKind() == Node::EXPRESSION)
        _expr.as<Expression>()->setType(type);
    else
        typeError("setType");
    if (Options::instance().bTypeInfo) {
        std::wcout << L"\n\033[34m    Set Type \033[49m [" + nodeToString(*type) + L"] " + nodeToString(*_expr);
    }
}

bool TC::isKind (TypePtr _type, int _kind) {
    if (_type.ptr()->getKind() == _kind)
        return true;
    if (_type->getKind() == Type::RANGE) {
        TypePtr tMin = _type.as<Range>()->getMin()->getType();
        TypePtr tMax = _type.as<Range>()->getMax()->getType();
        TypePtr tJoin = getJoin(tMin, tMax);
        _type = _type.as<Range>()->asSubtype();
        TC::setType(_type.as<Subtype>()->getParam(), tJoin);
    }
    if (_type->getKind() == Type::SUBTYPE) {
        TypePtr t = _type.as<Subtype>()->getParam()->getType();
        if (t->getKind() == _kind)
            return true;
        if (t->getKind() == Type::SUBTYPE)
            return isKind(t, _kind);
    }
    return false;
}

TypePtr TC::getJoin(const TypePtr &_t1, const TypePtr &_t2) {
    TypePtr t;
    if (_t1->getKind() == Type::SUBTYPE)
        t = getJoin(_t1.as<Subtype>()->getParam()->getType(), _t2);
    else if (_t1->getKind() == Type::RANGE)
        t = getJoin(getJoin(_t1.as<Range>()->getMin()->getType() ,
                            _t1.as<Range>()->getMax()->getType()), _t2);
    else if (_t2->getKind() == Type::SUBTYPE)
        t = getJoin(_t2.as<Subtype>()->getParam()->getType(), _t1);
    else if (_t2->getKind() == Type::RANGE)
        t = getJoin(getJoin(_t2.as<Range>()->getMin()->getType() ,
                            _t2.as<Range>()->getMax()->getType()), _t1);
    else
        t = _t1->getJoin(*_t2);
    if (Options::instance().bTypeInfo) {
        std::wcout.fill( '.' );
        std::wcout << L"\n\033[34m    Get Join \033[49m" + nodeToString(*_t1) + L", " + nodeToString(*_t2) +
                      L" -> " + nodeToString(*t);
    }
    return t;
}

bool TC::isFresh(const TypePtr &_t) {
    if ((_t->getKind() == Type::FRESH) || (_t->getKind() == Type::GENERIC) ||
            (_t->getKind() == Type::ARRAY && isFresh(_t.as<ArrayType>()->getBaseType())) ||
            (_t->getKind() == Type::RANGE && (isFresh(_t.as<Range>()->getMin()->getType()) ||
                    isFresh(_t.as<Range>()->getMax()->getType())))||
            (_t->getKind() == Type::SUBTYPE && isFresh(_t.as<Subtype>()->getParam()->getType())) ||
            (_t->getKind() == Type::MAP && isFresh(_t.as<MapType>()->getBaseType())) ||
            (_t->getKind() == Type::LIST && isFresh(_t.as<ListType>()->getBaseType())))
        return true;
    return false;
}

//-----------Collector------------
class Collector : public Visitor {
public:
    Collector(tc::Formulas & _constraints, ir::Context & _ctx);
    virtual ~Collector() {}

    virtual bool visitRange(Range &_type);
    virtual bool visitArrayType(ArrayType &_type);
    virtual bool visitVariableReference(VariableReference &_var);
    virtual bool visitPredicateReference(PredicateReference &_ref);
    virtual bool visitFormulaCall(FormulaCall &_call);
    virtual bool visitFunctionCall(FunctionCall &_call);
    virtual bool visitLambda(Lambda &_lambda);
    virtual bool visitBinder(Binder &_binder);
    virtual bool visitCall(Call &_call);
    virtual bool visitLiteral(Literal &_lit);
    virtual bool visitUnary(Unary &_unary);
    virtual bool visitBinary(Binary &_binary);
    virtual bool visitTernary(Ternary &_ternary);
    virtual bool visitFormula(Formula &_formula);
    virtual bool visitArrayPartExpr(ArrayPartExpr &_array);
    virtual bool visitRecognizerExpr(RecognizerExpr& _expr);
    virtual bool visitAccessorExpr(AccessorExpr& _expr);
    virtual bool visitStructConstructor(StructConstructor &_cons);
    virtual bool visitUnionConstructor(UnionConstructor &_cons);
    virtual bool visitListConstructor(ListConstructor &_cons);
    virtual bool visitSetConstructor(SetConstructor &_cons);
    virtual bool visitArrayConstructor(ArrayConstructor &_cons);
    virtual bool visitArrayIteration(ArrayIteration& _iter);
    virtual bool visitMapConstructor(MapConstructor &_cons);
    virtual bool visitFieldExpr(FieldExpr &_field);
    virtual bool visitReplacement(Replacement &_repl);
    virtual bool visitCastExpr(CastExpr &_cast);
    virtual bool visitAssignment(Assignment &_assignment);
    virtual bool visitVariableDeclaration(VariableDeclaration &_var);
    virtual bool visitIf(If &_if);

    virtual bool visitTypeExpr(TypeExpr &_expr);

    virtual int handlePredicateInParam(Node &_node);
    virtual int handlePredicateOutParam(Node &_node);
    virtual int handleFormulaBoundVariable(Node &_node);
    virtual int handleParameterizedTypeParam(Node &_node);
    virtual int handleVarDeclVar(Node &_node);
    virtual int handleSubtypeParam(Node &_node);
    virtual int handleSwitchCaseValuePost(Node &_node);

    virtual bool traverseSwitch(Switch &_stmt);

    tc::FreshTypePtr createFresh(const TypePtr &_pCurrent = NULL);

protected:
    tc::FreshTypePtr getKnownType(const TypePtr &_pType);
    void collectParam(const NamedValuePtr &_pParam, int _nFlags);

private:
    tc::Formulas & m_constraints;
    ir::Context & m_ctx;
    std::stack<SwitchPtr> m_switches;
};

Collector::Collector(tc::Formulas & _constraints, ir::Context & _ctx)
    : Visitor(CHILDREN_FIRST), m_constraints(_constraints), m_ctx(_ctx)
{
}

tc::FreshTypePtr Collector::getKnownType(const TypePtr &_pType) {
    if (_pType && _pType->getKind() == Type::NAMED_REFERENCE)
        if (TypePtr pType = _pType.as<NamedReferenceType>()->getDeclaration()->getType())
            if (pType->getKind() == Type::FRESH)
                return pType.as<tc::FreshType>();

    return NULL;
}

tc::FreshTypePtr Collector::createFresh(const TypePtr &_pCurrent) {
    if (tc::FreshTypePtr pFresh = getKnownType(_pCurrent))
        return pFresh;

    return new tc::FreshType();
}

void Collector::collectParam(const NamedValuePtr &_pParam, int _nFlags) {
    TypePtr pType = _pParam->getType();

    if (pType->getKind() == Type::FRESH)
        return;

    if (pType->getKind() == Type::TYPE
        && !pType.as<TypeType>()->getDeclaration()->getType()) {
        TypeTypePtr pTypeType = pType.as<TypeType>();
        pTypeType->getDeclaration()->setType(new tc::FreshType());
        return;
    }

    assert(pType);

    if (TC::isFresh(pType))
        TC::setType(_pParam, pType);
    else {
        tc::FreshTypePtr pFresh = createFresh(pType);
        _pParam->setType(pType);
        pFresh->addFlags(_nFlags);
    }
}

static const TypePtr _getContentsType(const ExpressionPtr _pExpr) {
    return _pExpr->getType() && _pExpr->getType()->getKind() == Type::TYPE
        ? _pExpr.as<TypeExpr>()->getContents()
        : _pExpr->getType();
}

static ExpressionPtr _getConditionForIndex(const ExpressionPtr& _pIndex, const VariableReferencePtr& _pVar, bool _bEquality) {
    ExpressionPtr pExpr;
    if (_pIndex->getKind() == Expression::TYPE) {
        const TypePtr pIndexType = _getContentsType(_pIndex);
        if (pIndexType->getKind() != Type::SUBTYPE)
            throw std::runtime_error("Expected subtype or range.");

        SubtypePtr pSubtype = pIndexType.as<Subtype>();

        pExpr = !_bEquality
            ? new Unary(Unary::BOOL_NEGATE, clone(*pSubtype->getExpression()))
            : clone(*pSubtype->getExpression());

        pExpr = Expression::substitute(pExpr, new VariableReference(pSubtype->getParam()), _pVar).as<Expression>();
    }
    else
        pExpr = new Binary(_bEquality ? Binary::EQUALS : Binary::NOT_EQUALS, _pVar, _pIndex);

    return pExpr;
}


bool Collector::visitRange(Range &_type) {
    TC::printInfo(_type);
    if (TC::isFresh(_type.getMin()->getType()) ||
        TC::isFresh(_type.getMax()->getType())) {
        SubtypePtr pSubtype = _type.asSubtype();
        collectParam(pSubtype->getParam(), tc::FreshType::PARAM_OUT);

        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                                             _type.getMin()->getType(), pSubtype->getParam()->getType()));
        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                                             _type.getMax()->getType(), pSubtype->getParam()->getType()));

        callSetter(pSubtype);
        return true;
    } else {
        TypePtr tMin = _type.getMin()->getType();
        TypePtr tMax = _type.getMax()->getType();
        TypePtr tJoin = TC::getJoin(tMin, tMax);
        SubtypePtr pSubtype = _type.asSubtype();
        TC::setType(pSubtype->getParam(), tJoin);
        TC::typeError("Range", tJoin->getKind() != Type::TOP);
        if (TC::isKind(tJoin, Type::INT) or
            TC::isKind(tJoin, Type::NAT) or
            TC::isKind(tJoin, Type::ENUM) or
            TC::isKind(tJoin, Type::CHAR)) {
            ExpressionPtr expr = new Binary(Binary::GREATER_OR_EQUALS, _type.getMax(), _type.getMin()); //FIXME enum and char as int
            TC::typeError("Range is empty", SMT::proveTheorem(expr));
            return true;
        }
        TC::typeError("Range");
        return true;
    }
}

bool Collector::visitArrayType(ArrayType &_type) {
    TC::printInfo(_type);
    if (TC::isFresh(_type.getDimensionType())) {
        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                                             _type.getDimensionType(), new Type(Type::INT, Number::GENERIC)));
        return true;
    } else {
        Collection<Type> dimension;
        _type.getDimensions(dimension);
        for (auto dem : dimension) {
            TC::typeError("Array demnsion", TC::isKind(dem, Type::INT) or
                                            TC::isKind(dem, Type::NAT) or TC::isKind(dem, Type::ENUM) or TC::isKind(dem, Type::CHAR));
            VariableReferencePtr x = new VariableReference (new NamedValue (L"x", dem));
            TC::setType(x, x->getTarget()->getType());
            VariableReferencePtr top = new VariableReference
                    (new NamedValue (L"top", new Type(Type::INT, Number::GENERIC)));
            VariableReferencePtr bot = new VariableReference
                    (new NamedValue (L"bot", new Type(Type::INT, Number::GENERIC))); //FIXME enum and char as int
            FormulaPtr f1 = new Formula(Formula::EXISTENTIAL, new Binary(Binary::GREATER, top, x));
            f1->getBoundVariables().add(top->getTarget());
            FormulaPtr f2 = new Formula(Formula::EXISTENTIAL, new Binary(Binary::GREATER, x, bot));
            f2->getBoundVariables().add(bot->getTarget());
            TC::typeError("Array demnsion is infinite",
                          SMT::proveTheorem(new Binary(Binary::BOOL_AND, f1, f2)));
            if (dem->getKind() == Type::RANGE) {
                TC::nodeToString(*TC::rangeToSubtype(dem));
                _type.rewrite(dem, TC::rangeToSubtype(dem));
            }
        }
        return true;
    }
}

bool Collector::visitVariableReference(VariableReference &_var) {
    _var.getTarget()->setName(_var.getName());
    TC::printInfo(_var);
    TC::setType(_var, _var.getTarget()->getType());
    return true;
}

bool Collector::visitPredicateReference(PredicateReference &_ref) {
    TC::printInfo(_ref);
    Predicates funcs;
    if (! m_ctx.getPredicates(_ref.getName(), funcs))
        assert(false);
    if (funcs.size() == 1) {
        TC::setType(_ref, funcs.get(0)->getType());
    } else {
        _ref.setType(createFresh(_ref.getType()));

        if (funcs.size() > 1) {
            tc::CompoundFormulaPtr pConstraint = new tc::CompoundFormula();

            for (size_t i = 0; i < funcs.size(); ++ i) {
                PredicatePtr pPred = funcs.get(i);
                tc::Formulas & part = pConstraint->addPart();

                part.insert(new tc::Formula(tc::Formula::EQUALS, pPred->getType(), _ref.getType()));
            }

            m_constraints.insert(pConstraint);
        } else {
            m_constraints.insert(new tc::Formula(tc::Formula::EQUALS, funcs.get(0)->getType(), _ref.getType()));
        }
    }
    return true;
}

bool Collector::visitFormulaCall(FormulaCall &_call){
    TC::printInfo(_call);
    TC::setType(_call, _call.getTarget()->getResultType());
    for (size_t i = 0; i < _call.getArgs().size(); ++i) {
        TypePtr tArg = _call.getArgs().get(i)->getType();
        TypePtr tParam = _call.getTarget()->getParams().get(i)->getType();
        if (TC::isFresh(tArg) or TC::isFresh(tParam))
            m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE, tArg, tParam));
        else {
            TC::typeError("Formula call", SMT::isContains(_call.getArgs().get(i), tParam));
        }
    }
    return true;
}

bool Collector::visitFunctionCall(FunctionCall &_call) {
    TC::printInfo(_call);
    PredicateReferencePtr pred = _call.getPredicate().as<PredicateReference>();
    Predicates funcs;
    if (!m_ctx.getPredicates(pred->getName(), funcs))
        assert(false);
    TypePtr typeCall;
    bool isFresh = false;
    bool isPred = false;
    for (size_t i = 0; i < funcs.size() && !isFresh; i++) {
        PredicatePtr f = funcs.get(i);
        if ((f->getOutParams().size() != 1) || (f->getOutParams().get(0)->size() != 1) ||
            (f->getInParams().size() != _call.getArgs().size()))
            continue;
        bool correct = true;
        for (size_t j = 0; (j < f->getInParams().size()) && correct && !isFresh; j++) {
            isFresh = TC::isFresh(_call.getArgs().get(j)->getType()) ||
                      TC::isFresh(funcs.get(i)->getInParams().get(j)->getType());
            correct = SMT::isContains(_call.getArgs().get(j), f->getInParams().get(j)->getType());
        }
        if (isPred && correct) { //multiple predicates
            isPred = false;
            break;
        }
        if (!isPred && correct) {
            isPred = true;
            typeCall = f->getOutParams().get(0)->get(0)->getType();
        }
    }
    if (isFresh) {
        PredicateTypePtr pType = new PredicateType();

        _call.setType(createFresh(_call.getType()));
        pType->getOutParams().add(new Branch());
        pType->getOutParams().get(0)->add(new Param(L"", _call.getType()));

        for (size_t i = 0; i < _call.getArgs().size(); ++ i)
            pType->getInParams().add(new Param(L"", _call.getArgs().get(i)->getType()));

        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                                             _call.getPredicate()->getType(), pType));
    } else {
        TC::typeError("Function call", isPred);
        TC::setType(_call, typeCall);
    }
    return true;
}

bool Collector::visitLambda(Lambda &_lambda) {
    TC::printInfo(_lambda);
    TC::setType(_lambda, _lambda.getPredicate().getType());
    return true;
}

bool Collector::visitBinder(Binder &_binder) {
    PredicateTypePtr
        pType = new PredicateType(),
        pPredicateType = new PredicateType();

    _binder.setType(createFresh(_binder.getType()));
    pType->getOutParams().add(new Branch());
    pPredicateType->getOutParams().add(new Branch());

    for (size_t i = 0; i < _binder.getArgs().size(); ++i)
        if (_binder.getArgs().get(i)) {
            pType->getInParams().add(new Param(L"", _binder.getArgs().get(i)->getType()));
            pPredicateType->getInParams().add(new Param(L"", _binder.getArgs().get(i)->getType()));
        } else
            pPredicateType->getInParams().add(new Param(L"", new Type(Type::TOP)));

    if (_binder.getPredicate()->getKind() == Expression::PREDICATE) {
        PredicatePtr pPredicate = _binder.getPredicate().as<PredicateReference>()->getTarget();
        pType->getOutParams().assign(pPredicate->getOutParams());
        pPredicateType->getOutParams().assign(pPredicate->getOutParams());
    }

    m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
            pPredicateType, _binder.getPredicate()->getType()));

    return true;
}

bool Collector::visitCall(Call &_call) {
    TC::printInfo(_call);
    PredicateReferencePtr pred = _call.getPredicate().as<PredicateReference>();
    Predicates preds;
    if (!m_ctx.getPredicates(pred->getName(), preds))
        assert(false);
    bool isFresh = false;
    bool isPred = false;
    for (size_t i = 0; i < preds.size() && !isFresh; i++) {
        PredicatePtr f = preds.get(i);
        if ((f->getOutParams().size() != _call.getBranches().size()) ||
            (f->getInParams().size() != _call.getArgs().size()))
            continue;
        bool correct = true;
        for (size_t j = 0; (j < f->getInParams().size()) && correct && !isFresh; j++) {
            isFresh = TC::isFresh(_call.getArgs().get(j)->getType()) ||
                      TC::isFresh(f->getInParams().get(j)->getType());
            correct = SMT::isContains(_call.getArgs().get(j), f->getInParams().get(j)->getType());
        }

        for (size_t j = 0; (j < f->getOutParams().size()) && correct && !isFresh; j++) {
            if (f->getOutParams().get(j)->size() != _call.getBranches().get(j)->size()) {
                correct = false;
                continue;
            }
            for (size_t k = 0; (k < f->getOutParams().get(j)->size()) && correct && !isFresh; k++) {
                isFresh = TC::isFresh(_call.getBranches().get(j)->get(k)->getType()) ||
                          TC::isFresh(f->getOutParams().get(j)->get(k)->getType());
                correct = TC::isEqual(_call.getBranches().get(j)->get(k)->getType(),
                                      f->getOutParams().get(j)->get(k)->getType());
            }
        }
        if (isPred && correct) { //multiple predicates
            isPred = false;
            break;
        }
        if (!isPred && correct)
            isPred = true;
    }
    if (isFresh) {
        PredicateTypePtr pType = new PredicateType();

        for (size_t i = 0; i < _call.getArgs().size(); ++i)
            pType->getInParams().add(new Param(L"", _call.getArgs().get(i)->getType()));

        for (size_t i = 0; i < _call.getBranches().size(); ++i) {
            CallBranch &br = *_call.getBranches().get(i);

            pType->getOutParams().add(new Branch());

            for (size_t j = 0; j < br.size(); ++j)
                pType->getOutParams().get(i)->add(new Param(L"", br.get(j)->getType()));
        }

        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                                             _call.getPredicate()->getType(), pType));
    } else {
        TC::typeError("Predicate call", isPred);
    }
    return true;
}

bool Collector::visitLiteral(Literal &_lit) {
    TC::printInfo(_lit);
    switch (_lit.getLiteralKind()) {
        case Literal::UNIT:
            TC::setType(_lit, new Type(Type::UNIT));
            break;
        case Literal::NUMBER: {
            const Number &n = _lit.getNumber();

            if (n.isNat())
                TC::setType(_lit, new Type(Type::NAT, Number::GENERIC));
            else if (n.isInt())
                TC::setType(_lit, new Type(Type::INT, Number::GENERIC));
            else
                TC::setType(_lit, new Type(Type::REAL, Number::GENERIC));

            break;
        }
        case Literal::BOOL:
            TC::setType(_lit, new Type(Type::BOOL));
            break;
        case Literal::CHAR:
            TC::setType(_lit, new Type(Type::CHAR));
            break;
        case Literal::STRING:
            TC::setType(_lit, new Type(Type::STRING));
            break;
        default:
            break;
    }

    return true;
}

bool Collector::visitUnary(Unary &_unary) {
    TC::printInfo(_unary);
    TypePtr tExpr = _unary.getExpression()->getType();
    if (TC::isFresh(tExpr)) {
        _unary.setType(createFresh(_unary.getType()));
        switch (_unary.getOperator()) {
            case Unary::MINUS:
                // Must contain negative values.
                m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                                                     new Type(Type::INT, 1), _unary.getType()));
                // no break;
            case Unary::PLUS:
                m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                                                     _unary.getExpression()->getType(), _unary.getType()));
                m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                                                     _unary.getExpression()->getType(), new Type(Type::REAL, Number::GENERIC)));
                m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                                                     _unary.getType(), new Type(Type::REAL, Number::GENERIC)));
                break;
            case Unary::BOOL_NEGATE:
                //               ! x : A = y : B
                // ------------------------------------------------
                // (A = bool and B = bool) or (A = {C} and B = {C})
            {
                tc::CompoundFormulaPtr p = new tc::CompoundFormula();

                // Boolean negation.
                tc::Formulas &part1 = p->addPart();

                part1.insert(new tc::Formula(tc::Formula::EQUALS,
                                             _unary.getExpression()->getType(), new Type(Type::BOOL)));
                part1.insert(new tc::Formula(tc::Formula::EQUALS,
                                             _unary.getType(), new Type(Type::BOOL)));

                // Set negation.
                tc::Formulas &part2 = p->addPart();
                SetTypePtr pSet = new SetType(NULL);

                pSet->setBaseType(createFresh());
                part2.insert(new tc::Formula(tc::Formula::EQUALS,
                                             _unary.getExpression()->getType(), pSet));
                part2.insert(new tc::Formula(tc::Formula::EQUALS,
                                             _unary.getType(), pSet));

                m_constraints.insert(p);
            }

                break;
            case Unary::BITWISE_NEGATE:
                m_constraints.insert(new tc::Formula(tc::Formula::EQUALS,
                                                     _unary.getExpression()->getType(), _unary.getType()));
                m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                                                     _unary.getExpression()->getType(), new Type(Type::INT, Number::GENERIC)));
        }
    } else {
        switch (_unary.getOperator()) {
            case Unary::MINUS:
                switch (tExpr->getKind()) {
                    case Type::INT:
                        TC::setType(_unary, new Type(Type::INT, Number::GENERIC));
                        return true;
                    case Type::REAL:
                        TC::setType(_unary, new Type(Type::REAL, Number::GENERIC));
                        return true;
                    case Type::NAT:
                        TC::setType(_unary, new Type(Type::INT, Number::GENERIC));
                        return true;
                    default:
                        TC::typeError("invalid type for unary minus");
                }
            case Unary::PLUS:
                switch (tExpr->getKind()) {
                    case Type::INT:
                        TC::setType(_unary, new Type(Type::INT, Number::GENERIC));
                        return true;
                    case Type::REAL:
                        TC::setType(_unary, new Type(Type::REAL, Number::GENERIC));
                        return true;
                    case Type::NAT:
                        TC::setType(_unary, new Type(Type::INT, Number::GENERIC));
                        return true;
                    default:
                        TC::typeError("invalid type for unary plus");
                }
            case Unary::BOOL_NEGATE:
                switch (tExpr->getKind()) {
                    case Type::BOOL:
                        TC::setType(_unary, new Type(Type::BOOL));
                        return true;
                    case Type::SET:
                        TC::setType(_unary, clone(tExpr));
                        return true;
                    default:
                        TC::typeError("invalid type for unary operation");
                }
            case Unary::BITWISE_NEGATE:
                switch (tExpr->getKind()) {
                    case Type::INT:
                        TC::setType(_unary, new Type(Type::INT, Number::GENERIC));
                        return true;
                    case Type::NAT:
                        TC::setType(_unary, new Type(Type::INT, Number::GENERIC));
                        return true;
                    default:
                        TC::typeError("invalid type for unary operation");
                }
        }
    }
    return true;
}

bool Collector::visitBinary(Binary &_binary) {
    _binary.setType(createFresh(_binary.getType()));

    switch (_binary.getOperator()) {
        case Binary::ADD:
        case Binary::SUBTRACT:
            //                       x : A + y : B = z :C
            // ----------------------------------------------------------------
            // (A <= B and C = B and A <= real and B <= real and C <= real)
            //   or (B < A and C = A and A <= real and B <= real and C <= real)
            //   or (A <= C and B <= C and C = {D})
            //   or (A <= C and B <= C and C = [[D]])   /* Operator "+" only. */
            //   or (A = T1[D1] & B = T2[D2] & C = T3[D3] & T1 <= T3 & T2 <= T3 & D1 <= D3 & D2 <= D3)   /* Operator "+" only. */
            {
                tc::CompoundFormulaPtr p = new tc::CompoundFormula();
                tc::Formulas & part1 = p->addPart();
                tc::Formulas & part2 = p->addPart();

                // Numeric operations.
                part1.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getLeftSide()->getType(),
                        _binary.getRightSide()->getType()));
                part1.insert(new tc::Formula(tc::Formula::EQUALS,
                        _binary.getType(),
                        _binary.getRightSide()->getType()));
                part1.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getLeftSide()->getType(),
                        new Type(Type::REAL, Number::GENERIC)));
                part1.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getRightSide()->getType(),
                        new Type(Type::REAL, Number::GENERIC)));
                part1.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getType(),
                        new Type(Type::REAL, Number::GENERIC)));
                part2.insert(new tc::Formula(tc::Formula::SUBTYPE_STRICT,
                        _binary.getRightSide()->getType(),
                        _binary.getLeftSide()->getType()));
                part2.insert(new tc::Formula(tc::Formula::EQUALS,
                        _binary.getType(),
                        _binary.getLeftSide()->getType()));
                part2.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getLeftSide()->getType(),
                        new Type(Type::REAL, Number::GENERIC)));
                part2.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getRightSide()->getType(),
                        new Type(Type::REAL, Number::GENERIC)));
                part2.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getType(),
                        new Type(Type::REAL, Number::GENERIC)));

                // Set operations.
                tc::Formulas & part3 = p->addPart();
                SetTypePtr pSet = new SetType(NULL);

                pSet->setBaseType(createFresh());

                part3.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getLeftSide()->getType(),
                        _binary.getType()));
                part3.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getRightSide()->getType(),
                        _binary.getType()));
                part3.insert(new tc::Formula(tc::Formula::EQUALS,
                        _binary.getType(), pSet));

                if (_binary.getOperator() == Binary::ADD) {
                    tc::Formulas & part4 = p->addPart();
                    ListTypePtr pList = new ListType(NULL);

                    pList->setBaseType(createFresh());

                    part4.insert(new tc::Formula(tc::Formula::SUBTYPE,
                            _binary.getLeftSide()->getType(),
                            _binary.getType()));
                    part4.insert(new tc::Formula(tc::Formula::SUBTYPE,
                            _binary.getRightSide()->getType(),
                            _binary.getType()));
                    part4.insert(new tc::Formula(tc::Formula::EQUALS,
                            _binary.getType(), pList));

                    tc::Formulas & part5 = p->addPart();

                    ArrayTypePtr
                        pA = new ArrayType(createFresh(), createFresh()),
                        pB = new ArrayType(createFresh(), createFresh()),
                        pC = new ArrayType(new tc::FreshType(tc::FreshType::PARAM_OUT),
                            new tc::FreshType(tc::FreshType::PARAM_OUT));

                    part5.insert(new tc::Formula(tc::Formula::EQUALS,
                        _binary.getLeftSide()->getType(), pA));
                    part5.insert(new tc::Formula(tc::Formula::EQUALS,
                        _binary.getRightSide()->getType(), pB));
                    part5.insert(new tc::Formula(tc::Formula::EQUALS,
                        _binary.getType(), pC));

                    part5.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        pA->getBaseType(), pC->getBaseType()));
                    part5.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        pB->getBaseType(), pC->getBaseType()));
                    part5.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        pA->getDimensionType(), pC->getDimensionType()));
                    part5.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        pB->getDimensionType(), pC->getDimensionType()));
                }

                m_constraints.insert(p);
            }
            break;

        case Binary::MULTIPLY:
        case Binary::DIVIDE:
        case Binary::REMAINDER:
        case Binary::POWER:
            //         x : A * y : B = z : C
            // -----------------------------------
            // ((A <= B and C = B) or (B < A and C = A)) and A <= real and B <= real and C <= real
            {
                tc::CompoundFormulaPtr p = new tc::CompoundFormula();
                tc::Formulas & part1 = p->addPart();
                tc::Formulas & part2 = p->addPart();

                part1.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getLeftSide()->getType(),
                        _binary.getRightSide()->getType()));
                part1.insert(new tc::Formula(tc::Formula::EQUALS,
                        _binary.getType(),
                        _binary.getRightSide()->getType()));
                part2.insert(new tc::Formula(tc::Formula::SUBTYPE_STRICT,
                        _binary.getRightSide()->getType(),
                        _binary.getLeftSide()->getType()));
                part2.insert(new tc::Formula(tc::Formula::EQUALS,
                        _binary.getType(),
                        _binary.getLeftSide()->getType()));
                m_constraints.insert(p);
            }

            // Support only numbers for now.
            m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                    _binary.getLeftSide()->getType(), new Type(Type::REAL, Number::GENERIC)));
            m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                    _binary.getRightSide()->getType(), new Type(Type::REAL, Number::GENERIC)));
            m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                    _binary.getType(), new Type(Type::REAL, Number::GENERIC)));

            break;

        case Binary::SHIFT_LEFT:
        case Binary::SHIFT_RIGHT:
            m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                    _binary.getLeftSide()->getType(), new Type(Type::INT, Number::GENERIC)));
            m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                    _binary.getRightSide()->getType(), new Type(Type::INT, Number::GENERIC)));
            m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                    _binary.getLeftSide()->getType(), _binary.getType()));
            break;

        case Binary::LESS:
        case Binary::LESS_OR_EQUALS:
        case Binary::GREATER:
        case Binary::GREATER_OR_EQUALS:
            m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                    _binary.getLeftSide()->getType(), new Type(Type::REAL, Number::GENERIC)));
            m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                    _binary.getRightSide()->getType(), new Type(Type::REAL, Number::GENERIC)));
            // no break;
        case Binary::EQUALS:
        case Binary::NOT_EQUALS:
            //       (x : A = y : B) : C
            // ----------------------------------
            //   C = bool and (A <= B or B < A)
            m_constraints.insert(new tc::Formula(tc::Formula::EQUALS,
                    _binary.getType(), new Type(Type::BOOL)));

            {
                tc::CompoundFormulaPtr p = new tc::CompoundFormula();

                p->addPart().insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getLeftSide()->getType(),
                        _binary.getRightSide()->getType()));
                p->addPart().insert(new tc::Formula(tc::Formula::SUBTYPE_STRICT,
                        _binary.getRightSide()->getType(),
                        _binary.getLeftSide()->getType()));

                //_binary.h

                m_constraints.insert(p);
            }
            break;

        case Binary::BOOL_AND:
        case Binary::BITWISE_AND:
        case Binary::BOOL_OR:
        case Binary::BITWISE_OR:
        case Binary::BOOL_XOR:
        case Binary::BITWISE_XOR:
            //       (x : A and y : B) : C
            // ----------------------------------
            //   (C = bool and A = bool and B = bool)
            //     or (A <= B and C = B and B <= int)      (bitwise AND)
            //     or (B < A and C = A and A <= int)
            //     or (A <= C and B <= C and C = {D})      (set operations)
            {
                tc::CompoundFormulaPtr p = new tc::CompoundFormula();
                tc::Formulas & part1 = p->addPart();
                tc::Formulas & part2 = p->addPart();
                tc::Formulas & part3 = p->addPart();

                part1.insert(new tc::Formula(tc::Formula::EQUALS,
                        _binary.getLeftSide()->getType(), new Type(Type::BOOL)));
                part1.insert(new tc::Formula(tc::Formula::EQUALS,
                        _binary.getRightSide()->getType(), new Type(Type::BOOL)));
                part1.insert(new tc::Formula(tc::Formula::EQUALS,
                        _binary.getType(), new Type(Type::BOOL)));

                part2.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getLeftSide()->getType(),
                        _binary.getRightSide()->getType()));
                part2.insert(new tc::Formula(tc::Formula::EQUALS,
                        _binary.getType(), _binary.getRightSide()->getType()));
                part2.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getRightSide()->getType(),
                        new Type(Type::INT, Number::GENERIC)));

                part3.insert(new tc::Formula(tc::Formula::SUBTYPE_STRICT,
                        _binary.getRightSide()->getType(),
                        _binary.getLeftSide()->getType()));
                part3.insert(new tc::Formula(tc::Formula::EQUALS,
                        _binary.getType(),
                        _binary.getLeftSide()->getType()));
                part3.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getLeftSide()->getType(),
                        new Type(Type::INT, Number::GENERIC)));

                // Set operations.
                tc::Formulas & part4 = p->addPart();
                SetTypePtr pSet = new SetType(NULL);

                pSet->setBaseType(createFresh());

                part4.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getLeftSide()->getType(),
                        _binary.getType()));
                part4.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getRightSide()->getType(),
                        _binary.getType()));
                part4.insert(new tc::Formula(tc::Formula::EQUALS,
                        _binary.getType(), pSet));

                m_constraints.insert(p);
            }
            break;

        case Binary::IMPLIES:
        case Binary::IFF:
            m_constraints.insert(new tc::Formula(tc::Formula::EQUALS,
                    _binary.getLeftSide()->getType(), new Type(Type::BOOL)));
            m_constraints.insert(new tc::Formula(tc::Formula::EQUALS,
                    _binary.getRightSide()->getType(), new Type(Type::BOOL)));
            m_constraints.insert(new tc::Formula(tc::Formula::EQUALS,
                    _binary.getType(), new Type(Type::BOOL)));
            break;

        case Binary::IN:
            {
                SetTypePtr pSet = new SetType(NULL);

                pSet->setBaseType(createFresh());
                m_constraints.insert(new tc::Formula(tc::Formula::EQUALS,
                        _binary.getRightSide()->getType(), pSet));
                m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _binary.getLeftSide()->getType(), pSet->getBaseType()));
                m_constraints.insert(new tc::Formula(tc::Formula::EQUALS,
                        _binary.getType(), new Type(Type::BOOL)));
            }
            break;
    }

    return true;
}

bool Collector::visitTernary(Ternary &_ternary) {
    _ternary.setType(createFresh(_ternary.getType()));
    m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE, _ternary.getIf()->getType(), new Type(Type::BOOL)));
    m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE, _ternary.getThen()->getType(), _ternary.getType()));
    m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE, _ternary.getElse()->getType(), _ternary.getType()));
    return true;
}

bool Collector::visitFormula(Formula &_formula) {
    _formula.setType(createFresh(_formula.getType()));
    tc::FreshTypePtr pFresh = new tc::FreshType(tc::FreshType::PARAM_OUT);

    m_constraints.insert(new tc::Formula(tc::Formula::EQUALS, _formula.getType(), new Type(Type::BOOL)));
    m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE, pFresh, new Type(Type::BOOL)));
    m_constraints.insert(new tc::Formula(tc::Formula::EQUALS, pFresh, _formula.getSubformula()->getType()));
    return true;
}

bool Collector::visitArrayPartExpr(ArrayPartExpr &_array) {
    _array.setType(new tc::FreshType(tc::FreshType::PARAM_OUT));

    const MapTypePtr pMapType = new MapType(new Type(Type::BOTTOM), _array.getType());
    const ListTypePtr pListType = new ListType(_array.getType());

    ArrayTypePtr
        pArrayType = new ArrayType(),
        pCurrentArray = pArrayType;

    for (size_t i=0; i<_array.getIndices().size(); ++i) {
        pCurrentArray->setDimensionType(new Type(Type::BOTTOM));
        if (i+1 == _array.getIndices().size())
            continue;
        pCurrentArray->setBaseType(new ArrayType());
        pCurrentArray = pCurrentArray->getBaseType().as<ArrayType>();
    }
    pCurrentArray->setBaseType(_array.getType());

    if (_array.getIndices().size() > 1) {
        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE, _array.getObject()->getType(), pArrayType));
        return true;
    }

    tc::CompoundFormulaPtr pFormula = new tc::CompoundFormula();

    // A = a[i], I i
    // A <= B[_|_, _|_, ...]
    pFormula->addPart().insert(new tc::Formula(tc::Formula::SUBTYPE, _array.getObject()->getType(), pArrayType));

    // or A <= {_|_ : B}
    pFormula->addPart().insert(new tc::Formula(tc::Formula::SUBTYPE, _array.getObject()->getType(), pMapType));

    // or A <= [[B]] and I <= nat
    tc::Formulas& formulas = pFormula->addPart();
    formulas.insert(new tc::Formula(tc::Formula::SUBTYPE, _array.getObject()->getType(), pListType));
    formulas.insert(new tc::Formula(tc::Formula::SUBTYPE, _array.getIndices().get(0)->getType(), new Type(Type::NAT, Number::GENERIC)));

    m_constraints.insert(pFormula);
    return true;
}

bool Collector::visitRecognizerExpr(RecognizerExpr& _expr) {
    _expr.setType(new Type(Type::BOOL));

    UnionTypePtr pUnionType = new UnionType();
    pUnionType->getConstructors().add(
        new UnionConstructorDeclaration(_expr.getConstructorName()));

    tc::FreshTypePtr pFields = createFresh();
    pUnionType->getConstructors().get(0)->setFields(pFields);

    m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE, pUnionType, _expr.getObject()->getType()));
    return true;
}

bool Collector::visitAccessorExpr(AccessorExpr& _expr) {
    _expr.setType(createFresh(_expr.getType()));

    UnionTypePtr pUnionType = new UnionType();
    pUnionType->getConstructors().add(
        new UnionConstructorDeclaration(_expr.getConstructorName()));
    pUnionType->getConstructors().get(0)->setFields(_expr.getType());

    m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE, pUnionType, _expr.getObject()->getType()));
    return true;
}

bool Collector::visitStructConstructor(StructConstructor &_cons) {
    StructTypePtr pStruct = new StructType();

    for (size_t i = 0; i < _cons.size(); ++i) {
        StructFieldDefinitionPtr pDef = _cons.get(i);
        NamedValuePtr pField = new NamedValue(pDef->getName());

        pField->setType(pDef->getValue()->getType());

        if (pDef->getName().empty())
            pStruct->getTypesOrd().add(pField);
        else
            pStruct->getNamesSet().add(pField);
        pDef->setField(pField);
    }

    assert(pStruct->getNamesSet().empty() || pStruct->getTypesOrd().empty());

    _cons.setType(pStruct);

    return true;
}

bool Collector::visitUnionConstructor(UnionConstructor &_cons) {
    UnionTypePtr pUnion = new UnionType();
    UnionConstructorDeclarationPtr pCons = new UnionConstructorDeclaration(_cons.getName());

    pUnion->getConstructors().add(pCons);

    for (size_t i = 0; i < _cons.size(); ++i) {
        StructFieldDefinitionPtr pDef = _cons.get(i);
        NamedValuePtr pField = new NamedValue(pDef->getName());

        pField->setType(pDef->getValue()->getType());
        pCons->getStructFields()->getNamesOrd().add(pField);
        pDef->setField(pField);
    }

    if (_cons.getType())
        m_constraints.insert(new tc::Formula(tc::Formula::EQUALS, pUnion, _cons.getType()));

    _cons.setType(pUnion);

    return true;
}

bool Collector::visitSetConstructor(SetConstructor &_cons) {
    SetTypePtr pSet = new SetType(NULL);

    pSet->setBaseType(new tc::FreshType(tc::FreshType::PARAM_OUT));
    _cons.setType(pSet);

    for (size_t i = 0; i < _cons.size(); ++i)
        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                _cons.get(i)->getType(), pSet->getBaseType()));

    return true;
}

bool Collector::visitListConstructor(ListConstructor &_cons) {
    ListTypePtr pList = new ListType(NULL);

    pList->setBaseType(new tc::FreshType(tc::FreshType::PARAM_OUT));
    _cons.setType(pList);

    for (size_t i = 0; i < _cons.size(); ++i)
        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                _cons.get(i)->getType(), pList->getBaseType()));

    return true;
}

bool Collector::visitArrayConstructor(ArrayConstructor &_cons) {
    ArrayTypePtr pArray = new ArrayType(NULL);

    pArray->setBaseType(new tc::FreshType(tc::FreshType::PARAM_OUT));
    _cons.setType(pArray);

    if (_cons.empty()) {
        pArray->setDimensionType(createFresh());
        return true;
    }

    for (size_t i = 0; i < _cons.size(); ++i)
        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
            _cons.get(i)->getValue()->getType(), pArray->getBaseType()));

    const tc::FreshTypePtr pParamType = new tc::FreshType(tc::FreshType::PARAM_OUT);
    SubtypePtr pEnumType = new Subtype(new NamedValue(L"", pParamType));
    bool bHaveIndices = false;

    for (size_t i = 0; i < _cons.size(); ++i)
        if (_cons.get(i)->getIndex()) {
            bHaveIndices = true;
            break;
        }

    if (!bHaveIndices) {
        LiteralPtr pUpperBound = new Literal(Number::makeNat(_cons.size()));

        pUpperBound->setType(new Type(Type::NAT,
                pUpperBound->getNumber().countBits(false)));
        m_constraints.insert(new tc::Formula(tc::Formula::EQUALS,
                pParamType, pUpperBound->getType()));
        pEnumType->setExpression(new Binary(Binary::LESS,
                new VariableReference(pEnumType->getParam()), pUpperBound));

        for (size_t i = 0; i < _cons.size(); ++i) {
            LiteralPtr pIndex = new Literal(Number::makeNat(i));
            pIndex->setType(new Type(Type::NAT, pIndex->getNumber().countBits(false)));
            _cons.get(i)->setIndex(pIndex);
        }
    } else {
        bool bFirst = true;
        size_t nInc = 0;
        for (size_t i = 0; i < _cons.size(); ++i) {
            if (_cons.get(i)->getIndex()) {
                const TypePtr pIndexType = _getContentsType(_cons.get(i)->getIndex());
                m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE, pIndexType, pParamType));
                ExpressionPtr pExpr = _getConditionForIndex(_cons.get(i)->getIndex(),
                        new VariableReference(pEnumType->getParam()), false);

                pEnumType->setExpression(pEnumType->getExpression()
                    ? new Binary(Binary::BOOL_AND, pEnumType->getExpression(), pExpr)
                    : pExpr);

                if (pIndexType->getKind() == Type::SUBTYPE)
                    m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        pIndexType.as<Subtype>()->getParam()->getType(), pEnumType->getParam()->getType()));
            }
            else {
                PredicatePtr pIndexer = Builtins::instance().find(bFirst ? L"zero" : L"inc");
                assert(pIndexer);

                PredicateReferencePtr pReference = new PredicateReference(pIndexer);
                pReference->setType(new PredicateType(pIndexer.as<AnonymousPredicate>()));

                FunctionCallPtr pCallIndexer = new FunctionCall(pReference);
                pCallIndexer->getArgs().add(new TypeExpr(pEnumType));

                if (!bFirst)
                    pCallIndexer->getArgs().add(new Literal(Number(intToStr(++nInc), Number::INTEGER)));

                bFirst = false;
                _cons.get(i)->setIndex(pCallIndexer);
            }
        }

        if (!pEnumType->getExpression())
            pEnumType->setExpression(new Literal(true));
    }

    if (_cons.size() == 1 && _cons.get(0)->getIndex()->getType()) {
        pArray->setDimensionType(_cons.get(0)->getIndex()->getType());
        return true;
    }

    const VariableReferencePtr pParam = new VariableReference(new NamedValue(L"", pParamType));
    SubtypePtr pSubtype = new Subtype(pParam->getTarget());
    pArray->setDimensionType(pSubtype);

    ExpressionPtr pExpr = NULL;
    for (size_t i = 0; i < _cons.size(); ++i) {
        ExpressionPtr pEquals = _getConditionForIndex(_cons.get(i)->getIndex(), pParam, true);
        pExpr = pExpr ? new Binary(Binary::BOOL_OR, pExpr, pEquals) : pEquals;
    }

    pSubtype->setExpression(pExpr);

    return true;
}

bool Collector::visitArrayIteration(ArrayIteration& _iter) {
    ArrayTypePtr pArrayType = new ArrayType();
    _iter.setType(pArrayType);

    std::vector<TypePtr> dimensions;
    const bool bUnknownDimensionType = _iter.getDefault();
    for (size_t i = 0; i < _iter.getIterators().size(); ++i)
        dimensions.push_back(TypePtr(!bUnknownDimensionType ?
                new tc::FreshType(tc::FreshType::PARAM_OUT) : new Type(Type::TOP)));

    TypePtr pBaseType = new tc::FreshType(tc::FreshType::PARAM_OUT);

    for (size_t i = 0; i < _iter.size(); ++i) {
        const Collection<Expression>& conds = _iter.get(i)->getConditions();
        for (size_t j = 0; j < conds.size(); ++j) {
            if (conds.get(j)->getKind() == Expression::CONSTRUCTOR
                && conds.get(j).as<Constructor>()->getConstructorKind() == Constructor::STRUCT_FIELDS)
            {
                const StructConstructor& constructor = *conds.get(j).as<StructConstructor>();
                if (_iter.getIterators().size() != constructor.size())
                    throw std::runtime_error("Count of iterators does not match count of fields.");

                for (size_t k = 0; k < constructor.size(); ++k) {
                    m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        _getContentsType(constructor.get(k)->getValue()), dimensions[k]));

                    auto pIterType = _iter.getIterators().get(k)->getType();

                    if (pIterType->getKind() != Type::GENERIC)
                        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                            _getContentsType(constructor.get(k)->getValue()), pIterType));
                    else
                        _iter.getIterators().get(k)->setType(dimensions[k]);
                }

                continue;
            }

            assert(_iter.getIterators().size() == 1);

            m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                _getContentsType(conds.get(j)), dimensions[0]));

            auto pIterType = _iter.getIterators().get(0)->getType();

            if (pIterType->getKind() != Type::GENERIC)
                m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                    _getContentsType(conds.get(j)), pIterType));
            else
                _iter.getIterators().get(0)->setType(dimensions[0]);
        }

        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
            _getContentsType(_iter.get(i)->getExpression()), pBaseType));
    }

    if (_iter.getDefault())
        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
            _getContentsType(_iter.getDefault()), pBaseType));

    for (std::vector<TypePtr>::iterator i = dimensions.begin();; ++i) {
        pArrayType->setDimensionType(*i);

        if (i == --dimensions.end())
            break;

        pArrayType->setBaseType(new ArrayType());
        pArrayType = pArrayType->getBaseType().as<ArrayType>();
    }

    pArrayType->setBaseType(pBaseType);

    return true;
}

bool Collector::visitMapConstructor(MapConstructor &_cons) {
    MapTypePtr pMap = new MapType(NULL, NULL);

    pMap->setBaseType(createFresh());
    pMap->setIndexType(createFresh());
    pMap->getBaseType().as<tc::FreshType>()->setFlags(tc::FreshType::PARAM_OUT);
    pMap->getIndexType().as<tc::FreshType>()->setFlags(tc::FreshType::PARAM_OUT);
    _cons.setType(pMap);

    for (size_t i = 0; i < _cons.size(); ++i) {
        ElementDefinitionPtr pElement = _cons.get(i);

        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                pElement->getIndex()->getType(), pMap->getIndexType()));
        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                pElement->getValue()->getType(), pMap->getBaseType()));
    }

    return true;
}

bool Collector::visitFieldExpr(FieldExpr &_field) {
    tc::FreshTypePtr pFresh = createFresh(_field.getType());
    ir::StructTypePtr pStruct = new StructType();
    ir::NamedValuePtr pField = new NamedValue(_field.getFieldName(), pFresh);

    _field.setType(pFresh);
    pStruct->getNamesSet().add(pField);

    if (_field.getObject()->getType()->getKind() == Type::FRESH)
        pFresh->setFlags(_field.getObject()->getType().as<tc::FreshType>()->getFlags());

    // (x : A).foo : B  |-  A <= struct(B foo)
    m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
            _field.getObject()->getType(), pStruct));
    m_constraints.insert(new tc::Formula(tc::Formula::EQUALS,
            _field.getType(), pField->getType()));

    return true;
}

bool Collector::visitCastExpr(CastExpr &_cast) {
    TypePtr pToType = (TypePtr)_cast.getToType()->getContents();

    _cast.setType(pToType);

    if (pToType->getKind() == Type::STRUCT &&
            _cast.getExpression()->getKind() == Expression::CONSTRUCTOR &&
            _cast.getExpression().as<Constructor>()->getConstructorKind() == Constructor::STRUCT_FIELDS)
    {
        // We can cast form tuples to structs explicitly.
        StructTypePtr pStruct = pToType.as<StructType>();
        StructConstructorPtr pFields = _cast.getExpression().as<StructConstructor>();
        bool bSuccess = true;

        // TODO: maybe use default values for fields.
        if (pStruct->getNamesOrd().size() == pFields->size()) {
            for (size_t i = 0; i < pFields->size(); ++i) {
                StructFieldDefinitionPtr pDef = pFields->get(i);
                size_t cOtherIdx = pDef->getName().empty() ? i : pStruct->getNamesOrd().findByNameIdx(pDef->getName());

                if (cOtherIdx == (size_t)-1) {
                    bSuccess = false;
                    break;
                }

                NamedValuePtr pField = pStruct->getNamesOrd().get(cOtherIdx);

                m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        pDef->getValue()->getType(), pField->getType()));
            }
        } else if (pStruct->getTypesOrd().size() == pFields->size()) {
            for (size_t i = 0; i < pFields->size(); ++i) {
                StructFieldDefinitionPtr pDef = pFields->get(i);
                NamedValuePtr pField = pStruct->getTypesOrd().get(i);

                m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                        pDef->getValue()->getType(), pField->getType()));
            }
        }

        if (bSuccess)
            return true;
    }

    // (A)(x : B) |- B <= A
    m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
            _cast.getExpression()->getType(), pToType));

    return true;
}

bool Collector::visitReplacement(Replacement &_repl) {
    _repl.setType(createFresh(_repl.getType()));

    m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
        _repl.getType(), _repl.getObject()->getType()));
    m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
        _repl.getType(), _repl.getNewValues()->getType()));

    return true;
}

bool Collector::visitAssignment(Assignment &_assignment) {
    // x : A = y : B |- B <= A
    m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
            _assignment.getExpression()->getType(),
            _assignment.getLValue()->getType()));
    return true;
}

int Collector::handleVarDeclVar(Node &_node) {
    collectParam((NamedValue *)&_node, tc::FreshType::PARAM_OUT);
    return 0;
}

int Collector::handleSubtypeParam(Node &_node) {
    collectParam((NamedValue *)&_node, 0);
    return 0;
}

bool Collector::visitVariableDeclaration(VariableDeclaration &_var) {
    if (_var.getValue())
        // x : A = y : B |- B <= A
        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
                _var.getValue()->getType(),
                _var.getVariable()->getType()));
    return true;
}

bool Collector::visitIf(If &_if) {
    m_constraints.insert(new tc::Formula(tc::Formula::EQUALS,
            _if.getArg()->getType(), new Type(Type::BOOL)));
    return true;
}

int Collector::handlePredicateInParam(Node &_node) {
    collectParam((NamedValue *)&_node, tc::FreshType::PARAM_IN);
    return 0;
}

int Collector::handlePredicateOutParam(Node &_node) {
    collectParam((NamedValue *)&_node, tc::FreshType::PARAM_OUT);
    return 0;
}

int Collector::handleFormulaBoundVariable(Node &_node) {
    collectParam((NamedValue *)&_node, tc::FreshType::PARAM_IN);
    return 0;
}

int Collector::handleParameterizedTypeParam(Node &_node) {
    collectParam((NamedValue *)&_node, 0);
    return 0;
}

bool Collector::visitTypeExpr(TypeExpr &_expr) {
    TypeTypePtr pType = new TypeType();
    pType->setDeclaration(new TypeDeclaration(L"", _expr.getContents()));
    _expr.setType(pType);
    return true;
}

bool Collector::traverseSwitch(Switch &_stmt) {
    m_switches.push(&_stmt);
    const bool b = Visitor::traverseSwitch(_stmt);
    m_switches.pop();
    return b;
}

int Collector::handleSwitchCaseValuePost(Node &_node) {
    ExpressionPtr pExpr((Expression *)&_node);

    if (m_switches.top()->getArg())
        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
            _getContentsType(pExpr), m_switches.top()->getArg()->getType()));
    else if (m_switches.top()->getParamDecl())
        m_constraints.insert(new tc::Formula(tc::Formula::SUBTYPE,
            _getContentsType(pExpr), m_switches.top()->getParamDecl()->getVariable()->getType()));

    return 0;
}

namespace {

struct Resolver : public Visitor {
    Collector &m_collector;
    Cloner &m_cloner;
    bool &m_bModified;

    Resolver(Collector &_collector, Cloner &_cloner, bool &_bModified) :
        Visitor(CHILDREN_FIRST), m_collector(_collector), m_cloner(_cloner),
        m_bModified(_bModified)
    {
        m_bModified = false;
    }

    virtual bool visitNamedReferenceType(NamedReferenceType &_type) {
        NodeSetter *pSetter = getNodeSetter();
        if (pSetter == NULL)
            return true;

        for (auto i: m_path) {
            if (i.pNode == _type.getDeclaration().ptr())
                return true;
            if (i.pNode && _type.getDeclaration() && _type.getDeclaration()->getType() &&
                *i.pNode == *_type.getDeclaration()->getType())
                return true;
        }

        TypePtr pDeclType = _type.getDeclaration()->getType();

        if (!pDeclType && Options::instance().typeCheck == TC_PREPROCESS)
            return true;

        m_bModified = true;

        if (!pDeclType || (pDeclType->getKind() == Type::FRESH &&
                !m_cloner.isKnown(pDeclType)))
        {
            TypePtr pFresh = m_collector.createFresh();

            _type.getDeclaration()->setType(pFresh);
            tc::ContextStack::top()->namedTypes.insert(std::make_pair(pFresh.as<tc::FreshType>(), &_type));

            if (pDeclType)
                m_cloner.inject(pFresh, pDeclType);
            else {
                m_cloner.inject(pFresh);
                pDeclType = pFresh;
            }
        }

        if (_type.getArgs().empty()) {
            pSetter->set(m_cloner.get(pDeclType));
            return true;
        }

        assert(pDeclType->getKind() == Type::PARAMETERIZED);
        ParameterizedTypePtr pOrigType = pDeclType.as<ParameterizedType>();
        TypePtr pType = m_cloner.get(pOrigType->getActualType());

        for (size_t i = 0; i < pOrigType->getParams().size(); ++i) {
            TypePtr pParamType = pOrigType->getParams().get(i)->getType();
            if (pParamType->getKind() != Type::TYPE) {
                ExpressionPtr pParamReference = new VariableReference(
                        pOrigType->getParams().get(i));
                ExpressionPtr pArgument = _type.getArgs().get(i);
                pType = Expression::substitute(pType, pParamReference, pArgument).as<Type>();
            } else {
                TypePtr pParamReference = new NamedReferenceType(
                        pParamType.as<TypeType>()->getDeclaration());
                TypePtr pArgument = _type.getArgs().get(i).as<TypeExpr>()->getContents();
                pType->rewrite(pParamReference, pArgument);
            }
        }

        pSetter->set(pType);
        return true;
    }
};

}

static
void _resolveNamedReferenceTypes(Node &_node, Collector &_collector) {
    Cloner cloner;
    bool bModified = false;

    do
        Resolver(_collector, cloner, bModified).traverseNode(_node);
    while (bModified);
}

tc::ContextPtr tc::collect(tc::Formulas &_constraints, Node &_node, ir::Context &_ctx) {
    Collector collector(_constraints, _ctx);

    tc::ContextStack::push(::ref(&_constraints));
    _resolveNamedReferenceTypes(_node, collector);

    if (Options::instance().typeCheck != TC_PREPROCESS)
        collector.traverseNode(_node);

    tc::ContextPtr pContext = tc::ContextStack::top();
    tc::ContextStack::pop();

    return pContext;
}
