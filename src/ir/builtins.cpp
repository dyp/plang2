/// \file ir_builtins.cpp
///

#include "ir/builtins.h"

namespace ir {

Builtins &Builtins::instance() {
    static Builtins builtins;
    return builtins;
}

PredicatePtr Builtins::find(const std::wstring &_name) const {
    for (size_t i = 0; i < m_predicates.size(); ++ i)
        if (m_predicates.get(i)->getName() == _name)
            return m_predicates.get(i);
    return NULL;
}

Builtins::Builtins() {
    //Predicate * pPred = NULL;

    m_predicates.add(new Predicate(L"print", true));
    m_predicates.add(new Predicate(L"len", true));
    m_predicates.add(new Predicate(L"inc", true));
    m_predicates.add(new Predicate(L"zero", true));
}

}
