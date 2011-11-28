/// \file pp_syntax.h
///

#ifndef PP_SYNTAX_H_
#define PP_SYNTAX_H_

#include <iostream>
#include "ir/base.h"

void prettyPrintSyntax(ir::Node &_node, std::wostream & _os = std::wcout);

#endif /* PP_SYNTAX_H_ */