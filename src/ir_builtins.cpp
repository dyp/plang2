/// \file ir_builtins.cpp
///

#include "ir/builtins.h"

namespace ir {


CBuiltins & CBuiltins::instance() {
    static CBuiltins builtins;
    return builtins;
}

CPredicate * CBuiltins::find(const std::wstring & _name) const {
    for (size_t i = 0; i < m_predicates.size(); ++ i)
        if (m_predicates.get(i)->getName() == _name)
            return m_predicates.get(i);
    return NULL;
}

CBuiltins::CBuiltins() {
    //CPredicate * pPred = NULL;

    m_predicates.add(new CPredicate(L"print", true));
}

}

