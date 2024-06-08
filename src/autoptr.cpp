/// \file autoptr.cpp
///

#include "autoptr.h"
#include "utils.h"

int Cloner::_getHandle(const void* _pObject) {
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
        const void* pObject = i->second;

        m_handles[pObject] = _nHandle;
        m_objects.erase(i);
        m_objects.insert(std::make_pair(_nHandle, pObject));
        i = iNext;
    }
}

void* operator new(size_t _cSize, Cloner& _cloner, void* _pOriginal) {
    return _pOriginal;
}
