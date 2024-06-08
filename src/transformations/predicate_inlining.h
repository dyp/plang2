/// \file predicate_inlining.h
///

#ifndef PREDICATE_INLINING_H_
#define PREDICATE_INLINING_H_

#include <set>
#include <iostream>

#include "ir/visitor.h"
#include "utils.h"
#include "predicate_ordering.h"
#include "pp_syntax.h"

namespace ir {

void predicateInlining(const ModulePtr &_module);

}

#endif /* PREDICATE_INLINING_H_ */
