/// \file lexer.cpp
///

#include <vector>
#include <algorithm>
#include <stack>
#include <locale>
#include <iostream>

#include <wchar.h>
#include <string.h>
#include <stdio.h>

#include "lexer.h"

namespace lexer {

void CToken::setPos(int _line, int _col) {
    m_line = _line;
    m_col = _col;
}

CToken & CToken::operator =(const CToken & _other) {
    m_kind  = _other.m_kind;
    m_col   = _other.m_col;
    m_line  = _other.m_line;
    m_value = _other.m_value;

    return * this;
}

bool CToken::operator ==(const CToken & _other) const {
    return m_kind  == _other.m_kind && m_col == _other.m_col &&
        m_line  == _other.m_line && m_value == _other.m_value;
}

struct lexeme_t {
    std::wstring lexeme;
    int tokenKind;

    lexeme_t(const std::wstring & _lexeme, int _tokenKind)
        : lexeme(_lexeme), tokenKind(_tokenKind)
    {}

    bool operator <(const lexeme_t & _other) const {
        return lexeme < _other.lexeme;
    }
};

class CLexemeCharCmp {
public:
    CLexemeCharCmp(unsigned int _nChar) : m_nChar (_nChar) {}

    bool operator ()(const lexeme_t & _lhs, const lexeme_t & _rhs) const {
        if (_rhs.lexeme.length() <= m_nChar)
            return false;

        if (_lhs.lexeme.length() <= m_nChar)
            return true;

        return _lhs.lexeme[m_nChar] < _rhs.lexeme[m_nChar];
    }

private:
    unsigned int m_nChar;
};

typedef std::vector<lexeme_t> lexemes_t;

class CTokenMap {
public:
    CTokenMap();

    void resetRequest();
    int addChar(wchar_t _c);

    int getCount() const {
        return m_bounds.second - m_bounds.first;
    }

    const lexeme_t & getLexeme() const {
        return * m_matched;
    }

    bool matched() const {
        return m_matched != m_knownTokens.end();
    }

    std::wstring getUnmatchedChars() const  {
        return matched() ? m_strCurrent.substr(getLexeme().lexeme.length()) : m_strCurrent;
    }

private:
    lexemes_t m_knownTokens;
    std::pair<lexemes_t::iterator, lexemes_t::iterator> m_bounds;
    std::wstring m_strCurrent;
    lexemes_t::iterator m_matched;

    void _add(const std::wstring & _lexeme, int _tokenKind) {
        m_knownTokens.push_back(lexeme_t(_lexeme, _tokenKind));
    }

};

CTokenMap::CTokenMap() {
    _add(L"+",   Plus);
	_add(L"-",   Minus);
	_add(L"<<",  ShiftLeft);
	_add(L">>",  ShiftRight);
	_add(L"*",   Asterisk);
	_add(L"^",   Caret);
	_add(L"/",   Slash);
	_add(L"%",   Percent);
	_add(L"!",   Bang);
	_add(L"~",   Tilde);
	_add(L"=",   Eq);
	_add(L"<",   Lt);
	_add(L"<=",  Lte);
	_add(L">",   Gt);
	_add(L">=",  Gte);
	_add(L"!=",  Ne);
	_add(L"&",   Ampersand);
	_add(L"?",   Question);
	_add(L"=>",  Implies);
	_add(L"<=>", Iff);
	_add(L"(",   LeftParen);
	_add(L")",   RightParen);
	_add(L"{",   LeftBrace);
	_add(L"}",   RightBrace);
	_add(L"[",   LeftBracket);
    _add(L"]",   RightBracket);
	_add(L"[{",  LeftMapBracket);
	_add(L"}]",  RightMapBracket);
	_add(L"[[",  LeftListBracket);
	_add(L"]]",  RightListBracket);
	_add(L"\'",  SingleQuote);
	_add(L"\"",  DoubleQuote);
	_add(L",",   Comma);
	_add(L";",   Semicolon);
	_add(L":",   Colon);
	_add(L".",   Dot);
	_add(L"..",  DoubleDot);
	_add(L"...", Ellipsis);
	_add(L"#",   Hash);
	_add(L"|",   Pipe);
    _add(L"||",  DoublePipe);
    _add(L"_",   Underscore);
	_add(L"inf",   Inf);
	_add(L"nan",   Nan);
	_add(L"true",  True);
	_add(L"false", False);
	_add(L"nil",   Nil);
	_add(L"in",    In);
	_add(L"or",    Or);
    _add(L"xor",   Xor);
    _add(L"forall",    Forall);
    _add(L"exists",    Exists);
	_add(L"module",    Module);
	_add(L"import",    Import);
	_add(L"type",      Type);
	_add(L"predicate", Predicate);
	_add(L"formula",   Formula);
	_add(L"pre",       Pre);
	_add(L"post",      Post);
	_add(L"process",   Process);
	_add(L"class",     Class);
	_add(L"extends",   Extends);
	_add(L"pragma",    Pragma);
	_add(L"mutable",   Mutable);
    _add(L"int",       IntType);
    _add(L"nat",       NatType);
    _add(L"real",      RealType);
    _add(L"bool",      BoolType);
    _add(L"char",      CharType);
	_add(L"subtype",   Subtype);
	_add(L"enum",      Enum);
	_add(L"struct",    Struct);
	_add(L"union",     Union);
	_add(L"string",    StringType);
	_add(L"seq",       Seq);
	_add(L"set",       Set);
	_add(L"array",     Array);
	_add(L"map",       Map);
    _add(L"list",      List);
    _add(L"var",       Var);
	_add(L"for",       For);
	_add(L"case",      Case);
	_add(L"default",   Default);
	_add(L"if",        If);
	_add(L"else",      Else);
	_add(L"switch",    Switch);
	_add(L"break",     Break);
	_add(L"while",     While);
	_add(L"receive",   Receive);
	_add(L"send",      Send);
	_add(L"after",     After);
	_add(L"message",   Message);
	_add(L"queue",     Queue);
	_add(L"new",       New);
	_add(L"with",      With);

	std::sort(m_knownTokens.begin(), m_knownTokens.end());
	resetRequest();
}

void CTokenMap::resetRequest() {
    m_bounds.first = m_knownTokens.begin();
    m_bounds.second = m_knownTokens.end();
    m_strCurrent = L"";
    m_matched = m_knownTokens.end();
}

int CTokenMap::addChar(wchar_t _c) {
    m_strCurrent += _c;

    if (getCount() == 1 && m_bounds.first->lexeme[m_strCurrent.size() - 1] != _c) {
        m_bounds.first = m_bounds.second;
        return 0;
    }

    m_bounds = std::equal_range(m_bounds.first, m_bounds.second,
            lexeme_t(m_strCurrent, -1), CLexemeCharCmp(m_strCurrent.length() - 1));

    const int count = m_bounds.second - m_bounds.first;

    if (count == 0)
        return 0;

    if (m_bounds.first->lexeme.length() == m_strCurrent.length())
        m_matched = m_bounds.first;

    return count;
}

class CTokenizer {
public:
    CTokenizer(tokens_t & _tokens, std::istream & _is);

    void run();

private:
    typedef std::codecvt<wchar_t, char, std::mbstate_t> wc2mbcs_t;
    typedef std::ctype<wchar_t> wctype_t;

    tokens_t & m_tokens;
    std::istream & m_is;
    CTokenMap m_tm;

    std::locale m_loc;
    std::mbstate_t m_state;
    const wc2mbcs_t & m_facet;
    wc2mbcs_t::result m_cvtresult;
    int m_mbcLen;
    char m_mbcBuf[32];
    const wctype_t & m_ctype;

    int m_nLine;
    int m_nCol;
    std::stack<int> m_colCount;

    wchar_t _get();
    wchar_t _peek();
    std::istream & _putback(wchar_t _c);
    void _putbackStr(const std::wstring & _s);

    bool _matchToken(CToken & _tok);
    void _skipWhitespaceAndComments();
    void _skipWhitespace();
    bool _skipComments();

    bool _matchIdentifier(CToken & _tok);
    bool _matchLabel(CToken & _tok);
    bool _matchIntegerLiteral(CToken & _tok);
    bool _matchRealLiteral(CToken & _tok);
    bool _matchCharLiteral(CToken & _tok);
    bool _matchStringLiteral(CToken & _tok);
    wchar_t _parseSingleChar(std::wstring & _charsRead);

    wchar_t _mbc2wc(const char * _mbcs, int _len);
    int _wc2mbc(char * _mbcs, int _len, wchar_t _wc);
    bool _isIdentChar(wchar_t _c);
};

CTokenizer::CTokenizer(tokens_t & _tokens, std::istream & _is) :
    m_tokens(_tokens), m_is(_is),
    m_loc(""), m_facet(std::use_facet<wc2mbcs_t>(m_loc)),
    m_mbcLen(m_facet.max_length()), m_ctype(std::use_facet<wctype_t>(m_loc)),
    m_nLine(1), m_nCol(1)
{
}

wchar_t CTokenizer::_mbc2wc(const char * _mbcs, int _len) {
    const char * pNextIn = _mbcs;
    wchar_t * pNextOut;
    wchar_t wc;

    memset((void *) & m_state, 0, sizeof(m_state));

    m_cvtresult = m_facet.in(m_state, _mbcs, _mbcs + _len, pNextIn,
            & wc, & wc + 1, pNextOut);

    if (m_cvtresult != wc2mbcs_t::ok || pNextOut == & wc)
        return EOF;

    return wc;
}

int CTokenizer::_wc2mbc(char * _mbcs, int _len, wchar_t _wc) {
    char * pNextOut = _mbcs;
    const wchar_t * pNextIn = & _wc;

    memset((void *) & m_state, 0, sizeof(m_state));

    m_cvtresult = m_facet.out(m_state, & _wc, (& _wc) + 1, pNextIn,
            (char *) _mbcs, (char *) _mbcs + _len, pNextOut);

    return pNextOut - _mbcs;
}

wchar_t CTokenizer::_get() {
    char * pBuf = m_mbcBuf;
    wchar_t wc = EOF;

    do {
        const int bt = m_is.get();

        if (bt == EOF || m_is.eof())
            break;

        * (pBuf ++) = bt;
        wc = _mbc2wc(m_mbcBuf, pBuf - m_mbcBuf);
    } while ((wc == EOF || m_cvtresult == wc2mbcs_t::partial) && pBuf - m_mbcBuf < m_mbcLen);

    if (m_cvtresult != wc2mbcs_t::ok) {
        m_is.clear();
        while ((pBuf --) != m_mbcBuf)
            m_is.putback(* pBuf);
        m_is.setstate(std::istream::eofbit);
        return EOF;
    }

    if (wc == L'\n') {
        m_colCount.push(m_nCol);
        ++ m_nLine;
        m_nCol = 1;
    } else {
        ++ m_nCol;
    }

    return wc;
}

wchar_t CTokenizer::_peek() {
    const int c = m_is.peek();

    if (c == EOF || m_is.eof())
        return EOF;

    char chr = c;
    wchar_t wc = _mbc2wc(& chr, 1);

    if (wc == EOF) {
        wc = _get();
        _putback(wc);
    }

    return wc;
}

std::istream & CTokenizer::_putback(wchar_t _c) {
    m_is.clear();

    const int count = _wc2mbc(m_mbcBuf, m_mbcLen, _c);

    if (count == 0 || m_cvtresult != wc2mbcs_t::ok) {
        m_is.setstate(std::istream::failbit);
        return m_is;
    }

    for (int i = 0; i < count; ++ i)
        m_is.putback(m_mbcBuf[count - i - 1]);

    if (m_is.fail())
        return m_is;

    if (_c == L'\n') {
        -- m_nLine;
        m_nCol = m_colCount.top();
        m_colCount.pop();
    } else {
        -- m_nCol;
    }

    return m_is;
}

bool CTokenizer::_isIdentChar(wchar_t _c) {
    return _c == L'_' || m_ctype.is(wctype_t::alnum, _c);
}

void CTokenizer::_putbackStr(const std::wstring & _s) {
    if (_s.empty())
        return;

    for (int i = _s.length() - 1; i >= 0; -- i)
        _putback(_s[i]);
}

void CTokenizer::_skipWhitespace() {
    while (! m_is.eof() && m_ctype.is(wctype_t::space, _peek()))
        _get();
}

bool CTokenizer::_skipComments() {
    if (m_is.eof())
        return false;

    const int nCol = m_nCol;
    const int nLine = m_nLine;

    const wchar_t c = _get();
    const wchar_t d = _peek();

    if (c != L'/' || (d != L'*' && d != L'/')) {
        _putback(c);
        return false;
    }

    if (d == L'/') {
        // Single-line comment.
        while (! m_is.eof() && _peek() != L'\n' && _peek() != L'\r')
            _get();
        return true;
    }

    // Multiline comment.
    _get(); // Asterisk.

    while (! m_is.eof()) {
        const wchar_t c = _get();

        if (c == L'*' && _peek() == L'/') {
            _get(); // Slash.
            return true;
        }
    }

    throw ELexerException("Unterminated comment", nLine, nCol);
}

void CTokenizer::_skipWhitespaceAndComments() {
    while (! m_is.eof()) {
        const wchar_t c = _peek();

        if (m_ctype.is(wctype_t::space, c))
            _skipWhitespace();
        else if (c == L'/') {
            if (! _skipComments())
                break;
        } else
            break;
    }
}

bool CTokenizer::_matchIdentifier(CToken & _tok) {
    const wchar_t c = _peek();

    if (! m_ctype.is(wctype_t::alpha, c) && c != L'_')
        return false;

    std::wstring s;

    do {
        s += _get();
    } while (! m_is.eof() && _isIdentChar(_peek()));

    _tok = CToken(Identifier);
    _tok.setValue(s);

    return true;
}

bool CTokenizer::_matchLabel(CToken & _tok) {
    if (! m_ctype.is(wctype_t::digit, _peek()))
        return false;

    std::wstring s;
    bool bHasLettersOrUnderscores = false;
    bool bValidHex = true;

    while (! m_is.eof() && _isIdentChar(_peek())) {
        const wchar_t c = _get();
        s += c;
        bHasLettersOrUnderscores = bHasLettersOrUnderscores || c == L'_' || m_ctype.is(wctype_t::alpha, c);
        bValidHex = bValidHex && ((s.length() < 3 && c == L"0x"[s.length() - 1]) || m_ctype.is(wctype_t::xdigit, c));
    }

    if (s.empty() || ! bHasLettersOrUnderscores || bValidHex) {
        _putbackStr(s);
        return false;
    }

    _tok = CToken(Label);
    _tok.setValue(s);

    return true;
}

bool CTokenizer::_matchIntegerLiteral(CToken & _tok) {
    if (! m_ctype.is(wctype_t::digit, _peek()))
        return false;

    std::wstring s;

    s += _get();

    if (s[0] == L'0' && _peek() == L'x') {
        do {
            s += _get();
        } while (! m_is.eof() && m_ctype.is(wctype_t::xdigit, _peek()));

        if (s.length() == 2) {
            _putbackStr(s);
            return false;
        }
    } else {
        while (! m_is.eof() && m_ctype.is(wctype_t::digit, _peek()))
            s += _get();
    }

    _tok = CToken(Integer);
    _tok.setValue(s);

    return true;
}

bool CTokenizer::_matchRealLiteral(CToken & _tok) {
    if (! m_ctype.is(wctype_t::digit, _peek()) && _peek() != L'.')
        return false;

    std::wstring s;
    bool bExponent = false;
    bool bFraction = false;

    do {
        const wchar_t c = _get();

        if (c == L'.') {
            const wchar_t d = _peek();
            if (bFraction || bExponent || d == L'.' || ((d == L'e' || d == L'E') && s.empty())) {
                _putback(c);
                break;
            }

            bFraction = true;
            s += c;
        } else if (c == L'e' || c == L'E') {
            if (bExponent) {
                _putback(c);
                break;
            }

            if (_peek() == L'-' || _peek() == L'+') {
                const wchar_t d = _get();

                if (! m_ctype.is(wctype_t::digit, _peek())) {
                    _putback(d);
                    _putback(c);
                    break;
                } else {
                    bExponent = true;
                    s += c;
                    s += d;
                }
            } else if (! m_ctype.is(wctype_t::digit, _peek())) {
                _putback(c);
                break;
            } else {
                bExponent = true;
                s += c;
            }
        } else if (m_ctype.is(wctype_t::digit, c)) {
            s += c;
        } else {
            _putback(c);
            break;
        }
    } while (! m_is.eof());

    if ((s.size() <= (bFraction ? 1 : 0)) || ! (bFraction || bExponent)) {
        _putbackStr(s);
        return false;
    }

    _tok = CToken(Real);
    _tok.setValue(s);

    return true;
}

wchar_t CTokenizer::_parseSingleChar(std::wstring & _charsRead) {
    _charsRead += _peek();

    if (_peek() != L'\\')
        return _get();

    _get(); // Back slash.

    // fragment ESCAPED_CHAR  : '\\' ('\'' | '\"' | '\\' | '0' | 'n' | 't' | 'r');
    // fragment CHAR_CODE     : '\\x' HEX_DIGIT (HEX_DIGIT (HEX_DIGIT HEX_DIGIT?)?)?;

    const wchar_t d = _get();
    std::wstring s;

    _charsRead += d;

    switch (d) {
        case L'\\': return L'\\';
        case L'\'': return L'\'';
        case L'\"': return L'\"';
        case L'0': return L'\0';
        case L'n': return L'\n';
        case L't': return L'\t';
        case L'r': return L'\r';
        case L'x':
            for (int i = 0; i < 4 && m_ctype.is(wctype_t::xdigit, _peek()); ++ i)
                s += _get();

            if (s.empty())
                break;

            _charsRead += s;

            return wcstol(s.data(), NULL, 16);
    }

    _charsRead.resize(_charsRead.length() - 2);

    _putback(d);
    _putback(L'\\');

    return -1;
}

bool CTokenizer::_matchCharLiteral(CToken & _tok) {
    if (_peek() != L'\'')
        return false;

    _get(); // Quote.

    std::wstring charsRead;
    const wchar_t c = _parseSingleChar(charsRead);

    if (c == EOF || _peek() != L'\'') {
        _putbackStr(charsRead);
        _putback(L'\'');
        return false;
    }

    _get(); // Quote.

    _tok = CToken(Char);
    _tok.setValue(std::wstring(& c, 1));

    return true;
}

bool CTokenizer::_matchStringLiteral(CToken & _tok) {
    if (_peek() != L'\"')
        return false;

    _get(); // Quote.

    std::wstring s;
    std::wstring charsRead;

    while (! m_is.eof() && _peek() != L'\"') {
        const wchar_t c = _parseSingleChar(charsRead);

        if (c == -1)
            break;

        s += c;
    }

    if (_peek() != L'\"') {
        _putbackStr(charsRead);
        _putback(L'\"');
        return false;
    }

    _get(); // Quote.

    _tok = CToken(String);
    _tok.setValue(s);

    return true;
}

void CTokenizer::run() {
    CToken tok;

    while (! m_is.eof()) {
        _skipWhitespaceAndComments();

        const int nLine = m_nLine, nCol = m_nCol;
        const wchar_t c = _peek();
        bool bMatched = false;

        if (m_is.eof() || c == EOF)
            break;

        if (c == L'\'')
            bMatched = _matchCharLiteral(tok);
        else if (c == L'\"')
            bMatched = _matchStringLiteral(tok);
        else if (c == L'.')
            bMatched = _matchRealLiteral(tok);
        else if (m_ctype.is(wctype_t::digit, c))
            bMatched = _matchLabel(tok) || _matchRealLiteral(tok) ||
                _matchIntegerLiteral(tok);

        if (! bMatched && _matchToken(tok)) {
            const wchar_t lastChar = * tok.getValue().rbegin();

            // Check if separator is absent.
            if (_isIdentChar(lastChar) && _isIdentChar(_peek())) {
                bMatched = false;
                _putbackStr(tok.getValue());
            } else {
                bMatched = true;
            }
        }

        if (! bMatched && (m_ctype.is(wctype_t::alpha, c) || c == L'_'))
            bMatched = _matchIdentifier(tok);

        if (! bMatched) {
            char buf[9], msg[64];

            m_is.getline(buf, 9);
            snprintf(msg, 64, "Unknown token starting with: %s... (char code %d)", buf, c);

            throw ELexerException(msg, m_nLine, m_nCol);
        }

        tok.setPos(nLine, nCol);
        m_tokens.push_back(tok);
    }

    m_tokens.push_back(CToken(Eof, m_nLine, m_nCol));
}

bool CTokenizer::_matchToken(CToken & _tok) {
    m_tm.resetRequest();

    while (! m_is.eof() && m_tm.getCount() > 0)
        m_tm.addChar(_get());

    _putbackStr(m_tm.getUnmatchedChars());

    if (m_tm.matched()) {
        const lexeme_t & lex = m_tm.getLexeme();
        _tok = CToken(lex.tokenKind);
        _tok.setValue(lex.lexeme);
    }

    return m_tm.matched();
}

void tokenize(tokens_t & _tokens, std::istream & _is) throw(ELexerException) {
    CTokenizer tok(_tokens, _is);
    tok.run();
}

}; // namespace lexer
