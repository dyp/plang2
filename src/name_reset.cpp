/// \file name_reset.cpp
///

#include "name_reset.h"

using namespace ir;

class ResetNames : public Visitor {
public:
    bool visitLabel(const LabelPtr &_label) override {
        _label->setName(L"");
        return true;
    }

    bool visitNamedValue(const NamedValuePtr &_val) override {
        _val->setName(L"");
        return true;
    }

    bool visitVariableReference(const VariableReferencePtr &_var) override {
        if (_var->getTarget())
            _var->setName(L"");
        return true;
    }
};

void ir::resetNames(const ModulePtr &_module) {
    ResetNames().traverseNode(_module);
}
