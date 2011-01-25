/// \file ir.cpp
///

#include "ir/expressions.h"
#include "ir/types.h"
#include "ir/declarations.h"
#include "ir/statements.h"
#include "typecheck.h"

namespace ir {

bool isTypeVariable(const NamedValue * _pVar, const Type * & _pType) {
    if (! _pVar || ! _pVar->getType())
        return false;

    return _pVar->getType()->getKind() == Type::TYPE;
}

const Type * resolveBaseType(const Type * _pType) {
    if (! _pType)
        return NULL;

    while (_pType) {
        if (_pType->getKind() == Type::NAMED_REFERENCE) {
            const NamedReferenceType * pRef = (NamedReferenceType *) _pType;

            if (pRef->getDeclaration() != NULL && pRef->getDeclaration()->getType() != NULL)
                _pType = pRef->getDeclaration()->getType();
            else
                break;
        } else if (_pType->getKind() == Type::PARAMETERIZED) {
            _pType = ((ParameterizedType *) _pType)->getActualType();
        } else
            break;
    }

    return _pType;
}

std::wstring NamedReferenceType::getName() const {
    return m_pDecl != NULL ? m_pDecl->getName() : L"";
}

std::wstring FormulaCall::getName() const {
    return m_pTarget != NULL ? m_pTarget->getName() : L"";
}

const std::wstring &PredicateReference::getName() const {
    return m_pTarget != NULL ? m_pTarget->getName() : m_strName;
}

bool UnionConstructor::isComplete() const {
    return m_decls.empty() && m_pProto && size() == m_pProto->getStruct().getFields().size();
}

AnonymousPredicate::~AnonymousPredicate() {
    _delete(m_pPreCond);
    _delete(m_pPostCond);
    _delete(m_pBlock);
    _delete(m_pType);
}

void AnonymousPredicate::updateType() const {
    PredicateType * pType = new PredicateType();

    pType->getInParams().append(m_paramsIn, false);
    pType->getOutParams().append(m_paramsOut, false);
    pType->setPreCondition(m_pPreCond, false);
    pType->setPostCondition(m_pPostCond, false);

    const_cast<AnonymousPredicate *>(this)->_assign(m_pType, pType, true);
}

std::wstring VariableDeclaration::getName() const {
    return m_pVar != NULL ? m_pVar->getName() : L"";
}

UnionFieldIdx UnionType::findField(const std::wstring & _strName) const {
    for (size_t i = 0; i < m_constructors.size(); ++ i) {
        size_t cIdx = m_constructors.get(i)->getStruct().getFields().findByNameIdx(_strName);
        if (cIdx != (size_t) -1)
            return UnionFieldIdx(i, cIdx);
    }

    return UnionFieldIdx((size_t) -1, (size_t) -1);
}

UnionAlternativeExpr::UnionAlternativeExpr(const UnionType * _pType, const UnionFieldIdx & _idx) :
    m_strName(_pType->getConstructors().get(_idx.first)->getStruct().getFields().get(_idx.second)->getName()), m_pType(_pType), m_idx(_idx)
{
}

const UnionConstructorDeclaration * UnionAlternativeExpr::getConstructor() const {
    return getUnionType()->getConstructors().get(m_idx.first);
}

const NamedValue * UnionAlternativeExpr::getField() const {
    return getConstructor()->getStruct().getFields().get(m_idx.second);
}

Type * FunctionCall::getType() const {
    FunctionCall * pThis = const_cast<FunctionCall *>(this);

    if (Expression::getType())
        return Expression::getType();

    if (! m_pPredicate || ! m_pPredicate->getType() || m_pPredicate->getType()->getKind() != Type::PREDICATE)
        return NULL;

    PredicateType * pType = (PredicateType *) m_pPredicate->getType();
    Branches & branches = pType->getOutParams();

    if (branches.empty()) {
        pThis->setType(new Type(Type::UNIT));
        return Expression::getType ();
    }

    if (branches.size() > 1)
        return NULL;

    Branch * pBranch = branches.get(0);

    if (pBranch->empty()) {
        pThis->setType(new Type(Type::UNIT));
    } else if (pBranch->size() == 1) {
        pThis->setType(pBranch->get(0)->getType());
    } else {
        StructType * pReturnType = new StructType();

        pReturnType->getFields().append(* pBranch, false);
        pThis->setType(pReturnType);
    }

    return Expression::getType ();
}

bool Type::hasFresh() const {
    return getKind() == FRESH;
}

Type * Type::clone() const {
    Type * pType = new Type(m_kind);

    pType->setBits(m_nBits);

    return pType;
}

static
int cmpBits(int _bitsL, int _bitsR) {
    if (_bitsL == _bitsR)
        return Type::ORD_EQUALS;
    if (_bitsL == Number::GENERIC)
        return Type::ORD_SUPER;
    if (_bitsR == Number::GENERIC)
        return Type::ORD_SUB;
    return _bitsR > _bitsL ? Type::ORD_SUB : Type::ORD_SUPER;
}

static
int cmpIntNat(int _bitsL, int _bitsR) {
    if (_bitsL != Number::GENERIC && (_bitsR == Number::GENERIC || _bitsL <= _bitsR))
        return Type::ORD_NONE;

    return Type::ORD_SUPER;
}

static
int cmpNatInt(int _bitsL, int _bitsR) {
    if (cmpIntNat(_bitsR, _bitsL) == Type::ORD_SUPER)
        return Type::ORD_SUB;

    return Type::ORD_NONE;
}

int Type::compare(const Type & _other) const {
    typedef std::pair<int, int> P;
    P kinds(getKind(), _other.getKind());

    if (this == & _other)
        return ORD_EQUALS;

    if (getKind() == FRESH || _other.getKind() == FRESH) {
        if (contains(&_other) || _other.contains(this))
            return ORD_NONE;
        return ORD_UNKNOWN;
    }

    /*if (hasFresh() || _other.hasFresh())
        return OrdUnknown;*/

    if (kinds == P(NAT, NAT))
        return cmpBits(getBits(), _other.getBits());
    if (kinds == P(NAT, INT))
        return cmpNatInt(getBits(), _other.getBits());
    if (kinds == P(NAT, REAL))
        return ORD_SUB;

    if (kinds == P(INT, NAT))
        return cmpIntNat(getBits(), _other.getBits());
    if (kinds == P(INT, INT))
        return cmpBits(getBits(), _other.getBits());
    if (kinds == P(INT, REAL))
        return ORD_SUB;

    if (kinds == P(REAL, NAT))
        return ORD_SUPER;
    if (kinds == P(REAL, INT))
        return ORD_SUPER;
    if (kinds == P(REAL, REAL))
        return cmpBits(getBits(), _other.getBits());

    if (kinds == P(BOOL, BOOL))
        return ORD_EQUALS;
    if (kinds == P(CHAR, CHAR))
        return ORD_EQUALS;
    if (kinds == P(STRING, STRING))
        return ORD_EQUALS;

    return ORD_NONE;
}

bool Type::compare(const Type & _other, int _order) const {
    return (compare(_other) & _order) != 0;
}

bool Type::operator ==(const Type & _other) const {
    return !(*this < _other || _other < *this);
}

bool Type::operator !=(const Type & _other) const {
    return *this < _other || _other < *this;
}

bool Type::less(const Type & _other) const {
    assert(getKind() == _other.getKind());

    if (getKind() == FRESH)
        return (this < & _other);

    const int nOrder = Type::compare(_other);

    // Should be comparable.
    assert(nOrder == ORD_SUB || nOrder == ORD_SUPER || nOrder == ORD_EQUALS);

    return nOrder == ORD_SUB;
}

bool Type::operator <(const Type & _other) const {
    if (getKind() < _other.getKind())
        return true;

    if (getKind() > _other.getKind())
        return false;

    return less(_other);
}

static
int minBitsIntNat(int _bitsInt, int _bitsNat) {
    if (_bitsInt == Number::GENERIC)
        return _bitsNat;
    if (_bitsNat == Number::GENERIC || _bitsInt <= _bitsNat)
        return _bitsInt - 1;
    return _bitsNat;
}

static
int maxBitsIntNat(int _bitsInt, int _bitsNat) {
    if (_bitsInt == Number::GENERIC || _bitsNat == Number::GENERIC)
        return Number::GENERIC;
    if (_bitsInt <= _bitsNat)
        return _bitsNat + 1;
    return _bitsInt;
}

Type::Extremum Type::getJoin(ir::Type & _other) {
    if (getKind() == FRESH || _other.getKind() == FRESH) {
        ir::Type &fresh = getKind() == FRESH ? *this : _other;
        ir::Type &other = getKind() == FRESH ? _other : *this;

        if (other.contains(&fresh))
            return Extremum(NULL, true);

        return Extremum(NULL, false);
    }

    switch (compare(_other)) {
        case ORD_SUB:
            return Extremum(& _other, false);
        case ORD_SUPER:
        case ORD_EQUALS:
            return Extremum(this, false);
    }

    typedef std::pair<int, int> P;
    P kinds(getKind(), _other.getKind());

    if (kinds == P(NAT, INT))
        return Extremum(new Type(INT, maxBitsIntNat(_other.getBits(), getBits())), false);

    if (kinds == P(INT, NAT))
        return Extremum(new Type(INT, maxBitsIntNat(getBits(), _other.getBits())), false);

    return Extremum(NULL, true);
}

Type::Extremum Type::getMeet(ir::Type & _other) {
    if (getKind() == FRESH || _other.getKind() == FRESH) {
        ir::Type &fresh = getKind() == FRESH ? *this : _other;
        ir::Type &other = getKind() == FRESH ? _other : *this;

        if (other.contains(&fresh))
            return Extremum(NULL, true);

        return Extremum(NULL, false);
    }

    switch (compare(_other)) {
        case ORD_SUB:
        case ORD_EQUALS:
            return Extremum(this, false);
        case ORD_SUPER:
            return Extremum(& _other, false);
    }

    typedef std::pair<int, int> P;
    P kinds(getKind(), _other.getKind());

    if (kinds == P(NAT, INT))
        return Extremum(new Type(NAT, minBitsIntNat(_other.getBits(), getBits())), false);

    if (kinds == P(INT, NAT))
        return Extremum(new Type(NAT, minBitsIntNat(getBits(), _other.getBits())), false);

    return Extremum(NULL, true);
}

bool PredicateType::hasFresh() const {
    for (size_t i = 0; i < m_paramsIn.size(); ++ i)
        if (m_paramsIn.get(i)->getType()->hasFresh())
            return true;

    for (size_t j = 0; j < m_paramsOut.size(); ++ j) {
        Branch & branch = * m_paramsOut.get(j);
        for (size_t i = 0; i < branch.size(); ++ i)
            if (branch.get(i)->getType()->hasFresh())
                return true;
    }

    return false;
}

bool PredicateType::rewrite(ir::Type * _pOld, ir::Type * _pNew) {
    bool bResult = false;

    for (size_t i = 0; i < m_paramsIn.size(); ++ i) {
        Type * p = m_paramsIn.get(i)->getType();
        if (tc::rewriteType(p, _pOld, _pNew)) {
            bResult = true;
            m_paramsIn.get(i)->setType(p, false);
        }
    }

    for (size_t j = 0; j < m_paramsOut.size(); ++ j) {
        Branch & branch = * m_paramsOut.get(j);
        for (size_t i = 0; i < branch.size(); ++ i) {
            Type * p = branch.get(i)->getType();
            if (tc::rewriteType(p, _pOld, _pNew)) {
                bResult = true;
                branch.get(i)->setType(p, false);
            }
        }
    }

    return bResult;
}

int PredicateType::compare(const Type &_other) const {
    if (_other.getKind() == FRESH)
        return ORD_UNKNOWN;

    if (_other.getKind() != getKind())
        return ORD_NONE;

    const PredicateType &other = (const PredicateType &)_other;

    if (getInParams().size() != other.getInParams().size())
        return ORD_NONE;

    if (getOutParams().size() != other.getOutParams().size())
        return ORD_NONE;

    bool bSub = false, bSuper = false;

    for (size_t i = 0; i < getInParams().size(); ++i) {
        const Param &p = *getInParams().get(i);
        const Param &q = *other.getInParams().get(i);

        switch (p.getType()->compare(*q.getType())) {
            case ORD_UNKNOWN:
                return ORD_UNKNOWN;
            case ORD_NONE:
                return ORD_NONE;
            case ORD_SUPER:
                if (bSuper)
                    return ORD_NONE;
                bSub = true;
                break;
            case ORD_SUB:
                if (bSub)
                    return ORD_NONE;
                bSuper = true;
                break;
        }
    }

    for (size_t j = 0; j < getOutParams().size(); ++j) {
        const Branch &b = *getOutParams().get(j);
        const Branch &c = *other.getOutParams().get(j);

        if (b.size() != c.size())
            return ORD_NONE;

        for (size_t i = 0; i < b.size(); ++ i) {
            const Param &p = *b.get(i);
            const Param &q = *c.get(i);

            // Sub/Super is inverted for output parameters.
            switch (p.getType()->compare(*q.getType())) {
                case ORD_UNKNOWN:
                    return ORD_UNKNOWN;
                case ORD_NONE:
                    return ORD_NONE;
                case ORD_SUB:
                    if (bSuper)
                        return ORD_NONE;
                    bSub = true;
                    break;
                case ORD_SUPER:
                    if (bSub)
                        return ORD_NONE;
                    bSuper = true;
                    break;
            }
        }
    }

    return bSub ? ORD_SUB : (bSuper ? ORD_SUPER : ORD_EQUALS);
}

// Unions.

int UnionType::compare(const Type & _other) const {
    if (_other.getKind() == FRESH)
        return ORD_UNKNOWN;

    if (_other.getKind() != UNION)
        return ORD_NONE;

    const UnionType & other = (const UnionType &) _other;
    size_t cUnmatched = 0, cOtherUnmatched = other.getConstructors().size();

    for (size_t i = 0; i < m_constructors.size(); ++ i) {
        const UnionConstructorDeclaration & cons = * m_constructors.get(i);
        const size_t cOtherConsIdx = other.getConstructors().findByNameIdx(cons.getName());

        if (cOtherConsIdx != (size_t) -1)
            -- cOtherUnmatched;
        else
            ++ cUnmatched;
    }

    // TODO: actually compare alternatives.

    if (cUnmatched == 0)
        return cOtherUnmatched > 0 ? ORD_SUB : ORD_EQUALS;

    return cOtherUnmatched == 0 ? ORD_SUPER : ORD_NONE;
}

// Structs.

bool StructType::hasFresh() const {
    for (size_t i = 0; i < m_fields.size(); ++ i)
        if (m_fields.get(i)->getType()->hasFresh())
            return true;

    return false;
}

bool StructType::rewrite(ir::Type * _pOld, ir::Type * _pNew) {
    bool bResult = false;

    for (size_t i = 0; i < m_fields.size(); ++ i) {
        Type * p = m_fields.get(i)->getType();
        if (tc::rewriteType(p, _pOld, _pNew)) {
            bResult = true;
            m_fields.get(i)->setType(p, false);
        }
    }

    return bResult;
}

bool StructType::allFieldsNamed() const {
    for (size_t i = 0; i < m_fields.size(); ++ i)
        if (m_fields.get(i)->getName().empty())
            return false;

    return true;
}

bool StructType::allFieldsUnnamed() const {
    for (size_t i = 0; i < m_fields.size(); ++ i)
        if (! m_fields.get(i)->getName().empty())
            return false;

    return true;
}

void StructType::_fillNames() const {
    for (size_t i = 0; i < getFields().size(); ++ i) {
        const NamedValue & field = * getFields().get(i);
        m_mapNames[field.getName()] = i;
    }
}

bool StructType::less(const Type & _other) const {
    assert(_other.getKind() == STRUCT);

    const StructType & other = (const StructType &) _other;

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
            const NamedValue & field = * getFields().get(i->second);
            const NamedValue & fieldOther = * other.getFields().get(j->second);

            if (field.getName() != fieldOther.getName())
                return field.getName() < fieldOther.getName();

            if (field.getType() != fieldOther.getType())
                return *field.getType() < *fieldOther.getType();
        }
    } else {
        for (size_t i = 0; i < getFields().size(); ++ i) {
            const NamedValue & field = * getFields().get(i);
            const NamedValue & fieldOther = * other.getFields().get(i);

            if (field.getName() != fieldOther.getName())
                return field.getName() < fieldOther.getName();

            if (field.getType() != fieldOther.getType())
                return *field.getType() < *fieldOther.getType();
        }
    }

    return false;
}

int StructType::compare(const Type & _other) const {
    if (_other.getKind() == FRESH)
        return ORD_UNKNOWN;

    if (_other.getKind() != STRUCT)
        return ORD_NONE;

    const StructType & other = (const StructType &) _other;

    if (allFieldsNamed() != other.allFieldsNamed())
        return ORD_NONE;

    size_t cUnmatched = 0, cOtherUnmatched = other.getFields().size();
    size_t cSub = 0, cSuper = 0, cUnknown = 0;

    for (size_t i = 0; i < m_fields.size(); ++ i) {
        const NamedValue & field = * m_fields.get(i);
        size_t cOtherIdx;

        if (field.getName().empty())
            cOtherIdx = i < m_fields.size() ? i : (size_t) -1;
        else
            cOtherIdx = other.getFields().findByNameIdx(field.getName());

        if (cOtherIdx != (size_t) -1) {
            const NamedValue & otherField = * other.getFields().get(cOtherIdx);
            const int cmp = field.getType()->compare(* otherField.getType());

            if (cmp == ORD_SUB)
                ++ cSub;
            else if (cmp == ORD_SUPER)
                ++ cSuper;
            else if (cmp == ORD_UNKNOWN)
                ++ cUnknown;
            else if (cmp == ORD_NONE)
                return ORD_NONE;

            -- cOtherUnmatched;
        } else
            ++ cUnmatched;
    }

    if (cUnmatched > 0 && cOtherUnmatched > 0)
        return ORD_NONE;

    if (cSub > 0 && cSuper > 0)
        return ORD_NONE;

    if (cUnknown > 0)
        return ORD_UNKNOWN;

    if (cUnmatched == 0 && cOtherUnmatched == 0) {
        if (cSub > 0)
            return ORD_SUB;
        if (cSuper > 0)
            return ORD_SUPER;
        return ORD_EQUALS;
    }

    if (cUnmatched > 0)
        return cSuper > 0 ? ORD_NONE : ORD_SUB;

    // cOtherUnmatched > 0
    return cSub > 0 ? ORD_NONE : ORD_SUPER;
}

Type::Extremum StructType::getMeet(ir::Type & _other) {
    Extremum meet = Type::getMeet(_other);

    if (!meet.second)
        return meet;

    if (_other.getKind() != STRUCT)
        return meet;

    const StructType & other = (const StructType &) _other;

    if (allFieldsNamed() != other.allFieldsNamed())
        return Extremum(NULL, false);

    StructType * pStruct = new StructType();
    size_t cOtherUnmatched = other.getFields().size();

    for (size_t i = 0; i < m_fields.size(); ++ i) {
        const NamedValue & field = * m_fields.get(i);
        size_t cOtherIdx;

        if (field.getName().empty())
            cOtherIdx = i < m_fields.size() ? i : (size_t) -1;
        else
            cOtherIdx = other.getFields().findByNameIdx(field.getName());

        if (cOtherIdx != (size_t) -1) {
            const NamedValue & otherField = * other.getFields().get(cOtherIdx);
            Extremum meetField = field.getType()->getMeet(* otherField.getType());

            if (meetField.first == NULL)
                return meetField;

            pStruct->getFields().add(new NamedValue(field.getName(), meetField.first));
            -- cOtherUnmatched;
        } else
            pStruct->getFields().add(new NamedValue(field));
    }

    for (size_t i = 0; cOtherUnmatched > 0 && i < other.getFields().size(); ++ i, -- cOtherUnmatched) {
        const NamedValue & field = * other.getFields().get(i);
        const size_t cIdx = m_fields.findByNameIdx(field.getName());

        if (cIdx != (size_t) -1)
            continue;

        -- cOtherUnmatched;
        pStruct->getFields().add(new NamedValue(field));
    }

    return Extremum(pStruct, false);
}

Type::Extremum StructType::getJoin(ir::Type & _other) {
    Extremum join = Type::getJoin(_other);

    if (!join.second)
        return join;

    if (_other.getKind() != STRUCT)
        return join;

    const StructType & other = (const StructType &) _other;

    if (allFieldsNamed() != other.allFieldsNamed())
        return Extremum(NULL, false);

    StructType * pStruct = new StructType();

    for (size_t i = 0; i < m_fields.size(); ++ i) {
        const NamedValue & field = * m_fields.get(i);
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

        const NamedValue & otherField = * other.getFields().get(cOtherIdx);
        Extremum joinField = field.getType()->getJoin(* otherField.getType());

        if (joinField.first == NULL)
            return joinField;

        pStruct->getFields().add(new NamedValue(field.getName(), joinField.first));
    }

    if (pStruct->getFields().empty()) {
        delete pStruct;
        pStruct = NULL;
    }

    return Extremum(pStruct, false);
}

// Derived types.

bool DerivedType::rewrite(Type * _pOld, Type * _pNew) {
    return tc::rewriteType(m_pBaseType, _pOld, _pNew);
}

int DerivedType::compare(const Type & _other) const {
    if (_other.getKind() == FRESH)
        return ORD_UNKNOWN;

    if (_other.getKind() != getKind())
        return ORD_NONE;

    return getBaseType()->compare(*((const DerivedType &)_other).getBaseType());
}

bool DerivedType::less(const Type & _other) const {
    return *getBaseType() < *((const DerivedType &)_other).getBaseType();
}

// Sets.

Type::Extremum SetType::getMeet(Type & _other) {
    Extremum meet = Type::getMeet(_other);

    if (!meet.second)
        return meet;

    if (_other.getKind() != SET)
        return meet;

    meet = getBaseType()->getMeet(*((const SetType &)_other).getBaseType());

    if (meet.first != NULL)
        meet.first = new SetType(meet.first);

    return meet;
}

Type::Extremum SetType::getJoin(Type & _other) {
    Extremum join = Type::getJoin(_other);

    if (!join.second)
        return join;

    if (_other.getKind() != SET)
        return join;

    join = getBaseType()->getJoin(*((const SetType &)_other).getBaseType());

    if (join.first != NULL)
        join.first = new SetType(join.first);

    return join;
}

// Lists.

Type::Extremum ListType::getMeet(Type & _other) {
    Extremum meet = Type::getMeet(_other);

    if (!meet.second)
        return meet;

    if (_other.getKind() != LIST)
        return meet;

    meet = getBaseType()->getMeet(*((const ListType &)_other).getBaseType());

    if (meet.first != NULL)
        meet.first = new ListType(meet.first);

    return meet;
}

Type::Extremum ListType::getJoin(Type & _other) {
    Extremum join = Type::getJoin(_other);

    if (!join.second)
        return join;

    if (_other.getKind() != LIST)
        return join;

    join = getBaseType()->getJoin(*((const ListType &)_other).getBaseType());

    if (join.first != NULL)
        join.first = new ListType(join.first);

    return join;
}

// Maps.

bool MapType::rewrite(Type *_pOld, Type *_pNew) {
    const bool b = DerivedType::rewrite(_pOld, _pNew);
    return tc::rewriteType(m_pIndexType, _pOld, _pNew) || b;
}

int MapType::compare(const Type &_other) const {
    if (_other.getKind() == FRESH)
        return ORD_UNKNOWN;

    if (_other.getKind() != getKind())
        return ORD_NONE;

    switch (m_pIndexType->compare(*((const MapType &)_other).getIndexType())) {
        case ORD_UNKNOWN:
            return ORD_UNKNOWN;
        case ORD_NONE:
            return ORD_NONE;
        case ORD_EQUALS:
            return getBaseType()->compare(*((const MapType &)_other).getBaseType());
        case ORD_SUB:
            switch (getBaseType()->compare(*((const MapType &)_other).getBaseType())) {
                case ORD_UNKNOWN:
                    return ORD_UNKNOWN;
                case ORD_NONE:
                case ORD_SUB:
                    return ORD_NONE;
                case ORD_EQUALS:
                case ORD_SUPER:
                    return ORD_SUPER;
            }
        case ORD_SUPER:
            switch (getBaseType()->compare(*((const MapType &)_other).getBaseType())) {
                case ORD_UNKNOWN:
                    return ORD_UNKNOWN;
                case ORD_NONE:
                case ORD_SUPER:
                    return ORD_NONE;
                case ORD_EQUALS:
                case ORD_SUB:
                    return ORD_SUB;
            }
    }

    return ORD_NONE;
}

bool MapType::less(const Type & _other) const {
    const MapType &other = (const MapType &)_other;

    if (*getBaseType() < *other.getBaseType())
        return true;

    if (*other.getBaseType() < *getBaseType())
        return false;

    return *getIndexType() < *other.getIndexType();
}

Type::Extremum MapType::getMeet(Type & _other) {
    Extremum meet = Type::getMeet(_other);

    if (!meet.second)
        return meet;

    if (_other.getKind() != MAP)
        return meet;

    Extremum join = getBaseType()->getJoin(*((const MapType &)_other).getBaseType());

    meet = getIndexType()->getMeet(*((const MapType &)_other).getIndexType());

    if (meet.first == NULL || join.first == NULL) {
        meet.second = meet.second || join.second;
        return meet;
    }

    meet.first = new MapType(meet.first, join.first);

    return meet;
}

Type::Extremum MapType::getJoin(Type & _other) {
    Extremum join = Type::getJoin(_other);

    if (!join.second)
        return join;

    if (_other.getKind() != MAP)
        return join;

    Extremum meet = getBaseType()->getMeet(*((const MapType &)_other).getBaseType());

    join = getIndexType()->getJoin(*((const MapType &)_other).getIndexType());

    if (meet.first == NULL || join.first == NULL) {
        join.second = meet.second || join.second;
        return join;
    }

    join.first = new MapType(join.first, meet.first);

    return join;
}

// 'type' type.

TypeType::TypeType() : m_pDecl(NULL) {
    setDeclaration(new TypeDeclaration());
}

TypeType::~TypeType() {
    _delete(m_pDecl);
}

bool TypeType::rewrite(ir::Type * _pOld, ir::Type * _pNew) {
    if (m_pDecl == NULL || m_pDecl->getType() == NULL)
        return false;

    Type *p = m_pDecl->getType();

    if (tc::rewriteType(p, _pOld, _pNew)) {
        m_pDecl->setType(p, false);
        return true;
    }

    return false;
}

int TypeType::compare(const Type &_other) const {
    if (_other.getKind() == FRESH)
        return ORD_UNKNOWN;

    if (_other.getKind() != getKind())
        return ORD_NONE;

    const TypeType &other = (const TypeType &)_other;

    if (m_pDecl != NULL && m_pDecl->getType() != NULL) {
        if (other.m_pDecl != NULL && other.m_pDecl->getType() != NULL)
            return m_pDecl->getType()->compare(*other.m_pDecl->getType());

        return ORD_NONE;
    }

    return (other.m_pDecl != NULL && other.m_pDecl->getType() != NULL) ? ORD_NONE : ORD_EQUALS;
}

bool TypeType::less(const Type & _other) const {
    const TypeType &other = (const TypeType &)_other;

    if (m_pDecl != NULL && m_pDecl->getType() != NULL) {
        if (other.m_pDecl != NULL && other.m_pDecl->getType() != NULL)
            return *m_pDecl->getType() < *other.m_pDecl->getType();

        return false;
    }

    return other.m_pDecl != NULL && other.m_pDecl->getType() != NULL;
}

};
