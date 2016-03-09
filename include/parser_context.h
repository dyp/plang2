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
    StatusMessage(int _kind, const lexer::Token &_where, const std::wstring &_str)
        : kind(_kind), where(_where), str(_str) {}
};

std::wostream &operator << (std::wostream &_os, const StatusMessage &_msg);

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
    ir::Overflow &overflow() { return m_overflow; }

    /// Get overflow strategy.
    /// \return Reference to overflow handling descriptor.
    const ir::Overflow &overflow() const { return m_overflow; }

private:
    int m_fields;
    int m_intBitness;
    int m_realBitness;
    ir::Overflow m_overflow;
};

namespace tc {
class FreshType;
}

namespace ir {

class Context {
public:
    // Parsing constants.
    enum {
        ALLOW_FORMULAS          = 0x01,
        RESTRICT_TYPES          = 0x02,
        MERGE_CONSTRUCTORS      = 0x04,
        PARSE_INTERNAL_TYPES    = 0x08,
    };

    typedef std::map<std::wstring, ir::ModulePtr> ModuleMap;
    typedef std::map<std::wstring, Context*> ModuleContextMap;
    typedef std::multimap<std::wstring, ir::PredicatePtr> PredicateMap;
    typedef std::map<std::wstring, ir::NamedValuePtr> VariableMap;
    typedef std::map<std::wstring, ir::TypeDeclarationPtr> TypeMap;
    typedef std::map<std::wstring, ir::LabelPtr> LabelMap;
    typedef std::map<std::wstring, ir::JumpPtr> JumpMap;
    typedef std::map<std::wstring, ir::ProcessPtr> ProcessMap;
    typedef std::map<std::wstring, ir::FormulaDeclarationPtr> FormulaMap;
    typedef std::multimap<std::wstring, ir::UnionConstructorDeclarationPtr> ConsMap;
    typedef std::map<std::wstring, Auto<tc::FreshType> > FreshTypeMap;

public:
    Context(lexer::Loc _loc, bool _bScope = false, int _flags = 0)
        : m_loc(_loc), m_bScope(_bScope), m_flags(_flags)
    {}

    Context(const ModulePtr& _pModule);

    ~Context();

    Context *getParent() const { return m_pParent; }
    void setParent(Context *_pParent) { m_pParent = _pParent; }

    Context *getChild() const { return m_pChild; }
    void setChild(Context *_pChild) { m_pChild = _pChild; }

    Context *createChild(bool _bScope = false, int _flags = 0);

    void mergeChildren(bool _bMergeFailed = false);

    lexer::Loc &loc() { return m_loc; }
    lexer::Loc nextLoc() { return next(m_loc); }

    const std::wstring &getValue() const { return m_loc->getValue(); }

    int getToken() const { return m_loc->getKind(); }

    bool is(int _t1, int _t2 = -1, int _t3 = -1, int _t4 = -1, int _t5 = -1, int _t6 = -1) const
        { return lexer::is(m_loc, _t1, _t2, _t3, _t4, _t5, _t6); }

    bool nextIs(int _t1, int _t2 = -1, int _t3 = -1, int _t4 = -1, int _t5 = -1, int _t6 = -1) const
        { return lexer::is(next(m_loc), _t1, _t2, _t3, _t4, _t5, _t6); }

    bool in(int _t1, int _t2 = -1, int _t3 = -1, int _t4 = -1, int _t5 = -1, int _t6 = -1) const
        { return lexer::in(m_loc, _t1, _t2, _t3, _t4, _t5, _t6); }

    bool nextIn(int _t1, int _t2 = -1, int _t3 = -1, int _t4 = -1, int _t5 = -1, int _t6 = -1) const
        { return lexer::in(next(m_loc), _t1, _t2, _t3, _t4, _t5, _t6); }

    Context &operator ++ () { ++m_loc; return *this; }
    Context &operator -- () { --m_loc; return *this; }

    bool consume(int _token1, int _token2 = -1, int _token3 = -1, int _token4 = -1);

    const std::wstring &scan(int _nScan = 1, int _nGet = 0);

    void skip(int _nSkip = 1);

    void fmtWarning(const wchar_t *_strFmt, ...);

    void fmtError(const wchar_t *_strFmt, ...);

    const StatusMessages &getMessages() const { return m_messages; }

    ir::ModulePtr getModule(const std::wstring &_strName) const;
    void addModule(const ir::ModulePtr &_pModule);

    Context* getModuleCtx(const std::wstring &_strName) const;
    void addModuleCtx(const ir::ModulePtr &_pModule, Context* _ctx);

    bool getPredicates(const std::wstring &_strName, ir::Predicates &_predicates) const;
    ir::PredicatePtr getPredicate(const std::wstring &_strName) const;
    bool addPredicate(const ir::PredicatePtr &_pPred);

    ir::NamedValuePtr getVariable(const std::wstring &_strName, bool _bLocal = false) const;
    bool addVariable(const ir::NamedValuePtr &_pVar);

    ir::TypeDeclarationPtr getType(const std::wstring &_strName) const;
    bool addType(const ir::TypeDeclarationPtr &_pType);

    ir::LabelPtr getLabel(const std::wstring &_strName) const;
    ir::LabelPtr createLabel(const std::wstring &_strName);
    void addLabel(const ir::LabelPtr &_pLabel);

    ir::JumpPtr getJump(const std::wstring &_strName) const;
    void addJump(const ir::JumpPtr &_pJump);

    ir::ProcessPtr getProcess(const std::wstring &_strName) const;
    void addProcess(const ir::ProcessPtr &_pProcess);

    ir::FormulaDeclarationPtr getFormula(const std::wstring &_strName) const;
    bool addFormula(const ir::FormulaDeclarationPtr &_pFormula);

    bool getConstructors(const std::wstring &_strName, ir::UnionConstructorDeclarations &_cons) const;
    void addConstructor(const ir::UnionConstructorDeclarationPtr &_pCons);
    bool hasConstructor(const std::wstring &_strName) const;

    Auto<tc::FreshType> getFreshType(const std::wstring & _strName);
    const FreshTypeMap * getFreshTypes() const { return m_freshTypes; }

    // Constructor-parsing stuff.
    ir::UnionConstructorPtr getCurrentConstructor() const {
        return m_pCons ? m_pCons : (m_pParent ? m_pParent->getCurrentConstructor() : ir::UnionConstructorPtr());
    }
    void setCurrentConstructor(const ir::UnionConstructorPtr &_pCons) { m_pCons = _pCons; }

    bool isScope() const { return m_bScope; }

    void fail() { m_bFailed = true; }
    bool failed() const { return m_bFailed; }

    Pragma &getPragma() { return m_pragma; }

    int getIntBits() const;
    int getRealBits() const;
    const ir::Overflow &getOverflow() const;

    int getFlags() const;
    void setFlags(int _flags);

private:
    lexer::Loc m_loc;
    bool m_bScope;
    Context *m_pChild = nullptr, *m_pParent = nullptr, *m_pFailed = nullptr;
    StatusMessages m_messages;
    ModuleMap *m_modules = nullptr;
    ModuleContextMap *m_modulesCtxs = nullptr;
    PredicateMap *m_predicates = nullptr;
    VariableMap *m_variables = nullptr;
    TypeMap *m_types = nullptr;
    LabelMap *m_labels = nullptr;
    JumpMap *m_jumps = nullptr;
    ProcessMap *m_processes = nullptr;
    FormulaMap *m_formulas = nullptr;
    ConsMap *m_constructors = nullptr;
    FreshTypeMap *m_freshTypes = nullptr;
    bool m_bFailed = false;
    Pragma m_pragma;
    ir::UnionConstructorPtr m_pCons;
    int m_flags;

    void mergeTo(Context *_pCtx, bool _bMergeFailed);
};

}

#endif /* PARSER_CONTEXT_H_ */
