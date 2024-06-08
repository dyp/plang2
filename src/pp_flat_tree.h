/// \file pp_flat_tree.h
///

#ifndef PP_FLAT_TREE_H_
#define PP_FLAT_TREE_H_

#include <iostream>

#include "ir/base.h"

void prettyPrintFlatTree(const ir::NodePtr &_node, std::wostream &_os = std::wcout);

#endif /* PP_FLAT_TREE_H_ */
