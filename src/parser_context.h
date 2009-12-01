/// \file parser_context.h
///


#ifndef PARSER_CONTEXT_H_
#define PARSER_CONTEXT_H_

#include "lexer.h"
#include "ir/declarations.h"

#include <string>
#include <ostream>
#include <map>
#include <set>

struct message_t {
    enum { Warning, Error };
    int kind;
    lexer::CToken where;
    std::wstring str;

    message_t() : kind(0) {}
    message_t(int _kind, const lexer::CToken & _where, const std::wstring & _str)
        : kind(_kind), where(_where), str(_str) {}
};

std::wostream & operator << (std::wostream & _os, const message_t & _msg);

typedef std::list<message_t> messages_t;

/// Compiler directive.
/// Following pragmas are supported: int_bitness, real_bitness, overflow.
class CPragma : public ir::CNode {
public:
    /// Kind of compiler directive.
    /// Currently only bit size of primitive types and overflow handling can be
    /// specified. Passed as a parameter to CPragma constructor.
    enum {
        IntBitness  = 0x01, ///< pragma(int_bitness, ...)
        RealBitness = 0x02, ///< pragma(real_bitness, ...)
        Overflow    = 0x04, ///< pragma(overflow, ...)
        PragmaMask  = 0xFF
    };

    CPragma() : m_fields(0), m_intBitness(0), m_realBitness(0) {}

    /// Check if specific pragma is set.
    /// \param _flag One of #IntBitness, #RealBitness or #Overflow.
    /// \return True if _flag is set, false otherwise.
    bool isSet(int _flag) const { return (m_fields & _flag) != 0; }

    /// Set or unset a flag.
    /// \param _flag One of #IntBitness, #RealBitness or #Overflow.
    /// \param _bSet True if _flag should be set, false otherwise.
    void set(int _flag, bool _bSet) {
        m_fields = (m_fields & ~_flag) | (_bSet ? (PragmaMask & _flag) : 0);
    }

    /// \return Number of bits for int.
    int getIntBitness() const { return m_intBitness; }

    /// \param _bitness Number of bits for int. Only following values
    ///     are valid: Native, Generic, 1, ..., 64.
    void setIntBitness(int _bitness) { set(IntBitness, true); m_intBitness = _bitness; }

    /// \return Number of bits for real.
    int getRealBitness() const { return m_realBitness; }

    /// \param _bitness Number of bits for real. Only following values
    ///     are valid: Native, Generic, 32, 64, 128.
    void setRealBitness(int _bitness) { set(RealBitness, true); m_realBitness = _bitness; }

    /// Get or set overflow strategy.
    /// \return Reference to overflow handling descriptor.
    ir::COverflow & overflow() { return m_overflow; }

    /// Get overflow strategy.
    /// \return Reference to overflow handling descriptor.
    const ir::COverflow & overflow() const { return m_overflow; }

private:
    int m_fields;
    int m_intBitness;
    int m_realBitness;
    ir::COverflow m_overflow;
};

class CContext {
private:
    typedef std::map<std::wstring, ir::CPredicate *> predicate_map_t;
    typedef std::map<std::wstring, ir::CNamedValue *> variable_map_t;
    typedef std::map<std::wstring, ir::CTypeDeclaration *> type_map_t;
    typedef std::map<std::wstring, ir::CLabel *> label_map_t;
    typedef std::map<std::wstring, ir::CProcess *> process_map_t;
    typedef std::map<std::wstring, ir::CFormulaDeclaration *> formula_map_t;
    typedef std::list<ir::CNode *> nodes_t;

public:
    CContext(lexer::loc_t _loc, bool _bScope = false)
        : m_loc(_loc), m_bScope(_bScope), m_pChild(NULL), m_pParent(NULL), m_pFailed(NULL),
          m_predicates(NULL), m_variables(NULL), m_types(NULL), m_labels(NULL),
          m_processes(NULL), m_formulas(NULL), m_bFailed(false)
    {}

    ~CContext();

    CContext * getParent() const { return m_pParent; }
    void setParent(CContext * _pParent) { m_pParent = _pParent; }

    CContext * getChild() const { return m_pChild; }
    void setChild(CContext * _pChild) { m_pChild = _pChild; }

    CContext * createChild(bool _bScope = false);

    void mergeChildren(bool _bMergeFailed = false);

    template<class _Node>
    inline _Node * attach(_Node * _node);

    lexer::loc_t & loc() { return m_loc; }

    const std::wstring & getValue() const { return m_loc->getValue(); }

    int getToken() const { return m_loc->getKind(); }

    bool is(int _t1, int _t2 = -1, int _t3 = -1, int _t4 = -1, int _t5 = -1, int _t6 = -1) const
        { return lexer::is(m_loc, _t1, _t2, _t3, _t4, _t5, _t6); }

    bool nextIs(int _t1, int _t2 = -1, int _t3 = -1, int _t4 = -1, int _t5 = -1, int _t6 = -1) const
        { return lexer::is(next(m_loc), _t1, _t2, _t3, _t4, _t5, _t6); }

    bool in(int _t1, int _t2 = -1, int _t3 = -1, int _t4 = -1, int _t5 = -1, int _t6 = -1) const
        { return lexer::in(m_loc, _t1, _t2, _t3, _t4, _t5, _t6); }

    bool nextIn(int _t1, int _t2 = -1, int _t3 = -1, int _t4 = -1, int _t5 = -1, int _t6 = -1) const
        { return lexer::in(next(m_loc), _t1, _t2, _t3, _t4, _t5, _t6); }

    CContext & operator ++ () { ++ m_loc; return * this; }
    CContext & operator -- () { -- m_loc; return * this; }

    bool consume(int _token1, int _token2 = -1, int _token3 = -1, int _token4 = -1);

    const std::wstring & scan(int _nScan = 1, int _nGet = 0);

    void skip(int _nSkip = 1);

    void fmtWarning(const wchar_t * _strFmt, ...);

    void fmtError(const wchar_t * _strFmt, ...);

    const messages_t & getMessages() const { return m_messages; }

    ir::CPredicate * getPredicate(const std::wstring & _strName) const;
    void addPredicate(ir::CPredicate * _pPred);

    ir::CNamedValue * getVariable(const std::wstring & _strName, bool _bLocal = false) const;
    void addVariable(ir::CNamedValue * _pVar);

    ir::CTypeDeclaration * getType(const std::wstring & _strName) const;
    void addType(ir::CTypeDeclaration * _pType);

    ir::CLabel * getLabel(const std::wstring & _strName) const;
    void addLabel(ir::CLabel * _pLabel);

    ir::CProcess * getProcess(const std::wstring & _strName) const;
    void addProcess(ir::CProcess * _pProcess);

    ir::CFormulaDeclaration * getFormula(const std::wstring & _strName) const;
    void addFormula(ir::CFormulaDeclaration * _pFormula);

    bool isScope() const { return m_bScope; }

    void fail() { m_bFailed = true; }
    bool failed() const { return m_bFailed; }

    CPragma & getPragma() { return m_pragma; }

    int getIntBits() const;
    int getRealBits() const;
    const ir::COverflow & getOverflow() const;

private:
    lexer::loc_t m_loc;
    bool m_bScope;
    CContext * m_pChild, * m_pParent, * m_pFailed;
    messages_t m_messages;
    predicate_map_t * m_predicates;
    variable_map_t * m_variables;
    type_map_t * m_types;
    label_map_t * m_labels;
    process_map_t * m_processes;
    formula_map_t * m_formulas;
    nodes_t m_nodes;
    bool m_bFailed;
    CPragma m_pragma;

    void mergeTo(CContext * _pCtx, bool _bMergeFailed);
    void cleanAdopted();
};

template<class _Node>
inline _Node * CContext::attach(_Node * _node) {
    if (_node && ! _node->getParent())
        m_nodes.push_back(_node);

    return _node;
}

#endif /* PARSER_CONTEXT_H_ */
