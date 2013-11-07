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
    IDENTIFIER, LABEL, INTEGER, REAL, CHAR, STRING,

    PLUS, MINUS, SHIFTLEFT, SHIFTRIGHT, ASTERISK, CARET, SLASH, PERCENT, BANG,
    TILDE, EQ, LT, LTE, GT, GTE, NE, AMPERSAND, QUESTION, IMPLIES, IFF,

    LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET,
    MAP_LBRACKET, MAP_RBRACKET, LIST_LBRACKET, LIST_RBRACKET,

    SINGLE_QUOTE, DOUBLE_QUOTE, COMMA, SEMICOLON, COLON, DOT, DOUBLE_DOT,
    ELLIPSIS, HASH, PIPE, DOUBLE_PIPE, UNDERSCORE,

    INF, NOT_A_NUMBER, TRUE, FALSE, NIL, IN, AND, OR, XOR, FORALL, EXISTS,

    MODULE, IMPORT, AS, TYPE, PREDICATE, FORMULA, LEMMA, MEASURE, PRE, POST,
    PROCESS, CLASS, EXTENDS, PRAGMA, MUTABLE, VALID, INVALID, UNKNOWN,

    INT_TYPE, NAT_TYPE, REAL_TYPE, BOOL_TYPE, CHAR_TYPE, SUBTYPE, ENUM, STRUCT,
    UNION, STRING_TYPE, SEQ, SET, ARRAY, MAP, LIST, VAR,

    FOR, CASE, DEFAULT, IF, ELSE, SWITCH, BREAK, WHILE, RECEIVE, SEND,
    AFTER, MESSAGE, QUEUE, NEW, WITH,

    END_OF_FILE
};

class Token {
public:
    Token(int _kind = -1, int _line = 0, int _col = 0)
        : m_kind(_kind), m_line(_line), m_col(_col)
    {}

    Token(const Token & _other) { (* this) = _other; }

    int getKind() const { return m_kind; }

    void setPos(int _line, int _col);
    int getLine() const { return m_line; }
    int getCol() const  { return m_col; }

    bool hasLeadingSpace() const { return m_bHasLeadingSpace; }
    void setLeadingSpace(bool _bValue) { m_bHasLeadingSpace = _bValue; }

    void setValue(const std::wstring & _str) { m_value = _str; }
    const std::wstring & getValue() const    { return m_value; }

    Token & operator =(const Token & _other);

    bool operator ==(const Token & _other) const;
    bool operator <(const Token & _other) const;

private:
    int m_kind, m_line, m_col;
    std::wstring m_value;
    bool m_bHasLeadingSpace = false;
};

typedef std::list<Token> Tokens;

typedef Tokens::iterator Loc;

inline Loc next(Loc _loc) { return ++ _loc; }
inline Loc prev(Loc _loc) { return -- _loc; }

inline bool is(Loc _loc, int _token) {
    return _token < 0 || _loc->getKind() == _token;
}

inline bool is(Loc _loc, int _t1, int _t2) {
    return is(_loc, _t1) && is(next(_loc), _t2);
}

inline bool is(Loc _loc, int _t1, int _t2, int _t3) {
    return is(_loc, _t1) && is(next(_loc), _t2, _t3);
}

inline bool is(Loc _loc, int _t1, int _t2, int _t3, int _t4) {
    return is(_loc, _t1) && is(next(_loc), _t2, _t3, _t4);
}

inline bool is(Loc _loc, int _t1, int _t2, int _t3, int _t4, int _t5) {
    return is(_loc, _t1) && is(next(_loc), _t2, _t3, _t4, _t5);
}

inline bool is(Loc _loc, int _t1, int _t2, int _t3, int _t4, int _t5, int _t6) {
    return is(_loc, _t1) && is(next(_loc), _t2, _t3, _t4, _t5, _t6);
}

inline bool in(Loc _loc, int _t) {
    return _loc->getKind() == _t;
}

inline bool in(Loc _loc, int _t1, int _t2) {
    return in(_loc, _t1) || in(next(_loc), _t2);
}

inline bool in(Loc _loc, int _t1, int _t2, int _t3) {
    return in(_loc, _t1) || in(_loc, _t2, _t3);
}

inline bool in(Loc _loc, int _t1, int _t2, int _t3, int _t4) {
    return in(_loc, _t1) || in(_loc, _t2, _t3, _t4);
}

inline bool in(Loc _loc, int _t1, int _t2, int _t3, int _t4, int _t5) {
    return in(_loc, _t1) || in(_loc, _t2, _t3, _t4, _t5);
}

inline bool in(Loc _loc, int _t1, int _t2, int _t3, int _t4, int _t5, int _t6) {
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

void tokenize(Tokens & _tokens, std::istream & _is) throw(ELexerException);

}; // namespace lexer

#endif /* LEXER_H_ */
