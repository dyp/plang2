/// \file prettyprinter.h
///


#ifndef PRETTYPRINTER_H_
#define PRETTYPRINTER_H_

#include <iostream>

#include "ir/declarations.h"
#include "typecheck.h"

void prettyPrint(ir::Module & _module, std::wostream & _os);
void prettyPrint(tc::Formulas & _constraints, std::wostream & _os);


#endif /* PRETTYPRINTER_H_ */
