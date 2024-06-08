/// \file types.cpp
///

#include <list>
#include "ir/base.h"
#include "ir/types.h"
#include "ir/declarations.h"
#include "ir/expressions.h"
#include "typecheck.h"

using namespace ir;

namespace {

struct Rule {
    int nCurrent, nValue, nResult;
};

}

Type::Order &Type::Order::in(int _nOrder) {
    static const Rule rules[] = {
            { ORD_ANY,      ORD_UNKNOWN,    ORD_UNKNOWN },
            { ORD_UNKNOWN,  ORD_ANY,        ORD_UNKNOWN },
            { ORD_ANY,      ORD_NONE,       ORD_NONE },
            { ORD_NONE,     ORD_ANY,        ORD_NONE },
            { ORD_SUB,      ORD_SUB,        ORD_NONE },
            { ORD_SUB,      ORD_SUPER,      ORD_SUB },
            { ORD_SUB,      ORD_EQUALS,     ORD_SUB },
            { ORD_SUPER,    ORD_SUB,        ORD_SUPER },
            { ORD_SUPER,    ORD_SUPER,      ORD_NONE },
            { ORD_SUPER,    ORD_EQUALS,     ORD_SUPER },
            { ORD_EQUALS,   ORD_SUB,        ORD_SUPER },
            { ORD_EQUALS,   ORD_SUPER,      ORD_SUB },
            { ORD_EQUALS,   ORD_EQUALS,     ORD_EQUALS },
    };

    if (m_nOrder == 0) {
        m_nOrder = inverse(_nOrder);
        return *this;
    }

    return _update(_nOrder, rules);
}

Type::Order &Type::Order::in(const Type &_lhs, const Type &_rhs) {
    return in(_lhs.compare(_rhs));
}

Type::Order &Type::Order::out(int _nOrder) {
    static const Rule rules[] = {
            { ORD_ANY,      ORD_UNKNOWN,    ORD_UNKNOWN },
            { ORD_UNKNOWN,  ORD_ANY,        ORD_UNKNOWN },
            { ORD_ANY,      ORD_NONE,       ORD_NONE },
            { ORD_NONE,     ORD_ANY,        ORD_NONE },
            { ORD_SUB,      ORD_SUB,        ORD_SUB },
            { ORD_SUB,      ORD_SUPER,      ORD_NONE },
            { ORD_SUB,      ORD_EQUALS,     ORD_SUB },
            { ORD_SUPER,    ORD_SUB,        ORD_NONE },
            { ORD_SUPER,    ORD_SUPER,      ORD_SUPER },
            { ORD_SUPER,    ORD_EQUALS,     ORD_SUPER },
            { ORD_EQUALS,   ORD_SUB,        ORD_SUB },
            { ORD_EQUALS,   ORD_SUPER,      ORD_SUPER },
            { ORD_EQUALS,   ORD_EQUALS,     ORD_EQUALS },
    };

    if (m_nOrder == 0) {
        m_nOrder = _nOrder;
        return *this;
    }

    return _update(_nOrder, rules);
}

Type::Order &Type::Order::out(const Type &_lhs, const Type &_rhs) {
    return out(_lhs.compare(_rhs));
}

template<typename _Table>
Type::Order &Type::Order::_update(int _nOrder, const _Table &_table) {
    // Exact match only.
    if (_nOrder == ORD_NONE)
        m_nOrder = ORD_NONE;

    if (m_nOrder == ORD_NONE)
        return *this;

    const int nCurrent = m_nOrder;

    m_nOrder = 0;

    for (const auto &rule : _table)
        if ((nCurrent & rule.nCurrent) == rule.nCurrent &&
                (_nOrder & rule.nValue) == rule.nValue)
            m_nOrder |= rule.nResult;

    return *this;
}

bool Type::hasFresh() const {
    return getKind() == FRESH;
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

int Type::compare(const Type &_other) const {
    typedef std::pair<int, int> P;
    P kinds(getKind(), _other.getKind());

    if (this == &_other)
        return ORD_EQUALS;

    if (getKind() == BOTTOM || _other.getKind() == TOP)
        return getKind() == _other.getKind() ? ORD_EQUALS : ORD_SUB;
    if (getKind() == TOP || _other.getKind() == BOTTOM)
        return getKind() == _other.getKind() ? ORD_EQUALS : ORD_SUPER;

    if (getKind() == FRESH || _other.getKind() == FRESH) {
        if (contains(_other) || _other.contains(*this))
            return ORD_NONE;
        return ORD_UNKNOWN;
    }

    /*if (hasFresh() || _other.hasFresh())
        return OrdUnknown;*/

    if (_other.getKind() == SUBTYPE)
        return inverse(_other.compare(*this));

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
    if (kinds == P(UNIT, UNIT))
        return ORD_EQUALS;

    return ORD_NONE;
}

bool Type::compare(const Type &_other, int _nRequested) const {
    const int nOrder = compare(_other);
    return (nOrder & _nRequested) != 0 && (nOrder & ~_nRequested) == 0;
}

bool Type::less(const Type &_other) const {
    assert(getKind() == _other.getKind());

    if (getKind() == FRESH)
        return (((const tc::FreshType *)this)->getOrdinal() < ((const tc::FreshType &)_other).getOrdinal());

    const int nOrder = Type::compare(_other);

    // Should be comparable.
    assert(nOrder & (ORD_SUB | ORD_SUPER | ORD_EQUALS));

    return (nOrder & (ORD_SUPER | ORD_EQUALS)) == 0;
}

bool Type::less(const Node& _other) const {
    assert(_other.getNodeKind() == Node::TYPE);
    const Type& other = (const Type&)_other;
    if (getKind() != other.getKind())
        return getKind() < other.getKind();
    return less(other);
}

bool Type::equals(const Node& _other) const {
    assert(_other.getNodeKind() == Node::TYPE);
    const Type& other = (const Type&)_other;
    return getKind() == other.getKind() && !less(other) && !other.less(*this);
}

NodePtr Type::clone(Cloner &_cloner) const {
    return NEW_CLONE(this, _cloner, m_kind, m_nBits);
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

/* virtual */ TypePtr Type::getJoin(const TypePtr &_other) {
    return _getJoin(_other).first;
}

/* protected */ SideType Type::_getJoin(const TypePtr &_other) {
    if (getKind() == TOP || _other->getKind() == BOTTOM)
        return std::make_pair(shared_from_this()->as<Type>(), false);

    if (getKind() == BOTTOM || _other->getKind() == TOP)
        return std::make_pair(_other, false);

    if (getKind() == FRESH || _other->getKind() == FRESH) {
        const auto fresh = getKind() == FRESH ? as<Type>() : _other;
        const auto other = getKind() == FRESH ? _other : as<Type>();

        if (other->contains(*fresh))
            return std::make_pair(std::make_shared<Type>(TOP), false);

        return std::make_pair(TypePtr(), false);
    }

    const int nOrd = compare(*_other.get());

    if ((nOrd & ORD_SUB) && !(nOrd & ~(ORD_SUB | ORD_EQUALS)))
        return std::make_pair(_other, false);

    if ((nOrd & (ORD_SUPER | ORD_EQUALS)) && !(nOrd & ~(ORD_SUPER | ORD_EQUALS)))
        return std::make_pair(shared_from_this()->as<Type>(), false);

    typedef std::pair<int, int> P;
    P kinds(getKind(), _other->getKind());

    if (kinds == P(NAT, INT))
        return std::make_pair(std::make_shared<Type>(INT, maxBitsIntNat(_other->getBits(), getBits())), false);

    if (kinds == P(INT, NAT))
        return std::make_pair(std::make_shared<Type>(INT, maxBitsIntNat(getBits(), _other->getBits())), false);

    if (getKind() != _other->getKind()) {
        if (nOrd == ORD_UNKNOWN)
            return std::make_pair(TypePtr(), true);
        return std::make_pair(std::make_shared<Type>(TOP), false);
    }

    return std::make_pair(TypePtr(), false);
}

/* virtual */ TypePtr Type::getMeet(const ir::TypePtr &_other) {
    return _getMeet(_other).first;
}

/* protected */ SideType Type::_getMeet(const ir::TypePtr &_other) {
    if (getKind() == TOP || _other->getKind() == BOTTOM)
        return std::make_pair(_other, false);

    if (getKind() == BOTTOM || _other->getKind() == TOP)
        return std::make_pair(shared_from_this()->as<Type>(), false);

    if (getKind() == FRESH || _other->getKind() == FRESH) {
        const auto fresh = getKind() == FRESH ? as<Type>() : _other;
        const auto other = getKind() == FRESH ? _other : as<Type>();

        if (other->contains(*fresh))
            return std::make_pair(std::make_shared<Type>(BOTTOM), false);

        return std::make_pair(TypePtr(), false);
    }

    const int nOrd = compare(*_other.get());

    if ((nOrd & (ORD_SUB | ORD_EQUALS)) && !(nOrd & ~(ORD_SUB | ORD_EQUALS)))
        return std::make_pair(shared_from_this()->as<Type>(), false);

    if ((nOrd & ORD_SUPER) && !(nOrd & ~(ORD_SUPER | ORD_EQUALS)))
        return std::make_pair(_other, false);

    typedef std::pair<int, int> P;
    P kinds(getKind(), _other->getKind());

    if (kinds == P(NAT, INT))
        return std::make_pair(std::make_shared<Type>(NAT, minBitsIntNat(_other->getBits(), getBits())), false);

    if (kinds == P(INT, NAT))
        return std::make_pair(std::make_shared<Type>(NAT, minBitsIntNat(getBits(), _other->getBits())), false);

    if (getKind() != _other->getKind()) {
        if (nOrd == ORD_UNKNOWN)
            return std::make_pair(TypePtr(), true);
        return std::make_pair(std::make_shared<Type>(BOTTOM), false);
    }

    return std::make_pair(TypePtr(), false);
}

void ArrayType::getDimensions(Collection<Type>& _dimensions) const {
    _dimensions.add(getDimensionType());
    if (getBaseType()->getKind() == ARRAY)
        getBaseType()->as<ArrayType>()->getDimensions(_dimensions);
}

RangePtr Subtype::asRange() {
    const auto matches = std::make_shared<Matches>();
    const auto pMask =
        std::make_shared<Binary>(Binary::BOOL_AND,
                   std::make_shared<Binary>(Binary::GREATER_OR_EQUALS,
                              std::make_shared<Wild>(L"c"),
                              std::make_shared<Wild>(L"a")),
                   std::make_shared<Binary>(Binary::LESS_OR_EQUALS,
                              std::make_shared<Wild>(L"c"),
                              std::make_shared<Wild>(L"b")));

    pMask->setType(std::make_shared<Type>(Type::BOOL));
    pMask->getLeftSide()->setType(std::make_shared<Type>(Type::BOOL));
    pMask->getRightSide()->setType(std::make_shared<Type>(Type::BOOL));

    if (!Expression::_matches(getExpression(), pMask->as<Expression>(), matches))
        return NULL;
    return std::make_shared<Range>(matches->getExprByName(L"a"), matches->getExprByName(L"b"));
}

NodePtr Subtype::clone(Cloner &_cloner) const {
    return NEW_CLONE(this, _cloner, _cloner.get<NamedValue>(getParam()), _cloner.get<Expression>(getExpression()));
}

SubtypePtr Range::asSubtype() const {
    const auto pParam = std::make_shared<NamedValue>(L"", std::make_shared<Type>(Type::GENERIC));
    const auto pExpr = std::make_shared<Binary>(Binary::BOOL_AND,
        std::make_shared<Binary>(Binary::GREATER_OR_EQUALS,
            std::make_shared<VariableReference>(pParam),
            getMin()),
        std::make_shared<Binary>(Binary::LESS_OR_EQUALS,
            std::make_shared<VariableReference>(pParam),
            getMax()));

    pExpr->setType(std::make_shared<Type>(Type::BOOL));
    pExpr->getLeftSide()->setType(std::make_shared<Type>(Type::BOOL));
    pExpr->getRightSide()->setType(std::make_shared<Type>(Type::BOOL));

    return std::make_shared<Subtype>(pParam, pExpr);
}

NodePtr Range::clone(Cloner &_cloner) const {
    return NEW_CLONE(this, _cloner, _cloner.get<Expression>(getMin()), _cloner.get<Expression>(getMax()));
}

bool Type::isMonotone(const Type &_var, bool _bStrict) const {
    return getMonotonicity(_var) & (MT_MONOTONE | (_bStrict ? 0 : MT_CONST));
}

bool Type::isAntitone(const Type &_var, bool _bStrict) const {
    return getMonotonicity(_var) & (MT_ANTITONE | (_bStrict ? 0 : MT_CONST));
}

int Type::getMonotonicity(const Type &_var) const {
    return compare(_var) == ORD_EQUALS ? MT_MONOTONE : MT_CONST;
}

// 'type' type.

TypeType::TypeType(const TypeDeclarationPtr &_pDeclaration) : m_pDecl(_pDeclaration) {
}

bool TypeType::rewrite(const TypePtr &_pOld, const TypePtr &_pNew, bool _bRewriteFlags) {
    if (!m_pDecl || !m_pDecl->getType())
        return false;

    TypePtr p = m_pDecl->getType();

    if (tc::rewriteType(p, _pOld, _pNew, _bRewriteFlags)) {
        m_pDecl->setType(p);
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

    if (_other.getKind() == SUBTYPE)
        return inverse(_other.compare(*this));

    if (m_pDecl && m_pDecl->getType()) {
        if (other.m_pDecl && other.m_pDecl->getType())
            return m_pDecl->getType()->compare(*other.m_pDecl->getType());

        return ORD_NONE;
    }

    return (other.m_pDecl && other.m_pDecl->getType()) ? ORD_NONE : ORD_EQUALS;
}

bool TypeType::less(const Type &_other) const {
    const TypeType &other = (const TypeType &)_other;

    if (m_pDecl && m_pDecl->getType()) {
        if (other.m_pDecl && other.m_pDecl->getType())
            return *m_pDecl->getType() < *other.m_pDecl->getType();

        return false;
    }

    return other.m_pDecl && other.m_pDecl->getType();
}

NodePtr TypeType::clone(Cloner &_cloner) const {
    return NEW_CLONE(this, _cloner, _cloner.get<TypeDeclaration>(getDeclaration()));
}

// Parameterized type.

int ParameterizedType::getMonotonicity(const Type &_var) const {
    if (!getActualType())
        return MT_CONST;

    // TODO: consider parameters after other operators of parameterized types get implemented.
    return getActualType()->getMonotonicity(_var);
}

NodePtr ParameterizedType::clone(Cloner &_cloner) const {
    const auto pCopy = NEW_CLONE(this, _cloner, _cloner.get<Type>(getActualType()));
    pCopy->getParams().appendClones(getParams(), _cloner);
    return pCopy;
}

NodePtr EnumValue::clone(Cloner &_cloner) const {
    const auto pCopy = NEW_CLONE(this, _cloner, getName(), getOrdinal(), _cloner.get<Type>(getType()));
    pCopy->setLoc(this->getLoc());
    return pCopy;
}

NodePtr EnumType::clone(Cloner &_cloner) const {
    const auto pCopy = NEW_CLONE(this, _cloner);
    pCopy->getValues().appendClones(getValues(), _cloner);
    return pCopy;
}

TypePtr& UnionConstructorDeclaration::getFields() {
    return m_pFields;
}

const TypePtr& UnionConstructorDeclaration::getFields() const {
    return m_pFields;
}

StructTypePtr UnionConstructorDeclaration::getStructFields() const {
    return m_pFields && m_pFields->getKind() == Type::STRUCT ?
        m_pFields->as<StructType>() : StructTypePtr();
}

void UnionConstructorDeclaration::setFields(const TypePtr& _pFields) {
    m_pFields = _pFields;
}

bool UnionConstructorDeclaration::less(const Node& _other) const {
    if (!Node::equals(_other))
        return Node::less(_other);
    const UnionConstructorDeclaration& other = (const UnionConstructorDeclaration&)_other;
    if (getName() != other.getName())
        return getName() < other.getName();
    return _less(getFields(), other.getFields());
}

bool UnionConstructorDeclaration::equals(const Node& _other) const {
    if (!Node::equals(_other))
        return false;
    const UnionConstructorDeclaration& other = (const UnionConstructorDeclaration&)_other;
    return getName() == other.getName()
        && _equals(getFields(), other.getFields());
}

NodePtr UnionConstructorDeclaration::clone(Cloner &_cloner) const {
    const auto pCopy = NEW_CLONE(this, _cloner, getName(), getOrdinal(), _cloner.get<UnionType>(getUnion()), _cloner.get<Type>(getFields()));
    pCopy->setLoc(this->getLoc());
    return pCopy;
}

bool NamedReferenceType::rewrite(const TypePtr &_pOld, const TypePtr &_pNew, bool _bRewriteFlags) {
    bool bResult = false;
    for (size_t i = 0; i < m_args.size(); ++i) {
        if (m_args.get(i)->getKind() != Expression::TYPE)
            continue;
        TypePtr pType = m_args.get(i)->as<TypeExpr>()->getContents();
        if (tc::rewriteType(pType, _pOld, _pNew, _bRewriteFlags)) {
            m_args.get(i)->as<TypeExpr>()->setContents(pType);
            bResult = true;
        }
    }
    return bResult;
}

bool NamedReferenceType::less(const Type &_other) const {
    assert(_other.getKind() == Type::NAMED_REFERENCE);
    const NamedReferenceType &other = (const NamedReferenceType &)_other;

    if (getDeclaration() != other.getDeclaration())
        return getDeclaration() < other.getDeclaration();
    return getArgs() < other.getArgs();
}

bool NamedReferenceType::equals(const Node &_other) const {
    if (!Type::equals(_other))
        return false;
    const NamedReferenceType& other = (const NamedReferenceType&)_other;
    assert(other.getKind() == Type::NAMED_REFERENCE);// should be always true due to upper check

    return getDeclaration() == other.getDeclaration()
        && getArgs() == other.getArgs();
}

NodePtr NamedReferenceType::clone(Cloner &_cloner) const {
    const auto pCopy = NEW_CLONE(this, _cloner, _cloner.get<TypeDeclaration>(getDeclaration(), true));
    pCopy->getArgs().appendClones(getArgs(), _cloner);
    return pCopy;
}

bool TypeDeclaration::less(const Node &_other) const {
    if (!Statement::equals(_other))
        return Statement::less(_other);
    const TypeDeclaration& other = (const TypeDeclaration&)_other;
    if (getKind() != other.getKind())
        return getKind() < other.getKind();
    if (!_equals(getType(), other.getType()))
        return _less(getType(), other.getType());
    return getName() < other.getName();
}

bool TypeDeclaration::equals(const Node &_other) const {
    if (!Statement::equals(_other))
        return false;
    const TypeDeclaration& other = (const TypeDeclaration&)_other;
    return getKind() == other.getKind()
        && _equals(getType(), other.getType())
        && getName() == other.getName();
}

NodePtr TypeDeclaration::clone(Cloner &_cloner) const {
    const auto pCopy = NEW_CLONE(this, _cloner, getName(), _cloner.get<Type>(getType()), _cloner.get<Label>(getLabel()));
    pCopy->setLoc(this->getLoc());
    return pCopy;
}
