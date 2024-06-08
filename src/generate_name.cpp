/// \file generate_name.cpp
///

#include "generate_name.h"
#include "ir/visitor.h"
#include "utils.h"

using namespace ir;

class NamesCollector : public Visitor {
public:
    NamesCollector(NameGenerator& _generator) : m_generator(_generator) {}

    bool visitNamedValue(const NamedValuePtr & _val) override {
        m_generator.addNamedValue(_val);
        return true;
    }

    bool visitVariableReference(const VariableReferencePtr & _var) override {
        if (_var->getTarget())
            visitNamedValue(_var->getTarget());
        return true;
    }

    bool visitLabel(const LabelPtr & _label) override {
        m_generator.addLabel(_label);
        return true;
    }

    bool visitTypeDeclaration(const TypeDeclarationPtr & _type) override {
        m_generator.addType(_type);
        return true;
    }

    bool visitNamedReferenceType(const NamedReferenceTypePtr & _type) override {
        if (_type->getDeclaration())
            visitTypeDeclaration(_type->getDeclaration());
        return true;
    }

    bool visitFormulaDeclaration(const FormulaDeclarationPtr & _formula) override {
        m_generator.addFormula(_formula);
        return true;
    }

    bool visitFormulaCall(const FormulaCallPtr & _formula) override {
        if (_formula->getTarget())
            visitFormulaDeclaration(_formula->getTarget());
        return true;
    }

private:
    NameGenerator& m_generator;
};

void NameGenerator::collect(const NodePtr& _node) {
    NamesCollector(*this).traverseNode(_node);
}

void NameGenerator::addNamedValue(const NamedValuePtr& _pVal) {
    if (!_pVal || _pVal->getName().empty())
        return;
    m_namedValues.insert({_pVal, _pVal->getName()});
    m_usedIdentifiers.insert(_pVal->getName());
}

void NameGenerator::addLabel(const LabelPtr& _pLabel) {
    if (!_pLabel || _pLabel->getName().empty())
        return;
    m_labels.insert({_pLabel, _pLabel->getName()});
    m_usedLabels.insert(_pLabel->getName());
}

void NameGenerator::addType(const ir::TypeDeclarationPtr& _pType) {
    if (!_pType || _pType->getName().empty())
        return;
    m_types.insert({_pType, _pType->getName()});
    m_usedIdentifiers.insert(_pType->getName());
}

void NameGenerator::addFormula(const ir::FormulaDeclarationPtr& _pFormula) {
    if (!_pFormula || _pFormula->getName().empty())
        return;
    m_formulas.insert({_pFormula, _pFormula->getName()});
    m_usedIdentifiers.insert(_pFormula->getName());
}

std::wstring NameGenerator::_generateUniqueName(std::set<std::wstring>& _used,
    int& _nCounter, const std::wstring& _strFormat, std::function<std::wstring(int)> _function)
{
    std::wstring strIdent;

    do {
        const std::wstring
            strUniquePart = _function(_nCounter++);
        size_t cSize =
            _strFormat.size() + strUniquePart.size() - 2;

        std::vector<wchar_t> buffer;

        do {
            buffer.resize(cSize + 1);

            const int nWritten = swprintf(buffer.data(), buffer.size(), _strFormat.c_str(), strUniquePart.c_str());
            assert(nWritten >= 0);

            if (nWritten < 0)
                return L"";

            cSize = (size_t)nWritten;
        } while (cSize != buffer.size() - 1);

        strIdent = buffer.data();
    } while (!_used.insert(strIdent).second);

    return strIdent;
}

std::wstring NameGenerator::getNamedValueName(const NamedValuePtr& _val) {
    auto iNamedValue = m_namedValues.find(_val);
    std::wstring strIdent = iNamedValue != m_namedValues.end() ?
        iNamedValue->second : L"";

    if (strIdent.empty()) {
        strIdent = _generateUniqueName(m_usedIdentifiers, m_nLastFoundValue, L"%ls", intToAlpha);
        m_namedValues[_val] = strIdent;
    }

    return strIdent;
}

std::wstring NameGenerator::getLabelName(const LabelPtr& _label) {
    auto iLabel = m_labels.find(_label);
    std::wstring strLabel = iLabel != m_labels.end() ?
        iLabel->second : L"";

    if (strLabel.empty()) {
        strLabel = _generateUniqueName(m_usedLabels, m_nLastFoundLabel, L"l_%ls");
        m_labels[_label] = strLabel;
    }

    return strLabel;
}

std::wstring NameGenerator::getTypeName(const ir::TypeDeclarationPtr& _type) {
    auto iType = m_types.find(_type);
    std::wstring strIdent = iType != m_types.end() ?
        iType->second : L"";

    if (strIdent.empty()) {
        strIdent = _generateUniqueName(m_usedIdentifiers, m_nLastFoundType, L"T_%ls");
        m_types[_type] = strIdent;
    }

    return strIdent;
}

std::wstring NameGenerator::getTypeName(const ir::NamedReferenceTypePtr& _type) {
    return !_type->getDeclaration() ?
        _generateUniqueName(m_usedIdentifiers, m_nLastFoundType, L"UnknownType_%ls") :
        getTypeName(_type->getDeclaration());
}

std::wstring NameGenerator::getFormulaName(const ir::FormulaDeclarationPtr& _formula) {
    auto iFormula = m_formulas.find(_formula);
    std::wstring strIdent = iFormula != m_formulas.end() ?
        iFormula->second : L"";

    if (strIdent.empty()) {
        strIdent = _generateUniqueName(m_usedIdentifiers, m_nLastFoundFormula, L"f_%ls");
        m_formulas[_formula] = strIdent;
    }

    return strIdent;
}

std::wstring NameGenerator::getFormulaName(const ir::FormulaCallPtr& _formula) {
    return !_formula->getTarget() ?
        _generateUniqueName(m_usedIdentifiers, m_nLastFoundFormula, L"unknownFormula_%ls") :
        getFormulaName(_formula->getTarget());
}

std::wstring NameGenerator::getNewLabelName(const std::wstring& _name) {
    for (size_t i = 1;; ++i) {
        const std::wstring strName = _name + fmtInt(i, L"%d");
        if (m_usedLabels.insert(strName).second)
            return strName;
    }

    return L"";
}

void NameGenerator::clear() {
    m_usedIdentifiers.clear();
    m_usedLabels.clear();
    m_namedValues.clear();
    m_labels.clear();
    m_types.clear();
    m_formulas.clear();
    m_nLastFoundValue = 0;
    m_nLastFoundType = 0;
    m_nLastFoundFormula = 0;
    m_nLastFoundLabel = 0;
}
