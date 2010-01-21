/// \file parser_context.cpp
///

#include <stdarg.h>

#include "parser_context.h"
#include "ir/numbers.h"
#include "ir/builtins.h"

using namespace lexer;

CContext::~CContext() {
    cleanAdopted();

    for (nodes_t::iterator iNode = m_nodes.begin(); iNode != m_nodes.end(); ++ iNode) {
        if (! (* iNode)->getParent())
            delete * iNode;
    }

    delete m_predicates;
    delete m_variables;
    delete m_types;
    delete m_labels;
    delete m_processes;
    delete m_formulas;
    delete m_pChild;

    if (m_pParent)
        m_pParent->setChild(NULL);
}

void CContext::cleanAdopted() {
    nodes_t::iterator iNode = m_nodes.begin();

    while (iNode != m_nodes.end()) {
        if ((* iNode)->getParent())
            iNode = m_nodes.erase(iNode);
        else
            ++ iNode;
    }

    if (m_pChild)
        m_pChild->cleanAdopted();
}

void CContext::mergeTo(CContext * _pCtx, bool _bMergeFailed) {
    if (! _pCtx)
        return;

    mergeChildren(_bMergeFailed);
    _pCtx->m_loc = m_loc;
    _pCtx->m_messages.splice(_pCtx->m_messages.end(), m_messages);
    _pCtx->m_bFailed = m_bFailed;

    if (m_bScope)
        return;

    for (nodes_t::iterator iNode = m_nodes.begin(); iNode != m_nodes.end(); ++ iNode) {
        if (! (* iNode)->getParent())
            _pCtx->m_nodes.push_back(* iNode);
    }

    m_nodes.clear();

    if (m_predicates) {
        if (! _pCtx->m_predicates)
            std::swap(_pCtx->m_predicates, m_predicates);
        else
            _pCtx->m_predicates->insert(m_predicates->begin(), m_predicates->end());
    }

    if (m_variables) {
        if (! _pCtx->m_variables)
            std::swap(_pCtx->m_variables, m_variables);
        else
            _pCtx->m_variables->insert(m_variables->begin(), m_variables->end());
    }

    if (m_types) {
        if (! _pCtx->m_types)
            std::swap(_pCtx->m_types, m_types);
        else
            _pCtx->m_types->insert(m_types->begin(), m_types->end());
    }

    if (m_labels) {
        if (! _pCtx->m_labels)
            std::swap(_pCtx->m_labels, m_labels);
        else
            _pCtx->m_labels->insert(m_labels->begin(), m_labels->end());
    }

    if (m_processes) {
        if (! _pCtx->m_processes)
            std::swap(_pCtx->m_processes, m_processes);
        else
            _pCtx->m_processes->insert(m_processes->begin(), m_processes->end());
    }

    if (m_formulas) {
        if (! _pCtx->m_formulas)
            std::swap(_pCtx->m_formulas, m_formulas);
        else
            _pCtx->m_formulas->insert(m_formulas->begin(), m_formulas->end());
    }
}

void CContext::mergeChildren(bool _bMergeFailed) {
    if (! m_pChild)
        return;

    cleanAdopted();

    if (_bMergeFailed || ! m_pChild->failed())
        m_pChild->mergeTo(this, _bMergeFailed);

    delete m_pChild;
    m_pChild = NULL;
}

void CContext::fmtWarning(const wchar_t * _strFmt, ...) {
    va_list ap;
    const size_t bufSize = 1024;
    wchar_t buf[bufSize];

    va_start(ap, _strFmt);
    vswprintf(buf, bufSize, _strFmt, ap);
    va_end(ap);
    m_messages.push_back(message_t(message_t::Warning, * m_loc, buf));
}

void CContext::fmtError(const wchar_t * _strFmt, ...) {
    va_list ap;
    const size_t bufSize = 1024;
    wchar_t buf[bufSize];

    va_start(ap, _strFmt);
    vswprintf(buf, bufSize, _strFmt, ap);
    va_end(ap);
    m_messages.push_back(message_t(message_t::Error, * m_loc, buf));
}

CContext * CContext::createChild(bool _bScope) {
    if (m_pChild) {
        cleanAdopted();
        delete m_pChild;
    }

    m_pChild = new CContext(m_loc, _bScope);
    m_pChild->setParent(this);

    return m_pChild;
}

ir::CPredicate * CContext::getPredicate(const std::wstring & _strName) const {
    if (m_predicates) {
        predicate_map_t::const_iterator i = m_predicates->find(_strName);
        if (i != m_predicates->end())
            return i->second;
    }

    if (m_pParent)
        return m_pParent->getPredicate(_strName);

    return ir::CBuiltins::instance().find(_strName);
}

void CContext::addPredicate(ir::CPredicate * _pPred) {
    if (! m_predicates)
        m_predicates = new predicate_map_t();
    (* m_predicates)[_pPred->getName()] = _pPred;
}

ir::CNamedValue * CContext::getVariable(const std::wstring & _strName, bool _bLocal) const {
    if (m_variables) {
        variable_map_t::const_iterator i = m_variables->find(_strName);
        if (i != m_variables->end())
            return i->second;
    }

    return (m_pParent && (! _bLocal || ! m_bScope)) ? m_pParent->getVariable(_strName) : NULL;
}

void CContext::addVariable(ir::CNamedValue * _pVar) {
    if (! m_variables)
        m_variables = new variable_map_t();
    (* m_variables)[_pVar->getName()] = _pVar;
}

ir::CTypeDeclaration * CContext::getType(const std::wstring & _strName) const {
    if (m_types) {
        type_map_t::const_iterator i = m_types->find(_strName);
        if (i != m_types->end())
            return i->second;
    }

    return m_pParent ? m_pParent->getType(_strName) : NULL;
}

void CContext::addType(ir::CTypeDeclaration * _pType) {
    if (! m_types)
        m_types = new type_map_t();
    (* m_types)[_pType->getName()] = _pType;
}

ir::CLabel * CContext::getLabel(const std::wstring & _strName) const {
    if (m_labels) {
        label_map_t::const_iterator i = m_labels->find(_strName);
        if (i != m_labels->end())
            return i->second;
    }

    return m_pParent ? m_pParent->getLabel(_strName) : NULL;
}

void CContext::addLabel(ir::CLabel * _pLabel) {
    if (! m_labels)
        m_labels = new label_map_t();
    (* m_labels)[_pLabel->getName()] = _pLabel;
}

ir::CProcess * CContext::getProcess(const std::wstring & _strName) const {
    if (m_processes) {
        process_map_t::const_iterator i = m_processes->find(_strName);
        if (i != m_processes->end())
            return i->second;
    }

    return m_pParent ? m_pParent->getProcess(_strName) : NULL;
}

void CContext::addProcess(ir::CProcess * _pProcess) {
    if (! m_processes)
        m_processes = new process_map_t();
    (* m_processes)[_pProcess->getName()] = _pProcess;
}

ir::CFormulaDeclaration * CContext::getFormula(const std::wstring & _strName) const {
    if (m_formulas) {
        formula_map_t::const_iterator i = m_formulas->find(_strName);
        if (i != m_formulas->end())
            return i->second;
    }

    return m_pParent ? m_pParent->getFormula(_strName) : NULL;
}

void CContext::addFormula(ir::CFormulaDeclaration * _pFormula) {
    if (! m_formulas)
        m_formulas = new formula_map_t();
    (* m_formulas)[_pFormula->getName()] = _pFormula;
}

bool CContext::consume(int _token1, int _token2, int _token3, int _token4) {
    if (::in(m_loc, _token1, _token2, _token3, _token4)) {
        ++ m_loc;
        return true;
    }

    return false;
}

const std::wstring & CContext::scan(int _nScan, int _nGet) {
    loc_t locGet;
    for (int i = 0; i < _nScan; ++ i, ++ m_loc)
        if (i == _nGet)
            locGet = m_loc;
    return locGet->getValue();
}

void CContext::skip(int _nSkip) {
    for (int i = 0; i < _nSkip; ++ i) ++ m_loc;
}

int CContext::getIntBits() const {
    if (m_pragma.isSet(CPragma::IntBitness))
        return m_pragma.getIntBitness();
    return getParent() ? getParent()->getIntBits() : CNumber::Native;
}

int CContext::getRealBits() const {
    if (m_pragma.isSet(CPragma::RealBitness))
        return m_pragma.getRealBitness();
    return getParent() ? getParent()->getRealBits() : CNumber::Native;
}

const ir::COverflow & CContext::getOverflow() const {
    if (m_pragma.isSet(CPragma::Overflow) || ! getParent())
        return m_pragma.overflow();
    return getParent()->getOverflow();
}

std::wostream & operator << (std::wostream & _os, const message_t & _msg) {
    return _os << L":" << _msg.where.getLine() << L":" << _msg.where.getCol()
                << L": " << (_msg.kind == message_t::Warning ? L"Warning: " : L"Error: ")
                << _msg.str << std::endl;
}
