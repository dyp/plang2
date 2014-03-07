/// \file generate_name.h
///

#ifndef GENERATE_NAME_H_
#define GENERATE_NAME_H_

#include "autoptr.h"
#include "ir/base.h"

#include <set>

class NameGenerator : public Counted {
public:
    NameGenerator() : m_nLastFoundIdentifier(0), m_nLastFoundLabel(0) {}

    void collect(ir::Node& _node);

    void addNamedValue(const ir::NamedValuePtr& _pVal);
    void addLabel(const ir::LabelPtr& _pLabel);

    std::wstring getNamedValueName(ir::NamedValue& _val);
    std::wstring getLabelName(ir::Label& _label);

    // FIXME Delete when will be useless.
    std::wstring getNewLabelName(const std::wstring& _name);

    void clear();

private:
    std::set<std::wstring> m_usedIdentifiers;
    std::map<ir::NamedValuePtr, std::wstring> m_namedValues;
    std::set<std::wstring> m_usedLabels;
    std::map<ir::LabelPtr, std::wstring> m_labels;
    int m_nLastFoundIdentifier;
    int m_nLastFoundLabel;

    static std::wstring _generateUniqueName(std::set<std::wstring>& _used,
        int& _cCounter, const std::wstring& _strFormat);
};

#endif /* GENERATE_NAME_H_ */
