/// \file typecheck.h
///


#ifndef TYPECHECK_H_
#define TYPECHECK_H_

#include <assert.h>

#include <ir/types.h>
#include "parser_context.h"
#include "ir/expressions.h"

#include <vector>
#include <list>
#include <map>
#include <set>

namespace tc {

using FormulaPtr = std::shared_ptr<class Formula>;
using CompoundFormulaPtr = std::shared_ptr<class CompoundFormula>;
using FlagsPtr = std::shared_ptr<class Flags>;

class Flags {
public:
    Flags() = default;
    Flags(const Flags &_other) : m_flags(_other.m_flags) {}

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

using FreshTypePtr = std::shared_ptr<FreshType>;

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
    virtual ir::TypePtr getMeet(const ir::TypePtr &_other);
    virtual ir::TypePtr getJoin(const ir::TypePtr &_other);
    virtual bool less(const Type &_other) const;
    virtual int getMonotonicity(const Type &_var) const;

private:
    ir::NamedValuesPtr m_pFields;
};

using TupleTypePtr = std::shared_ptr<class TupleType>;

class Formula : std::enable_shared_from_this<Formula> {
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

    Formula(int _kind, const ir::TypePtr &_pLhs = NULL, const ir::TypePtr &_pRhs = NULL, const ir::ExpressionPtr& _pCond = NULL) :
        m_kind(_kind), m_pLhs(_pLhs), m_pRhs(_pRhs)
    {
        if (_pCond)
            m_conditions.insert(_pCond);
    }

    Formula(const Formula &_other) :
        m_kind(_other.m_kind), m_pLhs(_other.m_pLhs), m_pRhs(_other.m_pRhs), m_conditions(_other.m_conditions) {}

    virtual ~Formula() = default;

    Formula &operator =(const Formula &_other) {
        m_kind = _other.m_kind;
        m_pLhs = _other.m_pLhs;
        m_pRhs = _other.m_pRhs;
        m_conditions = _other.m_conditions;
        return *this;
    }

    const std::set<ir::ExpressionPtr>& getConditions() const { return m_conditions; }
    std::set<ir::ExpressionPtr>& getConditions() { return m_conditions; }

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

    virtual bool contains(const ir::Type &_type) const;

    template <class _Class>
    std::shared_ptr<_Class> as() {
        return std::static_pointer_cast<_Class>(shared_from_this());
    }

    template <class _Class>
    const std::shared_ptr<_Class> as() const {
        return std::static_pointer_cast<_Class>(shared_from_this());
    }
private:
    int m_kind;
    ir::TypePtr m_pLhs, m_pRhs;
    std::set<ir::ExpressionPtr> m_conditions;
};

struct FormulaCmp {
    bool operator()(const FormulaPtr &_lhs, const FormulaPtr &_rhs) const;
};

struct FormulaEquals {
    bool operator()(const FormulaPtr &_lhs, const FormulaPtr &_rhs) const { return *_lhs == *_rhs; }
};

typedef std::list<FormulaPtr> FormulaList;

using FormulasPtr = std::shared_ptr<class Formulas>;

struct Formulas : public std::set<FormulaPtr, FormulaCmp> {
    FlagsPtr pFlags;

    Formulas() : pFlags(new Flags()) {}

    void swap(Formulas &_other);
    bool rewrite(const ir::TypePtr &_pOld, const ir::TypePtr &_pNew, bool _bRewriteFlags = true);
    bool implies(Formula &_f) const;
    iterator beginCompound();
    iterator findSubst(const ir::TypePtr &_pType) const;
    const std::set<ir::ExpressionPtr>& getConditions() const { return m_conditions; }
    std::set<ir::ExpressionPtr>& getConditions() { return m_conditions; }

    void insertFormulas(const tc::Formulas& _formulas);

private:
    std::set<ir::ExpressionPtr> m_conditions;
};

using ContextPtr = std::shared_ptr<class Context>;

struct Context : public std::enable_shared_from_this<Context> {
    FormulasPtr pFormulas;
    FormulasPtr pSubsts;
    std::shared_ptr<Context> pParent;
    std::shared_ptr<class Lattice> pTypes;
    std::map<tc::FreshTypePtr, ir::NamedReferenceTypePtr> namedTypes;

    Context();
    Context(const FormulasPtr &_pFormulas, const FormulasPtr &_pSubsts);
    Context(const FormulasPtr &_pFormulas, const ContextPtr &_pParent);
    virtual ~Context() = default;

    ir::TypePtr lookup(const tc::Formula &_f, const tc::Formula &_cond);
    bool rewrite(const ir::TypePtr &_pOld, const ir::TypePtr &_pNew, bool _bRewriteFlags = true);
    bool implies(Formula &_f);
    bool implies(Formulas &_fs);
    virtual ContextPtr clone(Cloner &_cloner) const;
    bool add(const FormulaPtr &_pFormula);
    bool add(int _kind, const ir::TypePtr &_pLhs, const ir::TypePtr &_pRhs);
    Flags &flags() { return *pFormulas->pFlags; }
    const std::set<ir::ExpressionPtr>& getConditions() const { return pFormulas->getConditions(); }
    std::set<ir::ExpressionPtr>& getConditions() { return pFormulas->getConditions(); }
    void rewriteTypesInConditions();

    Formulas &operator *() const { return *pFormulas; }

    const FormulasPtr &formulas() const { return pFormulas; }

    template<typename T>
    void insert(T _begin, T _end) {
        pFormulas->insert(_begin, _end);
        for(T i = _begin; i != _end; ++i)
            getConditions().insert((*i)->getConditions().begin(), (*i)->getConditions().end());
    }

    template <class _Class>
    std::shared_ptr<_Class> as() {
        return std::static_pointer_cast<_Class>(shared_from_this());
    }

    template <class _Class>
    std::shared_ptr<const _Class> as() const {
        return std::static_pointer_cast<const _Class>(shared_from_this());
    }

    void insertFormulas(const tc::Formulas& _formulas);
    void restoreNamedTypes();
    void clearBodiesOfTypeDeclarations() const;
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
    FormulasPtr m_pFormulas;
    bool m_bSkipCompound;
    bool m_bSkipTopSubsts;
    Formulas::iterator m_iter;
};

// Global stack of constraint lists.
struct ContextStack {
    static ContextPtr top();
    static ContextPtr root();
    static void push(const ContextPtr &_ctx);
    static void push(const FormulasPtr &_fs);
    static void pop();
    static bool empty();
    static void clear();
};

class CompoundFormula : public Formula {
public:
    CompoundFormula() : Formula(COMPOUND) {}
    ~CompoundFormula() override = default;

    size_t size() const { return m_parts.size(); }
    Formulas &getPart(size_t _i) { return *m_parts[_i]; }
    const Formulas &getPart(size_t _i) const { return *m_parts[_i]; }
    const FormulasPtr &getPartPtr(size_t _i) const { return m_parts[_i]; }
    Formulas &addPart();
    void addPart(const FormulasPtr &_pFormulas);
    void removePart(size_t _i) { m_parts.erase(m_parts.begin() + _i); }

    virtual bool rewrite(const ir::TypePtr &_pOld, const ir::TypePtr &_pNew, bool _bRewriteFlags = true);
    virtual int eval() const;
    size_t count() const;
    virtual FormulaPtr clone(Cloner &_cloner) const;

    bool contains(const ir::Type &_type) const override;

    virtual bool operator ==(const Formula &_other) const;

private:
    std::vector<FormulasPtr> m_parts;
};

bool rewriteType(ir::TypePtr &_pType, const ir::TypePtr &_pOld, const ir::TypePtr &_pNew, bool _bRewriteFlags = true);

bool solve(const ContextPtr& _pContext);

ContextPtr collect(const FormulasPtr &_constraints, const ir::NodePtr &_node, ir::Context &_ctx);

void linkPredicates(ir::Context &_ctx, ir::Node &_node);

void apply(const ContextPtr& _pContext, const ir::NodePtr &_node);

}; // namespace tc

#endif /* TYPECHECK_H_ */
