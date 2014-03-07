/// \file pp_flat_tree.cpp
///

#include "pp_syntax.h"
#include "utils.h"

using namespace ir;

class PrettyPrinterFlatBase: public PrettyPrinterBase {
public:
    PrettyPrinterFlatBase(std::wostream &_os, Node &_node) : PrettyPrinterBase(_os), m_nPrevDepth(-1) {
        m_path.push_back(L"");
        m_pPrevNode = &_node;
    }

    std::list<std::wstring> m_path;
    NodePtr m_pPrevNode;
    int m_nPrevDepth;

    void print() {
        m_os << setInline(true);
        for (std::list<std::wstring>::iterator i = m_path.begin(); i != m_path.end(); ++i)
            m_os << L"/" << *i;
        if (m_pPrevNode) {
            m_os << L" = ";
            prettyPrintCompact(*m_pPrevNode, m_os, PPC_NO_INCOMPLETE_TYPES);
        }
        m_os << setInline(false);
        m_os << L"\n";
    }

#define VISITOR(_NODE, ...)                             \
        virtual bool visit##_NODE(_NODE &_node) {    \
            m_path.back() += L"|" WIDEN(#_NODE);\
            return true;                                \
        }

#define HANDLER(_ROLE)                          \
        virtual int handle##_ROLE(Node &_node) {   \
            print(); \
            m_pPrevNode = &_node; \
            while (m_path.size() >=  getDepth()) \
                m_path.pop_back(); \
            m_path.push_back(WIDEN(#_ROLE)); \
            m_nPrevDepth = getDepth();\
            return 0;                               \
        }

#define NODE(_Node, _Parent) VISITOR(_Node);
#include "ir/nodes.inl"
#undef NODE

protected:
#define ROLE(_ROLE) HANDLER(_ROLE)
#include "ir/roles.inl"
#undef ROLE
};

class PrettyPrinterFlat: public PrettyPrinterFlatBase {
public:
    PrettyPrinterFlat(std::wostream &_os, Node &_node) : PrettyPrinterFlatBase(_os, _node) {}

    void run() {
        traverseNode(*m_pPrevNode);
        print();
    }

#define NAMED(_NODE, _PROP)                         \
    virtual bool visit##_NODE(_NODE &_node) {       \
        PrettyPrinterFlatBase::visit##_NODE(_node); \
        m_path.back() += L"|";                      \
        m_path.back() += _node.get##_PROP();        \
        printName(_node.get##_PROP());              \
        return true;                                \
    }

    NAMED(NamedValue, Name);
    NAMED(Label, Name);
    NAMED(Predicate, Name);
    NAMED(Message, Name);
    NAMED(Process, Name);
    NAMED(TypeDeclaration, Name);
    NAMED(FormulaDeclaration, Name);
    NAMED(VariableDeclaration, Name);
    NAMED(Class, Name);
    NAMED(Module, Name);
    NAMED(VariableReference, Name);
    NAMED(FieldExpr, FieldName);
    NAMED(RecognizerExpr, ConstructorName);
    NAMED(AccessorExpr, ConstructorName);
    NAMED(StructFieldDefinition, Name);
    NAMED(UnionConstructorDeclaration, Name);
    NAMED(NamedReferenceType, Name);

    virtual bool visitFormula(ir::Formula &_node) {
        PrettyPrinterFlatBase::visitFormula(_node);
        printQuantifier(_node.getQuantifier());
        return true;
    }
    virtual bool visitLemmaDeclaration(ir::LemmaDeclaration &_node) {
        printLemmaStatus(_node.getStatus());
        PrettyPrinterFlatBase::visitLemmaDeclaration(_node);
        return true;
    }

    virtual bool visitNode(ir::Node &_node) {
        if (_node.getLoc() && m_path.size() > 1)
            printLine(_node.getLoc());
        return true;
    }

    virtual bool visitJump(ir::Jump &_node) {
        PrettyPrinterFlatBase::visitJump(_node);
        if (_node.getDestination() && _node.getDestination()->getLoc())
            printDestination(_node.getDestination()->getLoc());
        return true;
    }

    virtual bool visitPredicateReference(ir::PredicateReference &_node) {
        PrettyPrinterFlatBase::visitPredicateReference(_node);
        m_path.back() += L"|";
        m_path.back() += _node.getName();
        printName(_node.getName());

        if (_node.getTarget() && _node.getTarget()->getLoc())
            printDestination(_node.getTarget()->getLoc());
        return true;
    }

protected:
    const void printQuantifier(int _quantifier) {
        m_os << setInline(true);
        for (std::list<std::wstring>::iterator i = m_path.begin(); i != m_path.end(); ++i)
            m_os << L"/" << *i;
        m_os << "/Quantifier = ";
        switch (_quantifier) {
            case ir::Formula::NONE:          m_os << L"none";    break;
            case ir::Formula::UNIVERSAL:     m_os << L"forall";  break;
            case ir::Formula::EXISTENTIAL:   m_os << L"exists";  break;
        }
        m_os << setInline(false) << "\n";
    }

    const void printLemmaStatus(int _status) {
        m_os << setInline(true);
        for (std::list<std::wstring>::iterator i = m_path.begin(); i != m_path.end(); ++i)
            m_os << L"/" << *i;
        m_os << L"/Status = ";
        switch (_status) {
            case ir::LemmaDeclaration::VALID:     m_os << L"valid";     break;
            case ir::LemmaDeclaration::INVALID:   m_os << L"invalid";   break;
            case ir::LemmaDeclaration::UNKNOWN:   m_os << L"unknown";   break;
        }
        m_os << setInline(false) << "\n";
    }

    const void printName(const std::wstring &_strName) {
        m_os << setInline(true);
        for (std::list<std::wstring>::iterator i = m_path.begin(); i != m_path.end(); ++i)
            m_os << L"/" << *i;
        m_os << "/Name = " << _strName << "\n";
        m_os << setInline(false) << "\n";
    }

    const void printLine(const lexer::Token *_pLoc) {
        m_os << setInline(true);
        for (std::list<std::wstring>::iterator i = m_path.begin(); i != m_path.end(); ++i)
            m_os << L"/" << *i;
        m_os << "/Line = " << _pLoc->getLine();
        m_os << setInline(false) << "\n";
    }

    const void printDestination(const lexer::Token *_pLoc) {
        m_os << setInline(true);
        for (std::list<std::wstring>::iterator i = m_path.begin(); i != m_path.end(); ++i)
            m_os << L"/" << *i;
        m_os << "/Destination/Line = " << _pLoc->getLine();
        m_os << setInline(false) << "\n";
    }
};

void prettyPrintFlatTree(ir::Node &_node, std::wostream &_os) {
    PrettyPrinterFlat pp(_os, _node);
    Param::updateUsed(_node);
    pp.run();
}
