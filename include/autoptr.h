/// \file utils.h
/// Miscellaneous utility functions.

#ifndef AUTOPTR_H_
#define AUTOPTR_H_

#include <string>
#include <map>
#include <stack>
#include <stdint.h>
#include <assert.h>

// Smart pointer.

struct Counted {
    virtual ~Counted() {}

    // Increment ref count (if dynamically allocated.)
    // \param _pObj can be NULL, hence method is static (checking this != NULL is ugly).
    static void ref(const Counted *_pObj);

    // Decrement ref count (if dynamically allocated.)
    // \returns true if object was dynamically allocated and count reaches zero.
    bool unref() const;

    // Only dynamically allocated objects' references are tracked.
    // Also, when using multiple inheritance and wanting to benefit from inheriting from Counted,
    //      Counted or it's descendant must be listed first in the base list.
    void *operator new(size_t _c, bool _bAcquire = true);

    // Create wrapper with counter or use the object itself if it already got one.
    static const Counted *createCountedWrapper(const void *_pObj, bool _bAcquire);
    static const Counted *createCountedWrapper(const Counted *_pObj, bool /* _bAcquire */);

    template<class>
    friend class Auto;

    friend class Cloner;

protected:
    Counted();

private:
    mutable const void *m_pCountedPtr;
    mutable int m_nCountedRefs;
    static std::stack<void *> m_allocs;

    static bool _isManaged(const void *_pObj, bool _bPop = true);

    Counted(const void *_ptr);
};

template<typename _Obj>
class Auto {
public:
    Auto() : m_pObj(NULL) {}

    Auto(const Auto &_other) : m_pObj(_other.m_pObj) {
        Counted::ref(m_pObj);
    }

    template<typename _Other>
    Auto(const Auto<_Other> &_other) : m_pObj(NULL) {
        // Condition here serves to statically check compatibility of _Obj* vs. _Other*.
        if (_Obj *pOther = _other.ptr()) {
            m_pObj = _other.m_pObj;
            Counted::ref(m_pObj);
        }
    }

    // No Auto(void *) constructor is provided to prevent accidentally
    // calling delete on stack-allocated non-counted object, etc.
    Auto(const Counted *_pObj) : m_pObj(_pObj) {
        Counted::ref(m_pObj);
    }

    ~Auto() {
        _release();
    }

    template<typename _Other>
    Auto<_Other> as() const {
        return Auto<_Other>(m_pObj);
    }

    _Obj *ptr() const {
        return m_pObj != NULL ? (_Obj *)m_pObj->m_pCountedPtr : NULL;
    }

    Auto &operator =(const Auto &_other) {
        if (_other.ptr() != ptr()) {
            _release();
            m_pObj = _other.m_pObj;
            Counted::ref(m_pObj);
        }

        return *this;
    }

    _Obj &operator *() const {
        return *ptr();
    }

    _Obj *operator ->() const {
        return ptr();
    }

    template<typename _Other>
    bool operator <(const Auto<_Other> &_other) const {
        return (void *)ptr() < (void *)_other.ptr();
    }

    template<typename _Other>
    bool operator ==(const Auto<_Other> &_other) const {
        return (void *)ptr() == (void *)_other.ptr();
    }

    bool operator !() const {
        return ptr() == NULL;
    }

    operator bool() const {
        return ptr() != NULL;
    }

    template<class>
    friend class Auto;

    friend class Cloner;

private:
    const Counted *m_pObj;

    void _release() {
        if (m_pObj != NULL && !m_pObj->unref()) {
            if (m_pObj->m_pCountedPtr != m_pObj) {
                if (m_pObj->m_pCountedPtr)
                    delete (_Obj *)m_pObj->m_pCountedPtr;
                delete m_pObj;
            } else
                delete (_Obj *)m_pObj;
        }
    }
};

template<typename _Obj>
inline Auto<_Obj> ptr(const _Obj *_pObj) {
    return Auto<_Obj>(Counted::createCountedWrapper(_pObj, true));
}

template<typename _Obj>
inline Auto<_Obj> ref(const _Obj *_pObj) {
    return Auto<_Obj>(Counted::createCountedWrapper(_pObj, false));
}

// Cloner.

class Cloner {
public:
    template<class _Obj>
    Auto<_Obj> get(const _Obj *_pObj) {
        if (_pObj == NULL)
            return NULL;

        Cache::iterator iObj = m_cache.find(_pObj);

        if (iObj != m_cache.end())
            return iObj->second;

        Auto<_Obj> pClone = _pObj->clone(*this).template as<_Obj>();

        pClone.m_pObj->unref();

        return pClone;
    }

    template<class _Obj>
    Auto<_Obj> get(const Auto<_Obj> &_pObj) {
        return get(_pObj.ptr());
    }

    void *allocate(size_t _cSize, const void *_pOriginal);
    void *allocate(size_t _cSize, const Counted *_pOriginal);

    friend void *operator new(size_t, Cloner &, const void *);

private:
    typedef std::map<const void *, const Counted *> Cache;

    Cache m_cache;
};

void *operator new(size_t _cSize, Cloner &_cloner, const void *_pOriginal);

template<typename _Obj>
inline Auto<_Obj> clone(const _Obj &_obj) {
    Cloner cloner;
    return cloner.get(&_obj);
}

// We need a sequence point between allocation and evaluation of constructor arguments
// in order to cache uninitialized object and prevent infinite recursion on cyclic references.
#define NEW_CLONE(_ORIGINAL, _CLONER, _CTOR) \
    ((_CLONER).allocate(sizeof(*_ORIGINAL), _ORIGINAL), ::new((_CLONER), _ORIGINAL) _CTOR)

#endif /* AUTOPTR_H_ */
