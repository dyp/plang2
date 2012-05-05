/// \file typecheck.h
///


#ifndef TYPECHECK_H_
#define TYPECHECK_H_

#include <assert.h>

#include <ir/types.h>
#include "parser_context.h"

#include <vector>
#include <list>
#include <map>

namespace tc {

typedef Auto<class Formula> FormulaPtr;
typedef Auto<class CompoundFormula> CompoundFormulaPtr;
typedef Auto<class Flags> FlagsPtr;

class Flags : public Counted {
public:
    int get(size_t _cIdx) const;
    void set(size_t _cIdx, int _flags);
    int add(size_t _cIdx, int _flags);
    void mergeTo(Flags &_to);
    void filterTo(Flags &_to, const class Formula &_from);
    void filterTo(Flags &_to, const class Formulas &_from);

private:
    std::vector<int> m_flags;
};

class FreshType : public ir::Type {
public:
    FreshType(int _flags = 0) {
        m_cOrd = g_cOrdMax++;
        addFlags(_flags);
    }

    FreshType(const FreshType &_other) : m_cOrd(_other.m_cOrd) {
        addFlags(_other.getFlags());
    }

    enum {
        NONE      = 0x00,
        PARAM_IN  = 0x01,
        PARAM_OUT = 0x02,
    };

    int getFlags() const;

    void setFlags(int _flags);

    int addFlags(int _flags);

    virtual bool rewriteFlags(int _flags) {
        addFlags(_flags);
        return true;
    }

    /// Get type kind.
    /// \returns #Fresh.
    virtual int getKind() const { return FRESH; }

    virtual bool less(const ir::Type & _other) const;
    int compare(const ir::Type &_other) const;

    virtual ir::NodePtr clone(Cloner &_cloner) const;

    size_t getOrdinal() const { return m_cOrd; }

private:
    size_t m_cOrd;
    static size_t g_cOrdMax;
    static std::vector<int> m_flags;
};

typedef Auto<FreshType> FreshTypePtr;

class TupleType : public ir::Type {
public:
    /// Default constructor.
    TupleType(const ir::NamedValuesPtr &_pFields) : m_pFields(_pFields) {}

    /// Get type kind.
    /// \returns #Tuple.
    virtual int getKind() const { return TUPLE; }

    /// Get list of struct fields.
    /// \return List of fields.
    ir::NamedValues &getFields() { return *m_pFields; }
    const ir::NamedValues &getFields() const { return *m_pFields; }

    virtual int compare(const Type &_other) const;
    virtual ir::TypePtr getMeet(ir::Type &_other);
    virtual ir::TypePtr getJoin(ir::Type &_other);
    virtual bool less(const Type &_other) const;
    virtual int getMonotonicity(const Type &_var) const;

private:
    ir::NamedValuesPtr m_pFields;
};

typedef Auto<TupleType> TupleTypePtr;

class Formula : public Counted {
public:
    enum {
        EQUALS          = 0x01,
        SUBTYPE         = 0x02,
        SUBTYPE_STRICT  = 0x04,
        COMPARABLE      = 0x08,
        INCOMPARABLE    = 0x10,
        NO_JOIN         = 0x20,
        HAS_JOIN        = 0x40,
        NO_MEET         = 0x80,
        HAS_MEET        = 0x100,
        COMPOUND        = 0x200,
    };

    enum {
        UNKNOWN,
        FALSE,
        TRUE,
    };

    Formula(int _kind, const ir::TypePtr &_pLhs = NULL, const ir::TypePtr &_pRhs = NULL) :
        m_kind(_kind), m_pLhs(_pLhs), m_pRhs(_pRhs) {}

    Formula(const Formula &_other) :
        m_kind(_other.m_kind), m_pLhs(_other.m_pLhs), m_pRhs(_other.m_pRhs) {}

    bool is(int _kind) const { return (m_kind & _kind) != 0; }

    int getKind() const { return m_kind; }
    void setKind(int _kind) { m_kind = _kind; }

    const ir::TypePtr &getLhs() const { return m_pLhs; }
    void setLhs(const ir::TypePtr &_pType) { m_pLhs = _pType; }

    const ir::TypePtr &getRhs() const { return m_pRhs; }
    void setRhs(const ir::TypePtr &_pType) { m_pRhs = _pType; }

    bool hasFresh() const;

    virtual bool rewrite(const ir::TypePtr &_pOld, const ir::TypePtr &_pNew, bool _bRewriteFlags = true);

    virtual int eval() const;

    bool implies(const Formula &_other);

    bool isSymmetric() const;

    virtual FormulaPtr clone(Cloner &_cloner) const;

    virtual bool operator ==(const Formula &_other) const {
        return m_kind == _other.m_kind &&
                (m_pLhs == _other.m_pLhs || *m_pLhs == *_other.m_pLhs) &&
                (m_pRhs == _other.m_pRhs || *m_pRhs == *_other.m_pRhs);
    }

    bool operator !=(const Formula &_other) const {
        return !(*this == _other);
    }

    virtual bool contains(const ir::TypePtr &_pType) const;

private:
    int m_kind;
    ir::TypePtr m_pLhs, m_pRhs;
};

struct FormulaCmp {
    bool operator()(const FormulaPtr &_lhs, const FormulaPtr &_rhs) const;
};

struct FormulaEquals {
    bool operator()(const FormulaPtr &_lhs, const FormulaPtr &_rhs) const { return *_lhs == *_rhs; }
};

typedef std::list<FormulaPtr> FormulaList;

struct Formulas : public std::set<FormulaPtr, FormulaCmp> {
    FlagsPtr pFlags;

    Formulas() : pFlags(new Flags()) {}

    void swap(Formulas &_other);
    bool rewrite(const ir::TypePtr &_pOld, const ir::TypePtr &_pNew, bool _bRewriteFlags = true);
    bool implies(Formula &_f) const;
    iterator beginCompound();
    iterator findSubst(const ir::TypePtr &_pType) const;

};

struct Context : public Counted {
    Auto<Formulas> pFormulas;
    Auto<Formulas> pSubsts;
    Auto<Context> pParent;
    Auto<class Lattice> pTypes;

    Context();
    Context(const Auto<Formulas> &_pFormulas, const Auto<Formulas> &_pSubsts);
    Context(const Auto<Formulas> &_pFormulas, const Auto<Context> &_pParent = NULL);

    ir::TypePtr lookup(const tc::Formula &_f, const tc::Formula &_cond);
    bool rewrite(const ir::TypePtr &_pOld, const ir::TypePtr &_pNew, bool _bRewriteFlags = true);
    bool implies(Formula &_f);
    bool implies(Formulas &_fs);
    virtual Auto<Context> clone(Cloner &_cloner) const;
    bool add(const FormulaPtr &_pFormula);
    bool add(int _kind, const ir::TypePtr &_pLhs, const ir::TypePtr &_pRhs);
    Flags &flags() { return *pFormulas->pFlags; }

    Formulas &operator *() const { return *pFormulas; }
    Formulas *operator ->() const { return pFormulas.ptr(); }
};

typedef Auto<Context> ContextPtr;

class ContextIterator {
public:
    ContextIterator(const ContextIterator &_other);
    ContextIterator(Context *_pCtx, bool _bSkipCompound = true, bool _bSkipTopSubsts = false);

    bool start();
    FormulaPtr get() const { return *m_iter; }
    Formulas::iterator getIter() { return m_iter; }
    Formulas &getFormulas() { return *m_pFormulas; }
    bool next();
    bool eof();
    ContextIterator find(const FormulaPtr &_f);

private:
    Context *m_pCtx;
    Context *m_pCurrent;
    Auto<Formulas> m_pFormulas;
    bool m_bSkipCompound;
    bool m_bSkipTopSubsts;
    Formulas::iterator m_iter;
};

// Global stack of constraint lists.
struct ContextStack {
    static ContextPtr top();
    static ContextPtr root();
    static void push(const ContextPtr &_ctx);
    static void push(const Auto<Formulas> &_fs);
    static void pop();
    static bool empty();
    static void clear();
};

class CompoundFormula : public Formula {
public:
    CompoundFormula() : Formula(COMPOUND) {}

    size_t size() const { return m_parts.size(); }
    Formulas &getPart(size_t _i) { return *m_parts[_i]; }
    const Formulas &getPart(size_t _i) const { return *m_parts[_i]; }
    const Auto<Formulas> &getPartPtr(size_t _i) const { return m_parts[_i]; }
    Formulas &addPart();
    void addPart(const Auto<Formulas> &_pFormulas);
    void removePart(size_t _i) { m_parts.erase(m_parts.begin() + _i); }

    virtual bool rewrite(const ir::TypePtr &_pOld, const ir::TypePtr &_pNew, bool _bRewriteFlags = true);
    virtual int eval() const;
    size_t count() const;
    virtual FormulaPtr clone(Cloner &_cloner) const;

    virtual bool contains(const ir::TypePtr &_pType) const;

    virtual bool operator ==(const Formula &_other) const;

private:
    std::vector<Auto<Formulas> > m_parts;
};

bool rewriteType(ir::TypePtr &_pType, const ir::TypePtr &_pOld, const ir::TypePtr &_pNew, bool _bRewriteFlags = true);

bool solve(Formulas &_formulas, Formulas &_substs);

void collect(Formulas &_constraints, ir::Node &_node, ir::Context &_ctx);

void linkPredicates(ir::Context &_ctx, ir::Node &_node);

void apply(Formulas &_constraints, ir::Node &_node);

}; // namespace tc

#endif /* TYPECHECK_H_ */
