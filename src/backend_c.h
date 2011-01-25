/// \file backend_c.h
///


#ifndef BACKEND_C_H_
#define BACKEND_C_H_

#include <iostream>

#include "llir.h"

namespace backend {

void generateC(const llir::Module & _module, std::wostream & _os);

};

#endif /* BACKEND_C_H_ */
