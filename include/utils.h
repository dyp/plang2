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
    struct Counted {
        T * ptr;
        mutable size_t cRefs;

        Counted(T * _ptr) : ptr(_ptr), cRefs(1) {}

//        template<typename Q>
//        Counted(Q * _ptr) : ptr(static_cast<T *>(_ptr)), cRefs(1) {}

        ~Counted() { if (ptr) delete ptr; }
    };

//public:
//    Auto() : m_pObj(new Counted(new T)) {}
    Auto() : m_pObj(new Counted(NULL)) {}
//    Auto(const T & _value) : m_pObj(new Counted(new T(_value))) {}
    Auto(const Auto & _other) : m_pObj(_other.m_pObj) { ++ m_pObj->cRefs; }

    template<typename Q>
    Auto(const Auto<Q> & _other) {
//        Q * q = _other.m_pObj->ptr;
//        T * t = dynamic_cast<T *> (q);
        if (_other.m_pObj == NULL || _other.m_pObj->ptr == NULL || dynamic_cast<T *> (_other.m_pObj->ptr) != NULL)
            m_pObj = (Counted *) _other.m_pObj;
        else
            assert(false);
        ++ m_pObj->cRefs;
    }

    Auto(T * _ptr) : m_pObj(new Counted(_ptr)) {}
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

    bool operator <(const Auto & _other) const {
        return (m_pObj && _other.m_pObj) ? (* ptr() < * _other.ptr()) : (m_pObj < _other.m_pObj);
    }

    bool operator ==(const Auto & _other) const {
        return (m_pObj && _other.m_pObj) ? (* ptr() == * _other.ptr()) : (m_pObj == _other.m_pObj);
    }

    bool empty() const { return m_pObj->ptr == NULL; }

//private:
    Counted * m_pObj;

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

template<typename Iterator>
Iterator next(Iterator i) { return ++ i; }

template<typename Iterator>
Iterator prev(Iterator i) { return -- i; }


// String operations.

std::string intToStr(int64_t _n);

std::string strNarrow(const std::wstring & _s);
std::wstring strWiden(const std::string & _s);

std::wstring fmtQuote(const std::wstring & _s);
std::wstring fmtAddParens(const std::wstring & _s, bool _bAddLeadingSpace = true);
std::wstring fmtAddDetail(const std::wstring & _s);

std::wstring fmtInt(int64_t _n, const wchar_t * _fmt = L"%lld");


// Command-line args.

typedef bool (*ArgHandler)(const std::string &_val, void *_p);

struct Option {
	const char *strName;
	char chNameShort;
	ArgHandler argParser;
	bool *pbFlag;
	std::string *pstrValue;
	bool bMultiple;
};

bool parseOptions(size_t _cArgs, const char **_pArgs, Option *_pOptions, void *_pParam,
		ArgHandler _notAnOptionHandler);

#endif /* UTILS_H_ */
