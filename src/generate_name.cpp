/// \file generate_name.cpp
///

#include "generate_name.h"
#include "ir/visitor.h"
#include "utils.h"

using namespace ir;

class NamesCollector : public Visitor {
public:
    NamesCollector(NameGenerator& _generator) : m_generator(_generator) {}

    virtual bool visitNamedValue(NamedValue & _val) {
        m_generator.addNamedValue(&_val);
        return true;
    }

    virtual bool visitVariableReference(VariableReference & _var) {
        if (_var.getTarget())
            visitNamedValue(*_var.getTarget());
        return true;
    }

    virtual bool visitLabel(Label & _label) {
        m_generator.addLabel(&_label);
        return true;
    }

private:
    NameGenerator& m_generator;
};

void NameGenerator::collect(Node& _node) {
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

std::wstring NameGenerator::_generateUniqueName(std::set<std::wstring>& _used,
    int& _nCounter, const std::wstring& _strFormat)
{
    std::wstring strIdent;

    do {
        const std::wstring
            strUniquePart = intToAlpha(_nCounter++);
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

std::wstring NameGenerator::getNamedValueName(NamedValue& _val) {
    auto iNamedValue = m_namedValues.find(&_val);
    std::wstring strIdent = iNamedValue != m_namedValues.end() ?
        iNamedValue->second : L"";

    if (strIdent.empty()) {
        strIdent = _generateUniqueName(m_usedIdentifiers, m_nLastFoundIdentifier, L"%ls");
        m_namedValues[&_val] = strIdent;
    }

    return strIdent;
}

std::wstring NameGenerator::getLabelName(Label& _label) {
    auto iLabel = m_labels.find(&_label);
    std::wstring strLabel = iLabel != m_labels.end() ?
        iLabel->second : L"";

    if (strLabel.empty()) {
        strLabel = _generateUniqueName(m_usedLabels, m_nLastFoundLabel, L"l_%ls");
        m_labels[&_label] = strLabel;
    }

    return strLabel;
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
    m_nLastFoundIdentifier = 0;
    m_nLastFoundLabel = 0;
}
