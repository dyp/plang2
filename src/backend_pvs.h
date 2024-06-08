/// \file backend_pvs.h
///

#ifndef BACKEND_PVS_H_
#define BACKEND_PVS_H_

#include <iostream>
#include "ir/base.h"

void generatePvs(const ir::ModulePtr &_module, std::wostream & _os = std::wcout);

#endif /* BACKEND_PVS_H_ */
