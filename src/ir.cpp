/// \file ir.cpp
///

#include "ir/expressions.h"
#include "ir/types.h"
#include "ir/declarations.h"
#include "ir/statements.h"
#include "typecheck.h"

namespace ir {

bool CUnionConstructor::isComplete() const {
    return m_decls.empty() && m_pDef && size() == m_pDef->getStruct().getFields().size();
}

CAnonymousPredicate::~CAnonymousPredicate() {
    _delete(m_pPreCond);
    _delete(m_pPostCond);
    _delete(m_pBlock);
    _delete(m_pType);
}

void CAnonymousPredicate::updateType() const {
    CPredicateType * pType = new CPredicateType();

    pType->getInParams().append(m_paramsIn, false);
    pType->getOutParams().append(m_paramsOut, false);
    pType->setPreCondition(m_pPreCond, false);
    pType->setPostCondition(m_pPostCond, false);

    const_cast<CAnonymousPredicate *>(this)->_assign(m_pType, pType, true);
}

/*const CNamedValue * CStructFieldExpr::getField() const {
    return getStructType()->getFields().get(m_cFieldIdx);
}*/

union_field_idx_t CUnionType::findField(const std::wstring & _strName) const {
    for (size_t i = 0; i < m_constructors.size(); ++ i) {
        size_t cIdx = m_constructors.get(i)->getStruct().getFields().findByNameIdx(_strName);
        if (cIdx != (size_t) -1)
            return union_field_idx_t(i, cIdx);
    }

    return union_field_idx_t((size_t) -1, (size_t) -1);
}

CUnionAlternativeExpr::CUnionAlternativeExpr(const CUnionType * _pType, const union_field_idx_t & _idx) :
    m_strName(_pType->getConstructors().get(_idx.first)->getStruct().getFields().get(_idx.second)->getName()), m_pType(_pType), m_idx(_idx)
{
}

const CUnionConstructorDefinition * CUnionAlternativeExpr::getConstructor() const {
    return getUnionType()->getConstructors().get(m_idx.first);
}

const CNamedValue * CUnionAlternativeExpr::getField() const {
    return getConstructor()->getStruct().getFields().get(m_idx.second);
}

CType * CFunctionCall::getType() const {
    CFunctionCall * pThis = const_cast<CFunctionCall *>(this);

    if (CExpression::getType())
        return CExpression::getType();

    if (! m_pPredicate || ! m_pPredicate->getType() || m_pPredicate->getType()->getKind() != CType::Predicate)
        return NULL;

    CPredicateType * pType = (CPredicateType *) m_pPredicate->getType();
    CBranches & branches = pType->getOutParams();

    if (branches.empty()) {
        pThis->setType(new CType(CType::Unit));
        return CExpression::getType ();
    }

    if (branches.size() > 1)
        return NULL;

    CBranch * pBranch = branches.get(0);

    if (pBranch->empty()) {
        pThis->setType(new CType(CType::Unit));
    } else if (pBranch->size() == 1) {
        pThis->setType(pBranch->get(0)->getType());
    } else {
        CStructType * pReturnType = new CStructType();

        pReturnType->getFields().append(* pBranch, false);
        pThis->setType(pReturnType);
    }

    return CExpression::getType ();
}

bool CType::hasFresh() const {
    return getKind() == Fresh;
}

CType * CType::clone() const {
    CType * pType = new CType(m_kind);

    pType->setBits(m_nBits);

    return pType;
}

static
int cmpBits(int _bitsL, int _bitsR) {
    if (_bitsL == _bitsR)
        return CType::OrdEquals;
    if (_bitsL == CNumber::Generic)
        return CType::OrdSuper;
    if (_bitsR == CNumber::Generic)
        return CType::OrdSub;
    return _bitsR > _bitsL ? CType::OrdSub : CType::OrdSuper;
}

static
int cmpIntNat(int _bitsL, int _bitsR) {
    if (_bitsL != CNumber::Generic && (_bitsR == CNumber::Generic || _bitsL <= _bitsR))
        return CType::OrdNone;

    return CType::OrdSuper;
}

static
int cmpNatInt(int _bitsL, int _bitsR) {
    if (cmpIntNat(_bitsR, _bitsL) == CType::OrdSuper)
        return CType::OrdSub;

    return CType::OrdNone;
}

int CType::compare(const CType & _other) const {
    typedef std::pair<int, int> P;
    P kinds(getKind(), _other.getKind());

    if (this == & _other)
        return OrdEquals;

    if (getKind() == Fresh || _other.getKind() == Fresh)
        return OrdUnknown;

    /*if (hasFresh() || _other.hasFresh())
        return OrdUnknown;*/

    if (kinds == P(Nat, Nat))
        return cmpBits(getBits(), _other.getBits());
    if (kinds == P(Nat, Int))
        return cmpNatInt(getBits(), _other.getBits());
    if (kinds == P(Nat, Real))
        return OrdSub;

    if (kinds == P(Int, Nat))
        return cmpIntNat(getBits(), _other.getBits());
    if (kinds == P(Int, Int))
        return cmpBits(getBits(), _other.getBits());
    if (kinds == P(Int, Real))
        return OrdSub;

    if (kinds == P(Real, Nat))
        return OrdSuper;
    if (kinds == P(Real, Int))
        return OrdSuper;
    if (kinds == P(Real, Real))
        return cmpBits(getBits(), _other.getBits());

    if (kinds == P(Bool, Bool))
        return OrdEquals;
    if (kinds == P(Char, Char))
        return OrdEquals;
    if (kinds == P(String, String))
        return OrdEquals;

    return OrdNone;
}

bool CType::compare(const CType & _other, int _order) const {
    return (compare(_other) & _order) != 0;
}

bool CType::operator ==(const CType & _other) const {
    return !(*this < _other || _other < *this);
}

bool CType::operator !=(const CType & _other) const {
    return *this < _other || _other < *this;
}

bool CType::less(const CType & _other) const {
    assert(getKind() == _other.getKind());

    if (getKind() == Fresh)
        return (this < & _other);

    return CType::compare(_other) == OrdSub;
}

bool CType::operator <(const CType & _other) const {
    if (getKind() < _other.getKind())
        return true;

    if (getKind() > _other.getKind())
        return false;

    return less(_other);
}

static
int minBitsIntNat(int _bitsInt, int _bitsNat) {
    if (_bitsInt == CNumber::Generic)
        return _bitsNat;
    if (_bitsNat == CNumber::Generic || _bitsInt <= _bitsNat)
        return _bitsInt - 1;
    return _bitsNat;
}

static
int maxBitsIntNat(int _bitsInt, int _bitsNat) {
    if (_bitsInt == CNumber::Generic || _bitsNat == CNumber::Generic)
        return CNumber::Generic;
    if (_bitsInt <= _bitsNat)
        return _bitsNat + 1;
    return _bitsInt;
}

CType::Extremum CType::getJoin(ir::CType & _other) {
    if (getKind() == Fresh || _other.getKind() == Fresh)
        return Extremum(NULL, false);

    switch (compare(_other)) {
        case OrdSub:
            return Extremum(& _other, false);
        case OrdSuper:
        case OrdEquals:
            return Extremum(this, false);
    }

    typedef std::pair<int, int> P;
    P kinds(getKind(), _other.getKind());

    if (kinds == P(Nat, Int))
        return Extremum(new CType(Int, maxBitsIntNat(_other.getBits(), getBits())), false);

    if (kinds == P(Int, Nat))
        return Extremum(new CType(Int, maxBitsIntNat(getBits(), _other.getBits())), false);

    return Extremum(NULL, true);
}

CType::Extremum CType::getMeet(ir::CType & _other) {
    if (getKind() == Fresh || _other.getKind() == Fresh)
        return Extremum(NULL, false);

    switch (compare(_other)) {
        case OrdSub:
        case OrdEquals:
            return Extremum(this, false);
        case OrdSuper:
            return Extremum(& _other, false);
    }

    typedef std::pair<int, int> P;
    P kinds(getKind(), _other.getKind());

    if (kinds == P(Nat, Int))
        return Extremum(new CType(Nat, minBitsIntNat(_other.getBits(), getBits())), false);

    if (kinds == P(Int, Nat))
        return Extremum(new CType(Nat, minBitsIntNat(getBits(), _other.getBits())), false);

    return Extremum(NULL, true);
}

bool CPredicateType::hasFresh() const {
    for (size_t i = 0; i < m_paramsIn.size(); ++ i)
        if (m_paramsIn.get(i)->getType()->hasFresh())
            return true;

    for (size_t j = 0; j < m_paramsOut.size(); ++ j) {
        CBranch & branch = * m_paramsOut.get(j);
        for (size_t i = 0; i < branch.size(); ++ i)
            if (branch.get(i)->getType()->hasFresh())
                return true;
    }

    return false;
}

bool CPredicateType::rewrite(ir::CType * _pOld, ir::CType * _pNew) {
    bool bResult = false;

    for (size_t i = 0; i < m_paramsIn.size(); ++ i) {
        CType * p = m_paramsIn.get(i)->getType();
        if (tc::rewriteType(p, _pOld, _pNew)) {
            bResult = true;
            m_paramsIn.get(i)->setType(p, false);
        }
    }

    for (size_t j = 0; j < m_paramsOut.size(); ++ j) {
        CBranch & branch = * m_paramsOut.get(j);
        for (size_t i = 0; i < branch.size(); ++ i) {
            CType * p = branch.get(i)->getType();
            if (tc::rewriteType(p, _pOld, _pNew)) {
                bResult = true;
                branch.get(i)->setType(p, false);
            }
        }
    }

    return bResult;
}

// Unions.

int CUnionType::compare(const CType & _other) const {
    if (_other.getKind() == Fresh)
        return OrdUnknown;

    if (_other.getKind() != Union)
        return OrdNone;

    const CUnionType & other = (const CUnionType &) _other;
    size_t cUnmatched = 0, cOtherUnmatched = other.getConstructors().size();

    for (size_t i = 0; i < m_constructors.size(); ++ i) {
        const CUnionConstructorDefinition & cons = * m_constructors.get(i);
        const size_t cOtherConsIdx = other.getConstructors().findByNameIdx(cons.getName());

        if (cOtherConsIdx != (size_t) -1)
            -- cOtherUnmatched;
        else
            ++ cUnmatched;
    }

    if (cUnmatched == 0)
        return cOtherUnmatched > 0 ? OrdSub : OrdEquals;

    return cOtherUnmatched == 0 ? OrdSuper : OrdNone;
}

// Structs.

bool CStructType::hasFresh() const {
    for (size_t i = 0; i < m_fields.size(); ++ i)
        if (m_fields.get(i)->getType()->hasFresh())
            return true;

    return false;
}

bool CStructType::rewrite(ir::CType * _pOld, ir::CType * _pNew) {
    bool bResult = false;

    for (size_t i = 0; i < m_fields.size(); ++ i) {
        CType * p = m_fields.get(i)->getType();
        if (tc::rewriteType(p, _pOld, _pNew)) {
            bResult = true;
            m_fields.get(i)->setType(p, false);
        }
    }

    return bResult;
}

bool CStructType::allFieldsNamed() const {
    for (size_t i = 0; i < m_fields.size(); ++ i)
        if (m_fields.get(i)->getName().empty())
            return false;

    return true;
}

bool CStructType::allFieldsUnnamed() const {
    for (size_t i = 0; i < m_fields.size(); ++ i)
        if (! m_fields.get(i)->getName().empty())
            return false;

    return true;
}

void CStructType::_fillNames() const {
    for (size_t i = 0; i < getFields().size(); ++ i) {
        const CNamedValue & field = * getFields().get(i);
        m_mapNames[field.getName()] = i;
    }
}

bool CStructType::less(const CType & _other) const {
    assert(_other.getKind() == Struct);

    const CStructType & other = (const CStructType &) _other;

    if (getFields().size() != other.getFields().size())
        return getFields().size() < other.getFields().size();

    const bool bAllFieldsNamed = allFieldsNamed();
    const bool bOtherAllFieldsNamed = other.allFieldsNamed();

    if (bAllFieldsNamed != bOtherAllFieldsNamed)
        return bOtherAllFieldsNamed;

    if (bAllFieldsNamed && m_mapNames.empty())
        _fillNames();

    if (bOtherAllFieldsNamed && other.m_mapNames.empty())
        other._fillNames();

    // We really want any two structs with the same fields to represent the same types (ignoring the field order).
    if (bAllFieldsNamed) {
        typedef std::map<std::wstring, size_t>::iterator I;
        I i = m_mapNames.begin();
        I j = other.m_mapNames.begin();

        for (; i != m_mapNames.end() && j != m_mapNames.end(); ++i, ++j) {
            const CNamedValue & field = * getFields().get(i->second);
            const CNamedValue & fieldOther = * other.getFields().get(j->second);

            if (field.getName() != fieldOther.getName())
                return field.getName() < fieldOther.getName();

            if (field.getType() != fieldOther.getType())
                return *field.getType() < *fieldOther.getType();
        }
    } else {
        for (size_t i = 0; i < getFields().size(); ++ i) {
            const CNamedValue & field = * getFields().get(i);
            const CNamedValue & fieldOther = * other.getFields().get(i);

            if (field.getName() != fieldOther.getName())
                return field.getName() < fieldOther.getName();

            if (field.getType() != fieldOther.getType())
                return *field.getType() < *fieldOther.getType();
        }
    }

    return false;
}

int CStructType::compare(const CType & _other) const {
    if (_other.getKind() == Fresh)
        return OrdUnknown;

    if (_other.getKind() != Struct)
        return OrdNone;

    const CStructType & other = (const CStructType &) _other;

    if (allFieldsNamed() != other.allFieldsNamed())
        return OrdNone;

    size_t cUnmatched = 0, cOtherUnmatched = other.getFields().size();
    size_t cSub = 0, cSuper = 0, cUnknown = 0;

    for (size_t i = 0; i < m_fields.size(); ++ i) {
        const CNamedValue & field = * m_fields.get(i);
        size_t cOtherIdx;

        if (field.getName().empty())
            cOtherIdx = i < m_fields.size() ? i : (size_t) -1;
        else
            cOtherIdx = other.getFields().findByNameIdx(field.getName());

        if (cOtherIdx != (size_t) -1) {
            const CNamedValue & otherField = * other.getFields().get(cOtherIdx);
            const int cmp = field.getType()->compare(* otherField.getType());

            if (cmp == OrdSub)
                ++ cSub;
            else if (cmp == OrdSuper)
                ++ cSuper;
            else if (cmp == OrdUnknown)
                ++ cUnknown;
            else if (cmp == OrdNone)
                return OrdNone;

            -- cOtherUnmatched;
        } else
            ++ cUnmatched;
    }

    if (cUnmatched > 0 && cOtherUnmatched > 0)
        return OrdNone;

    if (cSub > 0 && cSuper > 0)
        return OrdNone;

    if (cUnknown > 0)
        return OrdUnknown;

    if (cUnmatched == 0 && cOtherUnmatched == 0) {
        if (cSub > 0)
            return OrdSub;
        if (cSuper > 0)
            return OrdSuper;
        return OrdEquals;
    }

    if (cUnmatched > 0)
        return cSuper > 0 ? OrdNone : OrdSub;

    // cOtherUnmatched > 0
    return cSub > 0 ? OrdNone : OrdSuper;
}

CType::Extremum CStructType::getMeet(ir::CType & _other) {
    Extremum meet = CType::getMeet(_other);

    if (!meet.second)
        return meet;

    if (_other.getKind() != Struct)
        return meet;

    const CStructType & other = (const CStructType &) _other;

    if (allFieldsNamed() != other.allFieldsNamed())
        return Extremum(NULL, false);

    CStructType * pStruct = new CStructType();
    size_t cOtherUnmatched = other.getFields().size();

    for (size_t i = 0; i < m_fields.size(); ++ i) {
        const CNamedValue & field = * m_fields.get(i);
        size_t cOtherIdx;

        if (field.getName().empty())
            cOtherIdx = i < m_fields.size() ? i : (size_t) -1;
        else
            cOtherIdx = other.getFields().findByNameIdx(field.getName());

        if (cOtherIdx != (size_t) -1) {
            const CNamedValue & otherField = * other.getFields().get(cOtherIdx);
            Extremum meetField = field.getType()->getMeet(* otherField.getType());

            if (meetField.first == NULL)
                return meetField;

            pStruct->getFields().add(new CNamedValue(field.getName(), meetField.first));
            -- cOtherUnmatched;
        } else
            pStruct->getFields().add(new CNamedValue(field));
    }

    for (size_t i = 0; cOtherUnmatched > 0 && i < other.getFields().size(); ++ i, -- cOtherUnmatched) {
        const CNamedValue & field = * other.getFields().get(i);
        const size_t cIdx = m_fields.findByNameIdx(field.getName());

        if (cIdx != (size_t) -1)
            continue;

        -- cOtherUnmatched;
        pStruct->getFields().add(new CNamedValue(field));
    }

    return Extremum(pStruct, false);
}

CType::Extremum CStructType::getJoin(ir::CType & _other) {
    Extremum join = CType::getJoin(_other);

    if (!join.second)
        return join;

    if (_other.getKind() != Struct)
        return join;

    const CStructType & other = (const CStructType &) _other;

    if (allFieldsNamed() != other.allFieldsNamed())
        return Extremum(NULL, false);

    CStructType * pStruct = new CStructType();

    for (size_t i = 0; i < m_fields.size(); ++ i) {
        const CNamedValue & field = * m_fields.get(i);
        size_t cOtherIdx;

        if (field.getName().empty()) {
            if (i >= m_fields.size())
                break;
            cOtherIdx = i;
        } else {
            cOtherIdx = other.getFields().findByNameIdx(field.getName());
            if (cOtherIdx == (size_t) -1)
                continue;
        }

        const CNamedValue & otherField = * other.getFields().get(cOtherIdx);
        Extremum joinField = field.getType()->getJoin(* otherField.getType());

        if (joinField.first == NULL)
            return joinField;

        pStruct->getFields().add(new CNamedValue(field.getName(), joinField.first));
    }

    if (pStruct->getFields().empty()) {
        delete pStruct;
        pStruct = NULL;
    }

    return Extremum(pStruct, false);
}

// Derived types.

bool CDerivedType::rewrite(CType * _pOld, CType * _pNew) {
    return tc::rewriteType(m_pBaseType, _pOld, _pNew);
}

int CDerivedType::compare(const CType & _other) const {
    if (_other.getKind() == Fresh)
        return OrdUnknown;

    if (_other.getKind() != getKind())
        return OrdNone;

    return getBaseType()->compare(*((const CDerivedType &)_other).getBaseType());
}

bool CDerivedType::less(const CType & _other) const {
    return getBaseType()->less(*((const CDerivedType &)_other).getBaseType());
}

// Sets.

CType::Extremum CSetType::getMeet(CType & _other) {
    Extremum meet = CType::getMeet(_other);

    if (!meet.second)
        return meet;

    if (_other.getKind() != Struct)
        return meet;

    meet = getBaseType()->getMeet(*((const CSetType &)_other).getBaseType());

    if (meet.first != NULL)
        meet.first = new CSetType(meet.first);

    return meet;
}

CType::Extremum CSetType::getJoin(CType & _other) {
    Extremum join = CType::getJoin(_other);

    if (!join.second)
        return join;

    if (_other.getKind() != Set)
        return join;

    join = getBaseType()->getJoin(*((const CSetType &)_other).getBaseType());

    if (join.first != NULL)
        join.first = new CSetType(join.first);

    return join;
}

};
