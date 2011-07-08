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

class TypeSetterBase {
public:
    virtual ~TypeSetterBase() {}
    virtual void setType(ir::Type * _pType) = 0;
};

template <typename Node, void (Node::*Method)(ir::Type *, bool)>
class TypeSetter : public TypeSetterBase {
public:
    TypeSetter(Node * _pNode) : m_pNode(_pNode) {}

    virtual void setType(ir::Type * _pType) { ((*m_pNode).*(Method))(_pType, true); }

protected:
    Node * m_pNode;
};

template <typename Node>
inline TypeSetterBase *createTypeSetter(Node *_pNode) {
    return new TypeSetter<Node, &Node::setType>(_pNode);
}

template <typename Node>
inline TypeSetterBase *createBaseTypeSetter(Node *_pNode) {
    return new TypeSetter<Node, &Node::setBaseType>(_pNode);
}

template <typename Node>
inline TypeSetterBase *createIndexTypeSetter(Node *_pNode) {
    return new TypeSetter<Node, &Node::setIndexType>(_pNode);
}

class FreshType : public ir::Type {
public:
    //template <typename Node>
    FreshType(/*Node * _pNode*/) : m_flags(0) {
        //m_pTypeSetter = new TypeSetter<Node>(_pNode);
        m_cOrd = ++ g_cOrdMax;
    }

    virtual ~FreshType() { /*delete m_pTypeSetter;*/ }

    enum {
        NONE      = 0x00,
        PARAM_IN  = 0x01,
        PARAM_OUT = 0x02,
    };

    int getFlags() const { return m_flags; }
    void setFlags(int _flags) { m_flags = _flags; }
    int addFlags(int _flags) { m_flags |= _flags; return m_flags; }

    virtual bool rewriteFlags(int _flags) { addFlags(_flags); return true; }

    //void replaceType(ir::Type * _pType) { m_pTypeSetter->setType(_pType); }

    /// Get type kind.
    /// \returns #Fresh.
    virtual int getKind() const { return FRESH; }

    virtual bool less(const ir::Type & _other) const;

    virtual ir::Type * clone() const;

    size_t getOrdinal() const { return m_cOrd; }

private:
    /*FreshType(TypeSetterBase * _pTypeSetter, size_t _cOrd) : m_pTypeSetter(_pTypeSetter), m_cOrd(_cOrd) {
    }*/

    //TypeSetterBase * m_pTypeSetter;
    size_t m_cOrd;
    static size_t g_cOrdMax;
    int m_flags;
};

class TupleType : public ir::Type {
public:
    /// Default constructor.
    TupleType(const ir::NamedValues *_pFields) : m_fields(*_pFields) {}

    /// Get type kind.
    /// \returns #Tuple.
    virtual int getKind() const { return TUPLE; }

    /// Get list of struct fields.
    /// \return List of fields.
    ir::NamedValues &getFields() { return (ir::NamedValues &)m_fields; }
    const ir::NamedValues &getFields() const { return m_fields; }

    virtual int compare(const Type &_other) const;
    virtual ir::Type *getMeet(ir::Type &_other);
    virtual ir::Type *getJoin(ir::Type &_other);
    virtual bool less(const Type &_other) const;

private:
    const ir::NamedValues &m_fields;
};

typedef std::multimap<FreshType *, TypeSetterBase *> FreshTypes;

class Formula {
public:
    enum {
        EQUALS          = 0x01,
        SUBTYPE         = 0x02,
        SUBTYPE_STRICT  = 0x04,
        COMPARABLE      = 0x08,
        INCOMPARABLE    = 0x10,
        NO_JOIN         = 0x20,
        NO_MEET         = 0x40,
        COMPOUND        = 0x80,
    };

    enum {
        UNKNOWN,
        FALSE,
        TRUE,
    };

    Formula(int _kind, ir::Type * _pLhs = NULL, ir::Type * _pRhs = NULL) : m_kind(_kind), m_pLhs(_pLhs), m_pRhs(_pRhs) {}

    bool is(int _kind) const { return (m_kind & _kind) != 0; }
    int getKind() const { return m_kind; }
    ir::Type * getLhs() const { return m_pLhs; }
    void setLhs(ir::Type * _pType) { m_pLhs = _pType; }
    ir::Type * getRhs() const { return m_pRhs; }
    void setRhs(ir::Type * _pType) { m_pRhs = _pType; }

    bool hasFresh() const;

    virtual bool rewrite(ir::Type * _pOld, ir::Type * _pNew);

    virtual int eval() const;

    // return A && B if it can be expressed as a single formula.
    Formula * mergeAnd(Formula & _other);

    // return A || B if it can be expressed as a single formula.
    Formula * mergeOr(Formula & _other);

    bool implies(Formula & _other);

    bool isSymmetric() const;

    virtual Formula *clone() const;

//    int invKind() const;

private:
    int m_kind;
    ir::Type * m_pLhs, * m_pRhs;
};

struct FormulaCmp {
    typedef Formula * T;
    bool operator()(const T & _lhs, const T & _rhs) const;
};

typedef std::set<Formula *, FormulaCmp> FormulaSet;
typedef std::list<Formula *> FormulaList;

struct Formulas : public FormulaSet {
    FormulaSet substs;

    bool rewrite(ir::Type * _pOld, ir::Type * _pNew, bool _bKeepOrig = false);
    Formula * lookup(int _op, int _ordLhs, ir::Type * _pLhs, int _ordRhs, ir::Type * _pRhs);
    bool implies(Formula & _f) const;
    virtual Formulas *clone() const;
};

class CompoundFormula : public Formula {
public:
    CompoundFormula() : Formula(COMPOUND) {}

    size_t size() const { return m_parts.size(); }
    Formulas & getPart(size_t _i) { return * m_parts[_i]; }
    const Formulas & getPart(size_t _i) const { return * m_parts[_i]; }
    Formulas & addPart();
    void addPart(Formulas *_pFormulas);
    void removePart(size_t _i) { m_parts.erase(m_parts.begin() + _i); }

    virtual bool rewrite(ir::Type * _pOld, ir::Type * _pNew);
    void merge(Formulas & _dest);
    virtual int eval() const;
    size_t count() const;
    virtual Formula *clone() const;

private:
    std::vector<Formulas *> m_parts;
};

bool rewriteType(ir::Type * & _pType, ir::Type * _pOld, ir::Type * _pNew);

bool solve(Formulas & _formulas);

void collect(Formulas & _constraints, ir::Node &_node, Context & _ctx, FreshTypes & _types);

void apply(Formulas & _constraints, tc::FreshTypes & _types);

}; // namespace tc

#endif /* TYPECHECK_H_ */
