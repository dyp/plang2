/// \file utils.h
/// Miscellaneous utility functions.


#ifndef UTILS_H_
#define UTILS_H_

#include <string>
#include <stdint.h>
#include <assert.h>

template<typename T>
class Auto {
public:
    struct counted_t {
        T * ptr;
        mutable size_t cRefs;

        counted_t(T * _ptr) : ptr(_ptr), cRefs(1) {}

//        template<typename Q>
//        counted_t(Q * _ptr) : ptr(static_cast<T *>(_ptr)), cRefs(1) {}

        ~counted_t() { if (ptr) delete ptr; }
    };

//public:
//    Auto() : m_pObj(new counted_t(new T)) {}
    Auto() : m_pObj(new counted_t(NULL)) {}
//    Auto(const T & _value) : m_pObj(new counted_t(new T(_value))) {}
    Auto(const Auto & _other) : m_pObj(_other.m_pObj) { ++ m_pObj->cRefs; }

    template<typename Q>
    Auto(const Auto<Q> & _other) {
//        Q * q = _other.m_pObj->ptr;
//        T * t = dynamic_cast<T *> (q);
        if (_other.m_pObj == NULL || _other.m_pObj->ptr == NULL || dynamic_cast<T *> (_other.m_pObj->ptr) != NULL)
            m_pObj = (counted_t *) _other.m_pObj;
        else
            assert(false);
        ++ m_pObj->cRefs;
    }

    Auto(T * _ptr) : m_pObj(new counted_t(_ptr)) {}
    ~Auto() { release(); }

    Auto & operator =(const Auto & _other) {
        if (& _other != this) {
            release();
            m_pObj = _other.m_pObj;
            ++ m_pObj->cRefs;
        }
        return * this;
    }

    T * ptr() const { return m_pObj->ptr; };

    //operator T *() const { return m_pObj->ptr; };
    T & operator *() { return * m_pObj->ptr; };
    const T & operator *() const { return * m_pObj->ptr; };
    T * operator ->() const { return m_pObj->ptr; };
    /*operator T &() { return * m_pObj->ptr; };
    operator const T &() const { return * m_pObj->ptr; };*/

    bool operator ==(const Auto & _other) const { return m_pObj == _other.m_pObj; };

    bool empty() const { return m_pObj->ptr == NULL; }

//private:
    counted_t * m_pObj;

    void release() {
        -- m_pObj->cRefs;
        if (m_pObj->cRefs == 0)
            delete m_pObj;
    }
};
/*
#ifndef min
#define min(X, Y) (((X) < (Y)) ? (X) : (Y))
#endif

#ifndef max
#define max(X, Y) (((X) > (Y)) ? (X) : (Y))
#endif
*/
std::string intToStr(int64_t _n);

std::string strNarrow(const std::wstring & _s);
std::wstring strWiden(const std::string & _s);

std::wstring fmtQuote(const std::wstring & _s);
std::wstring fmtAddParens(const std::wstring & _s, bool _bAddLeadingSpace = true);
std::wstring fmtAddDetail(const std::wstring & _s);

std::wstring fmtInt(int64_t _n, const wchar_t * _fmt = L"%lld");

#endif /* UTILS_H_ */
