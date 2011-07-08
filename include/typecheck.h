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

    virtual ir::TypePtr clone() const;

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

    virtual FormulaPtr clone() const;

private:
    int m_kind;
    ir::TypePtr m_pLhs, m_pRhs;
};

struct FormulaCmp {
    typedef FormulaPtr T;
    bool operator()(const T &_lhs, const T &_rhs) const;
};

typedef std::set<FormulaPtr, FormulaCmp> FormulaSet;
typedef std::list<FormulaPtr> FormulaList;

struct Formulas : public FormulaSet {
    FormulaSet substs;

    bool rewrite(const ir::TypePtr &_pOld, const ir::TypePtr &_pNew);
    bool implies(Formula &_f) const;
    virtual Auto<Formulas> clone() const;
};

class CompoundFormula : public Formula {
public:
    CompoundFormula() : Formula(COMPOUND) {}

    size_t size() const { return m_parts.size(); }
    Formulas &getPart(size_t _i) { return *m_parts[_i]; }
    const Formulas &getPart(size_t _i) const { return *m_parts[_i]; }
    Formulas &addPart();
    void addPart(const Auto<Formulas> &_pFormulas);
    void removePart(size_t _i) { m_parts.erase(m_parts.begin() + _i); }

    virtual bool rewrite(const ir::TypePtr &_pOld, const ir::TypePtr &_pNew);
    virtual int eval() const;
    size_t count() const;
    virtual FormulaPtr clone() const;

private:
    std::vector<Auto<Formulas> > m_parts;
};

bool rewriteType(ir::TypePtr &_pType, const ir::TypePtr &_pOld, const ir::TypePtr &_pNew);

bool solve(Formulas &_formulas);

void collect(Formulas &_constraints, ir::Node &_node, Context &_ctx, FreshTypes &_types);

void apply(Formulas &_constraints, tc::FreshTypes &_types);

}; // namespace tc

#endif /* TYPECHECK_H_ */
