/// \file pp_flat_tree.cpp
///

#include "prettyprinter.h"
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
        for (std::list<std::wstring>::iterator i = m_path.begin(); i != m_path.end(); ++i)
            std::wcout << L"/" << *i;
        if (m_pPrevNode) {
            std::wcout << L" = ";
            prettyPrintCompact(*m_pPrevNode, std::wcout, PPC_NO_INCOMPLETE_TYPES);
        }
        std::wcout << L"\n";
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

#define NAMED(_NODE, _PROP)                             \
    virtual bool visit##_NODE(_NODE &_node) {    \
        PrettyPrinterFlatBase::visit##_NODE(_node); \
        m_path.back() += L"|"; \
        m_path.back() += _node.get##_PROP(); \
        return true;                                \
    }

    NAMED(NamedValue, Name);
    NAMED(Label, Name);
    NAMED(Predicate, Name);
    NAMED(Message, Name);
    NAMED(Process, Name);
    NAMED(TypeDeclaration, Name);
    NAMED(FormulaDeclaration, Name);
    NAMED(Class, Name);
    NAMED(Module, Name);
    NAMED(VariableReference, Name);
    NAMED(PredicateReference, Name);
    NAMED(FieldExpr, FieldName);
    NAMED(UnionAlternativeExpr, Name);
    NAMED(StructFieldDefinition, Name);
    NAMED(UnionConstructorDeclaration, Name);

    virtual bool visitFormula(ir::Formula &_node) {
        PrettyPrinterFlatBase::visitFormula(_node);
        printQuantifier(_node.getQuantifier());
        return true;
    }

protected:
    const void printQuantifier(int _quantifier) {
        for (std::list<std::wstring>::iterator i = m_path.begin(); i != m_path.end(); ++i)
            std::wcout << L"/" << *i;
        std::wcout << "/Quantifier = ";
        switch (_quantifier) {
            case ir::Formula::NONE:          std::wcout << L"none\n";    break;
            case ir::Formula::UNIVERSAL:     std::wcout << L"forall\n";  break;
            case ir::Formula::EXISTENTIAL:   std::wcout << L"exists\n";  break;
        }
    }

};

void prettyPrintFlatTree(ir::Node &_node, std::wostream &_os) {
    PrettyPrinterFlat pp(_os, _node);
    pp.run();
}
