/// \file generate_name.h
///

#ifndef GENERATE_NAME_H_
#define GENERATE_NAME_H_

#include "autoptr.h"
#include "ir/base.h"
#include "utils.h"

#include <set>
#include <functional>

class NameGenerator {
public:
    NameGenerator() :
        m_nLastFoundValue(0), m_nLastFoundType(0), m_nLastFoundFormula(0),
        m_nLastFoundLabel(0)
    {}

    void collect(const ir::NodePtr& _node);

    void addNamedValue(const ir::NamedValuePtr& _pVal);
    void addLabel(const ir::LabelPtr& _pLabel);
    void addType(const ir::TypeDeclarationPtr& _pType);
    void addFormula(const ir::FormulaDeclarationPtr& _pFormula);

    std::wstring getNamedValueName(const ir::NamedValuePtr& _val);
    std::wstring getLabelName(const ir::LabelPtr& _label);
    std::wstring getTypeName(const ir::TypeDeclarationPtr& _type);
    std::wstring getTypeName(const ir::NamedReferenceTypePtr& _type);
    std::wstring getFormulaName(const ir::FormulaDeclarationPtr& _formula);
    std::wstring getFormulaName(const ir::FormulaCallPtr& _formula);

    // FIXME Delete when will be useless.
    std::wstring getNewLabelName(const std::wstring& _name);

    void clear();

private:
    std::set<std::wstring> m_usedIdentifiers;
    std::map<ir::NamedValuePtr, std::wstring> m_namedValues;
    std::map<ir::TypeDeclarationPtr, std::wstring> m_types;
    std::map<ir::FormulaDeclarationPtr, std::wstring> m_formulas;
    std::set<std::wstring> m_usedLabels;
    std::map<ir::LabelPtr, std::wstring> m_labels;
    int m_nLastFoundValue, m_nLastFoundType, m_nLastFoundFormula, m_nLastFoundLabel;

    static std::wstring _generateUniqueName(std::set<std::wstring>& _used, int& _cCounter,
        const std::wstring& _strFormat, std::function<std::wstring(int)> _function = &intToWideStr);
};

#endif /* GENERATE_NAME_H_ */
