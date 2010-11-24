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
    virtual void setType(ir::CType * _pType) = 0;
};

template <typename Node, void (Node::*Method)(ir::CType *, bool)>
class TypeSetter : public TypeSetterBase {
public:
    TypeSetter(Node * _pNode) : m_pNode(_pNode) {}

    virtual void setType(ir::CType * _pType) { ((*m_pNode).*(Method))(_pType, true); }

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

class FreshType : public ir::CType {
public:
    //template <typename Node>
    FreshType(/*Node * _pNode*/) : m_flags(0) {
        //m_pTypeSetter = new TypeSetter<Node>(_pNode);
        m_cOrd = ++ g_cOrdMax;
    }

    virtual ~FreshType() { /*delete m_pTypeSetter;*/ }

    enum {
        None     = 0x00,
        ParamIn  = 0x01,
        ParamOut = 0x02,
    };

    int getFlags() const { return m_flags; }
    void setFlags(int _flags) { m_flags = _flags; }
    int addFlags(int _flags) { m_flags |= _flags; return m_flags; }

    virtual bool rewriteFlags(int _flags) { addFlags(_flags); return true; }

    //void replaceType(ir::CType * _pType) { m_pTypeSetter->setType(_pType); }

    /// Get type kind.
    /// \returns #Fresh.
    virtual int getKind() const { return Fresh; }

    bool operator <(const CType & _other) const;

    virtual ir::CType * clone() const;

private:
    /*FreshType(TypeSetterBase * _pTypeSetter, size_t _cOrd) : m_pTypeSetter(_pTypeSetter), m_cOrd(_cOrd) {
    }*/

    //TypeSetterBase * m_pTypeSetter;
    size_t m_cOrd;
    static size_t g_cOrdMax;
    int m_flags;
};

typedef std::multimap<FreshType *, TypeSetterBase *> FreshTypes;

class Formula {
public:
    enum {
        Equals        = 0x01,
        Subtype       = 0x02,
        SubtypeStrict = 0x04,
        Comparable    = 0x08,
        Incomparable  = 0x10,
        NoJoin        = 0x20,
        NoMeet        = 0x40,
        Compound      = 0x80,
    };

    enum {
        Unknown,
        False,
        True,
    };

    Formula(int _kind, ir::CType * _pLhs = NULL, ir::CType * _pRhs = NULL) : m_kind(_kind), m_pLhs(_pLhs), m_pRhs(_pRhs) {}

    bool is(int _kind) const { return (m_kind & _kind) != 0; }
    int getKind() const { return m_kind; }
    ir::CType * getLhs() const { return m_pLhs; }
    void setLhs(ir::CType * _pType) { m_pLhs = _pType; }
    ir::CType * getRhs() const { return m_pRhs; }
    void setRhs(ir::CType * _pType) { m_pRhs = _pType; }

    bool hasFresh() const;

    virtual bool rewrite(ir::CType * _pOld, ir::CType * _pNew);

    virtual int eval();

    // return A && B if it can be expressed as a single formula.
    Formula * mergeAnd(Formula & _other);

    // return A || B if it can be expressed as a single formula.
    Formula * mergeOr(Formula & _other);

    bool implies(Formula & _other);

    bool isSymmetric() const;

//    int invKind() const;

private:
    int m_kind;
    ir::CType * m_pLhs, * m_pRhs;
};

struct FormulaCmp {
    typedef Formula * T;
    bool operator()(const T & _lhs, const T & _rhs) const;
};

typedef std::set<Formula *, FormulaCmp> FormulaSet;
typedef std::list<Formula *> FormulaList;

struct Formulas : public FormulaSet {
    FormulaSet substs;

    bool rewrite(ir::CType * _pOld, ir::CType * _pNew, bool _bKeepOrig = false);
    Formula * lookup(int _op, int _ordLhs, ir::CType * _pLhs, int _ordRhs, ir::CType * _pRhs);
    bool implies(Formula & _f) const;
};

class CompoundFormula : public Formula {
public:
    CompoundFormula() : Formula(Compound) {}

    size_t size() const { return m_parts.size(); }
    Formulas & getPart(size_t _i) { return * m_parts[_i]; }
    const Formulas & getPart(size_t _i) const { return * m_parts[_i]; }
    Formulas & addPart();
    void removePart(size_t _i) { m_parts.erase(m_parts.begin() + _i); }

    virtual bool rewrite(ir::CType * _pOld, ir::CType * _pNew);
    void merge(Formulas & _dest);
    virtual int eval();
    size_t count() const;

private:
    std::vector<Formulas *> m_parts;
};

bool rewriteType(ir::CType * & _pType, ir::CType * _pOld, ir::CType * _pNew);

bool solve(Formulas & _formulas);

void collect(Formulas & _constraints, ir::CPredicate & _pred, CContext & _ctx, FreshTypes & _types);

void apply(Formulas & _constraints, tc::FreshTypes & _types);

}; // namespace tc

#if 0
namespace ir {




//class CC








class CConstraint : public CNode {
public:
    enum {
        Equals,
        Subtype,
        SubtypeStrict,
    };

    CConstraint() : m_pLhs(NULL), m_pRhs(NULL) {}
    CConstraint(CType * _pLhs, CType * _pRhs) : m_pLhs(NULL), m_pRhs(NULL) {
        _assign(m_pLhs, _pLhs, true);
        _assign(m_pRhs, _pRhs, true);
    }

    /*CConstraint(const CConstraint & _other) : m_pLhs(NULL), m_pRhs(NULL) {
        (* this) = _other;
    }*/

    virtual ~CConstraint() {
        /*_delete(m_pLhs);
        _delete(m_pRhs);*/
    }

    CType & lhs() const { assert(m_pLhs); return * m_pLhs; }
    CType & rhs() const { assert(m_pRhs); return * m_pRhs; }

    void setLhs(CType * _pLhs) { m_pLhs = _pLhs; /*_assign(m_pLhs, _pLhs, true);*/ }
    void setRhs(CType * _pRhs) { m_pRhs = _pRhs; /*_assign(m_pRhs, _pRhs, true);*/ }

    /*CConstraint & operator =(const CConstraint & _other) {
        if (_other.m_pLhs)
            _other.lhs().setParent(NULL);
        if (_other.m_pRhs)
            _other.rhs().setParent(NULL);
        _assign(m_pLhs, _other.m_pLhs, true);
        _assign(m_pRhs, _other.m_pRhs, true);
        return * this;
    }*/

private:
    CType * m_pLhs, * m_pRhs;
};

class CTypeSetterBase {
public:
    virtual ~CTypeSetterBase() {}
    virtual void setType(ir::CType * _pType) = 0;
};

template <typename Node>
class CTypeSetter : public CTypeSetterBase {
public:
    CTypeSetter(Node * _pNode) : m_pNode(_pNode) {}

    virtual void setType(ir::CType * _pType) { m_pNode->setType(_pType); }

private:
    Node * m_pNode;
};

class CFreshType : public CType {
public:
    template <typename Node>
    CFreshType(Node * _pNode) {
        m_pTypeSetter = new CTypeSetter<Node>(_pNode);
    }

    virtual ~CFreshType() { delete m_pTypeSetter; }

    void replaceType(ir::CType * _pType) { m_pTypeSetter->setType(_pType); }

    /// Get type kind.
    /// \returns #Fresh.
    virtual int getKind() const { return Fresh; }

private:
    CTypeSetterBase * m_pTypeSetter;
};

typedef std::list<CConstraint> constraints_t;

} // namespace ir

void typecheck(ir::CPredicate * _pPredicate, CContext & _ctx);
#endif

#endif /* TYPECHECK_H_ */
