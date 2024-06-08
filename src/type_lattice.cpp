/// \file type_lattice.cpp
///

#include <iostream>

#include "utils.h"
#include "typecheck.h"
#include "pp_syntax.h"
#include "ir/visitor.h"
#include "type_lattice.h"

using namespace tc;

Relation::Relation(const ir::TypePtr &_pLhs, const ir::TypePtr &_pRhs, bool _bStrict, bool _bUsed, const RelationPtrPairs &_inferedFrom) :
    Formula(_bStrict ? SUBTYPE_STRICT : SUBTYPE, _pLhs, _pRhs), bUsed(_bUsed), inferedFrom(_inferedFrom)
{
}

static const TypeNode g_emptyNode(NULL);

const Relations &Lattice::lowers(const ir::TypePtr &_pType) {
    return get(_pType).lowers;
}

const Relations &Lattice::uppers(const ir::TypePtr &_pType) {
    return get(_pType).uppers;
}

const TypeNodes &Lattice::nodes() {
    if (!isValid())
        update();

    return m_types;
}

const TypeNode &Lattice::get(const ir::TypePtr &_pType) {
    if (!isValid())
        update();

    TypeNodes::const_iterator iNode = m_types.find(_pType);

    return iNode == m_types.end() ? g_emptyNode : *iNode;
}

class TypeEnumerator : public ir::Visitor {
public:
    TypeEnumerator(const FormulaPtr &_pFormula, FormulasByType &_t2f) : m_pFormula(_pFormula), m_t2f(_t2f) {}

    bool visitType(const ir::TypePtr &_type) override {
        m_t2f.insert(std::make_pair(_type, m_pFormula));
        return true;
    }

private:
    FormulaPtr m_pFormula;
    FormulasByType &m_t2f;
};

bool TypeNodeTypeCmp::operator()(const TypeNode &_lhs, const TypeNode &_rhs) const {
    return *_lhs.pType < *_rhs.pType;
}

bool TypeNodePtrWeightCmp::operator()(const TypeNode *_pLhs, const TypeNode *_pRhs) const {
    // We need nodes with greater weights to pop up first.
    if (_pLhs->nWeight != _pRhs->nWeight)
        return _pLhs->nWeight > _pRhs->nWeight;

    // Weights being equal, we need nodes with fewer lowers to pop up first.
    if (_pLhs->lowers.size() != _pRhs->lowers.size())
        return _pLhs->lowers.size() < _pRhs->lowers.size();

    return *_pLhs->pType < *_pRhs->pType;
}

void Lattice::dump() {
    std::wcerr << L"-------- Lattice:\n";

    // Pretty-print only.
    for (TypeNodes::iterator i = m_types.begin(); i != m_types.end(); ++i) {
        Relations &lowers = i->lowers;
        Relations &uppers = i->uppers;
        ir::TypePtr pOrig = i->pType;

        prettyPrintCompact(pOrig, std::wcerr);
        std::wcerr << L"\n  uppers:";

        for (Relations::iterator iUpper = uppers.begin(); iUpper != uppers.end(); ++iUpper) {
            std::wcerr << L" ";
            prettyPrintCompact(uppers.getType(iUpper), std::wcerr);
            std::wcerr << ((*iUpper)->bUsed ? L"+" : L"-");
        }

        std::wcerr << L"\n  lowers:";

        for (Relations::iterator iLower = lowers.begin(); iLower != lowers.end(); ++iLower) {
            std::wcerr << L" ";
            prettyPrintCompact((*iLower)->getLhs(), std::wcerr);
            std::wcerr << ((*iLower)->bUsed ? L"+" : L"-");
        }

        std::wcerr << L"\n";
    }

    std::wcerr << L"-------- Implications:\n";

    for (Relations::iterator i = m_relations.begin(); i != m_relations.end(); ++i) {
        RelationPtr pRelation = *i;

        prettyPrint(*pRelation, std::wcerr, false);
        std::wcerr << L" -|";

        for (RelationPtrPairs::iterator j = pRelation->inferedFrom.begin(); j != pRelation->inferedFrom.end(); ++j) {
            std::wcerr << L"\n  ";

            if (j != pRelation->inferedFrom.begin())
                std::wcerr << L"or ";

            std::wcerr << L"(";

            prettyPrint(*j->first, std::wcerr, false);

            if (j->second) {
                std::wcerr << L" and ";
                prettyPrint(*j->second, std::wcerr, false);
            }

            std::wcerr << L")";
        }

        std::wcerr << L"\n";
    }
}

const TypeNodes &Lattice::reduce() {
    TypeNodeQueue tns;

    if (!isValid())
        update();

    makeNodes(tns);

    while (!tns.empty()) {
        TypeNodeQueue::iterator iNode = tns.begin();
        const TypeNode *pNode = *iNode;

        for (Relations::const_iterator i = pNode->uppers.begin(); i != pNode->uppers.end(); ++i) {
            TypeNodes::iterator iNext = m_types.find(pNode->uppers.getType(i));

            if (&*iNext == pNode) {
                assert(false); // Type is it's own upper/lower bound.
                continue;
            }

            const TypeNode *pNext = &*iNext;

            tns.erase(pNext);
            assert(pNext != pNode);

            if (pNext->pPrev) {
                const TypeNode *q = pNext->pPrev; // Try to find this node in current path.

                for (const TypeNode *p = pNode->pPrev; p; p = p->pPrev)
                    if (p == q) {
                        TypeNodes::iterator k;

                        // Mark relation as unused.
                        for (Relations::const_iterator j = p->uppers.begin(); j != p->uppers.end(); ++j) {
                            k = m_types.find(pNode->uppers.getType(j));
                            if (k != m_types.end() && &*k == pNext) {
                                (*j)->bUsed = false;
                                break;
                            }
                        }

                        for (Relations::const_iterator j = pNext->lowers.begin(); j != pNext->lowers.end(); ++j) {
                            k = m_types.find(pNode->lowers.getType(j));
                            if (k != m_types.end() && &*k == p) {
                                (*j)->bUsed = false;
                                break;
                            }
                        }

                        break;
                    }
            }

            pNext->pPrev = pNode;
            pNext->nWeight = pNode->nWeight + 1;
            tns.insert(pNext);
        }

        tns.erase(iNode);
    }

    // Mark relations that are implied by other relations in the reduced graph.
    for (Relations::iterator i = m_relations.begin(); i != m_relations.end(); ++i) {
        Relation &r = **i;

        if (r.bUsed) {
            for (RelationPtrPairs::iterator j = r.inferedFrom.begin(); j != r.inferedFrom.end(); ++j)
                if (m_relations.find(j->first) != m_relations.end() && (!j->second || m_relations.find(j->second) != m_relations.end())) {
                    r.bFlag = true;
                    break;
                }
        }
    }

    // Update used flags: mark redundant relations as unused.
    for (Relations::iterator i = m_relations.begin(); i != m_relations.end(); ++i) {
        Relation &r = **i;

        if (r.bUsed && (r.bFlag || r.eval() == Formula::TRUE))
            r.bUsed = false;
    }

    cleanUp();

    return m_types;
}

void Lattice::cleanUp() {
    // Cleanup unused.
    for (TypeNodes::iterator i = m_types.begin(); i != m_types.end();) {
        Relations &lowers = i->lowers;
        Relations &uppers = i->uppers;
        TypeNodes::iterator iNext = ::next(i);

        if (i->bNonFresh)
            m_types.erase(i);
        else {
            for (Relations::iterator j = lowers.begin(); j != lowers.end();) {
                Relations::iterator k = ::next(j);

                if (!(*j)->bUsed)
                    lowers.erase(j);

                j = k;
            }

            for (Relations::iterator j = uppers.begin(); j != uppers.end();) {
                Relations::iterator k = ::next(j);

                if (!(*j)->bUsed)
                    uppers.erase(j);

                j = k;
            }

            if (uppers.empty() && lowers.empty()) {
                m_types.erase(i);
            }
        }

        i = iNext;
    }
}

static
const TypeNode &_addNode(TypeNodes &_nodes, const ir::TypePtr &_pType, bool _bNonFresh) {
    return *_nodes.insert(TypeNode(_pType, _bNonFresh)).first;
}

void Lattice::makeNodes(TypeNodeQueue &_nodes) {
    TypeNodes added;

    // Insert non-fresh types that aren't in m_types yet.
    for (TypeNodes::iterator i = m_types.begin(); i != m_types.end(); ++i) {
        Relations &lowers = i->lowers;
        Relations &uppers = i->uppers;

        for (Relations::const_iterator j = lowers.begin(); j != lowers.end(); ++j) {
            ir::TypePtr pType = lowers.getType(j);

            if (m_types.find(TypeNode(pType)) == m_types.end())
                _addNode(added, pType, true).uppers.insert(*j);
        }

        for (Relations::const_iterator j = uppers.begin(); j != uppers.end(); ++j) {
            ir::TypePtr pType = uppers.getType(j);

            // Insert non-fresh type that isn't in m_types yet.
            if (m_types.find(TypeNode(pType)) == m_types.end())
                _addNode(added, pType, true).lowers.insert(*j);
        }
    }

    m_types.insert(added.begin(), added.end());

    for (TypeNodes::iterator i = m_types.begin(); i != m_types.end(); ++i)
        _nodes.insert(&*i);
}

const TypeNode &Lattice::_getNode(const ir::TypePtr &_pType, bool *_pbAdded) {
    std::pair<TypeNodes::iterator, bool> result = m_types.insert(TypeNode(_pType));

    if (_pbAdded)
        *_pbAdded |= result.second;

    return *result.first;
}

Lattice::MergeResult Lattice::_merge(std::list<RelationPtr> &_added, RelationHandler _handler, void *_pParam) {
    MergeResult result = MR_UNCHANGED;

    for (std::list<RelationPtr>::iterator i = _added.begin(); i != _added.end(); ++i) {
        RelationPtr pRelation = *i;
        const bool bLFresh = pRelation->getLhs()->hasFresh();
        const bool bRFresh = pRelation->getRhs()->hasFresh();

        if (!bLFresh && !bRFresh)
            continue;

        Relations::iterator k = m_relations.find(pRelation);

        if (k != m_relations.end()) {
            for (RelationPtrPairs::iterator j = pRelation->inferedFrom.begin(); j != pRelation->inferedFrom.end(); ++j) {
                RelationPtrPair p = *j;

                if (*p.first == **k || (p.second && *p.second == **k))
                    continue;

                if (p.second && p.second->eval() != Formula::UNKNOWN)
                    p.second = NULL;

                if (p.first->eval() != Formula::UNKNOWN)
                    p.first = p.second;

                if (p.first && (*k)->inferedFrom.insert(p).second)
                    result = MR_MERGED;
            }

            continue;
        }

        if (*pRelation->getRhs() == *pRelation->getLhs())
            continue;

        if (_handler && !(*_handler)(pRelation, *this, _pParam))
            return MR_FAILED;

        m_relations.insert(pRelation);

        if (bLFresh)
            _getNode(pRelation->getLhs()).uppers.insert(pRelation);

        if (bRFresh)
            _getNode(pRelation->getRhs()).lowers.insert(pRelation);

        result = MR_MERGED;
    }

    _added.clear();

    return result;
}

void Lattice::update(RelationHandler _handler, void *_pParam) {
    TypeMap substs;

    m_types.clear();
    m_relations.clear();

    for (ContextIterator it(m_pCtx, true, true); !it.eof(); it.next()) {
        Formula &f = *it.get();

        if (f.is(Formula::EQUALS))
            substs[f.getLhs()] = f.getRhs();
    }

    std::list<RelationPtr> added;
    MergeResult mr = MR_UNCHANGED;

    for (ContextIterator it(m_pCtx, true, true); !it.eof(); it.next()) {
        if (!it.get()->is(Formula::SUBTYPE | Formula::SUBTYPE_STRICT))
            continue;

        if (!it.get()->getLhs()->hasFresh() && !it.get()->getRhs()->hasFresh())
            continue;

        FormulaPtr pFormula = NULL;

        for (TypeMap::iterator j = substs.begin(); j != substs.end(); ++j) {
            if (!it.get()->contains(*j->first))
                continue;

            if (!pFormula)
                pFormula = std::make_shared<Formula>(*it.get());

            pFormula->rewrite(j->first, j->second, false);
        }

        RelationPtr pRelation = std::make_shared<Relation>(pFormula ? *pFormula : *it.get());

        if (pRelation->isStrict() || *pRelation->getLhs() != *pRelation->getRhs()) {
            added.push_back(pRelation);

            if (pRelation->getLhs()->hasFresh())
                _getNode(pRelation->getLhs()).uppers.insert(pRelation);

            if (pRelation->getRhs()->hasFresh())
                _getNode(pRelation->getRhs()).lowers.insert(pRelation);
        }
    }

    while ((mr = _merge(added, _handler, _pParam)) == MR_MERGED) {
        // Build index.
        FormulasByType t2f;

        for (Relations::iterator i = m_relations.begin(); i != m_relations.end(); ++i) {
            TypeEnumerator(*i, t2f).traverseType((*i)->getLhs());
            TypeEnumerator(*i, t2f).traverseType((*i)->getRhs());
        }

        // Rewrite contained types.
        for (TypeNodes::iterator i = m_types.begin(); i != m_types.end(); ++i) {
            ir::TypePtr pType = i->pType;
            Relations &lowers = i->lowers;
            Relations &uppers = i->uppers;
            std::pair<FormulasByType::iterator, FormulasByType::iterator> used = t2f.equal_range(pType);

            for (FormulasByType::iterator j = used.first; j != used.second; ++j) {
                const auto f = j->second;
                FormulaPtr pLowest = clone(f);
                FormulaPtr pHighest = clone(f);

                pLowest->rewrite(pType, std::make_shared<ir::Type>(ir::Type::BOTTOM), false);
                pHighest->rewrite(pType, std::make_shared<ir::Type>(ir::Type::TOP), false);

                const bool bDownwards = pLowest->getLhs()->compare(*f->getLhs(), ir::Type::ORD_SUB | ir::Type::ORD_EQUALS) &&
                        f->getRhs()->compare(*pLowest->getRhs(), ir::Type::ORD_SUB | ir::Type::ORD_EQUALS);
                const bool bUpwards = pHighest->getLhs()->compare(*f->getLhs(), ir::Type::ORD_SUB | ir::Type::ORD_EQUALS) &&
                        f->getRhs()->compare(*pHighest->getRhs(), ir::Type::ORD_SUB | ir::Type::ORD_EQUALS);

                if (!bDownwards && !bUpwards)
                    continue;

                const bool bRewriteLeft = *pType == *f->getLhs() || f->getLhs()->contains(*pType);
                const bool bRewriteRight = *pType == *f->getRhs() || f->getRhs()->contains(*pType);
                const int mtl = f->getLhs()->getMonotonicity(*pType);
                const int mtr = f->getRhs()->getMonotonicity(*pType);

                // Substitute pType->lowers into f.
                if (bDownwards)
                    for (Relations::iterator k = lowers.begin(); k != lowers.end(); ++k) {
                        // lower.pType <= pType (or < if lower.bStrict).

                        if (**k == *f)
                            continue;

                        const bool bStrict = (*k)->isStrict() || f->is(Formula::SUBTYPE_STRICT);
                        RelationPtr pNew = std::make_shared<Relation>(clone(f->getLhs()), clone(f->getRhs()), bStrict);

                        pNew->rewrite(pType, lowers.getType(k), false);
                        auto a = *k;
                        pNew->inferedFrom.insert(RelationPtrPair(f->as<Relation>(), *k));
                        added.push_back(pNew);

                        if (bRewriteLeft) {
                            if (mtl == ir::Type::MT_MONOTONE)
                                added.push_back(std::make_shared<Relation>(pNew->getLhs(), f->getLhs(), bStrict));
                            else if (mtl == ir::Type::MT_ANTITONE)
                                added.push_back(std::make_shared<Relation>(f->getLhs(), pNew->getLhs(), bStrict));

                            added.back()->inferedFrom.insert(RelationPtrPair(*k, NULL));
                        }

                        if (bRewriteRight) {
                            if (mtr == ir::Type::MT_MONOTONE)
                                added.push_back(std::make_shared<Relation>(pNew->getRhs(), f->getRhs(), bStrict));
                            else if (mtr == ir::Type::MT_ANTITONE)
                                added.push_back(std::make_shared<Relation>(f->getRhs(), pNew->getRhs(), bStrict));

                            added.back()->inferedFrom.insert(RelationPtrPair(*k, NULL));
                        }
                    }

                // Substitute pType->uppers into f.
                if (bUpwards)
                    for (Relations::iterator k = uppers.begin(); k != uppers.end(); ++k) {
                        // pType <= upper.pType (or < if upper.bStrict).

                        if (**k == *f)
                            continue;

                        const bool bStrict = (*k)->isStrict() || f->is(Formula::SUBTYPE_STRICT);
                        RelationPtr pNew = std::make_shared<Relation>(clone(f->getLhs()), clone(f->getRhs()), bStrict);

                        pNew->rewrite(pType, uppers.getType(k), false);
                        pNew->inferedFrom.insert(RelationPtrPair(f->as<Relation>(), *k));
                        added.push_back(pNew);

                        if (bRewriteLeft) {
                            if (mtl == ir::Type::MT_MONOTONE)
                                added.push_back(std::make_shared<Relation>(f->getLhs(), pNew->getLhs(), bStrict));
                            else if (mtl == ir::Type::MT_ANTITONE)
                                added.push_back(std::make_shared<Relation>(pNew->getLhs(), f->getLhs(), bStrict));

                            added.back()->inferedFrom.insert(RelationPtrPair(*k, NULL));
                        }

                        if (bRewriteRight) {
                            if (mtr == ir::Type::MT_MONOTONE)
                                added.push_back(std::make_shared<Relation>(f->getRhs(), pNew->getRhs(), bStrict));
                            else if (mtr == ir::Type::MT_ANTITONE)
                                added.push_back(std::make_shared<Relation>(pNew->getRhs(), f->getRhs(), bStrict));

                            added.back()->inferedFrom.insert(RelationPtrPair(*k, NULL));
                        }
                    }
            }
        }

        // Find std::make_shared<meets: given A <= B, A <= C, D = B/\C, add A <= D, D <= B, D <= C.
        for (TypeNodes::iterator i = m_types.begin(); i != m_types.end(); ++i) {
            ir::TypePtr pType = i->pType;
            Relations &lowers = i->lowers;
            Relations &uppers = i->uppers;

            for (Relations::iterator j = uppers.begin(); j != uppers.end(); ++j) {
                const auto u = *j;

                for (Relations::iterator k = ::next(j); k != uppers.end(); ++k) {
                    const auto v = *k;
                    const auto pMeet = v->getRhs()->getMeet(u->getRhs());

                    if (pMeet) {
                        if (*pMeet != *u->getRhs()) { // pMeet <= u.getRhs()
                            added.push_back(std::make_shared<Relation>(pMeet, u->getRhs()));
                        } else { // u.getRhs() <= v.getRhs(), u implies v
                            added.push_back(std::make_shared<Relation>(*v));
                            added.back()->inferedFrom.insert(RelationPtrPair(u, NULL));
                        }

                        if (*pMeet != *v->getRhs()) {// pMeet <= v.getRhs()
                            added.push_back(std::make_shared<Relation>(pMeet, v->getRhs()));
                        } else { // v.getRhs() <= u.getRhs(), v implies u
                            added.push_back(std::make_shared<Relation>(*u));
                            added.back()->inferedFrom.insert(RelationPtrPair(v, NULL));
                        }

                        if (*pMeet != *pType) // pType <= pMeet
                            added.push_back(std::make_shared<Relation>(pType, pMeet));
                    }
                }
            }

            // Find std::make_shared<joins: given A <= C, B <= C, D = A\/B, add A <= D, B <= D, D <= C.
            for (Relations::iterator j = lowers.begin(); j != lowers.end(); ++j) {
                const auto u = *j;

                for (Relations::iterator k = ::next(j); k != lowers.end(); ++k) {
                    const auto v = *k;
                    const auto pJoin = v->getLhs()->getJoin(u->getLhs());

                    if (pJoin) {
                        if (*pJoin != *u->getLhs()) { // u.getLhs() <= pJoin
                            added.push_back(std::make_shared<Relation>(u->getLhs(), pJoin));
                        } else { // v.getLhs() <= u.getLhs(), u implies v
                            added.push_back(std::make_shared<Relation>(*v));
                            added.back()->inferedFrom.insert(RelationPtrPair(u, NULL));
                        }

                        if (*pJoin != *v->getLhs()) { // v.getLhs() <= pJoin
                            added.push_back(std::make_shared<Relation>(v->getLhs(), pJoin));
                        } else { // u.getLhs() <= v.getLhs(), v implies u
                            added.push_back(std::make_shared<Relation>(*u));
                            added.back()->inferedFrom.insert(RelationPtrPair(v, NULL));
                        }

                        if (*pJoin != *pType) // pJoin <= pType
                            added.push_back(std::make_shared<Relation>(pJoin, pType));
                    }
                }
            }
        }


    }

    m_bValid = mr != MR_FAILED;
}

bool Lattice::traverse(const Handler& _handler, bool _bOnlyFresh, const Types& _ignored) {
    bool bModified = false;

    for (tc::TypeNodes::const_iterator i = m_types.begin(); i != m_types.end(); ++i) {
        if (i->pType->getKind() != ir::Type::FRESH && _bOnlyFresh)
            continue;
        if (_ignored.find(i->pType) != _ignored.end())
            continue;
        bModified |= _handler(i->pType, i->lowers, i->uppers);
    }

    return bModified;
}
