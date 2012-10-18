/// \file prettyprinter.h
///


#ifndef PRETTYPRINTER_H_
#define PRETTYPRINTER_H_

#include <iostream>

#include "ir/declarations.h"
#include "typecheck.h"
#include "ir/visitor.h"

class PrettyPrinterBase: public ir::Visitor {
public:
    PrettyPrinterBase(std::wostream &_os) : m_os(_os) {}

protected:
    std::wostream &m_os;

    std::wstring fmtIndent(const std::wstring &_s);
    std::wstring fmtFreshType(tc::FreshType &_type);
    std::wstring fmtType(int _kind);
};


void prettyPrint(ir::Module &_module, std::wostream &_os);
void prettyPrint(tc::Context &_constraints, std::wostream &_os);
void prettyPrint(const tc::Formula &_formula, std::wostream &_os, bool _bNewLine = true);

// Compact pretty-printer flags.
enum {
    PPC_NONE = 0x00,
    PPC_NO_INT_BITS = 0x01,
    PPC_NO_REAL_BITS = 0x02,
    PPC_NO_INCOMPLETE_TYPES = 0x04,
};

void prettyPrintCompact(ir::Node &_node, std::wostream &_os, int _nFlags = 0);

void print(ir::Node &_node, std::wostream &_os = std::wcout);


#endif /* PRETTYPRINTER_H_ */
