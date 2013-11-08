/// \file type_lattice.h
///

#ifndef TYPE_LATTICE_H_
#define TYPE_LATTICE_H_

#include "typecheck.h"

namespace tc {

typedef Auto<class Relation> RelationPtr;
typedef std::set<class RelationPtrPair> RelationPtrPairs;

class Relation : public Formula {
public:
    // Doesn't affect sorting order, so can be made mutable.
    mutable bool bUsed = true, bFlag = false;
    RelationPtrPairs inferedFrom;

    Relation() : Formula(SUBTYPE, NULL, NULL) {}

    Relation(const Formula &_formula, const RelationPtrPairs &_inferedFrom = RelationPtrPairs()) :
        Formula(_formula), inferedFrom(_inferedFrom) {}

    Relation(const ir::TypePtr &_pLhs, const ir::TypePtr &_pRhs, bool _bStrict = false,
            bool _bUsed = true, const RelationPtrPairs &_inferedFrom = RelationPtrPairs());

    bool isStrict() const { return is(SUBTYPE_STRICT); }
    void setStrict(bool _bStrict) { return setKind(_bStrict ? SUBTYPE_STRICT : SUBTYPE); }
};

struct RelationPtrPair : public std::pair<RelationPtr, RelationPtr> {
    RelationPtrPair(const RelationPtr &_first, const RelationPtr &_second) {
        FormulaCmp less;

        if (!_second || (_first && less(_first, _second))) {
            first = _first;
            second = _second;
        } else {
            first = _second;
            second = _first;
        }
    }

    bool operator<(const RelationPtrPair &_other) const {
        FormulaCmp less;

        if (*first != *_other.first)
            return less(first, _other.first);
        if (_other.second)
            return second ? less(second, _other.second) : true;
        return false;
    }
};

class Relations : public std::set<RelationPtr, FormulaCmp> {
public:
    enum { ANY, LOWERS, UPPERS };

    Relations(int _kind = ANY) : m_kind(_kind) {}

    int getKind() const { return m_kind; }

    ir::TypePtr getType(const_iterator _iNode) const {
        assert(m_kind != ANY);
        return m_kind == LOWERS ? (*_iNode)->getLhs() : (*_iNode)->getRhs();
    }

    void setType(iterator _iNode, const ir::TypePtr &_pType) const {
        assert(m_kind != ANY);
        if (m_kind == LOWERS)
            (*_iNode)->setLhs(_pType);
        else
            (*_iNode)->setRhs(_pType);
    }

private:
    int m_kind;
};

typedef Auto<class TypeNode> TypeNodePtr;

struct TypeNode : public Counted {
    ir::TypePtr pType;
    mutable Relations lowers, uppers;

    // Fields below are only used for graph traversal.
    mutable const TypeNode *pPrev;
    mutable bool bPrevStrict;
    mutable int nWeight;

    bool bNonFresh;

    TypeNode(const ir::TypePtr &_pType = NULL, bool _bNonFresh = false) :
        pType(_pType), lowers(Relations::LOWERS), uppers(Relations::UPPERS), pPrev(NULL), bPrevStrict(false), nWeight(0), bNonFresh(_bNonFresh) {}

    TypeNode(const TypeNode &_other) :
        pType(_other.pType), lowers(_other.lowers), uppers(_other.uppers),
        pPrev(_other.pPrev), bPrevStrict(_other.bPrevStrict), nWeight(_other.nWeight),
        bNonFresh(_other.bNonFresh)
    {}

    TypeNode &operator =(const TypeNode &_other) {
        pType = _other.pType;
        lowers = _other.lowers;
        uppers = _other.uppers;
        pPrev = _other.pPrev;
        bPrevStrict = _other.bPrevStrict;
        nWeight = _other.nWeight;
        bNonFresh = _other.bNonFresh;
        return *this;
    }
};

struct TypeNodeTypeCmp {
    bool operator()(const TypeNode &_lhs, const TypeNode &_rhs) const;
};

struct TypeNodePtrWeightCmp {
    bool operator()(const TypeNode *_pLhs, const TypeNode *_pRhs) const;
};

typedef std::set<TypeNode, TypeNodeTypeCmp> TypeNodes;
typedef std::set<const TypeNode *, TypeNodePtrWeightCmp> TypeNodeQueue;
typedef std::multimap<ir::TypePtr, FormulaPtr, PtrLess<ir::Type> > FormulasByType;
typedef std::map<ir::TypePtr, ir::TypePtr, PtrLess<ir::Type> > TypeMap;

class Lattice : public Counted {
public:
    typedef bool (*RelationHandler)(const RelationPtr &, Lattice &, void *);

    Lattice(Context *_pCtx) : m_pCtx(_pCtx), m_bValid(false) {}

    void invalidate() { m_bValid = false; }
    const Relations &lowers(const ir::TypePtr &_pType);
    const Relations &uppers(const ir::TypePtr &_pType);
    const TypeNodes &nodes();
    const TypeNode &get(const ir::TypePtr &_pType);
    const TypeNodes &reduce();
    const Relations &relations() const { return m_relations; }

    bool isValid() const { return m_bValid; }
    void update(RelationHandler _handler = NULL, void *_pParam = NULL);
    void makeNodes(TypeNodeQueue &_nodes);
    void cleanUp();
    void dump();

private:
    Context *m_pCtx;
    bool m_bValid;
    TypeNodes m_types;
    Relations m_relations;

    enum MergeResult {
        MR_UNCHANGED,
        MR_MERGED,
        MR_FAILED
    };

    const TypeNode &_getNode(const ir::TypePtr &_pType, bool *_pbAdded = NULL);
    MergeResult _merge(std::list<RelationPtr> &_added, RelationHandler _handler, void *_pParam);
};

}; // namespace tc

#endif /* TYPE_LATTICE_H_ */
