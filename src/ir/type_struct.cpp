/// \file type_struct.cpp
///

#include "ir/base.h"
#include "ir/types.h"
#include "ir/declarations.h"
#include "typecheck.h"

using namespace ir;

// Structs.

StructType::StructType() :
    m_namesOrd(m_fields[0]), m_typesOrd(m_fields[1]), m_namesSet(m_fields[2])
{
}

bool StructType::empty() const {
    return m_fields[0].empty() && m_fields[1].empty() && m_fields[2].empty();
}

bool StructType::hasFresh() const {
    for (size_t j = 0; j < 3; ++j)
        for (size_t i = 0; i < m_fields[j].size(); ++i)
            if (m_fields[j].get(i)->getType()->hasFresh())
                return true;

    return false;
}

bool StructType::rewrite(const TypePtr &_pOld, const TypePtr &_pNew) {
    bool bResult = false;

    for (size_t j = 0; j < 3; ++j)
        for (size_t i = 0; i < m_fields[j].size(); ++i) {
            TypePtr p = m_fields[j].get(i)->getType();
            if (tc::rewriteType(p, _pOld, _pNew)) {
                bResult = true;
                m_fields[j].get(i)->setType(p);
            }
        }

    return bResult;
}

bool StructType::contains(const TypePtr &_pType) const {
    for (size_t j = 0; j < 3; ++j)
        for (size_t i = 0; i < m_fields[j].size(); ++i) {
            TypePtr pType = m_fields[j].get(i)->getType();
            if (*pType == *_pType || pType->contains(_pType))
                return true;
        }

    return false;
}

bool StructType::less(const Type &_other) const {
    assert(_other.getKind() == STRUCT);

    const StructType &other = (const StructType &)_other;

    if (tc::TupleType(&getNamesOrd()) < tc::TupleType(&other.getNamesOrd()))
        return true;

    if (tc::TupleType(&other.getNamesOrd()) < tc::TupleType(&getNamesOrd()))
        return false;

    if (tc::TupleType(&getTypesOrd()) < tc::TupleType(&other.getTypesOrd()))
        return true;

    if (tc::TupleType(&other.getTypesOrd()) < tc::TupleType(&getTypesOrd()))
        return false;

    if (getNamesSet().size() != other.getNamesSet().size())
        return getNamesSet().size() < other.getNamesSet().size();

    typedef std::map<std::wstring, std::pair<NamedValuePtr, NamedValuePtr> > NameMap;
    NameMap fields;

    for (size_t i = 0; i < getNamesSet().size(); ++i) {
        fields[getNamesSet().get(i)->getName()].first = getNamesSet().get(i);
        fields[other.getNamesSet().get(i)->getName()].second = other.getNamesSet().get(i);
    }

    for (NameMap::iterator i = fields.begin(); i != fields.end(); ++i) {
        NamedValuePtr pField = i->second.first;
        NamedValuePtr pOtherField = i->second.second;

        if (!pField)
            return false;

        if (!pOtherField)
            return true;

        if (*pField->getType() < *pOtherField->getType())
            return true;

        if (*pOtherField->getType() < *pField->getType())
            return false;
    }

    return false;
}

int StructType::compare(const Type & _other) const {
    if (_other.getKind() == FRESH)
        return ORD_UNKNOWN;

    if (_other.getKind() == TOP)
        return ORD_SUB;

    if (_other.getKind() == BOTTOM)
        return ORD_SUPER;

    if (_other.getKind() != STRUCT)
        return ORD_NONE;

    const StructType & other = (const StructType &) _other;
    size_t cSub = 0, cSuper = 0, cUnknown = 0;
    const size_t cOrdFields = getNamesOrd().size() + getTypesOrd().size();
    const size_t cOrdFieldsOther = other.getNamesOrd().size() + other.getTypesOrd().size();

    for (size_t i = 0; i < cOrdFields && i < cOrdFieldsOther; ++i) {
        NamedValuePtr pField = i < getNamesOrd().size() ? getNamesOrd().get(i) :
                getTypesOrd().get(i - getNamesOrd().size());
        NamedValuePtr pFieldOther = i < other.getNamesOrd().size() ? other.getNamesOrd().get(i) :
                other.getTypesOrd().get(i - other.getNamesOrd().size());

        if (i < getNamesOrd().size() && i < other.getNamesOrd().size())
            if (pField->getName() != pFieldOther->getName())
                return ORD_NONE;

        const int cmp = pField->getType()->compare(*pFieldOther->getType());

        if (cmp == ORD_SUB)
            ++cSub;
        else if (cmp == ORD_SUPER)
            ++cSuper;
        else if (cmp == ORD_UNKNOWN)
            ++cUnknown;
        else if (cmp == ORD_NONE)
            return ORD_NONE;
    }

    if (cOrdFields > cOrdFieldsOther)
        ++cSub;
    else if (cOrdFields < cOrdFieldsOther)
        ++cSuper;

    if (cSub > 0 && cSuper > 0)
        return ORD_NONE;

    typedef std::map<std::wstring, std::pair<NamedValuePtr, NamedValuePtr> > NameMap;
    NameMap fields;

    for (size_t i = 0; i < getNamesSet().size(); ++i)
        fields[getNamesSet().get(i)->getName()].first = getNamesSet().get(i);

    for (size_t i = 0; i < other.getNamesSet().size(); ++i)
        fields[other.getNamesSet().get(i)->getName()].second = other.getNamesSet().get(i);

    for (size_t i = 0; i < getNamesOrd().size(); ++i) {
        NamedValuePtr pField = getNamesOrd().get(i);
        NameMap::iterator j = fields.find(pField->getName());

        if (j != fields.end() && j->second.second)
            j->second.first = pField;
    }

    for (size_t i = 0; i < other.getNamesOrd().size(); ++i) {
        NamedValuePtr pField = other.getNamesOrd().get(i);
        NameMap::iterator j = fields.find(pField->getName());

        if (j != fields.end() && j->second.first)
            j->second.second = pField;
    }

    for (NameMap::iterator i = fields.begin(); i != fields.end(); ++i) {
        NamedValuePtr pField = i->second.first;
        NamedValuePtr pOtherField = i->second.second;

        if (!pField)
            ++cSuper;
        else if (!pOtherField)
            ++cSub;
        else {
            const int cmp = pField->getType()->compare(*pOtherField->getType());

            if (cmp == ORD_SUB)
                ++cSub;
            else if (cmp == ORD_SUPER)
                ++cSuper;
            else if (cmp == ORD_UNKNOWN)
                ++cUnknown;
            else if (cmp == ORD_NONE)
                return ORD_NONE;
        }
    }

    if (cSub > 0 && cSuper > 0)
        return ORD_NONE;

    if (cUnknown > 0)
        return ORD_UNKNOWN;

    if (cSub > 0)
        return ORD_SUB;

    if (cSuper > 0)
        return ORD_SUPER;

    return ORD_EQUALS;
}

TypePtr StructType::getMeet(ir::Type &_other) {
    TypePtr pMeet = Type::getMeet(_other);

    if (pMeet || _other.getKind() == FRESH)
        return pMeet;

    const StructType &other = (const StructType &)_other;
    StructTypePtr pStruct = new StructType();

    typedef std::map<std::wstring, std::pair<NamedValuePtr, NamedValuePtr> > NameMap;
    NameMap fields;

    for (size_t i = 0; i < getNamesSet().size(); ++i)
        fields[getNamesSet().get(i)->getName()].first = getNamesSet().get(i);

    for (size_t i = 0; i < other.getNamesSet().size(); ++i)
        fields[other.getNamesSet().get(i)->getName()].second = other.getNamesSet().get(i);

    const size_t cOrdFields = getNamesOrd().size() + getTypesOrd().size();
    const size_t cOrdFieldsOther = other.getNamesOrd().size() + other.getTypesOrd().size();

    for (size_t i = 0; i < cOrdFields || i < cOrdFieldsOther; ++i) {
        NamedValuePtr pField = i < cOrdFields ? (i < getNamesOrd().size() ? getNamesOrd().get(i) :
                getTypesOrd().get(i - getNamesOrd().size())) : NamedValuePtr();
        NamedValuePtr pFieldOther = i < cOrdFieldsOther ? (i < other.getNamesOrd().size() ? other.getNamesOrd().get(i) :
                other.getTypesOrd().get(i - other.getNamesOrd().size())) : NamedValuePtr();

        if (i < getNamesOrd().size() && i < other.getNamesOrd().size())
            if (pField->getName() != pFieldOther->getName())
                return new Type(BOTTOM);

        TypePtr pType = pField ? pField->getType() : TypePtr();
        TypePtr pTypeOther = pFieldOther ? pFieldOther->getType() : TypePtr();
        std::wstring strName = (!pField || (pFieldOther && pField->getName().empty())) ?
                pFieldOther->getName() : pField->getName();
        NameMap::iterator j = fields.find(strName);

        if (j != fields.end()) {
            if (j->second.first) {
                if (pTypeOther) {
                    pTypeOther = pTypeOther->getMeet(*j->second.first->getType());
                    if (!pTypeOther)
                        return NULL;
                } else
                    pTypeOther = j->second.first->getType();
            }

            if (j->second.second) {
                if (pType) {
                    pType = pType->getMeet(*j->second.second->getType());
                    if (!pType)
                        return NULL;
                } else
                    pType = j->second.second->getType();
            }

            j->second.first = NULL;
            j->second.second = NULL;
        }

        if (pTypeOther) {
            if (pType) {
                pType = pType->getMeet(*pTypeOther);
                if (!pType)
                    return NULL;
            } else
                pType = pTypeOther;
        }

        if (strName.empty())
            pStruct->getTypesOrd().add(new NamedValue(strName, pType));
        else
            pStruct->getNamesOrd().add(new NamedValue(strName, pType));
    }

    for (NameMap::iterator i = fields.begin(); i != fields.end(); ++i) {
        NamedValuePtr pField = i->second.first;
        NamedValuePtr pFieldOther = i->second.second;

        if (pField && pFieldOther) {
            TypePtr pMeetField = pField->getType()->getMeet(*pFieldOther->getType());
            if (!pMeetField)
                return NULL;
            pStruct->getNamesSet().add(new NamedValue(pField->getName(), pMeetField));
        } else if (pField)
            pStruct->getNamesSet().add(new NamedValue(pField->getName(), pField->getType()));
        else if (pFieldOther)
            pStruct->getNamesSet().add(new NamedValue(pFieldOther->getName(), pFieldOther->getType()));
    }

    return pStruct;
}

TypePtr StructType::getJoin(ir::Type &_other) {
    TypePtr pJoin = Type::getMeet(_other);

    if (pJoin || _other.getKind() == FRESH)
        return pJoin;

    const StructType &other = (const StructType &)_other;
    StructTypePtr pStruct = new StructType();

    typedef std::map<std::wstring, std::pair<NamedValuePtr, NamedValuePtr> > NameMap;
    NameMap fields;

    for (size_t i = 0; i < getNamesSet().size(); ++i)
        fields[getNamesSet().get(i)->getName()].first = getNamesSet().get(i);

    for (size_t i = 0; i < other.getNamesSet().size(); ++i)
        fields[other.getNamesSet().get(i)->getName()].second = other.getNamesSet().get(i);

    const size_t cOrdFields = getNamesOrd().size() + getTypesOrd().size();
    const size_t cOrdFieldsOther = other.getNamesOrd().size() + other.getTypesOrd().size();

    for (size_t i = 0; i < cOrdFields && i < cOrdFieldsOther; ++i) {
        NamedValuePtr pField = i < getNamesOrd().size() ? getNamesOrd().get(i) :
                getTypesOrd().get(i - getNamesOrd().size());
        NamedValuePtr pFieldOther = i < other.getNamesOrd().size() ? other.getNamesOrd().get(i) :
                other.getTypesOrd().get(i - other.getNamesOrd().size());
        std::wstring strName = pField->getName();

        if (pField->getName() != pFieldOther->getName())
            strName = L"";

        TypePtr pType = pField->getType();
        TypePtr pTypeOther = pFieldOther->getType();
        NameMap::iterator j = fields.find(strName);

        if (j != fields.end()) {
            if (j->second.first) {
                pTypeOther = pTypeOther->getJoin(*j->second.first->getType());
                if (!pTypeOther)
                    return NULL;
            }

            if (j->second.second) {
                pType = pType->getJoin(*j->second.second->getType());
                if (!pType)
                    return NULL;
            }

            j->second.first = NULL;
            j->second.second = NULL;
        }

        pJoin = pType->getJoin(*pTypeOther);

        if (!pJoin)
            return NULL;

        if (strName.empty())
            pStruct->getTypesOrd().add(new NamedValue(strName, pJoin));
        else
            pStruct->getNamesOrd().add(new NamedValue(strName, pJoin));
    }

    for (size_t i = pStruct->getNamesOrd().size(); i < getNamesOrd().size(); ++i)
        fields[getNamesOrd().get(i)->getName()].first = getNamesOrd().get(i);

    for (size_t i = pStruct->getNamesOrd().size(); i < other.getNamesOrd().size(); ++i)
        fields[other.getNamesOrd().get(i)->getName()].second = other.getNamesOrd().get(i);

    for (NameMap::iterator i = fields.begin(); i != fields.end(); ++i) {
        NamedValuePtr pField = i->second.first;
        NamedValuePtr pFieldOther = i->second.second;

        if (pField && pFieldOther) {
            pJoin = pField->getType()->getMeet(*pFieldOther->getType());
            if (!pJoin)
                return NULL;
            pStruct->getNamesSet().add(new NamedValue(pField->getName(), pJoin));
        }
    }

    if (pStruct->empty())
        return new Type(TOP);

    return pStruct;
}

int StructType::getMonotonicity(const Type &_var) const {
    bool bMonotone = false, bAntitone = false;

    for (size_t j = 0; j < 3; ++j)
        for (size_t i = 0; i < m_fields[j].size(); ++i) {
            TypePtr pType = m_fields[j].get(i)->getType();
            const int mt = pType->getMonotonicity(_var);

            bMonotone |= mt == MT_MONOTONE;
            bAntitone |= mt == MT_ANTITONE;

            if ((bMonotone && bAntitone) || mt == MT_NONE)
                return MT_NONE;
        }

    return bMonotone ? MT_MONOTONE : (bAntitone ? MT_ANTITONE : MT_CONST);
}

// Tuples.

bool tc::TupleType::less(const Type &_other) const {
    assert(_other.getKind() == TUPLE);

    const tc::TupleType &other = (const tc::TupleType &)_other;

    if (getFields().size() != other.getFields().size())
        return getFields().size() < other.getFields().size();

    for (size_t i = 0; i < getFields().size(); ++i) {
        const NamedValue &field = *getFields().get(i);
        const NamedValue &fieldOther = *other.getFields().get(i);

        if (*field.getType() < *fieldOther.getType())
            return true;

        if (*fieldOther.getType() < *field.getType())
            return false;
    }

    return false;
}

int tc::TupleType::compare(const Type &_other) const {
    if (_other.getKind() == FRESH)
        return ORD_UNKNOWN;

    if (_other.getKind() != TUPLE)
        return ORD_NONE;

    const tc::TupleType &other = (const tc::TupleType &)_other;

    size_t cSub = 0, cSuper = 0, cUnknown = 0;

    for (size_t i = 0; i < getFields().size() && i < other.getFields().size(); ++i) {
        const NamedValue &field = *getFields().get(i);
        const NamedValue &otherField = *other.getFields().get(i);
        const int cmp = field.getType()->compare(*otherField.getType());

        if (cmp == ORD_SUB)
            ++cSub;
        else if (cmp == ORD_SUPER)
            ++cSuper;
        else if (cmp == ORD_UNKNOWN)
            ++cUnknown;
        else if (cmp == ORD_NONE)
            return ORD_NONE;
    }

    if (cSub > 0 && cSuper > 0)
        return ORD_NONE;

    if (cUnknown > 0)
        return ORD_UNKNOWN;

    if (getFields().size() == other.getFields().size()) {
        if (cSub > 0)
            return ORD_SUB;
        if (cSuper > 0)
            return ORD_SUPER;
        return ORD_EQUALS;
    }

    if (getFields().size() > other.getFields().size())
        return cSuper > 0 ? ORD_NONE : ORD_SUB;

    // getFields().size() < other.getFields().size()
    return cSub > 0 ? ORD_NONE : ORD_SUPER;
}

TypePtr tc::TupleType::getMeet(ir::Type &_other) {
    TypePtr pMeet = Type::getMeet(_other);

    if (pMeet || _other.getKind() == FRESH)
        return pMeet;

    const tc::TupleType &other = (const tc::TupleType &)_other;
    tc::TupleTypePtr pTuple = new tc::TupleType(new NamedValues());

    for (size_t i = 0; i < getFields().size() && i < other.getFields().size(); ++i) {
        const NamedValue &field = *getFields().get(i);
        const NamedValue &otherField = *other.getFields().get(i);

        pMeet = field.getType()->getMeet(*otherField.getType());

        if (!pMeet)
            return NULL;

        pTuple->getFields().add(new NamedValue(field.getName(), pMeet));
    }

    for (size_t i = getFields().size(); i < other.getFields().size(); ++i)
        pTuple->getFields().add(new NamedValue(*other.getFields().get(i)));

    for (size_t i = other.getFields().size(); i < getFields().size(); ++i)
        pTuple->getFields().add(new NamedValue(*getFields().get(i)));

    return pTuple;
}

TypePtr tc::TupleType::getJoin(ir::Type &_other) {
    TypePtr pJoin = Type::getJoin(_other);

    if (pJoin || _other.getKind() == FRESH)
        return pJoin;

    const tc::TupleType &other = (const tc::TupleType &)_other;
    tc::TupleTypePtr pTuple = new tc::TupleType(new NamedValues());

    for (size_t i = 0; i < getFields().size() && i < other.getFields().size(); ++i) {
        const NamedValue &field = *getFields().get(i);
        const NamedValue &otherField = *other.getFields().get(i);

        pJoin = field.getType()->getJoin(*otherField.getType());

        if (!pJoin)
            return NULL;

        pTuple->getFields().add(new NamedValue(field.getName(), pJoin));
    }

    if (pTuple->getFields().empty())
        return new Type(TOP);

    return pTuple;
}

int tc::TupleType::getMonotonicity(const Type &_var) const {
    bool bMonotone = false, bAntitone = false;

    for (size_t i = 0; i < getFields().size(); ++i) {
        TypePtr pType = getFields().get(i)->getType();
        const int mt = pType->getMonotonicity(_var);

        bMonotone |= mt == MT_MONOTONE;
        bAntitone |= mt == MT_ANTITONE;

        if ((bMonotone && bAntitone) || mt == MT_NONE)
            return MT_NONE;
    }

    return bMonotone ? MT_MONOTONE : (bAntitone ? MT_ANTITONE : MT_CONST);
}
