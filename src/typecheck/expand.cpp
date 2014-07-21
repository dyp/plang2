/// \file expand.cpp
///

#include "operations.h"
#include "node_analysis.h"

using namespace ir;

namespace tc {

class Expand : public Operation {
protected:
    virtual bool _run(int & _nResult);

private:
    bool _expandPredicate(int _kind, const PredicateTypePtr &_pLhs,
            const PredicateTypePtr &_pRhs, tc::FormulaList &_formulas);
    bool _expandStruct(int _kind, const StructTypePtr &_pLhs, const StructTypePtr &_pRhs, tc::FormulaList &_formulas, bool _bAllowCompound);
    bool _expandSet(int _kind, const SetTypePtr &_pLhs, const SetTypePtr &_pRhs, tc::FormulaList &_formulas);
    bool _expandList(int _kind, const ListTypePtr &_pLhs, const ListTypePtr &_pRhs, tc::FormulaList &_formulas);
    bool _expandMap(int _kind, const MapTypePtr &_pLhs, const MapTypePtr &_pRhs, tc::FormulaList &_formulas);
    bool _expandArray(int _kind, const ArrayTypePtr &_pLhs, const ArrayTypePtr &_pRhs, tc::FormulaList &_formulas);
    bool _expandType(int _kind, const TypeTypePtr &_pLhs, const TypeTypePtr &_pRhs, tc::FormulaList &_formulas);
    bool _expandSubtype(int _kind, const SubtypePtr &_pLhs, const SubtypePtr &_pRhs, tc::FormulaList &_formulas);
    bool _expandSubtypeWithType(int _kind, const TypePtr &_pLhs, const TypePtr &_pRhs, tc::FormulaList &_formulas);
    bool _expandUnionType(int _kind, const UnionTypePtr& _pLhs, const UnionTypePtr& _pRhs, tc::FormulaList &_formulas);
};

bool Expand::_expandPredicate(int _kind, const PredicateTypePtr &_pLhs,
        const PredicateTypePtr &_pRhs, tc::FormulaList &_formulas)
{
    if (_pLhs->getInParams().size() != _pRhs->getInParams().size())
        return false;

    if (_pLhs->getOutParams().size() != _pRhs->getOutParams().size())
        return false;

    for (size_t i = 0; i < _pLhs->getInParams().size(); ++ i) {
        Param &p = *_pLhs->getInParams().get(i);
        Param &q = *_pRhs->getInParams().get(i);

        if (p.getType()->getKind() == Type::TYPE || q.getType()->getKind() == Type::TYPE)
            _formulas.push_back(new tc::Formula(tc::Formula::EQUALS, p.getType(), q.getType()));
        else
            _formulas.push_back(new tc::Formula(_kind, q.getType(), p.getType()));
    }

    for (size_t j = 0; j < _pLhs->getOutParams().size(); ++j) {
        Branch &b = *_pLhs->getOutParams().get(j);
        Branch &c = *_pRhs->getOutParams().get(j);

        if (b.size() != c.size())
            return false;

        for (size_t i = 0; i < b.size(); ++ i) {
            Param &p = *b.get(i);
            Param &q = *c.get(i);

            _formulas.push_back(new tc::Formula(_kind, p.getType(), q.getType()));
        }
    }

    return true;
}

bool Expand::_expandStruct(int _kind, const StructTypePtr &_pLhs, const StructTypePtr &_pRhs,
        tc::FormulaList & _formulas, bool _bAllowCompound)
{
    const size_t cOrdFieldsL = _pLhs->getNamesOrd().size() + _pLhs->getTypesOrd().size();
    const size_t cOrdFieldsR = _pRhs->getNamesOrd().size() + _pRhs->getTypesOrd().size();
    tc::CompoundFormulaPtr pStrict = _kind == tc::Formula::SUBTYPE_STRICT ? new tc::CompoundFormula() : NULL;

    for (size_t i = 0; i < cOrdFieldsL && i < cOrdFieldsR; ++i) {
        NamedValuePtr pFieldL = i < _pLhs->getNamesOrd().size() ? _pLhs->getNamesOrd().get(i) :
                _pLhs->getTypesOrd().get(i - _pLhs->getNamesOrd().size());
        NamedValuePtr pFieldR = i < _pRhs->getNamesOrd().size() ? _pRhs->getNamesOrd().get(i) :
                _pRhs->getTypesOrd().get(i - _pRhs->getNamesOrd().size());

        _formulas.push_back(new tc::Formula(_kind, pFieldL->getType(), pFieldR->getType()));

        if (pStrict)
            pStrict->addPart().insert(new tc::Formula(tc::Formula::SUBTYPE_STRICT,
                    pFieldL->getType(), pFieldR->getType()));
    }

    if (cOrdFieldsL < cOrdFieldsR)
        return false;

    typedef std::map<std::wstring, std::pair<NamedValuePtr, NamedValuePtr> > NameMap;
    NameMap fields;

    for (size_t i = 0; i < _pLhs->getNamesSet().size(); ++i)
        fields[_pLhs->getNamesSet().get(i)->getName()].first = _pLhs->getNamesSet().get(i);

    for (size_t i = 0; i < _pRhs->getNamesSet().size(); ++i)
        fields[_pRhs->getNamesSet().get(i)->getName()].second = _pRhs->getNamesSet().get(i);

    for (size_t i = 0; i < _pLhs->getNamesOrd().size(); ++i) {
        NamedValuePtr pField = _pLhs->getNamesOrd().get(i);
        NameMap::iterator j = fields.find(pField->getName());

        if (j != fields.end() && j->second.second)
            j->second.first = pField;
    }

    for (size_t i = 0; i < _pRhs->getNamesOrd().size(); ++i) {
        NamedValuePtr pField = _pRhs->getNamesOrd().get(i);
        NameMap::iterator j = fields.find(pField->getName());

        if (j != fields.end() && j->second.first)
            j->second.second = pField;
    }

    for (NameMap::iterator i = fields.begin(); i != fields.end(); ++i) {
        NamedValuePtr pFieldL = i->second.first;
        NamedValuePtr pFieldR = i->second.second;

        if (!pFieldL)
            return false;

        if (pFieldR) {
            _formulas.push_back(new tc::Formula(_kind, pFieldL->getType(), pFieldR->getType()));

            if (pStrict)
                pStrict->addPart().insert(new tc::Formula(tc::Formula::SUBTYPE_STRICT,
                        pFieldL->getType(), pFieldR->getType()));
        }
    }

    if (pStrict) {
        if (pStrict->size() == 1)
            _formulas.push_back(*pStrict->getPart(0).begin());
        else if (_bAllowCompound && pStrict->size() > 1)
            _formulas.push_back(pStrict);
    }

    return true;
}

bool Expand::_expandSet(int _kind, const SetTypePtr &_pLhs, const SetTypePtr &_pRhs,
        tc::FormulaList &_formulas)
{
    _formulas.push_back(new tc::Formula(_kind, _pLhs->getBaseType(), _pRhs->getBaseType()));
    return true;
}

bool Expand::_expandList(int _kind, const ListTypePtr &_pLhs, const ListTypePtr &_pRhs,
        tc::FormulaList &_formulas)
{
    _formulas.push_back(new tc::Formula(_kind, _pLhs->getBaseType(), _pRhs->getBaseType()));
    return true;
}

bool Expand::_expandMap(int _kind, const MapTypePtr &_pLhs, const MapTypePtr &_pRhs,
        tc::FormulaList & _formulas)
{
    _formulas.push_back(new tc::Formula(_kind, _pRhs->getIndexType(), _pLhs->getIndexType()));
    _formulas.push_back(new tc::Formula(_kind, _pLhs->getBaseType(), _pRhs->getBaseType()));
    return true;
}

bool Expand::_expandArray(int _kind, const ArrayTypePtr &_pLhs, const ArrayTypePtr &_pRhs, tc::FormulaList &_formulas) {
    _formulas.push_back(new tc::Formula(_kind, _pLhs->getBaseType(), _pRhs->getBaseType()));
    _formulas.push_back(new tc::Formula(_kind, _pRhs->getDimensionType(), _pLhs->getDimensionType()));
    return true;
}

bool Expand::_expandType(int _kind, const TypeTypePtr &_pLhs, const TypeTypePtr &_pRhs,
        tc::FormulaList & _formulas)
{
    if (_pLhs->getDeclaration() && _pLhs->getDeclaration()->getType() &&
            _pRhs->getDeclaration() && _pRhs->getDeclaration()->getType())
        _formulas.push_back(new tc::Formula(_kind, _pLhs->getDeclaration()->getType(),
                _pRhs->getDeclaration()->getType()));

    return true;
}

bool Expand::_expandSubtype(int _kind, const SubtypePtr &_pLhs, const SubtypePtr &_pRhs, tc::FormulaList &_formulas) {
    const tc::FreshTypePtr
        pMinType = new tc::FreshType(tc::FreshType::PARAM_IN);

    const NamedValuePtr
        pParam = new NamedValue(L"", pMinType);

    Cloner cloner;
    cloner.inject(pParam, _pLhs->getParam());
    cloner.inject(pParam, _pRhs->getParam());

    const ExpressionPtr
        pImplication = new Binary(Binary::IMPLIES,
            _pLhs->getExpression(), _pRhs->getExpression()),
        pCondition = na::generalize(cloner.get(pImplication));

    const ExpressionPtr
        pImplGen = na::generalize(pImplication),
        pE1 = cloner.get(pImplGen);

    _formulas.push_back(new tc::Formula(tc::Formula::SUBTYPE,
        pMinType, _pLhs->getParam()->getType(), pCondition));
    _formulas.push_back(new tc::Formula(tc::Formula::SUBTYPE,
        pMinType, _pRhs->getParam()->getType(), pCondition));

    return true;
}

bool Expand::_expandSubtypeWithType(int _kind, const TypePtr &_pLhs, const TypePtr &_pRhs, tc::FormulaList &_formulas) {
    assert(_pLhs->getKind() == Type::SUBTYPE
        || _pRhs->getKind() == Type::SUBTYPE);

    if (_pLhs->getKind() == Type::SUBTYPE) {
        _formulas.push_back(new tc::Formula(tc::Formula::SUBTYPE,
            _pLhs.as<Subtype>()->getParam()->getType(), _pRhs));
        return true;
    }

    const SubtypePtr& pSubtype = _pRhs.as<Subtype>();
    const TypePtr& pType = _pLhs;

    Cloner cloner;

    const NamedValuePtr
        pParam = new NamedValue(L"", pType);

    cloner.inject(pParam, pSubtype->getParam());

    const ExpressionPtr pCondition = cloner.get(pSubtype->getExpression());

    _formulas.push_back(new tc::Formula(tc::Formula::SUBTYPE,
        pType, pSubtype->getParam()->getType(), na::generalize(pCondition)));

    return true;

}

bool Expand::_expandUnionType(int _kind, const UnionTypePtr& _pLhs, const UnionTypePtr& _pRhs, tc::FormulaList &_formulas) {
    for (size_t i = 0; i < _pLhs->getConstructors().size(); ++i) {
        const UnionConstructorDeclaration &lCons = *_pLhs->getConstructors().get(i);
        const size_t cOtherConsIdx = _pRhs->getConstructors().findByNameIdx(lCons.getName());

        if (cOtherConsIdx == (size_t)-1)
            return false;

        const UnionConstructorDeclaration &rCons = *_pRhs->getConstructors().get(cOtherConsIdx);

        _formulas.push_back(new tc::Formula(_kind,
            lCons.getFields(), rCons.getFields()));
    }
    return true;
}

bool Expand::_run(int & _nResult) {
    tc::FormulaList formulas;
    bool bModified = false;
    tc::Formulas::iterator iCF = _context()->beginCompound();

    _nResult = tc::Formula::UNKNOWN;

    for (tc::Formulas::iterator i = _context()->begin(); i != iCF;) {
        tc::Formula &f = **i;
        TypePtr pLhs = f.getLhs(), pRhs = f.getRhs();
        bool bFormulaModified = false;

        if (f.is(tc::Formula::EQUALS | tc::Formula::SUBTYPE | tc::Formula::SUBTYPE_STRICT)) {
            bool bResult = true;

            bFormulaModified = true;

            if (pLhs->getKind() == Type::PREDICATE && pRhs->getKind() == Type::PREDICATE)
                bResult = _expandPredicate(f.getKind(), pLhs.as<PredicateType>(), pRhs.as<PredicateType>(), formulas);
            else if (pLhs->getKind() == Type::STRUCT && pRhs->getKind() == Type::STRUCT)
                bResult = _expandStruct(f.getKind(), pLhs.as<StructType>(), pRhs.as<StructType>(),
                        formulas, !_context().pParent);
            else if (pLhs->getKind() == Type::SET && pRhs->getKind() == Type::SET)
                bResult = _expandSet(f.getKind(), pLhs.as<SetType>(), pRhs.as<SetType>(), formulas);
            else if (pLhs->getKind() == Type::LIST && pRhs->getKind() == Type::LIST)
                bResult = _expandList(f.getKind(), pLhs.as<ListType>(), pRhs.as<ListType>(), formulas);
            else if (pLhs->getKind() == Type::MAP && pRhs->getKind() == Type::MAP)
                bResult = _expandMap(f.getKind(), pLhs.as<MapType>(), pRhs.as<MapType>(), formulas);
            else if (pLhs->getKind() == Type::ARRAY && pRhs->getKind() == Type::ARRAY)
                bResult = _expandArray(f.getKind(), pLhs.as<ArrayType>(), pRhs.as<ArrayType>(), formulas);
            else if (pLhs->getKind() == Type::TYPE && pRhs->getKind() == Type::TYPE)
                bResult = _expandType(f.getKind(), pLhs.as<TypeType>(), pRhs.as<TypeType>(), formulas);
            else if (f.getKind() == tc::Formula::SUBTYPE && pLhs->getKind() == Type::SUBTYPE && pRhs->getKind() == Type::SUBTYPE)
                bResult = _expandSubtype(f.getKind(), pLhs.as<Subtype>(), pRhs.as<Subtype>(), formulas);
            else if (f.getKind() == tc::Formula::SUBTYPE && (pLhs->getKind() == Type::SUBTYPE || pRhs->getKind() == Type::SUBTYPE))
                bResult = _expandSubtypeWithType(f.getKind(), pLhs, pRhs, formulas);
            else if (pLhs->getKind() == Type::UNION && pRhs->getKind() == Type::UNION)
                bResult = _expandUnionType(f.getKind(), pLhs.as<UnionType>(), pRhs.as<UnionType>(), formulas);
            else
                bFormulaModified = false;

            if (bFormulaModified) {
                bModified = true;
                _context()->erase(i++);
            } else
                ++i;

            if (!bResult) {
                _nResult = tc::Formula::FALSE;
                return true;
            }
        } else
            ++i;
    }

    if (bModified)
        _context()->insert(formulas.begin(), formulas.end());

    return _runCompound(_nResult) || bModified;
}

Auto<Operation> Operation::expand() {
    return new Expand();
}

}
