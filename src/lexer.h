/// \file lexer.h
///


#ifndef LEXER_H_
#define LEXER_H_

#include <string>
#include <list>
#include <istream>
#include <exception>

namespace lexer {

enum {
    Identifier, Label, Integer, Real, Char, String,

    Plus, Minus, ShiftLeft, ShiftRight, Asterisk, Caret, Slash, Percent, Bang,
    Tilde, Eq, Lt, Lte, Gt, Gte, Ne, Ampersand, Question, Implies, Iff,

    LeftParen, RightParen, LeftBrace, RightBrace, LeftBracket, RightBracket,
    LeftMapBracket, RightMapBracket, LeftListBracket, RightListBracket,

    SingleQuote, DoubleQuote, Comma, Semicolon, Colon, Dot, DoubleDot,
    Ellipsis, Hash, Pipe, DoublePipe, Underscore,

    Inf, Nan, True, False, Nil, In, And, Or, Xor, Forall, Exists,

    Module, Import, Type, Predicate, Formula, Pre, Post, Process, Class,
    Extends, Pragma, Mutable,

    IntType, NatType, RealType, BoolType, CharType, Subtype, Enum, Struct,
    Union, StringType, Seq, Set, Array, Map, List, Var,

    For, Case, Default, If, Else, Switch, Break, While, Receive, Send,
    After, Message, Queue, New, With,

    Eof
};

class CToken {
public:
    CToken(int _kind = -1, int _line = 0, int _col = 0)
        : m_kind(_kind), m_line(_line), m_col(_col)
    {}

    CToken(const CToken & _other) { (* this) = _other; }

    int getKind() const { return m_kind; }

    void setPos(int _line, int _col);
    int getLine() const { return m_line; }
    int getCol() const  { return m_col; }

    void setValue(const std::wstring & _str) { m_value = _str; }
    const std::wstring & getValue() const    { return m_value; }

    CToken & operator =(const CToken & _other);

    bool operator ==(const CToken & _other) const;
    bool operator <(const CToken & _other) const;

private:
    int m_kind, m_line, m_col;
    std::wstring m_value;
};

typedef std::list<CToken> tokens_t;

typedef tokens_t::iterator loc_t;

inline loc_t next(loc_t _loc) { return ++ _loc; }
inline loc_t prev(loc_t _loc) { return -- _loc; }

inline bool is(loc_t _loc, int _token) {
    return _token < 0 || _loc->getKind() == _token;
}

inline bool is(loc_t _loc, int _t1, int _t2) {
    return is(_loc, _t1) && is(next(_loc), _t2);
}

inline bool is(loc_t _loc, int _t1, int _t2, int _t3) {
    return is(_loc, _t1) && is(next(_loc), _t2, _t3);
}

inline bool is(loc_t _loc, int _t1, int _t2, int _t3, int _t4) {
    return is(_loc, _t1) && is(next(_loc), _t2, _t3, _t4);
}

inline bool is(loc_t _loc, int _t1, int _t2, int _t3, int _t4, int _t5) {
    return is(_loc, _t1) && is(next(_loc), _t2, _t3, _t4, _t5);
}

inline bool is(loc_t _loc, int _t1, int _t2, int _t3, int _t4, int _t5, int _t6) {
    return is(_loc, _t1) && is(next(_loc), _t2, _t3, _t4, _t5, _t6);
}

inline bool in(loc_t _loc, int _t) {
    return _loc->getKind() == _t;
}

inline bool in(loc_t _loc, int _t1, int _t2) {
    return in(_loc, _t1) || in(next(_loc), _t2);
}

inline bool in(loc_t _loc, int _t1, int _t2, int _t3) {
    return in(_loc, _t1) || in(_loc, _t2, _t3);
}

inline bool in(loc_t _loc, int _t1, int _t2, int _t3, int _t4) {
    return in(_loc, _t1) || in(_loc, _t2, _t3, _t4);
}

inline bool in(loc_t _loc, int _t1, int _t2, int _t3, int _t4, int _t5) {
    return in(_loc, _t1) || in(_loc, _t2, _t3, _t4, _t5);
}

inline bool in(loc_t _loc, int _t1, int _t2, int _t3, int _t4, int _t5, int _t6) {
    return in(_loc, _t1) || in(_loc, _t2, _t3, _t4, _t5, _t6);
}

class ELexerException : public std::exception {
public:
    ELexerException(const std::string & _message, int _line = 0, int _col = 0)
        : m_message(_message), m_line(_line), m_col(_col)
    {}

    virtual ~ELexerException() throw() {}

    virtual const char * what() const throw() { return m_message.c_str(); }

    int getLine() const { return m_line; }
    int getCol() const { return m_col; }

private:
    const std::string m_message;
    const int m_line, m_col;
};

void tokenize(tokens_t & _tokens, std::istream & _is) throw(ELexerException);

}; // namespace lexer

#endif /* LEXER_H_ */
