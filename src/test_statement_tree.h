/// \file test_statement_tree.h
///

#ifndef TEST_STATEMENT_TREE_H_
#define TEST_STATEMENT_TREE_H_

#include "ir/visitor.h"
#include "statement_tree.h"
#include "pp_syntax.h"

class TreePrinter {
public:
    TreePrinter(std::wostream & _os = std::wcout) :
        m_os(_os), m_cStatInd(0), m_cClusterInd(0)
    {}

    void print(const ir::ModulePtr& _pModule) {
        m_os << L"digraph G {\n";
        for (size_t i = 0; i < _pModule->getPredicates().size(); ++i) {
            if (!_pModule->getPredicates().get(i))
                continue;
            const std::wstring strName = _pModule->getPredicates().get(i)->getName();
            st::StmtVertex top(_pModule->getPredicates().get(i)->getBlock());
            top.expand();
            _printGraph(top, strName + L"_exp");
            top.modifyForVerification();
            _printGraph(top, strName + L"_mod");
            top.simplify();
            _printGraph(top, strName + L"_simp");
        }
        m_os<< L"}\n";
    }

private:
    std::wostream &m_os;
    size_t m_cStatInd, m_cClusterInd;

    void _fmtStatement(const ir::Statement& _stmt, size_t _cInd) {
        m_os << "\"";
        switch (_stmt.getKind()) {
            case ir::Statement::BLOCK:
                m_os << L"{}";
                break;
            case ir::Statement::PARALLEL_BLOCK:
                m_os << L"||";
                break;
            case ir::Statement::IF:
                m_os << L"if";
                break;
            case ir::Statement::SWITCH:
                m_os << L"switch";
                break;
            case ir::Statement::VARIABLE_DECLARATION_GROUP:
                m_os << L"var{}";
                break;
            default:
                prettyPrintCompact(const_cast<ir::Statement&>(_stmt), m_os);
                break;
        }
        m_os << "\"";
    }

    void _printTree(st::StmtVertex& _tree) {
        const size_t cInd = m_cStatInd;
        size_t cEdge = 0;
        m_os << L"        A" << cInd << " [ label = ";
        _fmtStatement(_tree.getStatement(), m_cStatInd);
        m_os << L" ];\n";
        for (std::list<st::StmtVertexPtr>::iterator i = _tree.getChildren().begin();
            i != _tree.getChildren().end(); ++i) {
            m_os << L"        A" << cInd << L" -> A" << ++m_cStatInd;
            if (_tree.getChildren().size() != 1)
                m_os << L" [ label = " << ++cEdge << L" ]";
            m_os << L";\n";
            _printTree(**i);
        }
    }

    void _printGraph(st::StmtVertex& _tree, const std::wstring& _strName = L"") {
        ++m_cStatInd;
        m_os << L"    subgraph cluster" << m_cClusterInd++ << L"{\n";
        m_os << L"        node [style=filled];\n";
        _printTree(_tree);
        m_os << L"        label = \"" << _strName << L"\";\n";
        m_os << L"        color = black\n";
        m_os << L"    }\n";
    }
};

#endif // TEST_STATEMENT_TREE_H_
