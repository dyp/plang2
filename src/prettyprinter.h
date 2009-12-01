/// \file prettyprinter.h
///


#ifndef PRETTYPRINTER_H_
#define PRETTYPRINTER_H_

#include <iostream>

#include "ir/declarations.h"

void prettyPrint(ir::CModule & _module, std::wostream & _os);


#endif /* PRETTYPRINTER_H_ */
