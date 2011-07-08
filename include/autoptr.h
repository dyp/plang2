/// \file utils.h
/// Miscellaneous utility functions.

#ifndef AUTOPTR_H_
#define AUTOPTR_H_

#include <string>
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

    friend class Parser; // Debug.

protected:
    Counted();

private:
    mutable const void *m_pCountedPtr;
    mutable int m_nCountedRefs;
    static void *m_pLastAlloc;

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

            m_pObj = NULL;
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

#endif /* AUTOPTR_H_ */
