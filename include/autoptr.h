/// \file utils.h
/// Miscellaneous utility functions.

#ifndef AUTOPTR_H_
#define AUTOPTR_H_

#include <string>
#include <map>
#include <stack>
#include <stdint.h>
#include <assert.h>
#include <memory>

// Cloner.
class Cloner {
public:
    ~Cloner() = default;

    template<class _Obj>
    std::shared_ptr<_Obj> get(const std::shared_ptr<_Obj>& _pObj, bool _bKeepOriginal = false) {
        if (!_pObj)
            return nullptr;

        const auto iObj = m_cache.find(_getHandle(_pObj.get()));

        if (iObj != m_cache.end())
            return std::static_pointer_cast<_Obj>(iObj->second);

        if (_bKeepOriginal)
            return _pObj;

        return std::static_pointer_cast<_Obj>(_pObj->clone(*this));
    }

    template<class _Obj>
    void alias(const std::shared_ptr<_Obj>& _pObj, const std::shared_ptr<_Obj>& _pOther) {
        _mergeHandles(_getHandle(_pObj.get()), _getHandle(_pOther.get()));
    }

    template<class _Obj>
    void inject(const std::shared_ptr<_Obj>& _pNew) {
        m_cache[_getHandle(_pNew.get())] = _pNew;
    }

    template<class _Obj>
    void inject(const std::shared_ptr<_Obj>& _pNew, const std::shared_ptr<_Obj>& _pOld) {
        const int nHandle = _getHandle(_pNew.get());
        _mergeHandles(nHandle, _getHandle(_pOld.get()));
        m_cache[nHandle] = _pNew;
    }

    template<class _Obj>
    bool isKnown(const std::shared_ptr<_Obj>& _pObj) const {
        return m_handles.find(_pObj.get()) != m_handles.cend();
    }

    template<class _Obj>
    void* allocate(size_t _cSize, const void* _pOriginal) {
        auto objPtr = std::shared_ptr<void>(std::malloc(_cSize), [](auto p)
        {
            delete ((_Obj *)p);
        });

        m_cache[_getHandle(_pOriginal)] = objPtr;

        return objPtr.get();
    }

    friend void* operator new(size_t, Cloner&, const void*);

private:
    typedef std::map<int, std::shared_ptr<void>> Cache;
    typedef std::map<const void*, int> Handles;
    typedef std::multimap<int, const void*> Objects;

    Cache m_cache;
    Handles m_handles;
    Objects m_objects;

    int _getHandle(const void* _pObject);
    void _mergeHandles(int _nHandle, int _nOther);
};

void* operator new(size_t _cSize, Cloner& _cloner, const void* _pOriginal);

template<typename _Obj>
inline std::shared_ptr<_Obj> clone(const std::shared_ptr<_Obj>& _obj) {
    Cloner cloner;
    return cloner.get(_obj);
}
// We need a sequence point between allocation and evaluation of constructor arguments
// in order to cache uninitialized object and prevent infinite recursion on cyclic references.
#define NEW_CLONE(_ORIGINAL, _CLONER, _CTOR) \
    ((_CLONER).allocate<std::remove_const<std::remove_reference<decltype(*_ORIGINAL)>::type>::type>(sizeof(*_ORIGINAL), _ORIGINAL), std::shared_ptr<std::remove_const<std::remove_reference<decltype(*_ORIGINAL)>::type>::type>(::new((_CLONER), _ORIGINAL) _CTOR))

template<class _Comparable>
struct PtrLess {
    bool operator()(const std::shared_ptr<const _Comparable>& _pLhs, const std::shared_ptr<const _Comparable>& _pRhs) const { return _pLhs.get() < _pRhs.get(); }
};

#endif /* AUTOPTR_H_ */
