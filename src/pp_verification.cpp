/// \file statement_tree.cpp
///

#include "pp_syntax.h"
#include "prettyprinter.h"
#include "utils.h"
#include "options.h"

using namespace vf;
using namespace na;

namespace pp {

std::wstring fmtRule(size_t _cRuleInd) {
    switch (_cRuleInd) {
        case vf::Context::TRANSFER:          return L"Transfer";
        case vf::Context::SIMPLIFICATION:    return L"Simplification";
        case vf::Context::SPLIT:             return L"Split";
        case vf::Context::RP:                return L"RP";
        case vf::Context::RS:                return L"RS";
        case vf::Context::RC:                return L"RC";
        case vf::Context::RB:                return L"RB";
        case vf::Context::QP:                return L"QP";
        case vf::Context::QSB:               return L"QSB";
        case vf::Context::QS:                return L"QS";
        case vf::Context::QC:                return L"QC";
        case vf::Context::FP:                return L"FP";
        case vf::Context::FS:                return L"FS";
        case vf::Context::FC:                return L"FC";
        case vf::Context::EP:                return L"EP";
        case vf::Context::ES:                return L"ES";
        case vf::Context::EC:                return L"EC";
        case vf::Context::EB:                return L"EB";
        case vf::Context::FLS:               return L"FLS";
        case vf::Context::FLC:               return L"FLC";
        case vf::Context::FLP:               return L"FLP";
        case vf::Context::FLB:               return L"FLB";
    }
    return L"";
}

void prettyPrint(const ConjunctPtr& _pConjunct, std::wostream &_os, const ContextPtr& _pContext) {
    ContextPtr pContext = !_pContext ? new Context() : _pContext;
    switch (_pConjunct->getKind()) {
        case Conjunct::QUANTIFIER: {
            const ValuesSet& bound = _pConjunct.as<QuantifierConjunct>()->getBound();
            _os << L"\x2203 ";
            for (ValuesSet::iterator i = bound.begin(); i != bound.end(); ++i) {
                prettyPrintSyntax(**i, _os, _pContext);
                if (i != ::prev(bound.end()))
                    _os << L", ";
            }
            _os << ". ";
            prettyPrint(_pConjunct.as<QuantifierConjunct>()->getConjunct(), _os, pContext);
            break;
        }

        case Conjunct::LOGIC:
            _os << "L(";
            prettyPrintCompact(*_pConjunct.as<LogicConjunct>()->getStatement(), _os, 0, pContext);
            _os << ")";
            break;

        case Conjunct::FORMULA:
            prettyPrintSyntax(*_pConjunct.as<FormulaConjunct>()->getExpression(), _os, pContext);
            break;
    }
}

static void _updateContext(const ConjunctionPtr& _pConj, const ContextPtr& _pContext) {
    if (!_pContext || !_pConj)
        return;
    for (std::set<ConjunctPtr>::const_iterator i = _pConj->getConjuncts().begin();
        i != _pConj->getConjuncts().end(); ++i) {
        ValuesSet container;
        (*i)->getFreeValues(container);
        for (ValuesSet::iterator i = container.begin(); i != container.end(); ++i)
            _pContext->addNamedValue(*i);
    }
}

void prettyPrint(const ConjunctionPtr& _pConj, std::wostream &_os, const ContextPtr& _pContext) {
    if (!_pConj)
        return;

    ContextPtr pContext = !_pContext ? new Context() : _pContext;
    _updateContext(_pConj, pContext);

    const std::set<ConjunctPtr>& conjuncts = _pConj->getConjuncts();
    for (std::set<ConjunctPtr>::const_iterator i = conjuncts.begin(); i != conjuncts.end(); ++i) {
        prettyPrint(*i, _os, pContext);
        if (::next(i) != conjuncts.end())
            _os << L" \x2227 ";
    }
}

void prettyPrint(const Condition& _cond, std::wostream &_os, const ContextPtr& _pContext) {
    ContextPtr pContext = !_pContext ? new Context() : _pContext;

    switch (_cond.getKind()) {
        case Condition::SEQUENT: {
            const Sequent& sequent = (const Sequent&)_cond;

            _updateContext(sequent.left(), pContext);
            _updateContext(sequent.right(), pContext);

            prettyPrint(sequent.left(), _os, pContext);
            _os << L" \x22A2 ";
            prettyPrint(sequent.right(), _os, pContext);
            break;
        }
        case Condition::CORRECTNESS: {
            const Correctness& corr = (const Correctness&)_cond;

            _os << L"{ ";
            prettyPrint(corr.getPrecondition(), _os);
            _os << L" } ";
            prettyPrintCompact(*corr.getStatement(), _os);
            _os << L" { ";
            prettyPrint(corr.getPostcondition(), _os);
            _os << L" }";
            break;
        }
    }
}

void prettyPrint(const vf::Context& _context, std::wostream &_os, const ContextPtr& _pContext) {
    ContextPtr pContext = !_pContext ? new Context() : _pContext;

    size_t cIndex = 1;

    for (std::list<std::pair<ConditionPtr, bool> >::const_iterator i = _context.m_conditions.begin();
        i != _context.m_conditions.end(); ++i) {
        _os << ((*i).second ? L"{" : L"[") << L"-" << cIndex++ << ((*i).second ? "}" : "]") << " ";
        prettyPrint(*(*i).first, _os, pContext);
        _os << L"\n";
    }

    cIndex = 1;
    _os << L"-------\n";

    for (std::list<std::pair<ir::ExpressionPtr, bool> >::const_iterator i = _context.m_lemmas.begin();
        i != _context.m_lemmas.end(); ++i) {
        _os << ((*i).second ? L"{" : L"[") << L"+" << cIndex++ << ((*i).second ? "}" : "]") << " ";
        prettyPrintSyntax(*(*i).first, _os);
        _os << L"\n";
    }
}

}
