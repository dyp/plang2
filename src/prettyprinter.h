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
    std::wstring fmtType(int _kind);
};


void prettyPrint(ir::Module & _module, std::wostream & _os);
void prettyPrint(tc::Formulas & _constraints, std::wostream & _os);

void print(ir::Node &_node, std::wostream &_os = std::wcout);


#endif /* PRETTYPRINTER_H_ */
