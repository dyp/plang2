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
private:
    struct FakeShared : public std::enable_shared_from_this<FakeShared> {
    };
public:
    ~Cloner() = default;

    template<class _Obj>
    std::shared_ptr<_Obj> get(const std::shared_ptr<_Obj>& _pObj, bool _bKeepOriginal = false) {
        if (!_pObj)
            return nullptr;

        const auto iObj = m_cache.find(_getHandle(_pObj.get()));

        if (iObj != m_cache.end())
            return std::reinterpret_pointer_cast<_Obj>(iObj->second);

        if (_bKeepOriginal)
            return _pObj;

        return std::reinterpret_pointer_cast<_Obj>(_pObj->clone(*this));
    }

    template<class _Obj>
    void alias(const std::shared_ptr<_Obj>& _pObj, const std::shared_ptr<_Obj>& _pOther) {
        _mergeHandles(_getHandle(_pObj.get()), _getHandle(_pOther.get()));
    }

    template<class _Obj>
    void inject(const std::shared_ptr<_Obj>& _pNew) {
        m_cache[_getHandle(_pNew.get())] = std::reinterpret_pointer_cast<FakeShared>(_pNew);
    }

    template<class _Obj>
    void inject(const std::shared_ptr<_Obj>& _pNew, const std::shared_ptr<_Obj>& _pOld) {
        const int nHandle = _getHandle(_pNew.get());
        _mergeHandles(nHandle, _getHandle(_pOld.get()));
        m_cache[nHandle] = std::reinterpret_pointer_cast<FakeShared>(_pNew);
    }

    template<class _Obj>
    bool isKnown(const std::shared_ptr<_Obj>& _pObj) const {
        return m_handles.find(_pObj.get()) != m_handles.cend();
    }

    template<class _Obj>
    std::shared_ptr<_Obj> allocate(size_t _cSize, const void* _pOriginal) {
        auto objPtr = std::shared_ptr<_Obj>((_Obj *)std::calloc(1, _cSize), [](auto p)
        {
        });

        const auto fakePtr = std::reinterpret_pointer_cast<FakeShared>(objPtr);
        m_cache[_getHandle(_pOriginal)] = fakePtr;

        return objPtr;
    }

    friend void* operator new(size_t, Cloner&, const void*);

    // We need a sequence point between allocation and evaluation of constructor arguments
    // in order to cache uninitialized object and prevent infinite recursion on cyclic references.
    template<typename _Obj, typename... _Args>
    std::shared_ptr<_Obj> clone2(const std::shared_ptr<_Obj>& ptr, const _Obj* _pOriginal, _Args&&... __args)
    {
        auto* newPtr = ::new(*this, ptr.get()) _Obj(std::forward<_Args>(__args)...);
        auto objPtr = std::shared_ptr<_Obj>(newPtr, [_pOriginal](auto p)
        {
            std::free (p);
        });
        m_cache[_getHandle(_pOriginal)] = std::reinterpret_pointer_cast<FakeShared>(objPtr);
        return objPtr;
    }

private:
    typedef std::map<int, std::shared_ptr<FakeShared>> Cache;
    typedef std::map<const void*, int> Handles;
    typedef std::multimap<int, const void*> Objects;

    Cache m_cache;
    Handles m_handles;
    Objects m_objects;

    int _getHandle(const void* _pObject);
    void _mergeHandles(int _nHandle, int _nOther);
};

void* operator new(size_t _cSize, Cloner& _cloner, void* _pOriginal);

template<typename _Obj>
inline std::shared_ptr<_Obj> clone(const std::shared_ptr<_Obj>& _obj) {
    Cloner cloner;
    return cloner.get(_obj);
}

// We need a sequence point between allocation and evaluation of constructor arguments
// in order to cache uninitialized object and prevent infinite recursion on cyclic references.
#define NEW_CLONE(_ORIGINAL, _CLONER, ...) \
        ({ \
            const auto ptr = _CLONER.allocate<typename std::remove_const<typename std::remove_reference<decltype(*(_ORIGINAL))>::type>::type>(sizeof(*(_ORIGINAL)), _ORIGINAL); \
            _CLONER.clone2(ptr, _ORIGINAL __VA_OPT__(,) __VA_ARGS__); \
        })

template<class _Comparable>
struct PtrLess {
    bool operator()(const std::shared_ptr<const _Comparable>& _pLhs, const std::shared_ptr<const _Comparable>& _pRhs) const { return *_pLhs < *_pRhs; }
};

#endif /* AUTOPTR_H_ */
