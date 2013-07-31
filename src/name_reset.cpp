/// \file name_reset.cpp
///

#include "name_reset.h"

using namespace ir;

class ResetNames : public Visitor {
public:
    virtual bool visitLabel(Label &_label) {
        _label.setName(L"");
        return true;
    }

    virtual bool visitNamedValue(NamedValue &_val) {
        _val.setName(L"");
        return true;
    }

    virtual bool visitVariableReference(VariableReference &_var) {
        if (_var.getTarget());
            _var.setName(L"");
        return true;
    }
};

void ir::resetNames(Module &_module) {
    ResetNames().traverseNode(_module);
}
