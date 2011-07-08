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

struct StatusMessage {
    enum { Warning, Error };
    int kind;
    lexer::Token where;
    std::wstring str;

    StatusMessage() : kind(0) {}
    StatusMessage(int _kind, const lexer::Token & _where, const std::wstring & _str)
        : kind(_kind), where(_where), str(_str) {}
};

std::wostream & operator << (std::wostream & _os, const StatusMessage & _msg);

typedef std::list<StatusMessage> StatusMessages;

/// Compiler directive.
/// Following pragmas are supported: int_bitness, real_bitness, overflow.
class Pragma : public ir::Node {
public:
    /// Kind of compiler directive.
    /// Currently only bit size of primitive types and overflow handling can be
    /// specified. Passed as a parameter to Pragma constructor.
    enum {
        IntBitness  = 0x01, ///< pragma(int_bitness, ...)
        RealBitness = 0x02, ///< pragma(real_bitness, ...)
        Overflow    = 0x04, ///< pragma(overflow, ...)
        PragmaMask  = 0xFF
    };

    Pragma() : m_fields(0), m_intBitness(0), m_realBitness(0) {}

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
    ir::Overflow & overflow() { return m_overflow; }

    /// Get overflow strategy.
    /// \return Reference to overflow handling descriptor.
    const ir::Overflow & overflow() const { return m_overflow; }

private:
    int m_fields;
    int m_intBitness;
    int m_realBitness;
    ir::Overflow m_overflow;
};

class Context {
public:
    typedef std::multimap<std::wstring, ir::Predicate *> PredicateMap;
    typedef std::map<std::wstring, ir::NamedValue *> VariableMap;
    typedef std::map<std::wstring, ir::TypeDeclaration *> TypeMap;
    typedef std::map<std::wstring, ir::Label *> LabelMap;
    typedef std::map<std::wstring, ir::Process *> ProcessMap;
    typedef std::map<std::wstring, ir::FormulaDeclaration *> FormulaMap;
    typedef std::multimap<std::wstring, ir::UnionConstructorDeclaration *> ConsMap;
    typedef std::list<ir::Node *> Nodes;

public:
    Context(lexer::Loc _loc, bool _bScope = false)
        : m_loc(_loc), m_bScope(_bScope), m_pChild(NULL), m_pParent(NULL), m_pFailed(NULL),
          m_predicates(NULL), m_variables(NULL), m_types(NULL), m_labels(NULL),
          m_processes(NULL), m_formulas(NULL), m_constructors(NULL), m_bFailed(false), m_pCons(NULL)
    {}

    ~Context();

    Context * getParent() const { return m_pParent; }
    void setParent(Context * _pParent) { m_pParent = _pParent; }

    Context * getChild() const { return m_pChild; }
    void setChild(Context * _pChild) { m_pChild = _pChild; }

    Context * createChild(bool _bScope = false);

    void mergeChildren(bool _bMergeFailed = false);

    template<class _Node>
    inline _Node * attach(_Node * _node);

    lexer::Loc & loc() { return m_loc; }
    lexer::Loc nextLoc() { return next(m_loc); }

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

    Context & operator ++ () { ++ m_loc; return * this; }
    Context & operator -- () { -- m_loc; return * this; }

    bool consume(int _token1, int _token2 = -1, int _token3 = -1, int _token4 = -1);

    const std::wstring & scan(int _nScan = 1, int _nGet = 0);

    void skip(int _nSkip = 1);

    void fmtWarning(const wchar_t * _strFmt, ...);

    void fmtError(const wchar_t * _strFmt, ...);

    const StatusMessages & getMessages() const { return m_messages; }

    bool getPredicates(const std::wstring & _strName, ir::Predicates & _predicates) const;
    ir::Predicate * getPredicate(const std::wstring & _strName) const;
    void addPredicate(ir::Predicate * _pPred);

    ir::NamedValue * getVariable(const std::wstring & _strName, bool _bLocal = false) const;
    void addVariable(ir::NamedValue * _pVar);

    ir::TypeDeclaration * getType(const std::wstring & _strName) const;
    void addType(ir::TypeDeclaration * _pType);

    ir::Label * getLabel(const std::wstring & _strName) const;
    void addLabel(ir::Label * _pLabel);

    ir::Process * getProcess(const std::wstring & _strName) const;
    void addProcess(ir::Process * _pProcess);

    ir::FormulaDeclaration * getFormula(const std::wstring & _strName) const;
    void addFormula(ir::FormulaDeclaration * _pFormula);

    bool getConstructors(const std::wstring & _strName, ir::UnionConstructorDeclarations & _cons) const;
    ir::UnionConstructorDeclaration * getConstructor(const std::wstring & _strName) const;
    void addConstructor(ir::UnionConstructorDeclaration * _pCons);
    bool isConstructor(const std::wstring & _strName) const;

    // Constructor-parsing stuff.
    ir::UnionConstructor * getCurrentConstructor() const { return m_pCons ? m_pCons : (m_pParent ? m_pParent->getCurrentConstructor() : NULL); }
    void setCurrentConstructor(ir::UnionConstructor * _pCons) { m_pCons = _pCons; }

    bool isScope() const { return m_bScope; }

    void fail() { m_bFailed = true; }
    bool failed() const { return m_bFailed; }

    Pragma & getPragma() { return m_pragma; }

    int getIntBits() const;
    int getRealBits() const;
    const ir::Overflow & getOverflow() const;

private:
    lexer::Loc m_loc;
    bool m_bScope;
    Context * m_pChild, * m_pParent, * m_pFailed;
    StatusMessages m_messages;
    PredicateMap * m_predicates;
    VariableMap * m_variables;
    TypeMap * m_types;
    LabelMap * m_labels;
    ProcessMap * m_processes;
    FormulaMap * m_formulas;
    ConsMap * m_constructors;
    Nodes m_nodes;
    bool m_bFailed;
    Pragma m_pragma;
    ir::UnionConstructor * m_pCons;

    void mergeTo(Context * _pCtx, bool _bMergeFailed);
    void cleanAdopted();
};

template<class _Node>
inline _Node * Context::attach(_Node * _node) {
    if (_node && ! _node->getParent())
        m_nodes.push_back(_node);

    return _node;
}

#endif /* PARSER_CONTEXT_H_ */
