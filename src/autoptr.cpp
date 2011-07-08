/// \file autoptr.cpp
///

#include "autoptr.h"

void *Counted::m_pLastAlloc = NULL;

void Counted::ref(const Counted *_pObj) {
    if (_pObj != NULL && _pObj->m_nCountedRefs >= 0)
        ++_pObj->m_nCountedRefs;
}

bool Counted::unref() const {
    if (m_nCountedRefs <= 0)
        return true; // Don't cause deletion if there were no refs to begin with.

    --m_nCountedRefs;

    return m_nCountedRefs > 0;
}

void *Counted::operator new(size_t _c, bool _bAcquire) {
    void *pMem = ::operator new(_c);
    if (_bAcquire)
        m_pLastAlloc = pMem;
    return pMem;
}

const Counted *Counted::createCountedWrapper(const void *_pObj, bool _bAcquire) {
    return new(_bAcquire) Counted(_pObj);
}

const Counted *Counted::createCountedWrapper(const Counted *_pObj, bool /* _bAcquire */) {
    return (const Counted *)_pObj;
}

Counted::Counted(const void *_ptr) :
        m_pCountedPtr(_ptr),
        m_nCountedRefs(m_pLastAlloc == this ? 0 : -1)
{
}

Counted::Counted() :
        m_pCountedPtr(this),
        m_nCountedRefs(m_pLastAlloc == this ? 0 : -1)
{
}
