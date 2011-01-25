/// \file parser_context.cpp
///

#include <stdarg.h>

#include "parser_context.h"
#include "ir/numbers.h"
#include "ir/builtins.h"

using namespace lexer;

Context::~Context() {
    cleanAdopted();

    for (Nodes::iterator iNode = m_nodes.begin(); iNode != m_nodes.end(); ++ iNode) {
        if (! (* iNode)->getParent())
            delete * iNode;
    }

    delete m_predicates;
    delete m_variables;
    delete m_types;
    delete m_labels;
    delete m_processes;
    delete m_formulas;
    delete m_constructors;
    delete m_pChild;

    if (m_pParent)
        m_pParent->setChild(NULL);
}

void Context::cleanAdopted() {
    Nodes::iterator iNode = m_nodes.begin();

    while (iNode != m_nodes.end()) {
        if ((* iNode)->getParent())
            iNode = m_nodes.erase(iNode);
        else
            ++ iNode;
    }

    if (m_pChild)
        m_pChild->cleanAdopted();
}

void Context::mergeTo(Context * _pCtx, bool _bMergeFailed) {
    if (! _pCtx)
        return;

    mergeChildren(_bMergeFailed);
    if (* _pCtx->m_loc < * m_loc)
        _pCtx->m_loc = m_loc;
    _pCtx->m_messages.splice(_pCtx->m_messages.end(), m_messages);
    _pCtx->m_bFailed = m_bFailed;

    if (m_bScope)
        return;

    for (Nodes::iterator iNode = m_nodes.begin(); iNode != m_nodes.end(); ++ iNode) {
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

    if (m_constructors) {
        if (! _pCtx->m_constructors)
            std::swap(_pCtx->m_constructors, m_constructors);
        else
            _pCtx->m_constructors->insert(m_constructors->begin(), m_constructors->end());
    }
}

void Context::mergeChildren(bool _bMergeFailed) {
    if (! m_pChild)
        return;

    cleanAdopted();

    if (_bMergeFailed || ! m_pChild->failed())
        m_pChild->mergeTo(this, _bMergeFailed);

    delete m_pChild;
    m_pChild = NULL;
}

void Context::fmtWarning(const wchar_t * _strFmt, ...) {
    va_list ap;
    const size_t bufSize = 1024;
    wchar_t buf[bufSize];

    va_start(ap, _strFmt);
    vswprintf(buf, bufSize, _strFmt, ap);
    va_end(ap);
    m_messages.push_back(StatusMessage(StatusMessage::Warning, * m_loc, buf));
}

void Context::fmtError(const wchar_t * _strFmt, ...) {
    va_list ap;
    const size_t bufSize = 1024;
    wchar_t buf[bufSize];

    va_start(ap, _strFmt);
    vswprintf(buf, bufSize, _strFmt, ap);
    va_end(ap);
    m_messages.push_back(StatusMessage(StatusMessage::Error, * m_loc, buf));
}

Context * Context::createChild(bool _bScope) {
    if (m_pChild) {
        cleanAdopted();
        delete m_pChild;
    }

    m_pChild = new Context(m_loc, _bScope);
    m_pChild->setParent(this);

    return m_pChild;
}

bool Context::getPredicates(const std::wstring & _strName, ir::Predicates & _predicates) const {
    if (m_predicates) {
        std::pair<PredicateMap::iterator, PredicateMap::iterator> bounds = m_predicates->equal_range(_strName);
        for (PredicateMap::iterator i = bounds.first; i != bounds.second; ++ i)
            _predicates.add(i->second, false);
    }

    if (m_pParent)
        m_pParent->getPredicates(_strName, _predicates);

    return ! _predicates.empty();
}

ir::Predicate * Context::getPredicate(const std::wstring & _strName) const {
    if (m_predicates) {
        PredicateMap::const_iterator i = m_predicates->find(_strName);
        if (i != m_predicates->end())
            return i->second;
    }

    if (m_pParent)
        return m_pParent->getPredicate(_strName);

    return ir::Builtins::instance().find(_strName);
}

void Context::addPredicate(ir::Predicate * _pPred) {
    if (! m_predicates)
        m_predicates = new PredicateMap();
    m_predicates->insert(std::make_pair(_pPred->getName(), _pPred));
}

ir::NamedValue * Context::getVariable(const std::wstring & _strName, bool _bLocal) const {
    if (m_variables) {
        VariableMap::const_iterator i = m_variables->find(_strName);
        if (i != m_variables->end())
            return i->second;
    }

    return (m_pParent && (! _bLocal || ! m_bScope)) ? m_pParent->getVariable(_strName) : NULL;
}

void Context::addVariable(ir::NamedValue * _pVar) {
    if (! m_variables)
        m_variables = new VariableMap();
    (* m_variables)[_pVar->getName()] = _pVar;
}

ir::TypeDeclaration * Context::getType(const std::wstring & _strName) const {
    if (m_types) {
        TypeMap::const_iterator i = m_types->find(_strName);
        if (i != m_types->end())
            return i->second;
    }

    return m_pParent ? m_pParent->getType(_strName) : NULL;
}

void Context::addType(ir::TypeDeclaration * _pType) {
    if (! m_types)
        m_types = new TypeMap();
    (* m_types)[_pType->getName()] = _pType;
}

ir::Label * Context::getLabel(const std::wstring & _strName) const {
    if (m_labels) {
        LabelMap::const_iterator i = m_labels->find(_strName);
        if (i != m_labels->end())
            return i->second;
    }

    return m_pParent ? m_pParent->getLabel(_strName) : NULL;
}

void Context::addLabel(ir::Label * _pLabel) {
    if (! m_labels)
        m_labels = new LabelMap();
    (* m_labels)[_pLabel->getName()] = _pLabel;
}

ir::Process * Context::getProcess(const std::wstring & _strName) const {
    if (m_processes) {
        ProcessMap::const_iterator i = m_processes->find(_strName);
        if (i != m_processes->end())
            return i->second;
    }

    return m_pParent ? m_pParent->getProcess(_strName) : NULL;
}

void Context::addProcess(ir::Process * _pProcess) {
    if (! m_processes)
        m_processes = new ProcessMap();
    (* m_processes)[_pProcess->getName()] = _pProcess;
}

ir::FormulaDeclaration * Context::getFormula(const std::wstring & _strName) const {
    if (m_formulas) {
        FormulaMap::const_iterator i = m_formulas->find(_strName);
        if (i != m_formulas->end())
            return i->second;
    }

    return m_pParent ? m_pParent->getFormula(_strName) : NULL;
}

void Context::addFormula(ir::FormulaDeclaration * _pFormula) {
    if (! m_formulas)
        m_formulas = new FormulaMap();
    (* m_formulas)[_pFormula->getName()] = _pFormula;
}

bool Context::getConstructors(const std::wstring & _strName, ir::UnionConstructorDeclarations & _cons) const {
    if (m_constructors) {
        std::pair<ConsMap::iterator, ConsMap::iterator> bounds = m_constructors->equal_range(_strName);
        for (ConsMap::iterator iCons = bounds.first; iCons != bounds.second; ++ iCons)
            _cons.add(iCons->second, false);
    }

    if (m_pParent)
        m_pParent->getConstructors(_strName, _cons);

    return ! _cons.empty();
}

ir::UnionConstructorDeclaration * Context::getConstructor(const std::wstring & _strName) const {
    ir::UnionConstructorDeclarations cons;
    getConstructors(_strName, cons);

    return cons.size() == 1 ? cons.get(0) : NULL;
}

void Context::addConstructor(ir::UnionConstructorDeclaration * _pCons) {
    if (! m_constructors)
        m_constructors = new ConsMap();
    m_constructors->insert(std::make_pair(_pCons->getName(), _pCons));
}

bool Context::consume(int _token1, int _token2, int _token3, int _token4) {
    if (::in(m_loc, _token1, _token2, _token3, _token4)) {
        ++ m_loc;
        return true;
    }

    return false;
}

const std::wstring & Context::scan(int _nScan, int _nGet) {
    Loc locGet;
    for (int i = 0; i < _nScan; ++ i, ++ m_loc)
        if (i == _nGet)
            locGet = m_loc;
    return locGet->getValue();
}

void Context::skip(int _nSkip) {
    for (int i = 0; i < _nSkip; ++ i) ++ m_loc;
}

int Context::getIntBits() const {
    if (m_pragma.isSet(Pragma::IntBitness))
        return m_pragma.getIntBitness();
    return getParent() ? getParent()->getIntBits() : Number::GENERIC;
//    return getParent() ? getParent()->getIntBits() : Number::Native;
}

int Context::getRealBits() const {
    if (m_pragma.isSet(Pragma::RealBitness))
        return m_pragma.getRealBitness();
    return getParent() ? getParent()->getRealBits() : Number::GENERIC;
//    return getParent() ? getParent()->getRealBits() : Number::Native;
}

const ir::Overflow & Context::getOverflow() const {
    if (m_pragma.isSet(Pragma::Overflow) || ! getParent())
        return m_pragma.overflow();
    return getParent()->getOverflow();
}

std::wostream & operator << (std::wostream & _os, const StatusMessage & _msg) {
    return _os << L":" << _msg.where.getLine() << L":" << _msg.where.getCol()
                << L": " << (_msg.kind == StatusMessage::Warning ? L"Warning: " : L"Error: ")
                << _msg.str << std::endl;
}
