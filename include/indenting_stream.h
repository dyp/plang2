/// \file indenting_stream.h
///

#ifndef INDENTING_STREAM_H_
#define INDENTING_STREAM_H_

#include <iostream>
#include <iomanip>
#include <streambuf>
#include <cassert>
#include <algorithm>

template<typename _Char, typename _Traits = std::char_traits<_Char> >
class IndentingStream : public std::basic_ostream<_Char, _Traits> {
    using String = std::basic_string<_Char, _Traits>;
    using Int = typename _Traits::int_type;
    using Parent = std::basic_ostream<_Char, _Traits>;
    static const _Char c_chSpace = _Traits::to_char_type((Int)' ');
    static const _Char c_strDefaultIndentation[];

    class StreamBuf : public std::basic_streambuf<_Char, _Traits> {
    public:
        StreamBuf(std::basic_streambuf<_Char, _Traits> *_pSlave,
                const String &_strIndentation = c_strDefaultIndentation) :
            m_pSlave(_pSlave), m_strIndentation(_strIndentation)
        {
        }

        const String getIndentation() const {
            return m_strIndentation;
        }

        void setIndentation(const String &_strIndentation) {
            m_strIndentation = _strIndentation;
        }

        int getLevel() const {
            return m_nLevel;
        }

        void setLevel(int _nLevel) {
            m_nLevel = _nLevel;
        }

        void modifyLevel(int _nDelta) {
            m_nLevel = std::max(0, m_nLevel + _nDelta);
        }

        void setInline(bool _bEnable) {
            // Do not put separators at the begging and the end of inline sequence.
            if (_bEnable) {
                if (isInline() && m_bLastCharIsPending && m_cColumn > 0) {
                    _sputc(m_chLast);
                    ++m_cColumn;
                }

                m_chLast = c_chSpace;
                m_bLastCharIsPending = false;
            } else if (!_bEnable)
                m_bLastCharIsPending = false;

            m_nInlineCount = std::max(0, m_nInlineCount + (_bEnable ? 1 : -1));
        }

        bool isInline() const {
            return m_nInlineCount > 0 && m_nVerbatimCount == 0;
        }

        void setVerbatim(bool _bEnable) {
            m_nVerbatimCount = std::max(0, m_nVerbatimCount + (_bEnable ? 1 : -1));
        }

    protected:
        virtual Int overflow(Int _c) {
            if (isInline()) {
                if (_isNewLine(_c)) {
                    if (_isSpace(m_chLast))
                        return _c;

                    m_chLast = c_chSpace;
                    m_bLastCharIsPending = true;
                    return _c;
                } else if (_isSpace(m_chLast) && _isSpace(_c))
                    return _c;
                else if (_isSpace(_c)) {
                    m_chLast = c_chSpace;
                    m_bLastCharIsPending = true;
                    return _c;
                } else if (m_bLastCharIsPending) {
                    assert(_isSpace(m_chLast) && !_isNewLine(m_chLast));
                    const Int n = _sputc(m_chLast);

                    if (n == _Traits::eof())
                        return n;

                    ++m_cColumn;
                }

                m_bLastCharIsPending = false;
            }

            if (m_cColumn == 0 && !_isNewLine(_c))
                _indent();

            const Int n = _sputc(_c);

            if (n != _Traits::eof()) {
                m_chLast = _c;

                if (_isNewLine(_c))
                    m_cColumn = 0;
                else
                    ++m_cColumn;
            }

            return n;
        }

        virtual std::streamsize xsputn(const _Char *_pChars,
                std::streamsize _nCount)
        {
            if (_nCount == 0)
                return 0;

            size_t cBegin = 0, cColumn = m_cColumn;
            _Char chLast = m_chLast;
            bool bLastCharIsPending = isInline() && m_bLastCharIsPending;

            auto writePart = [&](size_t _cEnd, std::streamsize &_nWritten) {
                const std::streamsize nLength = _cEnd - cBegin;

                _nWritten = _sputn(_pChars + cBegin, nLength);

                if (_nWritten > 0)
                    m_chLast = _pChars[cBegin + _nWritten - 1];

                if (_nWritten < nLength) {
                    m_cColumn += _nWritten;
                    return false;
                }

                return true;
            };

            std::streamsize nWritten = 0;

            for (size_t cPos = 0; static_cast<std::streamsize>(cPos) < _nCount; ++cPos) {
                _Char c = _pChars[cPos];

                if (isInline()) {
                    if (_isSpace(c)) {
                        if (!writePart(cPos, nWritten))
                            return cBegin + nWritten;

                        bLastCharIsPending |= !_isSpace(chLast);
                        chLast = c_chSpace;
                        cBegin = cPos + 1;
                        m_cColumn += nWritten;
                        continue;
                    } else if (bLastCharIsPending) {
                        _sputc(chLast);
                        ++cColumn;
                        ++m_cColumn;
                    }

                    bLastCharIsPending = false;
                }

                chLast = c;

                if (_isNewLine(c))
                    cColumn = 0;
                else {
                    if (cColumn == 0) {
                        if (!writePart(cPos, nWritten))
                            return cBegin + nWritten;

                        m_cColumn = 0;
                        cBegin = cPos;
                        _indent();
                    }

                    ++cColumn;
                }
            }

            assert(static_cast<std::streamsize>(cBegin) <= _nCount);

            if (static_cast<std::streamsize>(cBegin) == _nCount) {
                m_bLastCharIsPending = bLastCharIsPending;
                m_chLast = chLast;
                return _nCount;
            }

            if (isInline() && bLastCharIsPending)
                _sputc(chLast);

            writePart(_nCount, nWritten);
            m_cColumn = std::max<std::streamsize>(0, cColumn + nWritten - _nCount + cBegin);
            m_bLastCharIsPending = false;

            return cBegin + nWritten;
        }

    private:
        bool _isInline() const {
            return m_nInlineCount > 0 && m_nVerbatimCount == 0;
        }

        static bool _isNewLine(_Char _c) {
            return _c == '\n' || _c == '\r';
        }

        bool _isSpace(_Char _c) {
            return std::isspace(_c, this->getloc()) || _c == (_Char)-1;
        }

        bool _flush() {
            if (m_strPending.empty())
                return true;

            const size_t cWritten = static_cast<size_t>(m_pSlave->sputn(
                    m_strPending.data(), m_strPending.size()));

            if (cWritten < m_strPending.size()) {
                m_strPending = m_strPending.substr(cWritten, std::string::npos);
                return false;
            }

            m_strPending = String();
            return true;
        }

        std::streamsize _sputn(const _Char *_pChars, std::streamsize _cCount) {
            return _flush() ? m_pSlave->sputn(_pChars, _cCount) : 0;
        }

        Int _sputc(_Char _c) {
            return _flush() ? m_pSlave->sputc(_c) : _Traits::eof();
        }

        void _indent() {
            if (m_nVerbatimCount == 0 && !m_strIndentation.empty()) {
                for (int n = 0; n < m_nLevel; ++n)
                    m_strPending += m_strIndentation;

                m_cColumn += m_nLevel*m_strIndentation.size();

                if (m_nLevel > 0)
                    m_chLast = m_strIndentation.back();
            }
        }

        std::basic_streambuf<_Char, _Traits> *m_pSlave;
        String m_strIndentation, m_strPending;
        int m_nLevel = 0, m_nInlineCount = 0, m_nVerbatimCount = 0;
        size_t m_cColumn = 0;
        _Char m_chLast = -1;
        bool m_bLastCharIsPending = false;
    };

    StreamBuf *_rdbuf() const {
        return static_cast<StreamBuf *>(this->rdbuf());
    }

    bool m_bSharedBuffer = false;

public:
    IndentingStream(std::basic_ostream<_Char, _Traits> &_other) :
        std::basic_ostream<_Char, _Traits>(new StreamBuf(_other.rdbuf())) {}

    IndentingStream(IndentingStream<_Char, _Traits> &_other) :
        std::basic_ostream<_Char, _Traits>(_other._rdbuf()),
        m_bSharedBuffer(true) {}

    virtual ~IndentingStream() {
        if (!m_bSharedBuffer)
            delete this->rdbuf();
    }

    template<typename _T>
    IndentingStream &operator <<(const _T &_value) {
        static_cast<std::basic_ostream<_Char, _Traits> &>(*this) << _value;
        return *this;
    }

    IndentingStream &operator <<(IndentingStream &(*_manip)(IndentingStream &)) {
        return _manip(*this);
    }

    IndentingStream &operator <<(std::basic_ostream<_Char, _Traits> &(*_manip)(std::basic_ostream<_Char, _Traits> &)) {
        _manip(*this);
        return *this;
    }

    IndentingStream &operator <<(std::basic_ios<_Char, _Traits> &(*_manip)(std::basic_ios<_Char, _Traits> &)) {
        _manip(*this);
        return *this;
    }

    IndentingStream &operator <<(std::ios_base &(*_manip)(std::ios_base &)) {
        _manip(*this);
        return *this;
    }

    const String getIndentation() const {
        return _rdbuf()->getIndentation();
    }

    void setIndentation(const String &_strIndentation) {
        _rdbuf()->setIndentation(_strIndentation);
    }

    int getLevel() const {
        return _rdbuf()->getLevel();
    }

    void setLevel(int _nLevel) {
        _rdbuf()->setLevel(_nLevel);
    }

    void modifyLevel(int _nDelta) {
        _rdbuf()->modifyLevel(_nDelta);
    }

    void setInline(bool _bEnable) {
        _rdbuf()->setInline(_bEnable);
    }

    bool isInline() const {
        return _rdbuf()->isInline();
    }

    void setVerbatim(bool _bEnable) {
        _rdbuf()->setVerbatim(_bEnable);
    }
};

template<typename _Char, typename _Traits>
const _Char IndentingStream<_Char, _Traits>::c_strDefaultIndentation[] = {
        c_chSpace, c_chSpace, c_chSpace, c_chSpace, 0
};

template<typename _Char, typename _Traits = std::char_traits<_Char> >
IndentingStream<_Char, _Traits> &indent(IndentingStream<_Char, _Traits> &_s) {
    _s.modifyLevel(1);
    return _s;
}

template<typename _Char, typename _Traits = std::char_traits<_Char> >
IndentingStream<_Char, _Traits> &unindent(IndentingStream<_Char, _Traits> &_s) {
    _s.modifyLevel(-1);
    return _s;
}

template<typename _Char, typename _Traits = std::char_traits<_Char> >
struct Indentation {
    std::basic_string<_Char, _Traits> strValue;
};

template<typename _Char, typename _Traits = std::char_traits<_Char> >
Indentation<_Char, _Traits> setIndentation(
        const std::basic_string<_Char, _Traits> &_strValue)
{
    return {_strValue};
}

template<typename _Char, typename _Traits = std::char_traits<_Char> >
IndentingStream<_Char, _Traits> &operator <<(
        IndentingStream<_Char, _Traits> &_os,
        Indentation<_Char, _Traits> _indentation)
{
    _os.setIndentation(_indentation.strValue);
    return _os;
}

struct IndentationLevel {
    int nValue;
};

inline
IndentationLevel setLevel(int _nChars) {
    return {_nChars};
}

template<typename _Char, typename _Traits = std::char_traits<_Char> >
IndentingStream<_Char, _Traits> &operator <<(
        IndentingStream<_Char, _Traits> &_os, IndentationLevel _level)
{
    _os.setLevel(_level.nValue);
    return _os;
}

struct InlineToggle {
    bool bEnable;
};

inline
InlineToggle setInline(bool _bEnable = true) {
    return {_bEnable};
}

template<typename _Char, typename _Traits = std::char_traits<_Char> >
IndentingStream<_Char, _Traits> &operator <<(
        IndentingStream<_Char, _Traits> &_os, InlineToggle _inline)
{
    _os.setInline(_inline.bEnable);
    return _os;
}

struct VerbatimToggle {
    bool bEnable;
};

inline
VerbatimToggle setVerbatim(bool _bEnable = true) {
    return {_bEnable};
}

template<typename _Char, typename _Traits = std::char_traits<_Char> >
IndentingStream<_Char, _Traits> &operator <<(
        IndentingStream<_Char, _Traits> &_os, VerbatimToggle _verbatim)
{
    _os.setVerbatim(_verbatim.bEnable);
    return _os;
}

#endif // INDENTING_STREAM_H_
