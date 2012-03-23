/// \file autoptr.cpp
///

#include "autoptr.h"
#include "utils.h"

std::stack<void *> Counted::m_allocs;

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

bool Counted::_isManaged(const void *_pObj, bool _bPop) {
    const bool bResult = !m_allocs.empty() && m_allocs.top() == _pObj;

    if (bResult && _bPop)
        m_allocs.pop();

    return bResult;
}

void *Counted::operator new(size_t _c, bool _bAcquire) {
    void *pMem = ::operator new(_c);

    ((Counted *)pMem)->m_pCountedPtr = pMem;
    ((Counted *)pMem)->m_nCountedRefs = 0;

    if (_bAcquire)
        m_allocs.push(pMem);

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
        m_nCountedRefs(_isManaged(this) ? m_nCountedRefs : -1)
{
}

Counted::Counted() :
        m_pCountedPtr(this),
        m_nCountedRefs(_isManaged(this) ? m_nCountedRefs : -1)
{
}

void *Cloner::allocate(size_t _cSize, const void *_pOriginal) {
    Counted *pCounted = new(true) Counted(::operator new(_cSize));

    Counted::ref(pCounted);
    m_cache[_getHandle(_pOriginal)] = pCounted;

    return (void *)pCounted->m_pCountedPtr;
}

void *Cloner::allocate(size_t _cSize, const Counted *_pOriginal) {
    Counted *pCounted = (Counted *)Counted::operator new(_cSize, true);

    Counted::ref(pCounted);
    m_cache[_getHandle(_pOriginal)] = pCounted;

    return (void *)pCounted->m_pCountedPtr;
}

int Cloner::_getHandle(const void *_pObject) {
    std::pair<Handles::iterator, bool> handle = m_handles.insert(
        std::make_pair(_pObject, (int)m_handles.size()));

    if (handle.second)
        m_objects.insert(std::make_pair(handle.first->second, _pObject));

    return handle.first->second;
}

void Cloner::_mergeHandles(int _nHandle, int _nOther) {
    if (_nHandle == _nOther)
        return;

    std::pair<Objects::iterator, Objects::iterator> bounds =
        m_objects.equal_range(_nOther);

    for (Objects::iterator i = bounds.first; i != bounds.second;) {
        Objects::iterator iNext = ::next(i);
        const void *pObject = i->second;

        m_handles[pObject] = _nHandle;
        m_objects.erase(i);
        m_objects.insert(std::make_pair(_nHandle, pObject));
        i = iNext;
    }
}

void *operator new(size_t _cSize, Cloner &_cloner, const void *_pOriginal) {
    Cloner::Cache::iterator iObj = _cloner.m_cache.find(
        _cloner._getHandle(_pOriginal));
    assert(iObj != _cloner.m_cache.end());
    return (void *)iObj->second;
}
