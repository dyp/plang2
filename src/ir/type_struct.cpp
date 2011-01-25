/// \file type_struct.cpp
///

#include "ir/base.h"
#include "ir/types.h"
#include "ir/declarations.h"
#include "typecheck.h"

using namespace ir;

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
