/// \file backend_debug.h
///


#ifndef BACKEND_DEBUG_H_
#define BACKEND_DEBUG_H_

#include <iostream>

#include "llir.h"

namespace backend {

void generateDebug(const llir::CModule & _module, std::wostream & _os);

};

#endif /* BACKEND_DEBUG_H_ */
