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

class Formula;
typedef Auto<Formula> FormulaPtr;
class CompoundFormula;
typedef Auto<CompoundFormula> CompoundFormulaPtr;

class TypeSetterBase {
public:
    virtual ~TypeSetterBase() {}
    virtual void setType(const ir::TypePtr &_pType) = 0;
};

typedef Auto<TypeSetterBase> TypeSetterBasePtr;

template <typename _Node, void (_Node::*_Method)(const ir::TypePtr &)>
class TypeSetter : public TypeSetterBase {
public:
    TypeSetter(const Auto<_Node> &_pNode) : m_pNode(_pNode) {}

    virtual void setType(const ir::TypePtr &_pType) { ((*m_pNode).*(_Method))(_pType); }

protected:
    Auto<_Node> m_pNode;
};

template <typename _Node>
inline TypeSetterBasePtr createTypeSetter(const Auto<_Node> &_pNode) {
    return ptr(new TypeSetter<_Node, &_Node::setType>(_pNode));
}

template <typename _Node>
inline TypeSetterBasePtr createBaseTypeSetter(const Auto<_Node> &_pNode) {
    return ptr(new TypeSetter<_Node, &_Node::setBaseType>(_pNode));
}

template <typename _Node>
inline TypeSetterBasePtr createIndexTypeSetter(const Auto<_Node> &_pNode) {
    return ptr(new TypeSetter<_Node, &_Node::setIndexType>(_pNode));
}

class FreshType : public ir::Type {
public:
    FreshType() : m_flags(0) {
        m_cOrd = ++g_cOrdMax;
    }

    FreshType(const FreshType &_other) : m_cOrd(_other.m_cOrd), m_flags(_other.m_flags) {
    }

    enum {
        NONE      = 0x00,
        PARAM_IN  = 0x01,
        PARAM_OUT = 0x02,
    };

    int getFlags() const { return m_flags; }

    void setFlags(int _flags) { m_flags = _flags; }

    int addFlags(int _flags) {
        m_flags |= _flags;
        return m_flags;
    }

    virtual bool rewriteFlags(int _flags) {
        addFlags(_flags);
        return true;
    }

    /// Get type kind.
    /// \returns #Fresh.
    virtual int getKind() const { return FRESH; }

    virtual bool less(const ir::Type & _other) const;

    virtual ir::NodePtr clone(Cloner &_cloner) const;

    size_t getOrdinal() const { return m_cOrd; }

private:
    size_t m_cOrd;
    static size_t g_cOrdMax;
    int m_flags;
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

private:
    ir::NamedValuesPtr m_pFields;
};

typedef Auto<TupleType> TupleTypePtr;

typedef std::multimap<FreshTypePtr, TypeSetterBasePtr> FreshTypes;

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

    bool is(int _kind) const { return (m_kind & _kind) != 0; }

    int getKind() const { return m_kind; }

    const ir::TypePtr &getLhs() const { return m_pLhs; }
    void setLhs(const ir::TypePtr &_pType) { m_pLhs = _pType; }

    const ir::TypePtr &getRhs() const { return m_pRhs; }
    void setRhs(const ir::TypePtr &_pType) { m_pRhs = _pType; }

    bool hasFresh() const;

    virtual bool rewrite(const ir::TypePtr &_pOld, const ir::TypePtr &_pNew);

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
    bool rewrite(const ir::TypePtr &_pOld, const ir::TypePtr &_pNew);
    bool implies(Formula &_f) const;
    iterator beginCompound();
    iterator findSubst(const ir::TypePtr &_pType);
};

class Extrema;

struct Context : public Counted {
    Auto<Formulas> fs;
    Auto<Formulas> substs;
    Auto<Context> pParent;
    Auto<Extrema> pExtrema;

    Context();
    Context(const Auto<Formulas> &_fs, const Auto<Formulas> &_substs);
    Context(const Auto<Formulas> &_fs, const Auto<Context> &_pParent = NULL);

    ir::TypePtr lookup(const tc::Formula &_f, const tc::Formula &_cond);
    bool rewrite(const ir::TypePtr &_pOld, const ir::TypePtr &_pNew);
    bool implies(Formula &_f);
    virtual Auto<Context> clone(Cloner &_cloner) const;
    bool add(const FormulaPtr &_pFormula);
    bool add(int _kind, const ir::TypePtr &_pLhs, const ir::TypePtr &_pRhs);

    Formulas &operator *() const { return *fs; }
    Formulas *operator ->() const { return fs.ptr(); }
};

typedef Auto<Context> ContextPtr;

struct TypePtrCmp {
    bool operator()(const ir::TypePtr &_lhs, const ir::TypePtr &_rhs) const { return *_lhs < *_rhs; }
};

typedef std::set<ir::TypePtr, TypePtrCmp> TypeSet;
typedef std::map<ir::TypePtr, TypeSet> TypeSets;

class Extrema : public Counted {
public:
    Extrema(Context *_pCtx) : m_pCtx(_pCtx), m_bValid(false) {}

    void invalidate() { m_bValid = false; }
    const TypeSet &inf(const ir::TypePtr &_pType);
    const TypeSets &infs();
    const TypeSet &sup(const ir::TypePtr &_pType);
    const TypeSets &sups();

private:
    Context *m_pCtx;
    bool m_bValid;
    TypeSets m_lowers, m_uppers;

    void update();
};

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

    virtual bool rewrite(const ir::TypePtr &_pOld, const ir::TypePtr &_pNew);
    virtual int eval() const;
    size_t count() const;
    virtual FormulaPtr clone(Cloner &_cloner) const;

    virtual bool contains(const ir::TypePtr &_pType) const;

    virtual bool operator ==(const Formula &_other) const;

private:
    std::vector<Auto<Formulas> > m_parts;
};

bool rewriteType(ir::TypePtr &_pType, const ir::TypePtr &_pOld, const ir::TypePtr &_pNew);

bool solve(Formulas &_formulas, Formulas &_substs);

void collect(Formulas &_constraints, ir::Node &_node, ir::Context &_ctx, FreshTypes &_types);

void apply(Formulas &_constraints, tc::FreshTypes &_types);

}; // namespace tc

#endif /* TYPECHECK_H_ */
