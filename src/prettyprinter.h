/// \file prettyprinter.h
///


#ifndef PRETTYPRINTER_H_
#define PRETTYPRINTER_H_

#include <iostream>
#include <map>

#include "ir/declarations.h"
#include "typecheck.h"
#include "verification.h"
#include "ir/visitor.h"
#include "indenting_stream.h"

using FreshTypeNames = std::map<size_t, std::wstring>;

class PrettyPrinterBase: public ir::Visitor {
public:
    template<typename _Stream>
    PrettyPrinterBase(_Stream &_os) : m_os(_os) {}

    static void setFreshTypeNames(const FreshTypeNames & _names);

protected:
    IndentingStream<wchar_t> m_os;

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

void print(ir::Node &_node, std::wostream &_os = std::wcout);


#endif /* PRETTYPRINTER_H_ */
