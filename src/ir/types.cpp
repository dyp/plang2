/// \file types.cpp
///

#include "ir/base.h"
#include "ir/types.h"
#include "ir/declarations.h"
#include "typecheck.h"

using namespace ir;

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

    if (getKind() == BOTTOM || _other.getKind() == TOP)
        return getKind() == _other.getKind() ? ORD_EQUALS : ORD_SUB;
    if (getKind() == TOP || _other.getKind() == BOTTOM)
        return getKind() == _other.getKind() ? ORD_EQUALS : ORD_SUPER;

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
        return (((const tc::FreshType *)this)->getOrdinal() < ((const tc::FreshType &)_other).getOrdinal());

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

Type *Type::getJoin(ir::Type &_other) {
    if (getKind() == TOP || _other.getKind() == BOTTOM)
        return this;

    if (getKind() == BOTTOM || _other.getKind() == TOP)
        return &_other;

    if (getKind() == FRESH || _other.getKind() == FRESH) {
        ir::Type &fresh = getKind() == FRESH ? *this : _other;
        ir::Type &other = getKind() == FRESH ? _other : *this;

        if (other.contains(&fresh))
            return new Type(TOP);

        return (Type *)NULL;
    }

    switch (compare(_other)) {
        case ORD_SUB:
            return &_other;
        case ORD_SUPER:
        case ORD_EQUALS:
            return this;
    }

    typedef std::pair<int, int> P;
    P kinds(getKind(), _other.getKind());

    if (kinds == P(NAT, INT))
        return new Type(INT, maxBitsIntNat(_other.getBits(), getBits()));

    if (kinds == P(INT, NAT))
        return new Type(INT, maxBitsIntNat(getBits(), _other.getBits()));

    if (getKind() != _other.getKind())
        return new Type(TOP);

    return (Type *)NULL;
}

Type *Type::getMeet(ir::Type &_other) {
    if (getKind() == TOP || _other.getKind() == BOTTOM)
        return &_other;

    if (getKind() == BOTTOM || _other.getKind() == TOP)
        return this;

    if (getKind() == FRESH || _other.getKind() == FRESH) {
        ir::Type &fresh = getKind() == FRESH ? *this : _other;
        ir::Type &other = getKind() == FRESH ? _other : *this;

        if (other.contains(&fresh))
            return new Type(BOTTOM);

        return (Type *)NULL;
    }

    switch (compare(_other)) {
        case ORD_SUB:
        case ORD_EQUALS:
            return this;
        case ORD_SUPER:
            return &_other;
    }

    typedef std::pair<int, int> P;
    P kinds(getKind(), _other.getKind());

    if (kinds == P(NAT, INT))
        return new Type(NAT, minBitsIntNat(_other.getBits(), getBits()));

    if (kinds == P(INT, NAT))
        return new Type(NAT, minBitsIntNat(getBits(), _other.getBits()));

    if (getKind() != _other.getKind())
        return new Type(BOTTOM);

    return (Type *)NULL;
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

    if (_other.getKind() == TOP)
        return ORD_SUB;

    if (_other.getKind() == BOTTOM)
        return ORD_SUPER;

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
