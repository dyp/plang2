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

bool StructType::rewrite(ir::Type *_pOld, ir::Type *_pNew) {
    bool bResult = false;

    for (size_t j = 0; j < 3; ++j)
        for (size_t i = 0; i < m_fields[j].size(); ++i) {
            Type *p = m_fields[j].get(i)->getType();
            if (tc::rewriteType(p, _pOld, _pNew)) {
                bResult = true;
                m_fields[j].get(i)->setType(p, false);
            }
        }

    return bResult;
}

bool StructType::contains(const ir::Type *_pType) const {
    for (size_t j = 0; j < 3; ++j)
        for (size_t i = 0; i < m_fields[j].size(); ++i) {
            const Type *pType = m_fields[j].get(i)->getType();
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

    typedef std::map<std::wstring, std::pair<NamedValue *, NamedValue *> > NameMap;
    NameMap fields;

    for (size_t i = 0; i < getNamesSet().size(); ++i) {
        fields[getNamesSet().get(i)->getName()].first = getNamesSet().get(i);
        fields[other.getNamesSet().get(i)->getName()].second = other.getNamesSet().get(i);
    }

    for (NameMap::iterator i = fields.begin(); i != fields.end(); ++i) {
        NamedValue *pField = i->second.first;
        NamedValue *pOtherField = i->second.second;

        if (pField == NULL)
            return false;

        if (pOtherField == NULL)
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
        NamedValue *pField = i < getNamesOrd().size() ? getNamesOrd().get(i) :
                getTypesOrd().get(i - getNamesOrd().size());
        NamedValue *pFieldOther = i < other.getNamesOrd().size() ? other.getNamesOrd().get(i) :
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

    typedef std::map<std::wstring, std::pair<NamedValue *, NamedValue *> > NameMap;
    NameMap fields;

    for (size_t i = 0; i < getNamesSet().size(); ++i)
        fields[getNamesSet().get(i)->getName()].first = getNamesSet().get(i);

    for (size_t i = 0; i < other.getNamesSet().size(); ++i)
       fields[other.getNamesSet().get(i)->getName()].second = other.getNamesSet().get(i);

    for (size_t i = 0; i < getNamesOrd().size(); ++i) {
        NamedValue *pField = getNamesOrd().get(i);
        NameMap::iterator j = fields.find(pField->getName());

        if (j != fields.end() && j->second.second != NULL)
            fields[pField->getName()].first = pField;
    }

    for (size_t i = 0; i < other.getNamesOrd().size(); ++i) {
        NamedValue *pField = other.getNamesOrd().get(i);
        NameMap::iterator j = fields.find(pField->getName());

        if (j != fields.end() && j->second.first != NULL)
            fields[pField->getName()].second = pField;
    }

    for (NameMap::iterator i = fields.begin(); i != fields.end(); ++i) {
        NamedValue *pField = i->second.first;
        NamedValue *pOtherField = i->second.second;

        if (pField == NULL)
            ++cSuper;
        else if (pOtherField == NULL)
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

Type *StructType::getMeet(ir::Type &_other) {
    Type *pMeet = Type::getMeet(_other);

    if (pMeet != NULL || _other.getKind() == FRESH)
        return pMeet;

    const StructType &other = (const StructType &)_other;
    StructType *pStruct = new StructType();

    typedef std::map<std::wstring, std::pair<NamedValue *, NamedValue *> > NameMap;
    NameMap fields;

    for (size_t i = 0; i < getNamesSet().size(); ++i)
        fields[getNamesSet().get(i)->getName()].first = getNamesSet().get(i);

    for (size_t i = 0; i < other.getNamesSet().size(); ++i)
        fields[other.getNamesSet().get(i)->getName()].second = other.getNamesSet().get(i);

    const size_t cOrdFields = getNamesOrd().size() + getTypesOrd().size();
    const size_t cOrdFieldsOther = other.getNamesOrd().size() + other.getTypesOrd().size();

    for (size_t i = 0; i < cOrdFields || i < cOrdFieldsOther; ++i) {
        NamedValue *pField = i < cOrdFields ? (i < getNamesOrd().size() ? getNamesOrd().get(i) :
                getTypesOrd().get(i - getNamesOrd().size())) : NULL;
        NamedValue *pFieldOther = i < cOrdFieldsOther ? (i < other.getNamesOrd().size() ? other.getNamesOrd().get(i) :
                other.getTypesOrd().get(i - other.getNamesOrd().size())) : NULL;

        if (i < getNamesOrd().size() && i < other.getNamesOrd().size())
            if (pField->getName() != pFieldOther->getName())
                return new Type(BOTTOM);

        Type *pType = pField ? pField->getType() : NULL;
        Type *pTypeOther = pFieldOther ? pFieldOther->getType() : NULL;
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
        NamedValue *pField = i->second.first;
        NamedValue *pFieldOther = i->second.second;

        if (pField && pFieldOther) {
            Type *pMeetField = pField->getType()->getMeet(*pFieldOther->getType());
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

Type *StructType::getJoin(ir::Type & _other) {
    Type *pJoin = Type::getMeet(_other);

    if (pJoin != NULL || _other.getKind() == FRESH)
        return pJoin;

    const StructType &other = (const StructType &)_other;
    StructType *pStruct = new StructType();

    typedef std::map<std::wstring, std::pair<NamedValue *, NamedValue *> > NameMap;
    NameMap fields;

    for (size_t i = 0; i < getNamesSet().size(); ++i)
        fields[getNamesSet().get(i)->getName()].first = getNamesSet().get(i);

    for (size_t i = 0; i < other.getNamesSet().size(); ++i)
        fields[other.getNamesSet().get(i)->getName()].second = other.getNamesSet().get(i);

    const size_t cOrdFields = getNamesOrd().size() + getTypesOrd().size();
    const size_t cOrdFieldsOther = other.getNamesOrd().size() + other.getTypesOrd().size();

    for (size_t i = 0; i < cOrdFields && i < cOrdFieldsOther; ++i) {
        NamedValue *pField = i < getNamesOrd().size() ? getNamesOrd().get(i) :
                getTypesOrd().get(i - getNamesOrd().size());
        NamedValue *pFieldOther = i < other.getNamesOrd().size() ? other.getNamesOrd().get(i) :
                other.getTypesOrd().get(i - other.getNamesOrd().size());
        std::wstring strName = pField->getName();

        if (pField->getName() != pFieldOther->getName())
            strName = L"";

        Type *pType = pField->getType();
        Type *pTypeOther = pFieldOther->getType();
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
        NamedValue *pField = i->second.first;
        NamedValue *pFieldOther = i->second.second;

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

    for (size_t i = 0; i < m_fields.size() && i < other.getFields().size(); ++i) {
        const NamedValue &field = *m_fields.get(i);
        const NamedValue &otherField = *other.getFields().get(i);
        const int cmp = field.getType()->compare(*otherField.getType());

        if (cmp == ORD_SUB)
            ++ cSub;
        else if (cmp == ORD_SUPER)
            ++ cSuper;
        else if (cmp == ORD_UNKNOWN)
            ++ cUnknown;
        else if (cmp == ORD_NONE)
            return ORD_NONE;
    }

    if (cSub > 0 && cSuper > 0)
        return ORD_NONE;

    if (cUnknown > 0)
        return ORD_UNKNOWN;

    if (m_fields.size() == other.getFields().size()) {
        if (cSub > 0)
            return ORD_SUB;
        if (cSuper > 0)
            return ORD_SUPER;
        return ORD_EQUALS;
    }

    if (m_fields.size() > other.getFields().size())
        return cSuper > 0 ? ORD_NONE : ORD_SUB;

    // m_fields.size() < other.getFields().size()
    return cSub > 0 ? ORD_NONE : ORD_SUPER;
}

Type *tc::TupleType::getMeet(ir::Type & _other) {
    Type *pMeet = Type::getMeet(_other);

    if (pMeet != NULL || _other.getKind() == FRESH)
        return pMeet;

    const tc::TupleType &other = (const tc::TupleType &)_other;
    tc::TupleType *pTuple = new tc::TupleType(new NamedValues());

    for (size_t i = 0; i < m_fields.size() && i < other.getFields().size(); ++i) {
        const NamedValue &field = *m_fields.get(i);
        const NamedValue &otherField = *other.getFields().get(i);

        pMeet = field.getType()->getMeet(*otherField.getType());

        if (pMeet == NULL)
            return NULL;

        pTuple->getFields().add(new NamedValue(field.getName(), pMeet));
    }

    for (size_t i = getFields().size(); i < other.getFields().size(); ++i)
        pTuple->getFields().add(new NamedValue(*other.getFields().get(i)));

    for (size_t i = other.getFields().size(); i < m_fields.size(); ++i)
        pTuple->getFields().add(new NamedValue(*getFields().get(i)));

    return pTuple;
}

Type *tc::TupleType::getJoin(ir::Type & _other) {
    Type *pJoin = Type::getJoin(_other);

    if (pJoin != NULL || _other.getKind() == FRESH)
        return pJoin;

    const tc::TupleType &other = (const tc::TupleType &)_other;

    tc::TupleType *pTuple = new tc::TupleType(new NamedValues());

    for (size_t i = 0; i < m_fields.size() && i < other.getFields().size(); ++i) {
        const NamedValue &field = *m_fields.get(i);
        const NamedValue &otherField = *other.getFields().get(i);

        pJoin = field.getType()->getJoin(*otherField.getType());

        if (pJoin == NULL)
            return NULL;

        pTuple->getFields().add(new NamedValue(field.getName(), pJoin));
    }

    if (pTuple->getFields().empty())
        return new Type(TOP);

    return pTuple;
}
