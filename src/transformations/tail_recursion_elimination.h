/// \file tail_recursion_elimination.h
///

#ifndef TAIL_RECURSION_ELIMINATION_H_
#define TAIL_RECURSION_ELIMINATION_H_

#include <set>
#include <list>

#include "ir/declarations.h"
#include "ir/visitor.h"
#include "utils.h"
#include "pp_syntax.h"

namespace ir{

void tailRecursionElimination(const ModulePtr &_module);

}

#endif /* TAIL_RECURSION_ELIMINATION_H_ */
