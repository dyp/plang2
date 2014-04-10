/// \file generate_name.h
///

#ifndef GENERATE_NAME_H_
#define GENERATE_NAME_H_

#include "autoptr.h"
#include "ir/base.h"
#include "utils.h"

#include <set>
#include <functional>

class NameGenerator : public Counted {
public:
    NameGenerator() :
        m_nLastFoundValue(0), m_nLastFoundType(0), m_nLastFoundFormula(0),
        m_nLastFoundLabel(0)
    {}

    void collect(ir::Node& _node);

    void addNamedValue(const ir::NamedValuePtr& _pVal);
    void addLabel(const ir::LabelPtr& _pLabel);
    void addType(const ir::TypeDeclarationPtr& _pType);
    void addFormula(const ir::FormulaDeclarationPtr& _pFormula);

    std::wstring getNamedValueName(ir::NamedValue& _val);
    std::wstring getLabelName(ir::Label& _label);
    std::wstring getTypeName(ir::TypeDeclaration& _type);
    std::wstring getTypeName(ir::NamedReferenceType& _type);
    std::wstring getFormulaName(ir::FormulaDeclaration& _formula);
    std::wstring getFormulaName(ir::FormulaCall& _formula);

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
